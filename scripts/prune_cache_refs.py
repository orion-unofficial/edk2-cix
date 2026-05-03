#!/usr/bin/env python3
"""Report or delete generated source/cache refs."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from reconstruction_common import (
    CACHE_REF_PREFIX,
    ReconstructionError,
    base_tree_records,
    branch_to_ref,
    checked_out_worktree,
    git,
    main_wrapper,
    ref_exists,
    rendered_ref_records,
    repo_root,
    rev_parse,
    tree_id,
    truthy,
)


HELP = """prune-cache-refs

Optional variables:
  DELETE=0|1  Delete eligible cache refs. Default: 0.
  V=0|1       Print all inspected cache refs.

Only refs under refs/heads/source/cache/** are considered. DELETE=1 refuses to
delete unknown refs, checked-out refs, or refs whose tree differs from the
manifested tree ID.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--delete", default=os.environ.get("DELETE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def cache_refs(repo: Path) -> list[str]:
    result = git(repo, "for-each-ref", "--format=%(refname:lstrip=2)", "refs/heads/source/cache", check=False)
    if result.returncode != 0:
        return []
    return sorted(line for line in result.stdout.splitlines() if line)


def expected_cache_refs(repo: Path) -> dict[str, str]:
    records = {}
    for ref, record in {**base_tree_records(repo), **rendered_ref_records(repo)}.items():
        if not ref.startswith(CACHE_REF_PREFIX):
            raise ReconstructionError(f"cache manifest record is outside {CACHE_REF_PREFIX}: {ref}")
        tree = record.get("tree_id")
        if tree:
            records[ref] = tree
    return records


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    delete = truthy(args.delete)
    verbose = truthy(args.v)
    expected = expected_cache_refs(repo)
    actual = cache_refs(repo)

    eligible: list[str] = []
    problems: list[str] = []
    for ref in actual:
        reason = ""
        if not ref.startswith(CACHE_REF_PREFIX):
            reason = "outside source/cache namespace"
        elif ref not in expected:
            reason = "not described by cache manifests"
        elif not ref_exists(repo, ref):
            reason = "missing commit"
        else:
            wt = checked_out_worktree(repo, ref)
            if wt:
                reason = f"checked out in worktree {wt}"
            elif tree_id(repo, ref) != expected[ref]:
                reason = f"tree differs from manifest ({tree_id(repo, ref)} != {expected[ref]})"

        if reason:
            problems.append(f"{ref}: {reason}")
            if verbose or not delete:
                print(f"keep {ref}: {reason}")
        else:
            eligible.append(ref)
            if verbose or not delete:
                print(f"eligible {ref}")

    if delete and problems:
        details = "\n".join(f"  - {item}" for item in problems)
        raise ReconstructionError(f"cache prune refused due to unsafe refs:\n{details}")

    if delete:
        for ref in eligible:
            git(repo, "update-ref", "-d", branch_to_ref(ref), rev_parse(repo, ref))
            print(f"deleted {ref}")
    else:
        print(f"cache refs: {len(actual)} present, {len(eligible)} eligible, {len(problems)} blocked")
        print("set DELETE=1 to delete eligible source/cache refs")


if __name__ == "__main__":
    main_wrapper(main)
