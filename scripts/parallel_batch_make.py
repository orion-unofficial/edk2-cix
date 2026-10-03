#!/usr/bin/env python3
"""Apply recorded batch compiler concurrency after the rendered Makefile loads.

Only host-side recursive Make calls use this wrapper. The buildbox receives
ordinary compiler job variables through SRC_COMMON_ARGS; packaging concurrency,
host detection, firmware definitions and upstream builds retain their defaults.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path


def profile(jobs: int) -> str:
    if not isinstance(jobs, int) or isinstance(jobs, bool) or not 1 <= jobs <= 4096:
        raise ValueError("compiler jobs must be an integer from 1 to 4096")
    return ("# Generated batch compiler concurrency; packaging defaults unchanged.\n"
            "ifeq ($(ARTEFACT_MODE),custom)\n"
            f"SRC_COMMON_ARGS += EDK2_BUILD_JOBS={jobs} BASETOOLS_BUILD_JOBS={jobs}\n"
            "endif\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", required=True, type=int)
    parser.add_argument("--profile", required=True, type=Path)
    parser.add_argument("make_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.profile.read_text() != profile(args.jobs):
        parser.error("batch concurrency profile differs from recorded compiler jobs")
    make_args = args.make_args
    if make_args[:1] == ["--"]:
        make_args = make_args[1:]
    # GNU Make applies -C before loading either file. Loading this small profile
    # last appends to, rather than replaces, the original firmware arguments.
    os.execv("/usr/bin/make", ["make", "-f", "Makefile", "-f", str(args.profile.resolve()), *make_args])


if __name__ == "__main__":
    main()
