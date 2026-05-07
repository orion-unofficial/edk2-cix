#!/usr/bin/env python3
"""Advance unofficial release tags to matching release branch heads."""

from __future__ import annotations

import argparse
import os
import time
from pathlib import Path

from import_workflow import (
    ZERO_OID,
    full_tag_ref,
    ref_oid,
    release_branch_targets,
    transaction_update_refs,
)
from reconstruction_common import (
    ReconstructionError,
    format_duration,
    unofficial_release_branch_for_tag,
    unofficial_release_tag_for_branch,
    main_wrapper,
    ref_exists,
    repo_root,
    rev_parse,
    truthy,
)


HELP = """update-release-tags

Optional variables:
  TARGET_REF=<ref[,ref...]>
      Release branch or comma-separated release branch list to tag. If omitted,
      every source/unofficial/edk2-stable* release branch is checked.
  WRITE=0|1
      Required before tags are created or advanced.
  V=0|1
      Print release tags that are already current.

This target moves only refs/tags/source/unofficial/edk2/stable-* tags. It does
not move source/unofficial/** branches.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--target-ref", default=os.environ.get("TARGET_REF", ""))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def selected_targets(repo: Path, value: str) -> list[str]:
    if value.strip():
        refs = [item.strip() for item in value.split(",") if item.strip()]
    else:
        refs = release_branch_targets(repo)
    if not refs:
        raise ReconstructionError("no release branch refs selected")
    for ref in refs:
        if not ref.startswith("source/unofficial/edk2-stable"):
            raise ReconstructionError(f"not an unofficial release branch: {ref}")
        if not ref_exists(repo, ref):
            raise ReconstructionError(f"release branch is unavailable locally: {ref}")
    return refs


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    write = truthy(args.write)

    targets = selected_targets(repo, args.target_ref)
    updates: list[tuple[str, str, str]] = []
    current = 0
    for branch in targets:
        branch_oid = rev_parse(repo, branch)
        tag = unofficial_release_tag_for_branch(branch)
        old_oid = ref_oid(repo, tag, tag=True) or ZERO_OID
        if old_oid == branch_oid:
            current += 1
            if verbose:
                print(f"current: {tag} -> {branch}")
            continue
        updates.append((full_tag_ref(tag), branch_oid, old_oid))

    if not updates:
        print(
            f"release tags already current: {current} checked in "
            f"{format_duration(time.monotonic() - started)}"
        )
        return

    if not write:
        print("dry run; set WRITE=1 to update release tags")
        for full_ref, new_oid, old_oid in updates:
            branch = unofficial_release_branch_for_tag(full_ref.removeprefix("refs/tags/"))
            print(f"  {full_ref}: {old_oid} -> {new_oid} ({branch})")
        print(f"{len(updates)} tag(s) would be updated; {current} already current")
        return

    transaction_update_refs(repo, updates)
    print(f"updated release tag(s) in {format_duration(time.monotonic() - started)}:")
    for full_ref, new_oid, old_oid in updates:
        print(f"  {full_ref}: {old_oid} -> {new_oid}")
    if current:
        print(f"{current} release tag(s) were already current")


if __name__ == "__main__":
    main_wrapper(main)
