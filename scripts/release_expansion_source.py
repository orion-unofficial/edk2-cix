#!/usr/bin/env python3
"""Prepare one release in a private expansion repository; never publish refs."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess

from dsdt_cpu_conflict import (
    ACCEPTED_GIT_BLOB, ACCEPTED_SHA256, DSDT_CPU_PATH,
    DsdtConflictError, resolve_dsdt_cpu_conflict,
)
from integrate_source_release import manifest_path_for, ported_radxa_source_snapshot, upsert_manifest
from release_expansion import save
from render_release_branch import (
    cached_worktree_is_dirty, render_from_plan, validate_release_metadata,
)
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
from validate_radxa13_source import validate as validate_radxa13_source


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


def unofficial_seed(repo: Path, edk2: str, radxa: str,
                    vendor_seed: tuple[str, str, str] | None) -> tuple[str, str, str] | None:
    """Replay 1.3.x custom changes from the nearest valid checkpoint on that line."""
    if radxa not in ("1.3.0", "1.3.1"):
        return vendor_seed
    for older in reversed([r for r in matrix_release_values(repo)
                           if version_key(r) < version_key(edk2)]):
        if candidate := valid_source(repo, older, radxa):
            return older, radxa, candidate
    return vendor_seed


def validate_structural_source(repo: Path, source: str, edk2: str, radxa: str) -> None:
    """Reject a 1.3.x source whose clean merges lost reviewed firmware semantics."""
    if radxa not in ("1.3.0", "1.3.1"):
        return
    problems = validate_radxa13_source(repo, source, edk2, radxa)
    if problems:
        raise ReconstructionError("Radxa 1.3 structural preflight failed:\n" +
                                  "\n".join(f"  - {problem}" for problem in problems))


def validated_final_resolution(repo: Path, resolutions: dict, resolved: str,
                               destination_port: str, destination_source: str
                               ) -> tuple[str, str]:
    """Bind a reviewed final tree to its real parent and destination inputs."""
    required = (
        "unofficial_final_commit", "unofficial_final_tree",
        "unofficial_final_parent_ref", "unofficial_final_parent_commit",
        "unofficial_final_parent_port_ref", "unofficial_final_parent_port_commit",
        "unofficial_final_destination_port_ref", "unofficial_final_destination_port_commit",
        "unofficial_final_destination_source_ref",
    )
    if any(not isinstance(resolutions.get(key), str) or not resolutions[key]
           for key in required):
        raise ReconstructionError("final unofficial resolution requires explicit identity bindings: "
                                  + ", ".join(required))
    for key in ("unofficial_final_commit", "unofficial_final_tree",
                "unofficial_final_parent_commit", "unofficial_final_parent_port_commit",
                "unofficial_final_destination_port_commit"):
        if not re.fullmatch(r"[0-9a-f]{40}", resolutions[key]):
            raise ReconstructionError(f"final unofficial resolution has invalid {key}")
    if (resolutions["unofficial_final_commit"] != resolved or
            resolutions["unofficial_final_tree"] != tree_id(repo, resolved)):
        raise ReconstructionError("final unofficial resolution commit or tree differs")
    parents = git(repo, "rev-list", "--parents", "-n", "1", resolved).stdout.split()
    parent_ref = resolutions["unofficial_final_parent_ref"]
    parent_port = resolutions["unofficial_final_parent_port_ref"]
    parent_oid = resolutions["unofficial_final_parent_commit"]
    if (parents != [resolved, parent_oid] or
            not parent_ref.startswith("source/unofficial/") or
            rev_parse(repo, parent_ref) != parent_oid):
        raise ReconstructionError("final unofficial resolution parent differs")
    parent_records = [record for record in load_ref_records(repo)
                      if record.get("ref") == parent_ref]
    if (len(parent_records) != 1 or
            parent_records[0].get("type") != "unofficial-release-checkpoint" or
            parent_records[0].get("radxa_source_ref") != parent_port or
            rev_parse(repo, parent_port) != resolutions["unofficial_final_parent_port_commit"]):
        raise ReconstructionError("final unofficial resolution parent port differs")
    if (resolutions["unofficial_final_destination_port_ref"] != destination_port or
            resolutions["unofficial_final_destination_source_ref"] != destination_source or
            rev_parse(repo, destination_port) !=
            resolutions["unofficial_final_destination_port_commit"]):
        raise ReconstructionError("final unofficial resolution destination differs")
    return parent_ref, parent_port


def _blob(repo: Path, commit: str) -> bytes:
    return subprocess.run(
        ["git", "-C", str(repo), "show", f"{commit}:{DSDT_CPU_PATH}"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    ).stdout


def _resolution_journal(state: Path, edk2: str, radxa: str) -> Path:
    return state / "jobs" / f"{edk2}-{radxa}" / "dsdt-resolution.json"


def validated_dsdt_resolution(repo: Path, journal: Path, edk2: str, radxa: str,
                              source_ref: str, base_ref: str) -> str | None:
    """Reuse only a journal whose private commit still has the reviewed parent and blob."""
    if not journal.exists():
        return None
    row = json.loads(journal.read_text())
    conflict, resolved, ref = (row.get(key, "") for key in
                               ("conflict_commit", "resolution_commit", "resolution_ref"))
    if not all(re.fullmatch(r"[0-9a-f]{40}", oid) for oid in (conflict, resolved)):
        raise ReconstructionError("DSDT resolution journal contains invalid commit identities")
    if ref != f"refs/heads/batch/resolutions/{edk2}-{radxa}-dsdt-cpu":
        raise ReconstructionError("DSDT resolution journal contains an unexpected ref")
    if (row.get("path") != DSDT_CPU_PATH or row.get("stage") != "overlay" or
            row.get("sha256") != ACCEPTED_SHA256 or row.get("blob") != ACCEPTED_GIT_BLOB or
            row.get("source_ref") != source_ref or row.get("base_ref") != base_ref):
        raise ReconstructionError("DSDT resolution journal metadata differs")
    conflict_message = git(repo, "show", "-s", "--format=%B", conflict).stdout
    if (f"source-port: conflict tree for expansion-{edk2}-{radxa}\n" not in conflict_message or
            f"Source-Port-Input: {source_ref}\n" not in conflict_message or
            f"Source-Port-New-Base: {base_ref}\n" not in conflict_message or
            "Source-Port-Conflict-Stage: overlay\n" not in conflict_message):
        raise ReconstructionError("DSDT conflict does not bind to the selected source inputs")
    parents = git(repo, "rev-list", "--parents", "-n", "1", resolved).stdout.split()
    if parents != [resolved, conflict]:
        raise ReconstructionError("DSDT resolution has an unexpected parent")
    if git(repo, "diff", "--name-only", conflict, resolved).stdout.splitlines() != [DSDT_CPU_PATH]:
        raise ReconstructionError("DSDT resolution changed paths outside the reviewed file")
    try:
        expected = resolve_dsdt_cpu_conflict(_blob(repo, conflict))
    except (DsdtConflictError, subprocess.CalledProcessError) as exc:
        raise ReconstructionError(f"DSDT conflict preimage changed: {exc}") from exc
    if _blob(repo, resolved) != expected:
        raise ReconstructionError("DSDT resolution blob differs from reviewed output")
    current = git(repo, "rev-parse", "--verify", ref, check=False)
    if current.returncode == 0:
        if current.stdout.strip() != resolved:
            raise ReconstructionError("private DSDT resolution ref changed")
    else:
        git(repo, "update-ref", ref, resolved, "0" * 40)
    return resolved


def resolve_dsdt_worktree(repo: Path, state: Path, edk2: str, radxa: str,
                          source_ref: str, base_ref: str,
                          error: ReconstructionError) -> str:
    """Commit one exact overlay conflict in the private batch and journal it."""
    match = re.search(r"source-port conflict worktree preserved at: ([^\n]+)", str(error))
    if match is None:
        raise error
    worktree = Path(match.group(1)).resolve()
    if not worktree.is_relative_to((state / "tmp").resolve()) or worktree.name != "worktree":
        raise ReconstructionError("DSDT conflict worktree is outside the private batch") from error
    notes = worktree.parent / "README.md"
    if not notes.is_file() or not worktree.is_dir():
        raise ReconstructionError("DSDT conflict worktree or notes are missing") from error
    body = notes.read_text()
    paths = [line[4:] for line in body.splitlines() if line.startswith("  - ")]
    if paths != [DSDT_CPU_PATH] or "Conflict stage: overlay\n" not in body:
        raise ReconstructionError("DSDT resolver requires one overlay-stage DSDT conflict") from error
    if Path(git(worktree, "rev-parse", "--show-toplevel").stdout.strip()) != worktree:
        raise ReconstructionError("DSDT conflict path is not its Git worktree") from error
    # Imported CRLF blobs can appear modified after checkout even when their
    # raw bytes and modes match the index. Reject only actual worktree edits.
    if cached_worktree_is_dirty(worktree):
        raise ReconstructionError("DSDT conflict worktree is dirty") from error
    conflict = git(worktree, "rev-parse", "HEAD").stdout.strip()
    if git(repo, "rev-list", "--parents", "-n", "1", conflict).stdout.split() != [conflict]:
        raise ReconstructionError("DSDT conflict commit unexpectedly has a parent") from error
    message = git(repo, "show", "-s", "--format=%B", conflict).stdout
    if (f"Source Port Conflict: expansion-{edk2}-{radxa}" not in body or
            f"Source-Port-Input: {source_ref}\n" not in message or
            f"Source-Port-New-Base: {base_ref}\n" not in message or
            f"Source-Port-Conflict-Stage: overlay\n" not in message):
        raise ReconstructionError("DSDT conflict identity differs from selected pair") from error
    try:
        resolved_bytes = resolve_dsdt_cpu_conflict(_blob(repo, conflict))
    except (DsdtConflictError, subprocess.CalledProcessError) as exc:
        raise ReconstructionError(f"DSDT conflict is outside reviewed transform: {exc}") from error
    (worktree / DSDT_CPU_PATH).write_bytes(resolved_bytes)
    git(worktree, "add", "--", DSDT_CPU_PATH)
    git(worktree, "-c", "commit.gpgsign=false", "commit", "-m",
        f"Resolve {edk2} Radxa {radxa} DSDT CPU references by topology")
    resolved = git(worktree, "rev-parse", "HEAD").stdout.strip()
    ref = f"refs/heads/batch/resolutions/{edk2}-{radxa}-dsdt-cpu"
    journal = _resolution_journal(state, edk2, radxa)
    journal.parent.mkdir(parents=True, exist_ok=True)
    save(journal, {"conflict_commit": conflict, "resolution_commit": resolved,
                   "resolution_ref": ref, "path": DSDT_CPU_PATH, "stage": "overlay",
                   "sha256": ACCEPTED_SHA256, "blob": ACCEPTED_GIT_BLOB,
                   "source_ref": source_ref, "base_ref": base_ref})
    validated_dsdt_resolution(repo, journal, edk2, radxa, source_ref, base_ref)
    if cached_worktree_is_dirty(worktree):
        raise ReconstructionError("DSDT resolution worktree contains real changes")
    git(repo, "worktree", "remove", "--force", str(worktree))
    shutil.rmtree(notes.parent)
    return resolved


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
        custom_seed = unofficial_seed(repo, edk2, radxa, seed)
        if custom_seed is None:
            raise ReconstructionError(f"no reviewed unofficial seed for {target(edk2, radxa)}")
        old_edk2, old_radxa, old_source = custom_seed
        old_port = radxa_source_ref(repo, old_radxa, "edk2-stable" + old_edk2)
        label = f"expansion-{edk2}-{radxa}"
        if (str(resolutions.get("unofficial_stage", "auto")).strip().lower() == "final" and
                not resolutions.get("unofficial_ref")):
            raise ReconstructionError("final unofficial resolution requires unofficial_ref")
        resolved = None
        if resolutions.get("unofficial_ref"):
            resolved = rev_parse(repo, resolutions["unofficial_ref"])
            stage = resolved_source_port_stage(
                repo, resolved, resolutions.get("unofficial_stage", "auto"),
                stage_variable="unofficial_stage")
            if stage == "final":
                old_source, old_port = validated_final_resolution(
                    repo, resolutions, resolved, port, exact)
        message = (f"source: prepare custom Radxa {radxa} on {base}\n\n"
                   f"Source-Port-From: {old_port}\nSource-Port-To: {port}\n"
                   f"Source-Unofficial-From: {old_source}\n")
        if resolved is not None:
            tree = resume_source_delta_tree(
                repo, resolved=resolved, stage=stage, source_ref=old_source,
                new_base_ref=port, label=label, resume_variable="unofficial_ref", verbose=False)
            oid = git(repo, "commit-tree", tree, "-m", message).stdout.strip()
        else:
            dsdt_journal = _resolution_journal(repo.parent, edk2, radxa)
            resolved = validated_dsdt_resolution(repo, dsdt_journal,
                                                 edk2, radxa, old_source, port)
            if resolved is None:
                try:
                    oid = apply_source_delta_to_base(
                        repo, old_base_ref=old_port, source_ref=old_source, new_base_ref=port,
                        message=message, label=label, source_owned_paths=BUILD_INFRA_OVERLAY_PATHS,
                        normalise_source=True, resume_variable="unofficial_ref", verbose=False)
                except ReconstructionError as error:
                    resolved = resolve_dsdt_worktree(
                        repo, repo.parent, edk2, radxa, old_source, port, error)
            if resolved is not None:
                tree = resume_source_delta_tree(
                    repo, resolved=resolved, stage="overlay", source_ref=old_source,
                    new_base_ref=port, label=label, resume_variable="unofficial_ref", verbose=False)
                oid = git(repo, "commit-tree", tree, "-m", message).stdout.strip()
        source = align_release_metadata(repo, candidate=oid, new_port_ref=port,
                                        to_release=radxa, verbose=False)
    else:
        old_source = source

    enforce_source_tree_policy(repo, ref=source)
    validate_structural_source(repo, source, edk2, radxa)
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
