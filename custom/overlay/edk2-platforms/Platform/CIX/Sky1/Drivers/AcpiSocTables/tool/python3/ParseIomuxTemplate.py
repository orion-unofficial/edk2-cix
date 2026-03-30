## @file
#
#  Copyright 2026 Cix Technology Group Co., Ltd. All Rights Reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

import argparse
import logging
import os
import subprocess
import sys
from pathlib import Path


def setup_logging():
    quiet = os.getenv("EDK2_CIX_QUIET_PREBUILD") == "1"
    logging.basicConfig(
        level=logging.ERROR if quiet else logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        handlers=[logging.StreamHandler(sys.stdout)],
    )
    return logging.getLogger()


def run_m4_process(m4_file: Path, output_file: Path, includes: list[str]) -> bool:
    logger = logging.getLogger()

    include_flags: list[str] = []
    seen: set[str] = set()
    for include_dir in includes:
        if include_dir in seen:
            continue
        seen.add(include_dir)
        include_flags.append(f"-I{include_dir}")

    cmd = ["m4", *include_flags, str(m4_file)]

    logger.info(f"Executing: {' '.join(cmd)} > {output_file}")
    output_file.parent.mkdir(parents=True, exist_ok=True)

    try:
        with open(output_file, "w", encoding="utf-8") as output_handle:
            subprocess.run(
                cmd,
                stdout=output_handle,
                stderr=subprocess.PIPE,
                text=True,
                check=True,
            )
        return True
    except subprocess.CalledProcessError as error:
        logging.error(f"m4 failed with code {error.returncode}:\n{error.stderr}")
        return False


def main():
    workspace = os.getenv("WORKSPACE", "")
    if not workspace:
        print("ERROR: WORKSPACE environment variable not set!", file=sys.stderr)
        return 1

    parser = argparse.ArgumentParser()
    parser.add_argument("input_file", help="input file")
    parser.add_argument("output_file", help="output file")
    parser.add_argument("extra_args", nargs=argparse.REMAINDER, help="extra args")

    args = parser.parse_args()

    base_dir = Path(workspace)
    inc_dir = Path(__file__).resolve().parent
    m4_file = base_dir / args.input_file
    output_file = base_dir / args.output_file

    logger = setup_logging()
    logger.info("=== Starting ACPI PreBuild ===")

    if not m4_file.exists():
        logger.error(f"Input file not found: {m4_file}")
        return 1

    success = run_m4_process(
        m4_file=m4_file,
        output_file=output_file,
        includes=[str(m4_file.parent), str(inc_dir)],
    )

    if not success:
        logger.error("ACPI preprocessing failed!")
        return 1

    logger.info(f"Successfully generated: {output_file}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
