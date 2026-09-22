#!/usr/bin/env python3
"""Select actual firmware builds from the public source-target model."""

from __future__ import annotations

import json
import re
from pathlib import Path

from reconstruction_common import matrix_release_branches, source_target_name, release_entries
from validate_release_inputs import validate_inputs


ROOT = Path(__file__).resolve().parents[1]
CUSTOM_TARGET = re.compile(r"edk2-(?P<edk2>[^/]+)/radxa-(?P<radxa>[^/]+)/unofficial$")


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
    policy = json.loads((ROOT / "config/policies.json").read_text())
    qualification = policy["firmware_qualification_policy"]
    branches, _ = matrix_release_branches(ROOT)
    matrix = firmware_matrix(
        [source_target_name(branch) for branch in branches],
        qualification["radxa_releases"], qualification["edk2_releases"], qualification["boards"],
    )
    entries = {source_target_name(branch): entry for branch, entry in release_entries(ROOT).items()}
    for release in sorted({row["release"] for row in matrix["include"]}):
        validate_inputs(ROOT, entries[release])
    print(json.dumps(matrix))


if __name__ == "__main__":
    main()
