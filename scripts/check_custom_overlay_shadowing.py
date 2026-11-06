#!/usr/bin/env python3

from __future__ import annotations

import argparse
import sys
from pathlib import Path


PACKAGE_ROOTS = ("edk2", "edk2-platforms", "edk2-non-osi")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Reject partial custom overlay directories that sit inside an "
            "imported EDK2 module directory without also shadowing that "
            "module root."
        )
    )
    parser.add_argument("--overlay-root", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    return parser.parse_args()


def inf_files(directory: Path) -> list[Path]:
    return sorted(path for path in directory.glob("*.inf") if path.is_file())


def check_package_root(
    overlay_package_root: Path,
    source_package_root: Path,
) -> list[tuple[Path, Path, list[Path]]]:
    issues: dict[Path, tuple[Path, Path, list[Path]]] = {}

    if not overlay_package_root.is_dir():
        return []

    for overlay_path in sorted(path for path in overlay_package_root.rglob("*") if path.is_file()):
        current = overlay_path.parent
        while current != overlay_package_root and overlay_package_root in current.parents:
            relative_dir = current.relative_to(overlay_package_root)
            source_dir = source_package_root / relative_dir
            source_infs = inf_files(source_dir) if source_dir.is_dir() else []
            if source_infs:
                overlay_infs = inf_files(current)
                if not overlay_infs:
                    key = current.relative_to(overlay_package_root)
                    issues[key] = (overlay_path.relative_to(overlay_package_root), key, source_infs)
                break
            current = current.parent

    return [issues[key] for key in sorted(issues)]


def main() -> int:
    args = parse_args()
    overlay_root = args.overlay_root.resolve()
    source_root = args.source_root.resolve()

    issues: list[tuple[Path, Path, list[Path]]] = []
    for package_root in PACKAGE_ROOTS:
        issues.extend(
            check_package_root(
                overlay_root / package_root,
                source_root / package_root,
            )
        )

    if not issues:
        print("[overlay-check] no partial module overlays detected")
        return 0

    print(
        "[overlay-check] partial overlay directories would shadow imported EDK2 modules:",
        file=sys.stderr,
    )
    for offending_file, module_dir, source_infs in issues:
        inf_names = ", ".join(path.name for path in source_infs)
        print(
            f"[overlay-check] {offending_file} sits under module directory {module_dir} "
            f"from the imported tree ({inf_names}) without a matching overlay .inf at that module root",
            file=sys.stderr,
        )
    print(
        "[overlay-check] move data-only assets outside the imported module directory "
        "or shadow the full module root intentionally.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
