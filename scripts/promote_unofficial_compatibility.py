#!/usr/bin/env python3
"""Port a retained Unofficial EDK2 compatibility branch to a newer base."""

from __future__ import annotations

import argparse
import os
import time
from pathlib import Path

from import_workflow import (
    ZERO_OID,
    ensure_target_not_checked_out_dirty,
    full_tag_ref,
    ref_oid,
    transaction_update_refs,
)
from reconstruction_common import (
    BUILD_INFRA_OVERLAY_PATHS,
    CACHE_BASE_EDK2_PREFIX,
    ReconstructionError,
    branch_to_ref,
    check_immutable_refs,
    format_duration,
    git,
    main_wrapper,
    ref_exists,
    refresh_ref_record,
    repo_root,
    rev_parse,
    tree_id,
    truthy,
    unofficial_release_tag_for_branch,
)
from source_porting import apply_source_delta_to_base, normalise_source_tree


HELP = """promote-unofficial-compatibility

Required variables:
  EDK2_BASE=<edk2-stableYYYYMM>
      New EDK2 base to promote to.
  FROM_EDK2_BASE=<edk2-stableYYYYMM>
      Previous EDK2 base used to compute the retained compatibility delta.

Optional variables:
  FROM_REF=<ref>
      Compatibility source tree to port. Default:
      source/unofficial/<FROM_EDK2_BASE>.
  REF=<ref> or RESOLVED_REF=<ref>
      Already-resolved promoted source tree from a conflict handoff.
  ALLOW_REPLACE=0|1
      Allow an existing compatibility branch and tag to move.
      Default: 0.
  WRITE=0|1
      Required before the compatibility branch or tag is created or moved.
  V=0|1
      Print delegated Git operations.

The compatibility branch is deliberately independent of a mutable Unofficial
line. It remains the EDK2-release target used by focused project-change
propagation across every retained upstream base.
"""


def record_compatibility_ref(repo: Path, ref: str, edk2_base: str) -> None:
    refresh_ref_record(
        repo,
        "refs-unofficial.json",
        ref,
        {
            "edk2_base": edk2_base,
            "immutable": False,
            "type": "unofficial-edk2-compatibility",
        },
    )


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=HELP,
    )
    p.add_argument("--edk2-base", default=os.environ.get("EDK2_BASE", ""))
    p.add_argument(
        "--from-edk2-base", default=os.environ.get("FROM_EDK2_BASE", "")
    )
    p.add_argument("--from-ref", default=os.environ.get("FROM_REF", ""))
    p.add_argument(
        "--resolved-ref",
        default=os.environ.get("RESOLVED_REF", os.environ.get("REF", "")),
    )
    p.add_argument(
        "--allow-replace", default=os.environ.get("ALLOW_REPLACE", "0")
    )
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


def cache_base_ref(edk2_base: str) -> str:
    return f"{CACHE_BASE_EDK2_PREFIX}{normalise_edk2_base(edk2_base)}"


def repair_or_report_existing(
    repo: Path,
    *,
    target_ref: str,
    tag: str,
    write: bool,
    verbose: bool,
) -> bool:
    """Return True when an existing target was complete or safely repaired."""

    if not ref_exists(repo, target_ref):
        return False
    target_oid = rev_parse(repo, target_ref)
    tag_oid = ref_oid(repo, tag, tag=True)
    if tag_oid and tag_oid != target_oid:
        raise ReconstructionError(
            f"compatibility tag {tag} resolves to {tag_oid}, but {target_ref} "
            f"resolves to {target_oid}; review the mismatch before replacing either ref"
        )
    if tag_oid == target_oid:
        print(f"compatibility source already recorded: {target_ref} ({target_oid})")
        return True
    if not write:
        print("dry run; set WRITE=1 to create the missing compatibility tag")
        print(f"  {full_tag_ref(tag)}: {ZERO_OID} -> {target_oid}")
        return True
    transaction_update_refs(
        repo,
        [(full_tag_ref(tag), target_oid, ZERO_OID)],
    )
    if verbose:
        print(f"created compatibility tag {tag} -> {target_oid}")
    else:
        print(f"repaired missing compatibility tag: {tag}")
    return True


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    write = truthy(args.write)
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
    if edk2_base == from_edk2_base:
        raise ReconstructionError("EDK2_BASE and FROM_EDK2_BASE must differ")

    check_immutable_refs(repo)
    target_ref = f"source/unofficial/{edk2_base}"
    tag = unofficial_release_tag_for_branch(target_ref)
    if not allow_replace and repair_or_report_existing(
        repo,
        target_ref=target_ref,
        tag=tag,
        write=write,
        verbose=verbose,
    ):
        if write:
            record_compatibility_ref(repo, target_ref, edk2_base)
        return

    from_ref = args.from_ref.strip() or f"source/unofficial/{from_edk2_base}"
    if args.resolved_ref and not ref_exists(repo, args.resolved_ref):
        raise ReconstructionError(
            f"RESOLVED_REF/REF is unavailable locally: {args.resolved_ref}"
        )
    if not ref_exists(repo, from_ref):
        raise ReconstructionError(f"FROM_REF is unavailable locally: {from_ref}")

    source_oid = rev_parse(repo, from_ref)
    message = (
        f"source: promote unofficial EDK2 compatibility to {edk2_base}\n\n"
        f"Source-Base: {cache_base_ref(edk2_base)}\n"
        f"Source-Ported-From: {cache_base_ref(from_edk2_base)}\n"
        f"Source-Ported-Input: {from_ref}\n"
        f"Source-Ported-Input-Object: {source_oid}\n"
    )
    if args.resolved_ref:
        resolved = rev_parse(repo, args.resolved_ref)
        message += f"Source-Port-Resolution: {resolved}\n"
        changed = [
            line
            for line in git(
                repo,
                "diff",
                "--name-only",
                cache_base_ref(edk2_base),
                resolved,
            ).stdout.splitlines()
            if line
        ]
        tree, _result = normalise_source_tree(
            repo,
            tree=tree_id(repo, resolved),
            label=f"unofficial-compatibility-{edk2_base}",
            verbose=verbose,
            paths=changed,
        )
        candidate = git(repo, "commit-tree", tree, "-m", message).stdout.strip()
    else:
        candidate = apply_source_delta_to_base(
            repo,
            old_base_ref=cache_base_ref(from_edk2_base),
            source_ref=from_ref,
            new_base_ref=cache_base_ref(edk2_base),
            message=message,
            label=f"unofficial-compatibility-{edk2_base}",
            source_owned_paths=BUILD_INFRA_OVERLAY_PATHS,
            normalise_source=True,
            resume_variable="RESOLVED_REF",
            verbose=verbose,
        )

    old_target = ref_oid(repo, target_ref) or ZERO_OID
    old_tag = ref_oid(repo, tag, tag=True) or ZERO_OID
    if old_tag != ZERO_OID and old_tag != old_target and not allow_replace:
        raise ReconstructionError(
            f"compatibility tag already exists at a different object: {tag} -> {old_tag}"
        )
    updates = [
        (branch_to_ref(target_ref), candidate, old_target),
        (full_tag_ref(tag), candidate, old_tag),
    ]

    if not write:
        print("dry run; set WRITE=1 to promote the compatibility source")
        print(f"  candidate: {candidate} (tree {tree_id(repo, candidate)})")
        for full_ref, new_oid, old_oid in updates:
            print(f"  {full_ref}: {old_oid} -> {new_oid}")
        return

    if ref_exists(repo, target_ref):
        ensure_target_not_checked_out_dirty(repo, target_ref)
    transaction_update_refs(repo, updates)
    record_compatibility_ref(repo, target_ref, edk2_base)
    print(
        "promoted unofficial EDK2 compatibility source to "
        f"{edk2_base} in {format_duration(time.monotonic() - started)}"
    )
    for full_ref, new_oid, old_oid in updates:
        print(f"  {full_ref}: {old_oid} -> {new_oid}")


if __name__ == "__main__":
    main_wrapper(main)
