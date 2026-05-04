#!/usr/bin/env python3
"""Create a bare repository containing only build and required source refs."""

from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path

from reconstruction_common import ReconstructionError, format_duration, git, main_wrapper, repo_root, run, truthy


HELP = """create-minimised-clone

Required variables:
  DIR=<path>  Destination directory for the new bare repository.

Optional variables:
  REPACK=0|1  Repack the destination after exporting refs. Default: 1.
  V=0|1       Print delegated git operations.

The destination must not already contain data. The exported repository contains
only refs required by the source model: build, non-cache source/** branches, and
source/** tags. Generated source/cache/** branches and legacy/private branches
are intentionally omitted.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--dir", default=os.environ.get("DIR", ""))
    p.add_argument("--repack", default=os.environ.get("REPACK", "1"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def ref_list(repo: Path, namespace: str) -> list[str]:
    result = git(repo, "for-each-ref", "--format=%(refname)", namespace, check=False)
    if result.returncode != 0:
        return []
    return sorted(line for line in result.stdout.splitlines() if line)


def required_refspecs(repo: Path) -> list[tuple[str, str]]:
    refspecs: dict[str, str] = {"refs/heads/build": "refs/heads/build"}
    for ref in ref_list(repo, "refs/heads/source"):
        if not ref.startswith("refs/heads/source/cache/"):
            refspecs.setdefault(ref, ref)
    for ref in ref_list(repo, "refs/remotes/origin/source"):
        branch = ref.removeprefix("refs/remotes/origin/")
        if not branch.startswith("source/cache/"):
            refspecs.setdefault(f"refs/heads/{branch}", ref)
    for ref in ref_list(repo, "refs/tags/source"):
        refspecs.setdefault(ref, ref)
    return sorted((source, target) for target, source in refspecs.items())


def destination_ready(path: Path) -> bool:
    return not path.exists() or (path.is_dir() and not any(path.iterdir()))


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    if not args.dir:
        print(HELP)
        print("missing required variable: DIR", file=sys.stderr)
        raise SystemExit(2)
    repo = repo_root(Path(__file__))
    dest = Path(args.dir).expanduser().resolve()
    verbose = truthy(args.v)

    if not destination_ready(dest):
        raise ReconstructionError(f"destination already exists and is not empty: {dest}")
    if git(repo, "status", "--porcelain").stdout.strip():
        raise ReconstructionError("working tree is dirty; commit or stash changes before exporting a minimised clone")

    refspecs = required_refspecs(repo)
    dest.parent.mkdir(parents=True, exist_ok=True)
    git(repo, "init", "--bare", str(dest), capture=not verbose)
    push_refspecs = [f"+{source}:{target}" for source, target in refspecs]
    git(repo, "push", "--no-verify", str(dest), *push_refspecs, capture=not verbose)
    run(["git", "--git-dir", str(dest), "symbolic-ref", "HEAD", "refs/heads/build"], capture=not verbose)
    if truthy(args.repack):
        run(["git", "--git-dir", str(dest), "repack", "-Ad", "--depth=50", "--window=250"], capture=not verbose)
        run(["git", "--git-dir", str(dest), "prune-packed"], capture=not verbose)
    print(f"exported minimised bare repository: {dest}")
    print(f"refs exported: {len(refspecs)}")
    print(f"created minimised clone in {format_duration(time.monotonic() - started)}")


if __name__ == "__main__":
    main_wrapper(main)
