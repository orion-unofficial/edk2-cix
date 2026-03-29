#!/usr/bin/env python3
"""Compare the source cix_package_tool prototype against vendor outputs."""

from __future__ import annotations

import hashlib
import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
TOOL = REPO_ROOT / "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/source_tools/cix_package_tool/cix_package_tool.py"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_source_tool(*args: str, cwd: Path | None = None) -> None:
    subprocess.run(
        ["python3", str(TOOL), *args],
        cwd=str(cwd) if cwd is not None else None,
        check=True,
    )


def compare_file(label: str, left: Path, right: Path) -> None:
    if left.read_bytes() != right.read_bytes():
        raise SystemExit(
            f"{label} differs:\n"
            f"  left : {left} {sha256(left)}\n"
            f"  right: {right} {sha256(right)}"
        )
    print(f"{label}: match {sha256(left)}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--inspect-root",
        default=str(REPO_ROOT / ".buildbox/tmp/cix-package-tool-inspect"),
        help="Path containing all-check/, ota-check/, unpack/, and cix_flash_all.bin",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    inspect_root = Path(args.inspect_root).resolve()
    if not inspect_root.exists():
        raise SystemExit(f"missing inspect data at {inspect_root}")

    with tempfile.TemporaryDirectory(prefix="cix-package-tool-compare.") as temp_dir:
        temp = Path(temp_dir)
        all_out = temp / "cix_flash_all.raw"
        ota_out = temp / "cix_flash_ota.bin"

        run_source_tool(
            "-c",
            str(inspect_root / "all-check/spi_flash_config_all.json"),
            "-o",
            str(all_out),
        )
        compare_file(
            "full raw image",
            all_out,
            inspect_root / "all-check/cix_flash_all.raw",
        )

        run_source_tool(
            "-c",
            str(inspect_root / "ota-check/spi_flash_config_ota.json"),
            "-O",
            str(ota_out),
        )
        compare_file(
            "ota image",
            ota_out,
            inspect_root / "ota-check/cix_flash_ota.bin",
        )

        full_dump_cwd = temp / "dump-full"
        full_dump_cwd.mkdir()
        run_source_tool(
            "-c",
            str(inspect_root / "all-check/spi_flash_config_all.json"),
            "-d",
            str(inspect_root / "cix_flash_all.bin"),
            cwd=full_dump_cwd,
        )
        expected_dump = inspect_root / "unpack"
        source_dump = full_dump_cwd / "unpack"
        expected_names = sorted(path.name for path in expected_dump.iterdir())
        source_names = sorted(path.name for path in source_dump.iterdir())
        if expected_names != source_names:
            raise SystemExit(
                "dump file set differs:\n"
                f"  source:   {source_names}\n"
                f"  expected: {expected_names}"
            )
        for name in expected_names:
            compare_file(f"dump/{name}", source_dump / name, expected_dump / name)

        ota_dump_cwd = temp / "dump-ota"
        ota_dump_cwd.mkdir()
        run_source_tool(
            "-c",
            str(inspect_root / "ota-check/spi_flash_config_ota.json"),
            "-d",
            str(inspect_root / "ota-check/cix_flash_ota.bin"),
            cwd=ota_dump_cwd,
        )
        ota_unpack = ota_dump_cwd / "unpack"
        expected_bootloader3 = inspect_root / "ota-check/Firmwares/bootloader3.img"
        compare_file("ota dump/bootloader3.img", ota_unpack / "bootloader3.img", expected_bootloader3)

        shutil.rmtree(source_dump, ignore_errors=True)
        shutil.rmtree(ota_unpack, ignore_errors=True)

    print("source cix_package_tool prototype matches vendor outputs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
