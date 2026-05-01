#!/usr/bin/env python3
"""Import developer changes into local source and delta refs explicitly."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

from reconstruction_common import (
    ReconstructionError,
    create_delta_artefact,
    git,
    main_wrapper,
    ref_exists,
    repo_root,
    truthy,
)


HELP = """import-local-commits

Required variables:
  FROM_REF=<ref>        Developer branch or commit to import from.
  BASE_REF=<ref>        Rendered upstream/vendor ref the local changes are based on.

Optional variables:
  SOURCE_LOCAL_REF=source/unofficial/current
  UPDATE_LOCAL_SOURCE=0|1
                         Also advance SOURCE_LOCAL_REF to FROM_REF.
  TARGET_REF=source/delta/local/current
  WRITE=0|1              Required before refs are created or advanced.
  V=0|1                  Print delegated git operations.

Ordinary build and render targets never rewrite source/unofficial/current or
source/delta/local/*. This command is the explicit import gate for local source
and generated local delta artefacts.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--from-ref", default=os.environ.get("FROM_REF", ""))
    p.add_argument("--base-ref", default=os.environ.get("BASE_REF", ""))
    p.add_argument("--source-local-ref", default=os.environ.get("SOURCE_LOCAL_REF", "source/unofficial/current"))
    p.add_argument("--update-local-source", default=os.environ.get("UPDATE_LOCAL_SOURCE", "0"))
    p.add_argument("--target-ref", default=os.environ.get("TARGET_REF", "source/delta/local/current"))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def update_source_ref(repo, ref: str, from_ref: str, verbose: bool) -> None:
    if not ref.startswith("source/unofficial/"):
        raise ReconstructionError("SOURCE_LOCAL_REF must be under source/unofficial/")
    if ref_exists(repo, ref):
        git(repo, "branch", "-f", ref, from_ref, capture=not verbose)
    else:
        git(repo, "branch", ref, from_ref, capture=not verbose)


def main() -> None:
    args = parser().parse_args()
    if not args.from_ref or not args.base_ref:
        print(HELP)
        missing = [name for name, value in [("FROM_REF", args.from_ref), ("BASE_REF", args.base_ref)] if not value]
        print("missing required variable(s): " + ", ".join(missing), file=sys.stderr)
        raise SystemExit(2)
    repo = repo_root(Path(__file__))
    if not ref_exists(repo, args.from_ref):
        raise ReconstructionError(f"FROM_REF is unavailable locally: {args.from_ref}")
    if not ref_exists(repo, args.base_ref):
        raise ReconstructionError(f"BASE_REF is unavailable locally: {args.base_ref}")
    if not args.source_local_ref.startswith("source/unofficial/"):
        raise ReconstructionError("SOURCE_LOCAL_REF must be under source/unofficial/")
    if not args.target_ref.startswith("source/delta/local/"):
        raise ReconstructionError("TARGET_REF must be under source/delta/local/")
    if not truthy(args.write):
        print("dry run; set WRITE=1 to update local refs")
        if truthy(args.update_local_source):
            print(f"  {args.from_ref} -> {args.source_local_ref}")
        print(f"  {args.base_ref}..{args.from_ref} -> {args.target_ref}")
        return
    if truthy(args.v):
        if truthy(args.update_local_source):
            print(f"source {args.from_ref} -> {args.source_local_ref}")
        print(f"delta {args.base_ref}..{args.from_ref} -> {args.target_ref}")
    if truthy(args.update_local_source):
        update_source_ref(repo, args.source_local_ref, args.from_ref, truthy(args.v))
    create_delta_artefact(
        repo,
        args.base_ref,
        args.from_ref,
        args.target_ref,
        kind="local-delta",
        name=args.target_ref.removeprefix("source/delta/"),
        message="delta: import local changes",
        allow_replace=True,
    )
    print(f"updated {args.target_ref}")


if __name__ == "__main__":
    main_wrapper(main)
