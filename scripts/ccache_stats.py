#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path


def parse_print_stats(text: str) -> dict[str, int]:
    stats: dict[str, int] = {}
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 2:
            continue
        key, value = parts
        try:
            stats[key] = int(value)
        except ValueError:
            continue
    return stats


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Print ccache stats as JSON.")
    parser.add_argument("--cache-dir", required=True, type=Path)
    parser.add_argument("--ccache-bin", default="ccache")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    cache_dir = args.cache_dir
    ccache_bin = shutil.which(args.ccache_bin)

    if ccache_bin is None or not cache_dir.exists():
        json.dump({}, sys.stdout, sort_keys=True)
        sys.stdout.write("\n")
        return 0

    for command in (
        [ccache_bin, "--print-stats", "--format=json", "-d", str(cache_dir)],
        [ccache_bin, "--print-stats", "-d", str(cache_dir)],
    ):
        try:
            result = subprocess.run(
                command,
                check=True,
                capture_output=True,
                text=True,
            )
        except subprocess.CalledProcessError:
            continue

        if "--format=json" in command:
            sys.stdout.write(result.stdout)
            if not result.stdout.endswith("\n"):
                sys.stdout.write("\n")
            return 0

        json.dump(parse_print_stats(result.stdout), sys.stdout, sort_keys=True)
        sys.stdout.write("\n")
        return 0

    json.dump({}, sys.stdout, sort_keys=True)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
