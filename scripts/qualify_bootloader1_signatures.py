#!/usr/bin/env python3
"""Requalify the pinned BL1 catalogue with CIX's pinned vendor verifier."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from bootloader1_vendor import VendorUnavailable, verify_payloads
from reconstruction_common import ReconstructionError, main_wrapper
from validate_bootloader1 import CATALOG, ROOT, check_payload, git_bytes, load_catalog


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--tool", type=Path, help="path to the pinned x86-64 Linux cix_mkimage_rsa")
    source.add_argument("--download", action="store_true", help="download and checksum the pinned vendor tool")
    parser.add_argument("--runner", choices=("auto", "native", "docker", "podman"), default="auto")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--allow-unavailable", action="store_true",
                        help="warn and continue if the vendor tool cannot run, as ordinary builds do")
    args = parser.parse_args()
    if args.report:
        args.report.unlink(missing_ok=True)
    metadata = json.loads(CATALOG.read_text())["vendor_tool"]
    catalog = load_catalog()
    payloads = {}
    for digest, entry in catalog.items():
        origin = entry["provenance"][0]
        data = git_bytes(ROOT, origin["commit"], origin["path"])
        check_payload(data, origin["ref"], catalog, {digest})
        payloads[digest] = data
    try:
        result = verify_payloads(payloads, tool=args.tool, runner=args.runner, download=args.download)
    except VendorUnavailable as exc:
        if not args.allow_unavailable:
            raise ReconstructionError(f"BL1 signature qualification unavailable: {exc}") from exc
        print(f"[bl1-signatures] WARNING: qualification unavailable: {exc}", file=sys.stderr)
        result = {"status": "unavailable", "warning": str(exc)}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps({"vendor_tool": metadata, "board_fuse_acceptance": "not-tested",
                                          **result}, indent=2) + "\n")
    if result["status"] == "verified":
        print(f"[bl1-signatures] All {len(payloads)} pinned payloads passed vendor signature verification")


if __name__ == "__main__":
    main_wrapper(main)
