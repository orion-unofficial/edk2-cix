#!/usr/bin/env python3
"""Promote an Unofficial firmware line onto a newer EDK2 release base."""

from __future__ import annotations

import argparse
import os
import time
from pathlib import Path

from import_workflow import (
    ZERO_OID,
    current_unofficial_ref,
    ensure_target_not_checked_out_dirty,
    ref_oid,
    transaction_update_refs,
)
from reconstruction_common import (
    BUILD_INFRA_OVERLAY_PATHS,
    UNOFFICIAL_REFS_MANIFEST,
    ReconstructionError,
    branch_to_ref,
    check_immutable_refs,
    clear_metadata_caches,
    git,
    format_duration,
    load_json,
    main_wrapper,
    normalise_unofficial_immutability_policy,
    radxa_source_ref,
    ref_exists,
    release_for_edk2_ref,
    repo_root,
    rev_parse,
    selected_unofficial_line_policy,
    tree_id,
    truthy,
    unofficial_line_policies,
    unofficial_source_policy,
    update_ref_record,
    version_key,
    write_json,
)
from source_porting import (
    align_release_metadata,
    apply_source_delta_to_base,
    resolved_source_port_stage,
    resume_source_delta_tree,
)


HELP = """promote-unofficial-release

Required variables:
  EDK2_BASE=<edk2-stableYYYYMM>
      New EDK2 base to promote to.
  FROM_EDK2_BASE=<edk2-stableYYYYMM>
      Previous EDK2 base used to compute the Unofficial delta.

Optional variables:
  RADXA_RELEASE=<release>
      Radxa release carried by the line. Default: selected line policy.
  LINE=<major.minor>
      Unofficial development line to advance. Default: selected line policy.
  CIX_RELEASE=<release>
      CIX component release retained in line policy. Default: selected line
      policy.
  FROM_REF=<ref>
      Source tree to port from. Default: the policy-selected Unofficial line tip.
  REF=<ref> or RESOLVED_REF=<ref>
      Reviewed commit from a conflict handoff. The command resumes all stages
      which follow that conflict before recording the promoted source.
  RESOLVED_REF_STAGE=auto|source|overlay|final
      Override resume-stage detection. Use final only for a fully reviewed and
      already-normalised tree. Default: auto.
  UPDATE_CURRENT=0|1
      Also move the policy-selected Unofficial line tip to the promoted tree.
      Default: 1.
  UPDATE_POLICY=0|1
      Update the selected line in config/policies.json.
      Default: 1.
  ALLOW_REPLACE=0|1
      Allow an existing exact source/unofficial/<release>/<EDK2_BASE>
      checkpoint to be replaced.
      Default: 0.
  WRITE=0|1
      Required before refs, tags, or config/policies.json are updated.
  V=0|1
      Print delegated git operations.

The command materialises both EDK2 bases before diffing so nested upstream
submodule gitlinks do not appear as project source changes.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--edk2-base", default=os.environ.get("EDK2_BASE", ""))
    p.add_argument("--from-edk2-base", default=os.environ.get("FROM_EDK2_BASE", ""))
    p.add_argument("--radxa-release", default=os.environ.get("RADXA_RELEASE", ""))
    p.add_argument("--line", default=os.environ.get("LINE", ""))
    p.add_argument("--cix-release", default=os.environ.get("CIX_RELEASE", ""))
    p.add_argument("--from-ref", default=os.environ.get("FROM_REF", ""))
    p.add_argument("--resolved-ref", default=os.environ.get("RESOLVED_REF", os.environ.get("REF", "")))
    p.add_argument(
        "--resolved-ref-stage",
        default=os.environ.get("RESOLVED_REF_STAGE", "auto"),
    )
    p.add_argument("--update-current", default=os.environ.get("UPDATE_CURRENT", "1"))
    p.add_argument("--update-policy", default=os.environ.get("UPDATE_POLICY", "1"))
    p.add_argument("--allow-replace", default=os.environ.get("ALLOW_REPLACE", "0"))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def normalise_edk2_base(value: str) -> str:
    value = value.strip()
    if not value:
        return value
    if value.startswith("edk2-stable"):
        return value
    if value.startswith("edk2-"):
        return "edk2-stable" + value.removeprefix("edk2-")
    return "edk2-stable" + value


def update_policy(
    repo: Path,
    *,
    line: str,
    line_ref: str,
    radxa_release: str,
    edk2_base: str,
    cix_release: str,
) -> None:
    path = repo / "config" / "policies.json"
    data = load_json(repo, "config/policies.json")
    old_policy = data.get("unofficial_source_policy")
    if not isinstance(old_policy, dict):
        raise ReconstructionError("config/policies.json is missing unofficial_source_policy")

    default_line, lines = unofficial_line_policies(old_policy)
    migrated_lines = {name: dict(record) for name, record in lines.items()}
    migrated_lines[line] = {
        "current_cix_release": cix_release,
        "current_edk2_release": release_for_edk2_ref(edk2_base),
        "current_radxa_release": radxa_release,
        "current_ref": line_ref,
    }
    data["unofficial_source_policy"] = {
        "default_line": default_line or line,
        "lines": dict(sorted(migrated_lines.items(), key=lambda item: version_key(item[0]))),
        "prefer_versioned_default_alias": truthy(
            old_policy.get("prefer_versioned_default_alias", True)
        ),
    }
    normalise_unofficial_immutability_policy(data)
    write_json(path, data)


def checkpoint_record(
    repo: Path,
    *,
    ref: str,
    source_oid: str,
    line: str,
    radxa_release: str,
    edk2_base: str,
    previous_ref: str,
    previous_object_id: str,
) -> None:
    existing = next(
        (
            record
            for record in load_json(
                repo, f"config/{UNOFFICIAL_REFS_MANIFEST}"
            ).get("refs", [])
            if record.get("ref") == ref
        ),
        None,
    )
    if existing:
        retained_ref = str(existing.get("previous_unofficial_ref", "")).strip()
        if retained_ref and retained_ref != ref:
            previous_ref = retained_ref
            retained_object_id = str(
                existing.get("previous_unofficial_object_id", "")
            ).strip()
            if retained_object_id:
                previous_object_id = retained_object_id
            elif ref_exists(repo, retained_ref):
                previous_object_id = rev_parse(repo, retained_ref)
    update_ref_record(
        repo,
        UNOFFICIAL_REFS_MANIFEST,
        ref,
        {
            "edk2_base": edk2_base,
            "immutable": True,
            "line": line,
            "object_id": source_oid,
            "previous_unofficial_ref": previous_ref,
            "previous_unofficial_object_id": previous_object_id,
            "radxa_release": radxa_release,
            "radxa_source_ref": radxa_source_ref(repo, radxa_release, edk2_base),
            "tree_id": tree_id(repo, source_oid),
            "type": "unofficial-release-checkpoint",
        },
    )


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    policy = unofficial_source_policy(repo)
    selected_line, default_selected = selected_unofficial_line_policy(policy)
    line = args.line.strip() or selected_line
    if not line:
        raise ReconstructionError(
            "LINE is required when config/policies.json has no selected Unofficial line"
        )
    _default_line, lines = unofficial_line_policies(policy)
    selected = lines.get(line, default_selected)
    radxa_release = args.radxa_release.strip() or str(
        selected.get("current_radxa_release", "")
    ).strip()
    if not radxa_release:
        raise ReconstructionError(
            "RADXA_RELEASE is required when the selected line policy has no current Radxa release"
        )
    cix_release = args.cix_release.strip() or str(
        selected.get("current_cix_release", "")
    ).strip()
    current_ref = current_unofficial_ref(repo, str(selected.get("current_ref", "")))
    expected_current_ref = f"source/unofficial/{line}/current"
    if isinstance(policy.get("lines"), dict) and current_ref != expected_current_ref:
        raise ReconstructionError(
            f"line {line} current_ref must be {expected_current_ref}, found {current_ref}"
        )
    if not isinstance(policy.get("lines"), dict):
        current_ref = expected_current_ref
    args.from_ref = args.from_ref or current_unofficial_ref(
        repo, str(selected.get("current_ref", ""))
    )
    verbose = truthy(args.v)
    write = truthy(args.write)
    update_current = truthy(args.update_current)
    update_policy_file = truthy(args.update_policy)
    allow_replace = truthy(args.allow_replace)

    edk2_base = normalise_edk2_base(args.edk2_base)
    from_edk2_base = normalise_edk2_base(args.from_edk2_base)
    missing = []
    if not edk2_base:
        missing.append("EDK2_BASE=<edk2-stableYYYYMM>")
    if not from_edk2_base:
        missing.append("FROM_EDK2_BASE=<edk2-stableYYYYMM>")
    if missing:
        print(HELP)
        print("missing required variable(s): " + ", ".join(missing))
        raise SystemExit(2)

    check_immutable_refs(repo)
    old_port_ref = radxa_source_ref(repo, radxa_release, from_edk2_base)
    new_port_ref = radxa_source_ref(repo, radxa_release, edk2_base)
    target_ref = f"source/unofficial/{radxa_release}/{edk2_base}"
    if ref_exists(repo, target_ref) and not allow_replace:
        raise ReconstructionError(f"target unofficial checkpoint already exists: {target_ref}")
    if args.resolved_ref and not ref_exists(repo, args.resolved_ref):
        raise ReconstructionError(f"RESOLVED_REF/REF is unavailable locally: {args.resolved_ref}")
    if not ref_exists(repo, args.from_ref):
        raise ReconstructionError(f"FROM_REF is unavailable locally: {args.from_ref}")
    previous_object_id = rev_parse(repo, args.from_ref)

    message = (
        f"source: promote Unofficial {line} firmware source to {edk2_base}\n\n"
        f"Source-Port-From: {old_port_ref}\n"
        f"Source-Port-To: {new_port_ref}\n"
        f"Source-Ported-Input: {args.from_ref}\n"
        f"Source-Ported-Input-Object: {previous_object_id}\n"
    )
    if args.resolved_ref:
        resolved = rev_parse(repo, args.resolved_ref)
        message += f"Source-Port-Resolution: {resolved}\n"
        stage = resolved_source_port_stage(
            repo,
            resolved,
            args.resolved_ref_stage,
            stage_variable="RESOLVED_REF_STAGE",
        )
        tree = resume_source_delta_tree(
            repo,
            resolved=resolved,
            stage=stage,
            source_ref=args.from_ref,
            new_base_ref=new_port_ref,
            label=f"unofficial-{line}-{radxa_release}-{edk2_base}",
            resume_variable="RESOLVED_REF",
            verbose=verbose,
        )
        candidate = git(repo, "commit-tree", tree, "-m", message).stdout.strip()
    else:
        candidate = apply_source_delta_to_base(
            repo,
            old_base_ref=old_port_ref,
            source_ref=args.from_ref,
            new_base_ref=new_port_ref,
            message=message,
            label=f"unofficial-{line}-{radxa_release}-{edk2_base}",
            source_owned_paths=BUILD_INFRA_OVERLAY_PATHS,
            normalise_source=True,
            resume_variable="RESOLVED_REF",
            verbose=verbose,
        )
    candidate = align_release_metadata(
        repo,
        candidate=candidate,
        new_port_ref=new_port_ref,
        to_release=radxa_release,
        verbose=verbose,
    )

    updates: list[tuple[str, str, str]] = []
    old_target = ref_oid(repo, target_ref) or ZERO_OID
    updates.append((branch_to_ref(target_ref), candidate, old_target))
    if update_current:
        updates.append((branch_to_ref(current_ref), candidate, ref_oid(repo, current_ref) or ZERO_OID))

    if not write:
        print("dry run; set WRITE=1 to promote unofficial source")
        print(f"  candidate: {candidate} (tree {tree_id(repo, candidate)})")
        for full_ref, new_oid, old_oid in updates:
            print(f"  {full_ref}: {old_oid} -> {new_oid}")
        if update_policy_file:
            print(
                f"  config/policies.json line {line} current_edk2_release "
                f"-> {release_for_edk2_ref(edk2_base)}"
            )
        return

    ensure_target_not_checked_out_dirty(repo, current_ref)
    if ref_exists(repo, target_ref):
        ensure_target_not_checked_out_dirty(repo, target_ref)
    transaction_update_refs(repo, updates)
    checkpoint_record(
        repo,
        ref=target_ref,
        source_oid=rev_parse(repo, target_ref),
        line=line,
        radxa_release=radxa_release,
        edk2_base=edk2_base,
        previous_ref=args.from_ref,
        previous_object_id=previous_object_id,
    )
    if update_policy_file:
        update_policy(
            repo,
            line=line,
            line_ref=current_ref,
            radxa_release=radxa_release,
            edk2_base=edk2_base,
            cix_release=cix_release,
        )
    clear_metadata_caches()
    print(f"promoted unofficial source to {edk2_base} in {format_duration(time.monotonic() - started)}")
    for full_ref, new_oid, old_oid in updates:
        print(f"  {full_ref}: {old_oid} -> {new_oid}")
    if update_policy_file:
        print(
            f"  config/policies.json line {line} current_edk2_release "
            f"-> {release_for_edk2_ref(edk2_base)}"
        )


if __name__ == "__main__":
    main_wrapper(main)
