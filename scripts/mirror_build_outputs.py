#!/usr/bin/env python3
"""Mirror useful rendered-worktree build outputs to the build-branch dist tree."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import sys
from pathlib import Path

from reconstruction_common import ReconstructionError, main_wrapper, truthy


HELP = """mirror-build-outputs

The build branch renders firmware source targets into cached worktrees, then
delegates the actual firmware build there. This helper copies the useful output
artefacts back to the build checkout so users do not have to find them below
.cache/edk2-cix/worktrees/.

Mirrored outputs:
  - everything produced below the rendered worktree dist/ directory, preserving
    relative paths;
  - selected raw firmware images and build metadata from src/Build/<board>/<target>.

The mirror is intentionally a copy, not a symlink, so outputs remain available
after stale rendered worktrees are cleaned.
"""

RAW_OUTPUTS = (
    "BuildOptions",
    "cix_flash_all.bin",
    "cix_flash_all.raw",
    "cix_flash_ota.bin",
    "cix_flash_ota.bin.tmp",
    "Firmwares/bootloader3.img",
    "Firmwares/csu_pm_config.bin",
    "Firmwares/memory_config.bin",
    "FV/SKY1_BL33_UEFI.fd",
)

SAFE_COMPONENT_RE = re.compile(r"[^A-Za-z0-9._+-]+")


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=HELP,
    )
    p.add_argument("--repo-root", type=Path, required=True, help="build-branch checkout root")
    p.add_argument("--worktree", type=Path, required=True, help="rendered source worktree root")
    p.add_argument("--dist-root", type=Path, required=True, help="destination dist directory")
    p.add_argument("--release", required=True, help="source-target label used for destination paths")
    p.add_argument("--build-target", required=True, help="delegated build target that produced the outputs")
    p.add_argument("--board", required=True, help="firmware board, for example O6")
    p.add_argument("--firmware-target", required=True, help="firmware target, for example RELEASE")
    p.add_argument("--artefact-mode", required=True, help="firmware artefact mode, for example custom")
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def firmware_target_prefix(target: str) -> str:
    value = target.strip()
    if not value:
        raise ReconstructionError("firmware target must not be empty")
    return value.upper()


def find_firmware_build_root(
    worktree: Path,
    board: str,
    target: str,
) -> tuple[Path, str] | None:
    build_root = worktree / "src" / "Build" / board
    if not build_root.is_dir():
        return None

    prefix = firmware_target_prefix(target)
    if "_" in prefix:
        candidate = build_root / prefix
        return (candidate, prefix) if candidate.is_dir() else None

    candidates = [
        path
        for path in sorted(build_root.glob(f"{prefix}_*"))
        if path.is_dir()
        and re.fullmatch(rf"{re.escape(prefix)}_[A-Z0-9]+", path.name)
        and any((path / relative_name).is_file() for relative_name in RAW_OUTPUTS)
    ]
    if not candidates:
        return None
    if len(candidates) > 1:
        leaves = ", ".join(path.name for path in candidates)
        raise ReconstructionError(
            f"multiple {prefix} firmware output trees exist for {board}: {leaves}; "
            "clean the rendered source worktree before rebuilding"
        )
    return candidates[0], candidates[0].name


def safe_path_components(value: str) -> list[str]:
    label = value.strip()
    for prefix in ("refs/heads/",):
        if label.startswith(prefix):
            label = label[len(prefix) :]
    for prefix in ("source/cache/release/",):
        if label.startswith(prefix):
            label = label[len(prefix) :]

    parts: list[str] = []
    for raw_part in label.split("/"):
        part = raw_part.strip()
        if not part or part in {".", ".."}:
            continue
        parts.append(SAFE_COMPONENT_RE.sub("_", part))
    if not parts:
        return ["default-source-target"]
    return parts


def ensure_inside(path: Path, root: Path) -> None:
    try:
        path.resolve().relative_to(root.resolve())
    except ValueError as exc:
        raise ReconstructionError(f"refusing to write outside dist root: {path}") from exc


def copy_file(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.is_symlink():
        if destination.exists() or destination.is_symlink():
            if destination.is_dir() and not destination.is_symlink():
                raise ReconstructionError(f"cannot replace directory with symlink: {destination}")
            destination.unlink()
        os.symlink(os.readlink(source), destination)
        return
    shutil.copy2(source, destination)


def mirror_dist_outputs(worktree: Path, dist_root: Path) -> list[Path]:
    source_root = worktree / "dist"
    copied: list[Path] = []
    if not source_root.is_dir():
        return copied

    for item in sorted(source_root.rglob("*")):
        if item.is_dir():
            continue
        relative = item.relative_to(source_root)
        destination = dist_root / relative
        ensure_inside(destination, dist_root)
        copy_file(item, destination)
        copied.append(destination)
    return copied


def mirror_raw_outputs(
    worktree: Path,
    dist_root: Path,
    release: str,
    artefact_mode: str,
    board: str,
    firmware_target: str,
) -> list[Path]:
    copied: list[Path] = []
    resolved = find_firmware_build_root(worktree, board, firmware_target)
    if resolved is None:
        return copied
    source_root, leaf = resolved

    destination_root = dist_root / "build"
    for component in safe_path_components(release):
        destination_root /= component
    destination_root = destination_root / artefact_mode / board / leaf
    ensure_inside(destination_root, dist_root)
    if destination_root.exists():
        shutil.rmtree(destination_root)

    for relative_name in RAW_OUTPUTS:
        source = source_root / relative_name
        if not source.is_file() and not source.is_symlink():
            continue
        destination = destination_root / relative_name
        ensure_inside(destination, dist_root)
        copy_file(source, destination)
        copied.append(destination)
    return copied


def main() -> None:
    args = parser().parse_args()
    verbose = truthy(args.v)
    repo_root = args.repo_root.resolve()
    worktree = args.worktree.resolve()
    dist_root = args.dist_root
    if not dist_root.is_absolute():
        dist_root = repo_root / dist_root
    dist_root = dist_root.resolve()

    if not worktree.is_dir():
        raise ReconstructionError(f"rendered worktree does not exist: {worktree}")
    dist_root.mkdir(parents=True, exist_ok=True)

    copied = []
    copied.extend(mirror_dist_outputs(worktree, dist_root))
    copied.extend(
        mirror_raw_outputs(
            worktree,
            dist_root,
            args.release,
            args.artefact_mode,
            args.board,
            args.firmware_target,
        )
    )

    if not copied:
        return
    if verbose:
        for path in copied:
            print(f"mirrored {path.relative_to(dist_root)}", file=sys.stderr)
    print(f"[output] Mirrored {len(copied)} build artefact(s) to {dist_root}")


if __name__ == "__main__":
    main_wrapper(main)
