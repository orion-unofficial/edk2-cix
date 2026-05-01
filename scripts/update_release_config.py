#!/usr/bin/env python3
"""Report EDK2 source-ref discovery for a new supported release."""

from __future__ import annotations

import argparse
import os
import re
from pathlib import Path

from reconstruction_common import ReconstructionError, main_wrapper, matrix_release_values, ref_exists, repo_root


HELP = """update-release-config

Required variables:
  EDK2_RELEASE  EDK2 release to inspect, e.g. 202605 or edk2-stable202605.

This target is retained as a compatibility helper. Supported EDK2 releases are
now discovered from local source refs rather than a committed release-list file.
To add a release, integrate these refs:

  source/base/edk2/edk2/<edk2-release>
  source/base/edk2/edk2-platforms/<edk2-release>
  source/base/edk2/edk2-non-osi/<edk2-release>

Then add or regenerate the matching rendered base, Radxa delta, optional CIX
variant, and local delta refs before running make verify-build-matrix.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--edk2-release", default=os.environ.get("EDK2_RELEASE", ""))
    return p


def normalise_edk2_release(value: str) -> tuple[str, str]:
    if not value:
        raise ReconstructionError("EDK2_RELEASE is required")
    release = value.removeprefix("edk2-stable")
    if not re.fullmatch(r"\d{6}(?:\.\d+)?", release):
        raise ReconstructionError("EDK2_RELEASE must look like 202602 or edk2-stable202602")
    return release, f"edk2-stable{release}"


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    release, edk2_ref = normalise_edk2_release(args.edk2_release)
    required_refs = [
        f"source/base/edk2/edk2/{edk2_ref}",
        f"source/base/edk2/edk2-platforms/{edk2_ref}",
        f"source/base/edk2/edk2-non-osi/{edk2_ref}",
    ]
    discovered = release in matrix_release_values(repo)

    print(f"EDK2 release: {edk2_ref}")
    print(f"discoverable from source/base refs: {'yes' if discovered else 'no'}")
    print("required base refs:")
    for ref in required_refs:
        print(f"  {'ok     ' if ref_exists(repo, ref) else 'missing'} {ref}")
    print("no config file is updated; supported EDK2 releases are derived from source/base/edk2/edk2 refs")


if __name__ == "__main__":
    main_wrapper(main)
