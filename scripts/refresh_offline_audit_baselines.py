#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


SCRIPT_PATH = pathlib.Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent
FINAL_IMAGE_SCRIPT = REPO_ROOT / "scripts" / "audit_final_image_manifest.py"
ACPI_SCRIPT = REPO_ROOT / "scripts" / "audit_acpi_regression.py"
DEFAULT_MANIFEST_BASELINE = REPO_ROOT / "validation" / "final-image-manifests.json"
DEFAULT_ACPI_BASELINE = REPO_ROOT / "validation" / "acpi-audit-baselines.json"
DEFAULT_PROFILE = "upstream-1.2.1-bookworm"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Refresh both offline audit baseline files from a selected build tree."
    )
    parser.add_argument("--repo-root", type=pathlib.Path, default=REPO_ROOT)
    parser.add_argument("--build-dir", type=pathlib.Path)
    parser.add_argument("--board", default="O6")
    parser.add_argument("--target", default="RELEASE_GCC")
    parser.add_argument("--profile", default=DEFAULT_PROFILE)
    parser.add_argument("--report-dir", type=pathlib.Path)
    parser.add_argument("--manifest-baseline", type=pathlib.Path, default=DEFAULT_MANIFEST_BASELINE)
    parser.add_argument("--acpi-baseline", type=pathlib.Path, default=DEFAULT_ACPI_BASELINE)
    return parser.parse_args()


def resolve_build_dir(args: argparse.Namespace) -> pathlib.Path:
    if args.build_dir:
        return args.build_dir.resolve()
    return args.repo_root.resolve() / "src" / "Build" / args.board / args.target


def report_path(report_dir: pathlib.Path | None, board: str, target: str, profile: str, suffix: str) -> pathlib.Path | None:
    if report_dir is None:
        return None
    report_dir.mkdir(parents=True, exist_ok=True)
    return report_dir / f"{board}-{target}-{profile}-{suffix}.json"


def run_audit(script: pathlib.Path, build_dir: pathlib.Path, board: str, target: str, profile: str, baseline: pathlib.Path, report_json: pathlib.Path | None) -> None:
    command = [
        sys.executable,
        str(script),
        "--repo-root",
        str(REPO_ROOT),
        "--build-dir",
        str(build_dir),
        "--board",
        board,
        "--target",
        target,
        "--profile",
        profile,
        "--emit-baseline",
        str(baseline),
        "--emit-profile-name",
        profile,
    ]
    if report_json is not None:
        command.extend(["--report-json", str(report_json)])
    subprocess.run(command, check=True)


def main() -> int:
    args = parse_args()
    build_dir = resolve_build_dir(args)
    if not build_dir.is_dir():
        raise SystemExit(
            f"Build directory does not exist: {build_dir}\n"
            "Run a matching firmware build first, then rerun the baseline refresh."
        )
    print(f"[baseline-refresh] build directory: {build_dir}")
    print(f"[baseline-refresh] profile: {args.profile}")
    print(f"[baseline-refresh] board: {args.board}")

    run_audit(
        FINAL_IMAGE_SCRIPT,
        build_dir,
        args.board,
        args.target,
        args.profile,
        args.manifest_baseline,
        report_path(args.report_dir, args.board, args.target, args.profile, "manifest-audit"),
    )
    run_audit(
        ACPI_SCRIPT,
        build_dir,
        args.board,
        args.target,
        args.profile,
        args.acpi_baseline,
        report_path(args.report_dir, args.board, args.target, args.profile, "acpi-audit"),
    )
    print("[baseline-refresh] updated both offline audit baseline files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
