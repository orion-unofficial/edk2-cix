#!/usr/bin/env python3
"""Import developer changes into source/delta/local/current explicitly."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from reconstruction_common import ReconstructionError, git, main_wrapper, ref_exists, repo_root, truthy


HELP = """import-local-commits

Required variables:
  FROM_REF=<ref>        Developer branch or commit to import from.

Optional variables:
  TARGET_REF=source/delta/local/current
  WRITE=1              Required before TARGET_REF is created or advanced.
  V=1                  Print delegated git operations.

Ordinary build and render targets never rewrite source/delta/local/current.
This command is the explicit import gate for local development deltas.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--from-ref", default=os.environ.get("FROM_REF", ""))
    p.add_argument("--target-ref", default=os.environ.get("TARGET_REF", "source/delta/local/current"))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def main() -> None:
    args = parser().parse_args()
    if not args.from_ref:
        print(HELP)
        raise SystemExit(2)
    repo = repo_root(Path(__file__))
    if not ref_exists(repo, args.from_ref):
        raise ReconstructionError(f"FROM_REF is unavailable locally: {args.from_ref}")
    if not args.target_ref.startswith("source/delta/local/"):
        raise ReconstructionError("TARGET_REF must be under source/delta/local/")
    if not truthy(args.write):
        print("dry run; set WRITE=1 to update the local delta ref")
        print(f"  {args.from_ref} -> {args.target_ref}")
        return
    if truthy(args.v):
        print(f"branch -f {args.target_ref} {args.from_ref}")
    git(repo, "branch", "-f", args.target_ref, args.from_ref, capture=not truthy(args.v))
    print(f"updated {args.target_ref}")


if __name__ == "__main__":
    main_wrapper(main)
