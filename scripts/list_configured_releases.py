#!/usr/bin/env python3
"""Print configured firmware releases in the same layout as Makefile help."""

from __future__ import annotations

import json
import textwrap
from pathlib import Path


HELP_PAD = 31
HELP_GAP = 2
HELP_WIDTH = 80
DESC_WIDTH = HELP_WIDTH - HELP_PAD - HELP_GAP
DESC_INDENT = " " * (2 + HELP_PAD + HELP_GAP)


def print_help_line(label: str, description: str) -> None:
    wrapped = textwrap.wrap(description or "", width=DESC_WIDTH) or [""]
    if len(label) <= HELP_PAD:
        print(f"  {label:<{HELP_PAD}}{' ' * HELP_GAP}{wrapped[0]}")
    else:
        print(f"  {label}")
        print(f"{DESC_INDENT}{wrapped[0]}")
    for line in wrapped[1:]:
        print(f"{DESC_INDENT}{line}")


def main() -> None:
    data = json.loads(Path("config/releases.json").read_text(encoding="utf-8"))
    print("Configured Firmware Releases")
    print("\nDefault Release\n")
    print_help_line("default", data.get("default_release", ""))
    print("\nRelease Names\n")
    for branch, entry in sorted(data.get("releases", {}).items()):
        label = branch.removeprefix("source/release/")
        print_help_line(label, entry.get("description", ""))


if __name__ == "__main__":
    main()
