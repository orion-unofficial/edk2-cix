#!/usr/bin/env python3
"""Select actual firmware builds from the public source-target model."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from reconstruction_common import (
    ReconstructionError, main_wrapper, matrix_release_branches, source_target_name,
    release_entries, unofficial_line_policies,
)
from validate_release_inputs import validate_inputs


ROOT = Path(__file__).resolve().parents[1]


def qualification_releases(policy: dict) -> tuple[list[str], list[str]]:
    """Keep the vendor baseline and 1.2.4 while following the maintained stack.

    Discovery of a new upstream tag does not promote it. Updating the selected
    Unofficial line deliberately advances both CI and the advertised scope.
    """
    qualification = policy["firmware_qualification_policy"]
    line, lines = unofficial_line_policies(policy["unofficial_source_policy"])
    current = lines[line]
    edk2 = [qualification["baseline_edk2_release"], current["current_edk2_release"]]
    radxa = [qualification["retained_custom_radxa_release"], current["current_radxa_release"]]
    return list(dict.fromkeys(edk2)), list(dict.fromkeys(radxa))


def stock_matrix(targets: list[str], policy: dict) -> dict:
    """Enumerate exact vendor replays; each reusable workflow covers both boards."""
    qualification = policy["firmware_qualification_policy"]
    baseline = qualification["baseline_edk2_release"]
    versions = qualification["stock_radxa_releases"]
    _, custom_radxa = qualification_releases(policy)
    if not versions or len(set(versions)) != len(versions):
        raise ValueError("stock replay releases must be nonempty and unique")
    if not set(custom_radxa).issubset(versions):
        raise ValueError("maintained custom Radxa releases must also have stock replay coverage")
    requested = {f"edk2-{baseline}/radxa-{version}" for version in versions}
    missing = requested - set(targets)
    if missing:
        raise ValueError("missing maintained stock source targets: " + ", ".join(sorted(missing)))
    return {"include": [{"release": f"edk2-{baseline}/radxa-{version}", "version": version}
                        for version in versions]}


def validate_stock_entry(entry: dict, baseline: str, version: str) -> None:
    """A stock alias must select its exact vendor release, never a port or custom tree."""
    expected = f"source/vendor/radxa/{version}/edk2-stable{baseline}"
    if (entry["source_ref"] != expected or entry["render"]["base"]["ref"] != expected
            or entry["unofficial_delta"] or entry["radxa_release"] != version
            or entry["edk2_release"] != f"edk2-stable{baseline}"):
        raise ReconstructionError(f"stock replay must use the exact vendor source: {expected}")


def firmware_matrix(targets: list[str], radxa_releases: list[str],
                    edk2_releases: list[str], boards: list[str]) -> dict:
    """Cover every requested primary tuple; missing inputs must fail visibly."""
    requested = {f"edk2-{edk2}/radxa-{radxa}/unofficial"
                 for edk2 in edk2_releases for radxa in radxa_releases}
    missing = requested - set(targets)
    if missing:
        raise ValueError("missing primary source targets: " + ", ".join(sorted(missing)))
    include = [
        {
            "release": release,
            "board": board,
            "firmware_fixes": fixes,
            "experimental": experimental,
            "key": (f"{release.replace('/', '_')}-{board}-fixes-{str(fixes).lower()}"
                    f"-settings-{str(experimental).lower()}"),
        }
        for release in sorted(requested)
        for board in boards
        for fixes in (False, True)
        for experimental in (False, True)
    ]
    if len(include) > 256:
        raise ValueError("firmware matrix exceeds 256 jobs; shard it without dropping source targets")
    return {"include": include}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stock", action="store_true", help="Enumerate maintained vendor replays")
    args = parser.parse_args()
    policy = json.loads((ROOT / "config/policies.json").read_text())
    qualification = policy["firmware_qualification_policy"]
    branches, _ = matrix_release_branches(ROOT)
    targets = [source_target_name(branch) for branch in branches]
    edk2, radxa = qualification_releases(policy)
    matrix = (stock_matrix(targets, policy) if args.stock else
              firmware_matrix(targets, radxa, edk2, qualification["boards"]))
    entries = {source_target_name(branch): entry for branch, entry in release_entries(ROOT).items()}
    if args.stock:
        for row in matrix["include"]:
            validate_stock_entry(entries[row["release"]], qualification["baseline_edk2_release"], row["version"])
    else:
        for release in sorted({row["release"] for row in matrix["include"]}):
            validate_inputs(ROOT, entries[release])
    print(json.dumps(matrix))


if __name__ == "__main__":
    main_wrapper(main)
