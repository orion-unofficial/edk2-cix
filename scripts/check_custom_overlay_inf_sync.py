#!/usr/bin/env python3
"""Verify that custom overlay module INFs keep source dependency entries."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


TRACKED_SECTIONS = (
    "Packages",
    "LibraryClasses",
    "Protocols",
    "Guids",
    "Pcd",
    "FixedPcd",
    "Ppis",
    "PcdEx",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Ensure custom overlay module INFs stay in sync with the imported "
            "source module dependencies."
        )
    )
    parser.add_argument("--overlay-root", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    return parser.parse_args()


def normalize_line(raw_line: str) -> str:
    stripped = raw_line.split("#", 1)[0].split(";", 1)[0].strip()
    return " ".join(stripped.split())


def parse_inf_sections(path: Path) -> dict[str, set[str]]:
    sections: dict[str, set[str]] = {}
    current_section: str | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line:
            continue
        match = re.match(r"\[([^\]]+)\]", line)
        if match:
            current_section = match.group(1).split(".", 1)[0]
            continue
        if current_section not in TRACKED_SECTIONS:
            continue
        normalized = normalize_line(raw_line)
        if normalized:
            sections.setdefault(current_section, set()).add(normalized)
    return sections


def compare_overlay_inf(
    overlay_inf: Path,
    source_inf: Path,
    overlay_root: Path,
) -> list[str]:
    overlay_sections = parse_inf_sections(overlay_inf)
    source_sections = parse_inf_sections(source_inf)
    problems: list[str] = []

    for section_name, source_entries in source_sections.items():
        missing_entries = sorted(source_entries - overlay_sections.get(section_name, set()))
        for entry in missing_entries:
            problems.append(
                f"{overlay_inf.relative_to(overlay_root)} is missing [{section_name}] entry: {entry}"
            )

    return problems


def main() -> int:
    args = parse_args()
    overlay_root = args.overlay_root.resolve()
    source_root = args.source_root.resolve()

    overlay_infs = sorted(path for path in overlay_root.rglob("*.inf") if path.is_file())
    checked = 0
    problems: list[str] = []

    for overlay_inf in overlay_infs:
        relative_path = overlay_inf.relative_to(overlay_root)
        source_inf = source_root / relative_path
        if not source_inf.is_file():
            continue
        checked += 1
        problems.extend(compare_overlay_inf(overlay_inf, source_inf, overlay_root))

    if problems:
        for problem in problems:
            print(f"[overlay-sync] ERROR: {problem}", file=sys.stderr)
        return 1

    print(f"[overlay-sync] {checked} overlay module INF files matched source dependency sections")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
