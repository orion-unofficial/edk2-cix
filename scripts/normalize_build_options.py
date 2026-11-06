#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path
import re


WINDOWS_DRIVE_PATH_RE = re.compile(r"^[A-Za-z]:[\\/]")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Normalize the shipped BuildOptions metadata to stable repo-relative "
            "Active Platform and Flash Image Definition paths."
        )
    )
    parser.add_argument("--build-options", type=Path, required=True)
    parser.add_argument("--active-platform", required=True)
    parser.add_argument("--flash-definition", required=True)
    return parser.parse_args()


def is_repo_relative_path(value: str) -> bool:
    return not value.startswith("/") and WINDOWS_DRIVE_PATH_RE.match(value) is None


def normalize_build_options_text(
    text: str,
    active_platform: str,
    flash_definition: str,
) -> str:
    if not is_repo_relative_path(active_platform):
        raise ValueError(f"Active Platform must be repo-relative, got: {active_platform}")
    if not is_repo_relative_path(flash_definition):
        raise ValueError(f"Flash Image Definition must be repo-relative, got: {flash_definition}")

    lines = text.splitlines()
    found_active = False
    found_flash = False
    normalized: list[str] = []
    for line in lines:
        if line.startswith("Active Platform: "):
            normalized.append(f"Active Platform: {active_platform}")
            found_active = True
        elif line.startswith("Flash Image Definition: "):
            normalized.append(f"Flash Image Definition: {flash_definition}")
            found_flash = True
        else:
            normalized.append(line)

    if not found_active:
        raise ValueError("BuildOptions is missing an Active Platform line")
    if not found_flash:
        raise ValueError("BuildOptions is missing a Flash Image Definition line")
    return "\n".join(normalized) + "\n"


def normalize_build_options_file(
    build_options_path: Path,
    active_platform: str,
    flash_definition: str,
) -> None:
    text = build_options_path.read_text(encoding="utf-8")
    build_options_path.write_text(
        normalize_build_options_text(text, active_platform, flash_definition),
        encoding="utf-8",
    )


def main() -> None:
    args = parse_args()
    normalize_build_options_file(
        args.build_options.resolve(),
        args.active_platform,
        args.flash_definition,
    )


if __name__ == "__main__":
    main()
