#!/usr/bin/env python3
"""Select an already-recorded unofficial development line as the default."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
from typing import Any

from reconstruction_common import (
    ReconstructionError,
    load_json,
    main_wrapper,
    normalise_unofficial_immutability_policy,
    ref_exists,
    repo_root,
    truthy,
    unofficial_line_policies,
    unofficial_source_policy,
    write_json,
)


HELP = """select-unofficial-line

Selects an existing source/unofficial/<LINE>/current branch as the default
source line after that line has passed its release validation.

Required variables:
  LINE=<major.minor>  Existing unofficial development line.

Optional variables:
  WRITE=0|1           Required before config/policies.json is changed.
  V=0|1               Reserved for consistent Makefile verbosity handling.

Example:
  make select-unofficial-line LINE=1.3
  make select-unofficial-line LINE=1.3 WRITE=1
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=HELP,
    )
    p.add_argument("--line", default=os.environ.get("LINE", ""))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def validated_line(
    repo: Path,
    policy: dict[str, Any],
    line: str,
) -> tuple[str, dict[str, Any]]:
    default_line, lines = unofficial_line_policies(policy)
    if line not in lines:
        available = ", ".join(sorted(lines)) or "(none)"
        raise ReconstructionError(
            f"unofficial line {line!r} is not configured; available lines: {available}"
        )

    selected = lines[line]
    required = (
        "current_cix_release",
        "current_edk2_release",
        "current_radxa_release",
        "current_ref",
    )
    missing = [field for field in required if not str(selected.get(field, "")).strip()]
    if missing:
        raise ReconstructionError(
            f"unofficial line {line!r} is missing required field(s): {', '.join(missing)}"
        )

    current_ref = str(selected["current_ref"]).strip()
    expected_ref = f"source/unofficial/{line}/current"
    if current_ref != expected_ref:
        raise ReconstructionError(
            f"unofficial line {line!r} records {current_ref}, expected {expected_ref}"
        )
    if not ref_exists(repo, current_ref):
        raise ReconstructionError(
            f"unofficial line {line!r} current ref is unavailable locally: {current_ref}"
        )
    return default_line, selected


def select_line(repo: Path, line: str, *, write: bool) -> bool:
    policy = unofficial_source_policy(repo)
    default_line, selected = validated_line(repo, policy, line)
    current_ref = str(selected["current_ref"]).strip()
    release = str(selected["current_radxa_release"]).strip()
    edk2 = str(selected["current_edk2_release"]).strip()

    print(f"unofficial line: {line}")
    print(f"current ref:     {current_ref}")
    print(f"Radxa release:   {release}")
    print(f"EDK2 release:    edk2-stable{edk2}")
    if default_line == line:
        print(f"default line already matches {line}")
        return False
    if not write:
        print(f"dry run; set WRITE=1 to change default line: {default_line} -> {line}")
        return False

    path = repo / "config" / "policies.json"
    data = load_json(repo, "config/policies.json")
    source_policy = data.get("unofficial_source_policy")
    if not isinstance(source_policy, dict):
        raise ReconstructionError(
            "config/policies.json unofficial_source_policy must be an object"
        )
    source_policy["default_line"] = line
    normalise_unofficial_immutability_policy(data)
    write_json(path, data)
    print(f"selected unofficial default line: {default_line} -> {line}")
    return True


def main() -> None:
    args = parser().parse_args()
    line = args.line.strip()
    if not line:
        print(HELP)
        raise SystemExit("missing required variable: LINE=<major.minor>")
    select_line(repo_root(Path(__file__)), line, write=truthy(args.write))


if __name__ == "__main__":
    main_wrapper(main)
