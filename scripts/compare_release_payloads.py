#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys
from typing import Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Compare a built firmware output tree against a preserved upstream "
            "release payload directory."
        )
    )
    parser.add_argument("--build-dir", type=pathlib.Path, required=True)
    parser.add_argument("--reference-dir", type=pathlib.Path, required=True)
    parser.add_argument("--report-json", type=pathlib.Path)
    parser.add_argument("--strict", action="store_true")
    return parser.parse_args()


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def collect_reference_files(reference_dir: pathlib.Path) -> list[pathlib.Path]:
    return sorted(path for path in reference_dir.rglob("*") if path.is_file())


def main() -> int:
    args = parse_args()
    build_dir = args.build_dir.resolve()
    reference_dir = args.reference_dir.resolve()

    if not reference_dir.is_dir():
        print(f"Reference directory does not exist: {reference_dir}", file=sys.stderr)
        return 2

    reference_files = collect_reference_files(reference_dir)
    if not reference_files:
        print(f"No reference files found under: {reference_dir}", file=sys.stderr)
        return 2

    matched = 0
    mismatched = 0
    missing = 0
    report: dict[str, Any] = {
        "build_dir": str(build_dir),
        "reference_dir": str(reference_dir),
        "files": {},
        "summary": {},
    }

    for reference_path in reference_files:
        relative_path = reference_path.relative_to(reference_dir)
        build_path = build_dir / relative_path
        entry: dict[str, Any] = {
            "path": str(relative_path),
            "reference_exists": True,
            "reference_size": reference_path.stat().st_size,
            "reference_sha256": sha256_file(reference_path),
            "build_exists": build_path.is_file(),
            "status": "missing",
        }
        if build_path.is_file():
            entry["build_size"] = build_path.stat().st_size
            entry["build_sha256"] = sha256_file(build_path)
            if (
                entry["reference_size"] == entry["build_size"]
                and entry["reference_sha256"] == entry["build_sha256"]
            ):
                entry["status"] = "match"
                matched += 1
            else:
                entry["status"] = "mismatch"
                mismatched += 1
        else:
            missing += 1
        report["files"][str(relative_path)] = entry

    total = matched + mismatched + missing
    report["summary"] = {
        "matched": matched,
        "mismatched": mismatched,
        "missing": missing,
        "total": total,
    }

    print(f"Reference directory: {reference_dir}")
    print(f"Build directory: {build_dir}")
    print(
        "Release payload comparison: "
        f"{matched} of {total} matched, {mismatched} mismatched, {missing} missing"
    )

    for relative_path, entry in report["files"].items():
        if entry["status"] == "match":
            continue
        print(f"{relative_path}: {entry['status']}")
        if entry["status"] == "missing":
            continue
        print(
            "  "
            f"reference={entry['reference_sha256']} ({entry['reference_size']} bytes)"
        )
        print(
            "  "
            f"built={entry['build_sha256']} ({entry['build_size']} bytes)"
        )

    if args.report_json:
        args.report_json.parent.mkdir(parents=True, exist_ok=True)
        args.report_json.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(f"Release payload report: {args.report_json}")

    failed = mismatched or missing
    if failed:
        print(
            "WARNING: built release payload differs from the preserved upstream reference.",
            file=sys.stderr,
        )

    return 1 if args.strict and failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
