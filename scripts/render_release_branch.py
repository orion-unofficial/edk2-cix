#!/usr/bin/env python3
"""Resolve and optionally materialize a configured firmware release branch."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

from reconstruction_common import (
    ReconstructionError,
    cache_dir,
    check_immutable_refs,
    git,
    main_wrapper,
    ref_exists,
    release_entry,
    repo_root,
    resolve_branch_or_origin,
    rev_parse,
    safe_name,
    short_release,
    truthy,
)


HELP = """render-release-branch

Required variables:
  RELEASE    Short release name or full source/release/... branch name.

Optional variables:
  PERSIST=1  Create the rendered source/release/... branch if it is missing.
  V=1        Print delegated git operations and warnings.

Example:
  make render-release-branch \\
    RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local-1.2.1 \\
    PERSIST=1
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--release", default=os.environ.get("RELEASE", ""), help="short release name or source/release/... branch")
    p.add_argument("--require-release", action="store_true", help="fail if --release is empty instead of using the default")
    p.add_argument("--persist", default=os.environ.get("PERSIST", "0"), help="create a persistent source/release branch when set to 1")
    p.add_argument("--ensure-worktree", action="store_true", help="create or reuse a detached worktree for the resolved release")
    p.add_argument("--print-worktree", action="store_true", help="print only the worktree path")
    p.add_argument("--print-ref", action="store_true", help="print the resolved ref/branch")
    p.add_argument("--print-default-release", action="store_true", help="print the configured default short release")
    p.add_argument("--v", default=os.environ.get("V", "0"), help="verbosity flag propagated from make")
    return p


def ensure_worktree(repo: Path, branch: str, target_ref: str, verbose: bool) -> Path:
    commit = rev_parse(repo, target_ref)
    root = cache_dir(repo, "worktrees")
    path = root / f"{safe_name(branch)}-{commit[:12]}"
    if path.exists():
        status = git(path, "status", "--porcelain").stdout.strip()
        if status:
            raise ReconstructionError(f"cached worktree is dirty: {path}")
        return path
    if verbose:
        print(f"Creating detached release worktree {path} at {commit}", file=sys.stderr)
    git(repo, "worktree", "add", "--detach", str(path), commit, capture=not verbose)
    return path


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)

    releases = __import__("reconstruction_common").load_json(repo, "config/releases.json")
    if args.print_default_release:
        print(releases.get("default_release", ""))
        return

    if args.require_release and not args.release:
        print(HELP, file=sys.stderr)
        raise SystemExit(2)

    branch, entry = release_entry(repo, args.release or None, require=args.require_release)
    target_ref = resolve_branch_or_origin(repo, branch, verbose=verbose)
    source_ref = entry.get("source_ref")

    if target_ref is None and source_ref and ref_exists(repo, source_ref):
        target_ref = source_ref

    if target_ref is None:
        raise ReconstructionError(
            f"release branch {branch} is unavailable locally and could not be fetched from origin.\n"
            "External upstream/vendor remotes are not contacted for ordinary rendering; "
            "run integrate-source-release if reconstruction objects are missing."
        )

    check_immutable_refs(repo)

    if truthy(args.persist):
        if ref_exists(repo, branch):
            existing = rev_parse(repo, branch)
            target = rev_parse(repo, target_ref)
            if existing != target:
                raise ReconstructionError(
                    f"persistent branch {branch} already exists at {existing}, not {target}; "
                    "refusing to rewrite it outside integrate-source-release"
                )
        else:
            if verbose:
                print(f"Creating persistent branch {branch} from {target_ref}", file=sys.stderr)
            git(repo, "branch", branch, target_ref, capture=not verbose)
            target_ref = branch

    wt: Path | None = None
    if args.ensure_worktree:
        wt = ensure_worktree(repo, branch, target_ref, verbose=verbose)

    if args.print_worktree:
        if wt is None:
            wt = ensure_worktree(repo, branch, target_ref, verbose=verbose)
        print(wt)
    elif args.print_ref:
        print(branch if ref_exists(repo, branch) else target_ref)
    else:
        print(f"release: {short_release(branch)}")
        print(f"branch:  {branch}")
        print(f"ref:     {branch if ref_exists(repo, branch) else target_ref}")
        if wt:
            print(f"worktree:{wt}")


if __name__ == "__main__":
    main_wrapper(main)
