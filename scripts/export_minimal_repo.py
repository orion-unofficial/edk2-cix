#!/usr/bin/env python3
"""Create a bare repository containing only build and required source refs."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

from reconstruction_common import ReconstructionError, git, main_wrapper, repo_root, run, truthy


HELP = """export-minimal-repo

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


def required_refs(repo: Path) -> list[str]:
    refs = ["refs/heads/build"]
    refs.extend(
        ref
        for ref in ref_list(repo, "refs/heads/source")
        if not ref.startswith("refs/heads/source/cache/")
    )
    refs.extend(ref_list(repo, "refs/tags/source"))
    return sorted(set(refs))


def destination_ready(path: Path) -> bool:
    return not path.exists() or (path.is_dir() and not any(path.iterdir()))


def main() -> None:
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
        raise ReconstructionError("working tree is dirty; commit or stash changes before exporting a minimal repo")

    refs = required_refs(repo)
    dest.parent.mkdir(parents=True, exist_ok=True)
    git(repo, "init", "--bare", str(dest), capture=not verbose)
    refspecs = [f"+{ref}:{ref}" for ref in refs]
    git(repo, "push", "--no-verify", str(dest), *refspecs, capture=not verbose)
    run(["git", "--git-dir", str(dest), "symbolic-ref", "HEAD", "refs/heads/build"], capture=not verbose)
    if truthy(args.repack):
        run(["git", "--git-dir", str(dest), "repack", "-Ad", "--depth=50", "--window=250"], capture=not verbose)
        run(["git", "--git-dir", str(dest), "prune-packed"], capture=not verbose)
    print(f"exported minimal bare repository: {dest}")
    print(f"refs exported: {len(refs)}")


if __name__ == "__main__":
    main_wrapper(main)
