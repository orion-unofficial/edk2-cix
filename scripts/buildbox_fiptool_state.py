#!/usr/bin/env python3

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def has_fiptool_binary(build_dir: Path) -> bool:
    return any(build_dir.rglob("fiptool"))


def determine_cleanup_reason(
    build_dir: Path,
    stamp_path: Path,
    requested_distro: str,
) -> str | None:
    if not build_dir.is_dir():
        return None

    if stamp_path.is_file():
        built_distro = stamp_path.read_text(encoding="utf-8").strip()
        if built_distro and built_distro != requested_distro:
            return f"built for {built_distro}"
        return None

    if has_fiptool_binary(build_dir):
        return "existing build predates distro stamp"

    return None


def prepare_build_dir(
    build_dir: Path,
    stamp_path: Path,
    requested_distro: str,
) -> str | None:
    reason = determine_cleanup_reason(build_dir, stamp_path, requested_distro)
    if reason is None:
        return None
    shutil.rmtree(build_dir)
    return reason


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Manage the shared buildbox fiptool build directory."
    )
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--stamp-path", required=True, type=Path)
    parser.add_argument("--requested-distro", required=True)
    parser.add_argument(
        "--prepare",
        action="store_true",
        help="Remove the build directory when it is stale and print the reason.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.prepare:
        reason = prepare_build_dir(
            build_dir=args.build_dir,
            stamp_path=args.stamp_path,
            requested_distro=args.requested_distro,
        )
    else:
        reason = determine_cleanup_reason(
            build_dir=args.build_dir,
            stamp_path=args.stamp_path,
            requested_distro=args.requested_distro,
        )

    if reason:
        print(reason)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
