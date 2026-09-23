#!/usr/bin/env python3
"""Prepare one release in a private expansion repository; never publish refs."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from integrate_source_release import manifest_path_for, ported_radxa_source_snapshot, upsert_manifest
from release_expansion import save
from render_release_branch import render_from_plan, validate_release_metadata
from reconstruction_common import (
    BUILD_INFRA_OVERLAY_PATHS, ReconstructionError, check_immutable_refs,
    clear_metadata_caches, git, load_ref_records, main_wrapper, matrix_release_values,
    radxa_source_ref, ref_exists, rev_parse, tree_id, version_key,
    update_ref_record, synthesise_release_entry, release_to_branch,
)
from source_porting import (
    align_release_metadata, apply_source_delta_to_base, resolved_source_port_stage,
    resume_source_delta_tree,
)
from source_policy import enforce_source_tree_policy
from uplift_radxa_release import port_candidate
from validate_release_inputs import input_problems, validate_inputs


def checkpoint(edk2: str, radxa: str) -> str:
    return f"source/unofficial/{radxa}/edk2-stable{edk2}"


def target(edk2: str, radxa: str) -> str:
    return f"edk2-{edk2}/radxa-{radxa}/unofficial"


def valid_source(repo: Path, edk2: str, radxa: str) -> str | None:
    try:
        entry = synthesise_release_entry(repo, release_to_branch(target(edk2, radxa)))
        if not input_problems(repo, entry):
            return entry["source_ref"]
    except ReconstructionError:
        pass
    return None


def register_new(repo: Path, journal: Path, ref: str, oid: str, metadata: dict) -> None:
    """Journal before CAS creation so interruption cannot strand an unmanifested ref."""
    record = {"ref": ref, "object_id": oid, "tree_id": tree_id(repo, oid),
              "immutable": True, **metadata}
    manifest = "config/refs-unofficial.json" if ref.startswith("source/unofficial/") else manifest_path_for(ref)
    pending = {"ref": ref, "oid": oid, "record": record, "manifest": manifest,
               "manifest_before": (repo / manifest).read_text()}
    save(journal, pending)
    finish_registration(repo, journal)


def finish_registration(repo: Path, journal: Path) -> None:
    if not journal.exists():
        return
    clear_metadata_caches()
    pending = json.loads(journal.read_text())
    manifest = repo / pending["manifest"]
    try:
        json.loads(manifest.read_text())
    except (json.JSONDecodeError, FileNotFoundError):
        # A crash inside the existing manifest writer may leave a truncated
        # file. Restore only this journal's preimage before replaying its write.
        save(manifest, json.loads(pending["manifest_before"]))
    ref, oid = pending["ref"], pending["oid"]
    if ref_exists(repo, ref):
        if rev_parse(repo, ref) != oid:
            raise ReconstructionError(f"registration conflicts with existing ref: {ref}")
    else:
        git(repo, "update-ref", "refs/heads/" + ref, oid, "0" * 40)
    if ref.startswith("source/unofficial/"):
        update_ref_record(repo, "refs-unofficial.json", ref, pending["record"])
    else:
        upsert_manifest(repo, ref, pending["record"])
    clear_metadata_caches()
    journal.unlink()


def prepare(repo: Path, edk2: str, radxa: str, journal: Path,
            resolutions: dict) -> dict:
    finish_registration(repo, journal)
    check_immutable_refs(repo)
    base = "edk2-stable" + edk2
    exact = checkpoint(edk2, radxa)
    source = valid_source(repo, edk2, radxa)
    if ref_exists(repo, exact) and not source:
        raise ReconstructionError(f"existing checkpoint fails provenance; review required: {exact}")

    records = load_ref_records(repo)
    radxas = sorted({r["radxa_release"] for r in records
                     if r.get("type") == "vendor-source" and r.get("vendor") == "radxa"},
                    key=version_key)
    # Prefer the nearest usable earlier Radxa on this EDK2. Otherwise port the
    # same Radxa from the nearest earlier EDK2. Neither path guesses conflicts.
    seed = None
    for older in reversed([r for r in radxas if version_key(r) < version_key(radxa)]):
        if candidate := valid_source(repo, edk2, older):
            seed = (edk2, older, candidate)
            break
    if seed is None:
        for older in reversed([r for r in matrix_release_values(repo)
                               if version_key(r) < version_key(edk2)]):
            if candidate := valid_source(repo, older, radxa):
                seed = (older, radxa, candidate)
                break

    try:
        port = radxa_source_ref(repo, radxa, base)
    except ReconstructionError:
        if seed is None:
            raise ReconstructionError(f"no reviewed seed available for {target(edk2, radxa)}")
        old_edk2, old_radxa, old_source = seed
        old_base = "edk2-stable" + old_edk2
        port = f"source/port/radxa/{radxa}/{base}"
        if resolutions.get("port_ref"):
            oid = rev_parse(repo, resolutions["port_ref"])
        elif old_edk2 == edk2:
            oid = port_candidate(repo, from_release=old_radxa, to_release=radxa,
                                 edk2_base=base, resolved_ref="", verbose=False)
        else:
            oid = ported_radxa_source_snapshot(
                repo, radxa_source_ref(repo, radxa, old_base), radxa, old_base, base, False)
        enforce_source_tree_policy(repo, ref=oid)
        register_new(repo, journal, port, oid, {
            "type": "ported-vendor-source", "vendor": "radxa", "radxa_release": radxa,
            "edk2_base": base, "base_ref": f"source/cache/base/edk2/{base}",
            "ported_from": radxa_source_ref(repo, old_radxa, old_base),
            "format": "materialised source tree",
        })

    if source is None:
        if seed is None:
            raise ReconstructionError(f"no reviewed unofficial seed for {target(edk2, radxa)}")
        old_edk2, old_radxa, old_source = seed
        old_port = radxa_source_ref(repo, old_radxa, "edk2-stable" + old_edk2)
        label = f"expansion-{edk2}-{radxa}"
        message = (f"source: prepare custom Radxa {radxa} on {base}\n\n"
                   f"Source-Port-From: {old_port}\nSource-Port-To: {port}\n"
                   f"Source-Unofficial-From: {old_source}\n")
        if resolutions.get("unofficial_ref"):
            resolved = rev_parse(repo, resolutions["unofficial_ref"])
            stage = resolved_source_port_stage(
                repo, resolved, resolutions.get("unofficial_stage", "auto"),
                stage_variable="unofficial_stage")
            tree = resume_source_delta_tree(
                repo, resolved=resolved, stage=stage, source_ref=old_source,
                new_base_ref=port, label=label, resume_variable="unofficial_ref", verbose=False)
            oid = git(repo, "commit-tree", tree, "-m", message).stdout.strip()
        else:
            oid = apply_source_delta_to_base(
                repo, old_base_ref=old_port, source_ref=old_source, new_base_ref=port,
                message=message, label=label, source_owned_paths=BUILD_INFRA_OVERLAY_PATHS,
                normalise_source=True, resume_variable="unofficial_ref", verbose=False)
        source = align_release_metadata(repo, candidate=oid, new_port_ref=port,
                                        to_release=radxa, verbose=False)
    else:
        old_source = source

    enforce_source_tree_policy(repo, ref=source)
    if not ref_exists(repo, exact):
        register_new(repo, journal, exact, rev_parse(repo, source), {
            "type": "unofficial-release-checkpoint", "line": radxa.rsplit(".", 1)[0],
            "radxa_release": radxa, "edk2_base": base, "radxa_source_ref": port,
            "previous_unofficial_ref": old_source,
            "previous_unofficial_object_id": rev_parse(repo, old_source),
        })
    entry = synthesise_release_entry(repo, release_to_branch(target(edk2, radxa)))
    validate_inputs(repo, entry)
    branch = release_to_branch(target(edk2, radxa))
    print(f"[expansion] Rendering and verifying {branch}", flush=True)
    rendered = render_from_plan(repo, branch, entry, False)
    validate_release_metadata(repo, rendered, entry, branch)
    tree = git(repo, "ls-tree", "-r", rendered).stdout.splitlines()
    if any(line.startswith("160000 ") or line.endswith("\t.gitmodules") for line in tree):
        raise ReconstructionError("rendered candidate contains gitlinks or active root .gitmodules")
    enforce_source_tree_policy(repo, ref=rendered)
    rendered_ref = f"batch/rendered/{edk2}-{radxa}"
    if ref_exists(repo, rendered_ref):
        if tree_id(repo, rendered_ref) != tree_id(repo, rendered):
            raise ReconstructionError(f"rendered candidate changed: {rendered_ref}")
        rendered = rev_parse(repo, rendered_ref)
    else:
        git(repo, "update-ref", "refs/heads/" + rendered_ref, rendered, "0" * 40)
    return {"release": target(edk2, radxa), "source_ref": entry["source_ref"],
            "source_commit": rev_parse(repo, entry["source_ref"]),
            "source_tree": tree_id(repo, entry["source_ref"]), "port_ref": port,
            "port_commit": rev_parse(repo, port), "rendered_commit": rendered,
            "rendered_tree": tree_id(repo, rendered)}


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--state", required=True, type=Path)
    p.add_argument("--edk2", required=True)
    p.add_argument("--radxa", required=True)
    p.add_argument("--result", required=True, type=Path)
    args = p.parse_args()
    state = args.state.resolve()
    repo = Path(__file__).resolve().parents[1]
    if repo != state / "repo" or not (state / "plan.json").exists():
        raise ReconstructionError("source preparation is restricted to a private batch repository")
    resolutions = json.loads((state / "resolutions.json").read_text())
    result = prepare(repo, args.edk2, args.radxa, state / "registration.json",
                     resolutions.get(f"{args.edk2}/{args.radxa}", {}))
    save(args.result, result)


if __name__ == "__main__":
    main_wrapper(main)
