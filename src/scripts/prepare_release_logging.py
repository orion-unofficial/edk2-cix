#!/usr/bin/env python3
"""Overlay the selected MdePkg header without changing imported EDK2 sources."""

import argparse
from pathlib import Path
import shutil


def prepare(package: Path, extension: Path, output: Path) -> None:
    """Mirror only the parents of DebugLib.h; all other package inputs stay exact."""
    package = package.resolve()
    extension = extension.resolve()
    header = package / "Include/Library/DebugLib.h"
    original = header.read_bytes()
    policy = extension.read_bytes()
    if b"#define DEBUG(Expression)" not in original or b"#define _DEBUG_PRINT(" not in original:
        raise ValueError("unsupported upstream DebugLib.h logging macros")
    destination = output / "MdePkg"
    if destination.exists():
        shutil.rmtree(destination)
    for relative in (Path("."), Path("Include"), Path("Include/Library")):
        target = destination / relative
        target.mkdir(parents=True, exist_ok=True)
        for entry in (package / relative).iterdir():
            child = relative / entry.name
            if child.as_posix() in ("Include", "Include/Library", "Include/Library/DebugLib.h"):
                continue
            if child.as_posix() == "MdePkg.dec":
                # Keep package resolution rooted in this overlay, even for readers
                # that resolve descriptor symlinks before calculating include paths.
                shutil.copyfile(entry, target / entry.name)
            else:
                (target / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
    (destination / "Include/Library/DebugLib.h").write_bytes(original + b"\n" + policy)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--extension", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    prepare(args.package, args.extension, args.output)


if __name__ == "__main__":
    main()
