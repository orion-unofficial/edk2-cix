#!/usr/bin/env python3
"""Safely install a staged rendered-release firmware payload."""

from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path

from reconstruction_common import ReconstructionError, main_wrapper, truthy


HELP = """install-release-payload

The build-branch Makefile stages firmware through the rendered branch buildbox first,
then this helper copies the staged payload to the selected boot filesystem.

Defaults:
  INSTALL_ROOT=/boot/efi

Safety checks:
  - install root exists and is writable
  - containing filesystem is mounted read/write
  - free space is sufficient for the staged payload
  - existing destination files are not overwritten unless --force or FORCE=1
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=HELP,
    )
    p.add_argument("--worktree", type=Path, required=True, help="rendered release worktree")
    p.add_argument("--install-root", type=Path, default=Path(os.environ.get("INSTALL_ROOT", "/boot/efi")))
    p.add_argument("--source", default=os.environ.get("INSTALL_SOURCE", ""), help="optional staged payload path or path relative to dist/firmware")
    p.add_argument("--force", action="store_true", default=truthy(os.environ.get("FORCE", "0")), help="allow existing destination files to be replaced")
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def staged_firmware_root(worktree: Path) -> Path:
    root = worktree / "dist" / "firmware"
    if not root.is_dir():
        raise ReconstructionError(
            f"no staged firmware payload found at {root}; run the buildbox firmware-stage step first"
        )
    return root


def resolve_source(worktree: Path, source: str) -> Path:
    firmware_root = staged_firmware_root(worktree)
    if source:
        candidate = Path(source)
        if not candidate.is_absolute():
            candidate = firmware_root / candidate
        if not candidate.is_dir():
            raise ReconstructionError(f"requested staged payload does not exist: {candidate}")
        return candidate

    candidates = [
        path
        for path in firmware_root.rglob("*")
        if path.is_dir() and (path / "cix_flash_all.bin").is_file() and (path / "cix_flash_ota.bin").is_file()
    ]
    if not candidates:
        raise ReconstructionError(f"no installable staged payload was found below {firmware_root}")
    if len(candidates) != 1:
        listed = "\n".join(f"  - {path.relative_to(firmware_root)}" for path in candidates[:20])
        raise ReconstructionError(
            "multiple staged payloads are present; set INSTALL_SOURCE to select one:\n" + listed
        )
    return candidates[0]


def path_size(path: Path) -> int:
    total = 0
    for item in path.rglob("*"):
        if item.is_file() or item.is_symlink():
            total += item.lstat().st_size
    return total


def mountinfo_mounts() -> list[tuple[Path, bool]]:
    info = Path("/proc/self/mountinfo")
    if not info.exists():
        return []
    mounts: list[tuple[Path, bool]] = []
    for line in info.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = line.split()
        if len(parts) < 6:
            continue
        mount_point = parts[4].replace("\\040", " ")
        options = set(parts[5].split(","))
        mounts.append((Path(mount_point), "rw" in options))
    mounts.sort(key=lambda item: len(str(item[0])), reverse=True)
    return mounts


def containing_mount(path: Path) -> tuple[Path, bool] | None:
    try:
        resolved = path.resolve()
    except FileNotFoundError:
        resolved = path.absolute()
    for mount_point, writable in mountinfo_mounts():
        try:
            resolved.relative_to(mount_point)
        except ValueError:
            continue
        return mount_point, writable
    return None


def filesystem_read_write(path: Path) -> bool:
    mount = containing_mount(path)
    if mount is not None:
        return mount[1]
    try:
        return not bool(os.statvfs(path).f_flag & getattr(os, "ST_RDONLY", 1))
    except OSError as exc:
        raise ReconstructionError(f"cannot inspect filesystem flags for {path}: {exc}") from exc


def check_install_root(install_root: Path, required_bytes: int) -> None:
    if not install_root.exists():
        hint = ""
        if str(install_root) == "/boot/efi":
            hint = " Mount the EFI system partition or rerun with INSTALL_ROOT=/boot if that is correct for this system."
        raise ReconstructionError(f"install root does not exist: {install_root}.{hint}")
    if not install_root.is_dir():
        raise ReconstructionError(f"install root is not a directory: {install_root}")
    if not filesystem_read_write(install_root):
        raise ReconstructionError(f"install root is on a read-only filesystem: {install_root}")
    if not os.access(install_root, os.W_OK):
        raise ReconstructionError(
            f"install root is not writable by this user: {install_root}. "
            "Re-run with appropriate privileges or choose INSTALL_ROOT=<path>."
        )
    try:
        statvfs = os.statvfs(install_root)
    except OSError as exc:
        raise ReconstructionError(f"cannot inspect free space for {install_root}: {exc}") from exc
    available = statvfs.f_bavail * statvfs.f_frsize
    margin = 1024 * 1024
    if available < required_bytes + margin:
        raise ReconstructionError(
            f"not enough free space on {install_root}: need at least {required_bytes + margin} bytes, "
            f"available {available} bytes"
        )


def existing_destination_files(source: Path, destination: Path) -> list[Path]:
    conflicts: list[Path] = []
    for item in sorted(source.rglob("*")):
        rel = item.relative_to(source)
        target = destination / rel
        if item.is_dir():
            if target.exists() and not target.is_dir():
                conflicts.append(target)
            continue
        if target.exists() or target.is_symlink():
            conflicts.append(target)
    return conflicts


def copy_payload(source: Path, destination: Path, force: bool) -> None:
    for item in sorted(source.rglob("*")):
        rel = item.relative_to(source)
        target = destination / rel
        if item.is_dir():
            target.mkdir(parents=True, exist_ok=True)
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists() or target.is_symlink():
            if target.is_dir():
                raise ReconstructionError(f"cannot replace directory with file: {target}")
            if force:
                target.unlink()
        if item.is_symlink():
            os.symlink(os.readlink(item), target)
        else:
            shutil.copy2(item, target)


def main() -> None:
    args = parser().parse_args()
    verbose = truthy(args.v)
    worktree = args.worktree.resolve()
    source = resolve_source(worktree, args.source)
    firmware_root = staged_firmware_root(worktree)
    relative_payload = source.relative_to(firmware_root)
    destination = args.install_root.resolve() / relative_payload
    required = path_size(source)

    check_install_root(args.install_root, required)
    conflicts = existing_destination_files(source, destination)
    if conflicts and not args.force:
        sample = "\n".join(f"  - {path}" for path in conflicts[:20])
        extra = "" if len(conflicts) <= 20 else f"\n  ... and {len(conflicts) - 20} more"
        raise ReconstructionError(
            "install would overwrite existing file(s); no data was changed. "
            "Review the destination and rerun with FORCE=1 if replacement is intended.\n"
            + sample
            + extra
        )

    if verbose:
        print(f"Installing {source} -> {destination}", file=sys.stderr)
    copy_payload(source, destination, args.force)
    mode = "updated" if conflicts else "installed"
    print(f"[install] Firmware payload {mode} at {destination}")


if __name__ == "__main__":
    main_wrapper(main)
