#!/usr/bin/env python3
"""Regression tests for build-output mirroring."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path

from test_support import load_function_tests


def repo_root() -> Path:
    result = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return Path(result.stdout.strip())


def run_mirror(
    repo: Path,
    worktree: Path,
    dist: Path,
    *,
    build_target: str = "buildbox-targz",
    artefact_mode: str = "custom",
) -> None:
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
            build_target,
            "--board",
            "O6",
            "--firmware-target",
            "RELEASE",
            "--artefact-mode",
            artefact_mode,
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
    mirror_lines = [
        line for line in makefile.splitlines() if "--artefact-mode" in line
    ]
    if not mirror_lines or any(
        "ENABLE_FIRMWARE_FIXES" not in line or "+fixes" not in line
        for line in mirror_lines
    ):
        raise SystemExit("raw build mirrors must keep custom and custom+fixes separate")

    with tempfile.TemporaryDirectory(prefix="edk2-cix-mirror-test-") as tmp:
        root = Path(tmp)
        worktree = root / "rendered"
        dist = root / "dist"
        build_root = worktree / "src" / "Build" / "O6" / "RELEASE_GCC"
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

        expected_archive.unlink()
        run_mirror(
            repo,
            worktree,
            dist,
            build_target="buildbox-firmware-build",
            artefact_mode="custom+fixes",
        )
        if expected_archive.exists():
            raise SystemExit("ordinary firmware build recopied a stale archive")
        fixes_raw_root = (
            dist
            / "build"
            / "edk2-202602"
            / "cix-1.2"
            / "radxa-1.2.1"
            / "unofficial-1.2.1"
            / "custom+fixes"
            / "O6"
            / "RELEASE_GCC"
        )
        if (fixes_raw_root / "BuildOptions").read_text(encoding="utf-8") != "options":
            raise SystemExit("custom+fixes raw output was not mirrored separately")
        raw_root = (
            dist
            / "build"
            / "edk2-202602"
            / "cix-1.2"
            / "radxa-1.2.1"
            / "unofficial-1.2.1"
            / "custom"
            / "O6"
            / "RELEASE_GCC"
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

        shutil.rmtree(build_root)
        legacy_build_root = worktree / "src" / "Build" / "O6" / "RELEASE_GCC5"
        write_file(legacy_build_root / "cix_flash_all.bin", "legacy")
        run_mirror(repo, worktree, dist)
        legacy_raw_root = raw_root.parent / "RELEASE_GCC5"
        if (legacy_raw_root / "cix_flash_all.bin").read_text(encoding="utf-8") != "legacy":
            raise SystemExit("legacy GCC5 replay output was not mirrored")

        write_file(build_root / "cix_flash_all.bin", "current")
        result = subprocess.run(
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
                "source/unofficial/1.3/current",
                "--build-target",
                "buildbox-firmware-build",
                "--board",
                "O6",
                "--firmware-target",
                "RELEASE",
                "--artefact-mode",
                "custom",
            ],
            cwd=repo,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if result.returncode == 0 or "multiple RELEASE firmware output trees" not in result.stderr:
            raise SystemExit("ambiguous GCC/GCC5 output trees were not rejected")


def load_tests(loader, tests, pattern):
    return load_function_tests(globals())


if __name__ == "__main__":
    main()
