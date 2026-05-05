#!/usr/bin/env python3
"""Validate source-tree policy checks for source refs."""

from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path

from import_unofficial_commits import CURRENT_REF, checkpoint_targets
from reconstruction_common import ReconstructionError, format_duration, main_wrapper, ref_exists, repo_root
from source_policy import enforce_source_tree_policy


HELP = """verify-source-policy

Optional variables:
  REF=<ref>  Check one source ref instead of every unofficial source checkpoint.
  V=0|1      Print each checked ref.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--ref", default=os.environ.get("REF", ""))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    refs = [args.ref] if args.ref else [CURRENT_REF, *checkpoint_targets(repo)]
    checked = 0
    for ref in refs:
        if not ref_exists(repo, ref):
            raise ReconstructionError(f"source ref is unavailable locally: {ref}")
        if args.v.strip() not in {"", "0", "false", "False"}:
            print(f"checking source policy: {ref}", file=sys.stderr)
        enforce_source_tree_policy(repo, ref=ref, label=ref)
        checked += 1
    print(f"validated source policy for {checked} ref(s) in {format_duration(time.monotonic() - started)}")


if __name__ == "__main__":
    main_wrapper(main)
