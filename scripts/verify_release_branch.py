#!/usr/bin/env python3
"""Validate rendered firmware variant branch invariants."""

from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path

from reconstruction_common import (
    ReconstructionError,
    check_immutable_refs,
    format_duration,
    git,
    main_wrapper,
    ref_exists,
    release_entry,
    repo_root,
    truthy,
)


HELP = """verify-release-branch

Required variables:
  RELEASE    Firmware variant name from 'make help-variants', or a full
             source/release/... branch name.

Optional variables:
  WORKTREE=<path>  Existing worktree to use for log/blame checks.
  V=0|1            Print detailed validation progress.

Checks:
  - rendered tree has no gitlinks
  - rendered tree has no active root .gitmodules
  - missing source/release refs can be regenerated from source refs
  - immutable source refs match config/refs metadata
  - git log and git blame work on representative paths when a worktree is supplied
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--release", default=os.environ.get("RELEASE", ""))
    p.add_argument("--worktree", default=os.environ.get("WORKTREE", ""))
    p.add_argument("--allow-active-gitmodules", action="store_true")
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def tree_has_path(repo: Path, ref: str, path: str) -> bool:
    return git(repo, "cat-file", "-e", f"{ref}:{path}", check=False).returncode == 0


def text_sample_paths(repo: Path, ref: str) -> list[str]:
    preferred = [
        "README.md",
        "src/Makefile",
        "src/edk2/ReadMe.rst",
        "src/edk2-platforms/Readme.md",
        "src/edk2-non-osi/Readme.md",
    ]
    paths = [p for p in preferred if tree_has_path(repo, ref, p)]
    if paths:
        return paths[:3]
    result = git(repo, "ls-tree", "-r", "--name-only", ref)
    return [line for line in result.stdout.splitlines() if line][:3]


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    if not args.release:
        print(HELP)
        print("missing required variable(s): RELEASE", file=sys.stderr)
        raise SystemExit(2)

    branch, _entry = release_entry(repo, args.release, require=True)
    ref = branch if ref_exists(repo, branch) else None
    if ref is None:
        print(f"[verify] Rendering generated firmware variant for validation: {branch}", file=sys.stderr)
        from render_release_branch import render_from_plan

        _branch, entry = release_entry(repo, args.release, require=True)
        started = time.monotonic()
        ref = render_from_plan(repo, branch, entry, verbose)
        print(f"[verify] Rendered {branch} in {format_duration(time.monotonic() - started)}", file=sys.stderr)

    check_immutable_refs(repo)

    tree = git(repo, "ls-tree", "-r", ref).stdout.splitlines()
    gitlinks = [line for line in tree if line.startswith("160000 ")]
    if gitlinks:
        sample = "\n".join(f"  {line}" for line in gitlinks[:20])
        raise ReconstructionError(f"rendered branch contains gitlinks:\n{sample}")

    root_gitmodules = [line for line in tree if line.endswith("\t.gitmodules")]
    if root_gitmodules and not args.allow_active_gitmodules:
        sample = "\n".join(f"  {line}" for line in root_gitmodules[:20])
        raise ReconstructionError(f"rendered branch contains an active root .gitmodules file:\n{sample}")

    inert_gitmodules = [line for line in tree if line.endswith("/.gitmodules")]
    if inert_gitmodules and verbose:
        print(f"note: {len(inert_gitmodules)} nested .gitmodules files are present as inert source files")

    if args.worktree:
        wt = Path(args.worktree)
        if not wt.exists():
            raise ReconstructionError(f"WORKTREE does not exist: {wt}")
        for sample in text_sample_paths(repo, ref):
            if verbose:
                print(f"checking history UX for {sample}")
            git(wt, "log", "-1", "--", sample)
            blame = git(wt, "blame", "-L", "1,1", "--", sample, check=False)
            if blame.returncode != 0:
                raise ReconstructionError(f"git blame failed for {sample}: {(blame.stderr or blame.stdout).strip()}")
            parts = sample.split("/")
            if len(parts) > 1:
                nested = wt / parts[0]
                rel = "/".join(parts[1:])
                if nested.exists():
                    git(nested, "log", "-1", "--", rel)

    print(f"validated {branch} ({ref})")


if __name__ == "__main__":
    main_wrapper(main)
