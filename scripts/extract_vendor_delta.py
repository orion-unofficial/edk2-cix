#!/usr/bin/env python3
"""Extract or report a vendor delta between a base ref and a vendor ref."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

from reconstruction_common import ReconstructionError, git, main_wrapper, ref_exists, repo_root


HELP = """extract-vendor-delta

Required variables:
  VENDOR=<radxa|cix>      Vendor whose delta is being examined.
  BASE_REF=<ref>          Upstream/base ref to compare from.
  VENDOR_REF=<ref>        Vendor/materialised ref to compare to.

Optional variables:
  OUTPUT=<path>           Write a JSON report to this path.
  PATCH_OUTPUT=<path>     Write a git-format patch/diff to this path.
  V=0|1                   Print the diff stat to stdout.

This command is intentionally read-only unless OUTPUT or PATCH_OUTPUT is set.
It never updates source refs; use integrate-source-release for immutable vendor
source refs.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--vendor", default=os.environ.get("VENDOR", ""))
    p.add_argument("--base-ref", default=os.environ.get("BASE_REF", ""))
    p.add_argument("--vendor-ref", default=os.environ.get("VENDOR_REF", ""))
    p.add_argument("--output", default=os.environ.get("OUTPUT", ""))
    p.add_argument("--patch-output", default=os.environ.get("PATCH_OUTPUT", ""))
    p.add_argument("--target-ref", default=os.environ.get("TARGET_REF", ""))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def main() -> None:
    args = parser().parse_args()
    missing = [name for name, value in [("VENDOR", args.vendor), ("BASE_REF", args.base_ref), ("VENDOR_REF", args.vendor_ref)] if not value]
    if missing:
        print(HELP)
        print("missing required variable(s): " + ", ".join(missing), file=sys.stderr)
        raise SystemExit(2)
    repo = repo_root(Path(__file__))
    if args.vendor not in {"radxa", "cix"}:
        raise ReconstructionError("VENDOR must be radxa or cix")
    for ref in [args.base_ref, args.vendor_ref]:
        if not ref_exists(repo, ref):
            raise ReconstructionError(f"ref is unavailable locally: {ref}")

    stat = git(repo, "diff", "--stat", f"{args.base_ref}..{args.vendor_ref}").stdout
    summary = git(repo, "diff", "--summary", f"{args.base_ref}..{args.vendor_ref}").stdout
    changed = git(repo, "diff", "--name-status", f"{args.base_ref}..{args.vendor_ref}").stdout.splitlines()
    report = {
        "vendor": args.vendor,
        "base_ref": args.base_ref,
        "vendor_ref": args.vendor_ref,
        "changed_paths": len(changed),
        "name_status": changed,
        "stat": stat,
        "summary": summary,
    }
    if args.output:
        out = Path(args.output)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if args.patch_output:
        patch_result = subprocess.run(
            ["git", "-C", str(repo), "diff", "--binary", "--full-index", f"{args.base_ref}..{args.vendor_ref}"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if patch_result.returncode not in {0, 1}:
            raise ReconstructionError(patch_result.stderr.decode("utf-8", errors="ignore").strip())
        out = Path(args.patch_output)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(patch_result.stdout)
    if args.target_ref:
        raise ReconstructionError(
            "extract-vendor-delta does not update refs; "
            "use integrate-source-release TYPE=vendor for persistent vendor source refs"
        )
    if args.output or args.patch_output:
        print("vendor delta extracted")
    elif not args.target_ref:
        print(stat.rstrip() or "no changes")


if __name__ == "__main__":
    main_wrapper(main)
