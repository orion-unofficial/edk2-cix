#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Summarize build-validation JSON reports as short Markdown."
    )
    parser.add_argument("--report-root", type=Path, default=Path("build-validation"))
    parser.add_argument("--heading", default="Validation reports")
    return parser.parse_args()


def format_summary(summary: Any, mismatches: Any) -> str:
    if isinstance(summary, dict) and summary:
        parts = [f"{key}={value}" for key, value in sorted(summary.items())]
        return ", ".join(parts)
    if isinstance(mismatches, list):
        return f"mismatches={len(mismatches)}"
    return "-"


def main() -> int:
    args = parse_args()
    report_root = args.report_root
    print(f"### {args.heading}")
    print()

    reports = sorted(report_root.glob("*.json"))
    if not reports:
        print(f"No JSON reports found under `{report_root}`.")
        return 0

    print("| Report | Status | Profile | Board | Detail |")
    print("| --- | --- | --- | --- | --- |")
    for path in reports:
        payload = json.loads(path.read_text(encoding="utf-8"))
        status = payload.get("status", "unknown")
        profile = payload.get("profile", "-")
        board = payload.get("board", "-")
        detail = format_summary(payload.get("summary"), payload.get("mismatches"))
        print(f"| `{path.name}` | `{status}` | `{profile}` | `{board}` | `{detail}` |")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
