#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Format captured buildbox ccache stats.")
    parser.add_argument("stats_log", type=Path)
    parser.add_argument("--format", choices=("human", "json"), default="human")
    return parser.parse_args()


def load_stats(path: Path) -> dict[str, int]:
    text = path.read_text(encoding="utf-8")
    start = text.rfind("{")
    if start == -1:
        return {}
    data = json.loads(text[start:])
    return {str(key): int(value) for key, value in data.items()}


def human_summary(stats: dict[str, int]) -> str:
    hits = stats.get("direct_cache_hit", 0) + stats.get("preprocessed_cache_hit", 0)
    misses = stats.get("cache_miss", 0)
    cacheable = hits + misses
    hit_rate = (hits / cacheable * 100.0) if cacheable else 0.0
    size_mib = stats.get("cache_size_kibibyte", 0) / 1024.0
    files = stats.get("files_in_cache", 0)
    if not stats:
        return "[ccache] stats unavailable"
    return (
        f"[ccache] {cacheable} cacheable, {hits} hits, {misses} misses, "
        f"{hit_rate:.1f}% hit rate, {files} files, {size_mib:.1f} MiB cache"
    )


def main() -> int:
    args = parse_args()
    stats = load_stats(args.stats_log)
    if args.format == "json":
        print(json.dumps(stats, sort_keys=True))
    else:
        print(human_summary(stats))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
