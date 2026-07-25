#!/usr/bin/env python3
"""Validate the custom ACPI module overlays against imported sources."""

from __future__ import annotations

import argparse
import filecmp
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class OverlayDirectory:
    label: str
    overlay_root: str
    source_root: str
    module_inf: str
    allowed_extra_files: tuple[str, ...] = ()


@dataclass(frozen=True)
class OverlayFile:
    label: str
    overlay_path: str
    source_path: str


DIRECTORY_OVERLAYS: tuple[OverlayDirectory, ...] = (
    OverlayDirectory(
        label="Sky1 ACPI tables overlay",
        overlay_root="custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables",
        source_root="src/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables",
        module_inf="AcpiSocTables.inf",
        allowed_extra_files=("Dsdt-BusPerf.asl",),
    ),
    OverlayDirectory(
        label="O6 ACPI platform tables overlay",
        overlay_root="custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/AcpiPlatfomTables",
        source_root="src/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/AcpiPlatfomTables",
        module_inf="AcpiPlatfomTables.inf",
    ),
)

FILE_OVERLAYS: tuple[OverlayFile, ...] = (
    OverlayFile(
        label="O6 Linux ACPI config header overlay",
        overlay_path="custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/LinuxAcpiConfig.h",
        source_path="src/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/LinuxAcpiConfig.h",
    ),
    OverlayFile(
        label="O6N Linux ACPI config header overlay",
        overlay_path="custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6N/Drivers/LinuxAcpiConfig.h",
        source_path="src/edk2-platforms/Platform/Radxa/Orion/O6N/Drivers/LinuxAcpiConfig.h",
    ),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Verify that the custom ACPI overlays stay aligned with the imported "
            "Radxa/CIX sources."
        )
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path.cwd(),
        help="Path to the repository root (defaults to the current directory).",
    )
    return parser.parse_args()


def list_files(root: Path) -> set[Path]:
    return {
        path.relative_to(root)
        for path in root.rglob("*")
        if path.is_file()
    }


def compare_directory_overlay(
    repo_root: Path, overlay: OverlayDirectory, problems: list[str], notes: list[str]
) -> None:
    overlay_root = repo_root / overlay.overlay_root
    source_root = repo_root / overlay.source_root
    if not overlay_root.is_dir():
        problems.append(f"{overlay.label}: missing overlay directory {overlay_root}")
        return
    if not source_root.is_dir():
        problems.append(f"{overlay.label}: missing source directory {source_root}")
        return
    module_inf = overlay_root / overlay.module_inf
    if not module_inf.is_file():
        problems.append(
            f"{overlay.label}: missing module INF {module_inf.relative_to(repo_root)}"
        )
    overlay_files = list_files(overlay_root)
    source_files = list_files(source_root)
    allowed_extra_files = {Path(path) for path in overlay.allowed_extra_files}
    missing_in_overlay = sorted(source_files - overlay_files)
    missing_in_source = sorted(overlay_files - source_files - allowed_extra_files)
    if missing_in_overlay:
        problems.extend(
            f"{overlay.label}: overlay is missing mirrored file {overlay.overlay_root}/{path.as_posix()}"
            for path in missing_in_overlay
        )
    if missing_in_source:
        problems.extend(
            f"{overlay.label}: overlay file has no imported counterpart {overlay.overlay_root}/{path.as_posix()}"
            for path in missing_in_source
        )
    shared_files = sorted(overlay_files & source_files)
    changed = 0
    mirrored = 0
    for relpath in shared_files:
        if filecmp.cmp(overlay_root / relpath, source_root / relpath, shallow=False):
            mirrored += 1
        else:
            changed += 1
    if shared_files and changed == 0:
        problems.append(
            f"{overlay.label}: no files differ from imported sources; drop this overlay instead of shadowing the whole module"
        )
    notes.append(
        f"{overlay.label}: {len(shared_files)} files checked, {changed} customized, {mirrored} mirrored byte-for-byte"
    )


def compare_file_overlay(
    repo_root: Path, overlay: OverlayFile, problems: list[str], notes: list[str]
) -> None:
    overlay_path = repo_root / overlay.overlay_path
    source_path = repo_root / overlay.source_path
    if not overlay_path.is_file():
        problems.append(f"{overlay.label}: missing overlay file {overlay_path}")
        return
    if not source_path.is_file():
        problems.append(f"{overlay.label}: missing source file {source_path}")
        return
    identical = filecmp.cmp(overlay_path, source_path, shallow=False)
    state = "mirrored byte-for-byte" if identical else "customized"
    notes.append(f"{overlay.label}: {state}")


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    problems: list[str] = []
    notes: list[str] = []
    for overlay in DIRECTORY_OVERLAYS:
        compare_directory_overlay(repo_root, overlay, problems, notes)
    for overlay in FILE_OVERLAYS:
        compare_file_overlay(repo_root, overlay, problems, notes)
    if problems:
        for problem in problems:
            print(f"[check-custom-acpi-overlays] ERROR: {problem}")
        return 1
    for note in notes:
        print(f"[check-custom-acpi-overlays] {note}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
