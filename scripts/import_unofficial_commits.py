#!/usr/bin/env python3
"""Import developer changes into unofficial source refs explicitly."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

from reconstruction_common import (
    ReconstructionError,
    git,
    main_wrapper,
    ref_exists,
    repo_root,
    truthy,
)


HELP = """import-unofficial-commits

Required variables:
  FROM_REF=<ref>        Developer branch or commit to import from.

Optional variables:
  SOURCE_UNOFFICIAL_REF=source/unofficial/current
  WRITE=0|1              Required before refs are created or advanced.
  V=0|1                  Print delegated git operations.

Ordinary build and render targets never rewrite source/unofficial/current or
release-specific source/unofficial checkpoints. This command is the explicit
import gate for unofficial source refs.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--from-ref", default=os.environ.get("FROM_REF", ""))
    p.add_argument("--source-unofficial-ref", default=os.environ.get("SOURCE_UNOFFICIAL_REF", "source/unofficial/current"))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def update_source_ref(repo, ref: str, from_ref: str, verbose: bool) -> None:
    if not ref.startswith("source/unofficial/"):
        raise ReconstructionError("SOURCE_UNOFFICIAL_REF must be under source/unofficial/")
    if ref_exists(repo, ref):
        git(repo, "branch", "-f", ref, from_ref, capture=not verbose)
    else:
        git(repo, "branch", ref, from_ref, capture=not verbose)


def main() -> None:
    args = parser().parse_args()
    if not args.from_ref:
        print(HELP)
        print("missing required variable(s): FROM_REF", file=sys.stderr)
        raise SystemExit(2)
    repo = repo_root(Path(__file__))
    if not ref_exists(repo, args.from_ref):
        raise ReconstructionError(f"FROM_REF is unavailable locally: {args.from_ref}")
    if not args.source_unofficial_ref.startswith("source/unofficial/"):
        raise ReconstructionError("SOURCE_UNOFFICIAL_REF must be under source/unofficial/")
    if not truthy(args.write):
        print("dry run; set WRITE=1 to update unofficial refs")
        print(f"  {args.from_ref} -> {args.source_unofficial_ref}")
        return
    if truthy(args.v):
        print(f"source {args.from_ref} -> {args.source_unofficial_ref}")
    update_source_ref(repo, args.source_unofficial_ref, args.from_ref, truthy(args.v))
    print(f"updated {args.source_unofficial_ref}")


if __name__ == "__main__":
    main_wrapper(main)
