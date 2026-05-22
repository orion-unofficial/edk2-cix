#!/usr/bin/env python3
"""Promote source/unofficial/current onto a newer EDK2 release base."""

from __future__ import annotations

import argparse
import json
import os
import time
from pathlib import Path

from import_workflow import (
    CURRENT_REF,
    ZERO_OID,
    ensure_target_not_checked_out_dirty,
    full_tag_ref,
    ref_oid,
    transaction_update_refs,
)
from reconstruction_common import (
    CACHE_BASE_EDK2_PREFIX,
    ReconstructionError,
    branch_to_ref,
    check_immutable_refs,
    git,
    format_duration,
    load_json,
    main_wrapper,
    ref_exists,
    release_for_edk2_ref,
    repo_root,
    rev_parse,
    tree_id,
    truthy,
    unofficial_release_tag_for_branch,
)
from source_porting import apply_source_delta_to_base


HELP = """promote-unofficial-release

Required variables:
  EDK2_BASE=<edk2-stableYYYYMM>
      New EDK2 base to promote to.
  FROM_EDK2_BASE=<edk2-stableYYYYMM>
      Previous EDK2 base used to compute the source/unofficial delta.

Optional variables:
  FROM_REF=<ref>
      Source tree to port from. Default: source/unofficial/current.
  REF=<ref> or RESOLVED_REF=<ref>
      Already-resolved promoted source tree from a conflict handoff. When set,
      the command records this tree directly instead of replaying FROM_REF.
  UPDATE_CURRENT=0|1
      Also move source/unofficial/current to the promoted source tree.
      Default: 1.
  UPDATE_RELEASE_TAGS=0|1
      Move refs/tags/source/unofficial/edk2/stable-* for the new release branch.
      Default: 1.
  UPDATE_POLICY=0|1
      Update config/policies.json unofficial_source_policy current_edk2_release.
      Default: 1.
  ALLOW_REPLACE=0|1
      Allow an existing source/unofficial/edk2-stable* release branch to move.
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
    p.add_argument("--from-ref", default=os.environ.get("FROM_REF", CURRENT_REF))
    p.add_argument("--resolved-ref", default=os.environ.get("RESOLVED_REF", os.environ.get("REF", "")))
    p.add_argument("--update-current", default=os.environ.get("UPDATE_CURRENT", "1"))
    p.add_argument("--update-release-tags", default=os.environ.get("UPDATE_RELEASE_TAGS", "1"))
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


def cache_base_ref(edk2_base: str) -> str:
    return f"{CACHE_BASE_EDK2_PREFIX}{normalise_edk2_base(edk2_base)}"


def update_policy(repo: Path, edk2_base: str) -> None:
    path = repo / "config" / "policies.json"
    data = load_json(repo, "config/policies.json")
    policy = data.get("unofficial_source_policy")
    if not isinstance(policy, dict):
        raise ReconstructionError("config/policies.json is missing unofficial_source_policy")
    policy["current_edk2_release"] = release_for_edk2_ref(edk2_base)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    write = truthy(args.write)
    update_current = truthy(args.update_current)
    update_tags = truthy(args.update_release_tags)
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
    target_ref = f"source/unofficial/{edk2_base}"
    if ref_exists(repo, target_ref) and not allow_replace:
        raise ReconstructionError(f"target unofficial release branch already exists: {target_ref}")
    if args.resolved_ref and not ref_exists(repo, args.resolved_ref):
        raise ReconstructionError(f"RESOLVED_REF/REF is unavailable locally: {args.resolved_ref}")
    if not args.resolved_ref and not ref_exists(repo, args.from_ref):
        raise ReconstructionError(f"FROM_REF is unavailable locally: {args.from_ref}")

    message = (
        f"source: promote unofficial firmware source to {edk2_base}\n\n"
        f"Source-Base: {cache_base_ref(edk2_base)}\n"
        f"Source-Ported-From: {cache_base_ref(from_edk2_base)}\n"
        f"Source-Ported-Input: {args.from_ref}\n"
    )
    if args.resolved_ref:
        resolved = rev_parse(repo, args.resolved_ref)
        candidate = git(repo, "commit-tree", f"{resolved}^{{tree}}", "-m", message).stdout.strip()
    else:
        candidate = apply_source_delta_to_base(
            repo,
            old_base_ref=cache_base_ref(from_edk2_base),
            source_ref=args.from_ref,
            new_base_ref=cache_base_ref(edk2_base),
            message=message,
            label=f"unofficial-{edk2_base}",
            verbose=verbose,
        )

    updates: list[tuple[str, str, str]] = []
    old_target = ref_oid(repo, target_ref) or ZERO_OID
    updates.append((branch_to_ref(target_ref), candidate, old_target))
    if update_current:
        updates.append((branch_to_ref(CURRENT_REF), candidate, ref_oid(repo, CURRENT_REF) or ZERO_OID))
    if update_tags:
        tag = unofficial_release_tag_for_branch(target_ref)
        updates.append((full_tag_ref(tag), candidate, ref_oid(repo, tag, tag=True) or ZERO_OID))

    if not write:
        print("dry run; set WRITE=1 to promote unofficial source")
        print(f"  candidate: {candidate} (tree {tree_id(repo, candidate)})")
        for full_ref, new_oid, old_oid in updates:
            print(f"  {full_ref}: {old_oid} -> {new_oid}")
        if update_policy_file:
            print(f"  config/policies.json current_edk2_release -> {release_for_edk2_ref(edk2_base)}")
        return

    ensure_target_not_checked_out_dirty(repo, CURRENT_REF)
    if ref_exists(repo, target_ref):
        ensure_target_not_checked_out_dirty(repo, target_ref)
    transaction_update_refs(repo, updates)
    if update_policy_file:
        update_policy(repo, edk2_base)
    print(f"promoted unofficial source to {edk2_base} in {format_duration(time.monotonic() - started)}")
    for full_ref, new_oid, old_oid in updates:
        print(f"  {full_ref}: {old_oid} -> {new_oid}")
    if update_policy_file:
        print(f"  config/policies.json current_edk2_release -> {release_for_edk2_ref(edk2_base)}")


if __name__ == "__main__":
    main_wrapper(main)
