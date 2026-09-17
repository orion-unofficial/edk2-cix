#!/usr/bin/env python3
"""Select actual firmware builds from the public source-target model."""

from __future__ import annotations

import json
import re
from pathlib import Path

from reconstruction_common import matrix_release_branches, source_target_name


ROOT = Path(__file__).resolve().parents[1]
CUSTOM_TARGET = re.compile(r"edk2-(?P<edk2>[^/]+)/radxa-(?P<radxa>[^/]+)/unofficial$")


def firmware_matrix(targets: list[str], radxa_releases: list[str]) -> dict:
    """Keep every valid completion; do not replace historical targets with latest."""
    selected = sorted({
        target for target in targets
        if (match := CUSTOM_TARGET.fullmatch(target))
        and match["radxa"] in radxa_releases
    })
    for release in radxa_releases:
        if not any(CUSTOM_TARGET.fullmatch(target)["radxa"] == release for target in selected):
            raise ValueError(f"no supported custom source target for required Radxa {release}")
    include = [
        {
            "release": release,
            "board": board,
            "firmware_fixes": fixes,
            "key": f"{release.replace('/', '_')}-{board}-fixes-{str(fixes).lower()}",
        }
        for release in selected
        for board in ("O6", "O6N")
        for fixes in (False, True)
    ]
    if len(include) > 256:
        raise ValueError("firmware matrix exceeds 256 jobs; shard it without dropping source targets")
    return {"include": include}


def main() -> None:
    policy = json.loads((ROOT / "config/policies.json").read_text())
    releases = policy["firmware_qualification_policy"]["radxa_releases"]
    branches, _ = matrix_release_branches(ROOT)
    print(json.dumps(firmware_matrix([source_target_name(branch) for branch in branches], releases)))


if __name__ == "__main__":
    main()
