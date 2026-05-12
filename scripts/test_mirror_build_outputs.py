#!/usr/bin/env python3
"""Regression tests for build-output mirroring."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


def repo_root() -> Path:
    result = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return Path(result.stdout.strip())


def run_mirror(repo: Path, worktree: Path, dist: Path) -> None:
    subprocess.run(
        [
            "python3",
            "scripts/mirror_build_outputs.py",
            "--repo-root",
            str(repo),
            "--worktree",
            str(worktree),
            "--dist-root",
            str(dist),
            "--release",
            "source/cache/release/edk2-202602/cix-1.2/radxa-1.2.1/unofficial-1.2.1",
            "--build-target",
            "buildbox-targz",
            "--board",
            "O6",
            "--firmware-target",
            "RELEASE",
            "--artefact-mode",
            "custom",
        ],
        cwd=repo,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def main() -> None:
    repo = repo_root()
    makefile = (repo / "Makefile").read_text(encoding="utf-8")
    if "$$wt/.cache/edk2-cix/firmware" in makefile:
        raise SystemExit("build-branch firmware cache must not live below rendered worktrees")
    if 'CCACHE_DIR="$$cache_root/ccache"' in makefile:
        raise SystemExit("buildbox ccache path must be container-visible, not a host-only path")
    if 'BUILDBOX_HOST_TMPDIR="$$cache_root/buildbox"' not in makefile:
        raise SystemExit("buildbox host mount root must live below FIRMWARE_CACHE_ROOT")
    if 'CCACHE_DIR="$$container_cache_root/ccache"' not in makefile:
        raise SystemExit("buildbox ccache must be addressed through the container mount")

    with tempfile.TemporaryDirectory(prefix="edk2-cix-mirror-test-") as tmp:
        root = Path(tmp)
        worktree = root / "rendered"
        dist = root / "dist"
        build_root = worktree / "src" / "Build" / "O6" / "RELEASE_GCC5"
        write_file(worktree / "dist" / "edk2-cix-orion-o6-1.2.1-custom.tar.gz", "archive")
        write_file(worktree / "dist" / "firmware" / "O6" / "cix_flash_all.bin", "staged")
        write_file(build_root / "BuildOptions", "options")
        write_file(build_root / "cix_flash_all.bin", "all")
        write_file(build_root / "cix_flash_ota.bin", "ota")
        write_file(build_root / "FV" / "SKY1_BL33_UEFI.fd", "fd")
        write_file(build_root / "ignored.obj", "ignore")

        run_mirror(repo, worktree, dist)

        expected_archive = dist / "edk2-cix-orion-o6-1.2.1-custom.tar.gz"
        if expected_archive.read_text(encoding="utf-8") != "archive":
            raise SystemExit("worktree dist archive was not mirrored")
        expected_stage = dist / "firmware" / "O6" / "cix_flash_all.bin"
        if expected_stage.read_text(encoding="utf-8") != "staged":
            raise SystemExit("staged payload was not mirrored")
        raw_root = (
            dist
            / "build"
            / "edk2-202602"
            / "cix-1.2"
            / "radxa-1.2.1"
            / "unofficial-1.2.1"
            / "custom"
            / "O6"
            / "RELEASE_GCC5"
        )
        if (raw_root / "cix_flash_all.bin").read_text(encoding="utf-8") != "all":
            raise SystemExit("raw build cix_flash_all.bin was not mirrored")
        if (raw_root / "FV" / "SKY1_BL33_UEFI.fd").read_text(encoding="utf-8") != "fd":
            raise SystemExit("raw build firmware volume was not mirrored")
        if (raw_root / "ignored.obj").exists():
            raise SystemExit("unexpected raw build scratch file was mirrored")

        write_file(raw_root / "stale.txt", "stale")
        run_mirror(repo, worktree, dist)
        if (raw_root / "stale.txt").exists():
            raise SystemExit("raw mirror destination retained a stale file")


if __name__ == "__main__":
    main()
