#!/usr/bin/env python3
"""Print configured firmware variants in the same layout as Makefile help."""

from __future__ import annotations

import json
import textwrap
from pathlib import Path

from reconstruction_common import variant_name


WIDTH = 80
STAGE_ORDER = {"upstream": 0, "custom-radxa": 1, "vendor": 2, "custom-cix": 3, "other": 9}


def stage(branch: str) -> str:
    name = variant_name(branch)
    if "/local" in name and "/cix-" in name:
        return "custom-cix"
    if "/local" in name:
        return "custom-radxa"
    if "/cix-" in name:
        return "vendor"
    if "/radxa-" in name:
        return "upstream"
    return "other"


def edk2_key(name: str) -> str:
    first = name.split("/", 1)[0]
    return first.removeprefix("edk2-")


def sorted_variants(branches: list[str]) -> list[str]:
    def key(branch: str) -> tuple[str, int, str]:
        name = variant_name(branch)
        return (edk2_key(name), STAGE_ORDER.get(stage(branch), 9), name)

    return sorted(branches, key=key)


def canonical_branches(releases: dict[str, object]) -> tuple[list[str], list[str]]:
    canonical: list[str] = []
    alias_versions: set[str] = set()
    for branch, entry in releases.items():
        entry = entry if isinstance(entry, dict) else {}
        name = variant_name(branch)
        if "/local-" in name:
            alias_versions.add(name.rsplit("/local-", 1)[1])
            continue
        if entry.get("alias_of") or entry.get("alias_of_template"):
            continue
        canonical.append(branch)
    return sorted_variants(canonical), sorted(alias_versions)


def print_variant_list(branches: list[str]) -> None:
    current_edk2 = ""
    first = True
    for branch in branches:
        name = variant_name(branch)
        edk2 = edk2_key(name)
        if edk2 != current_edk2:
            current_edk2 = edk2
            if not first:
                print()
            first = False
        print(f"  {name}")


def paragraph(text: str) -> None:
    print(textwrap.fill(text, width=WIDTH))


def indented(text: str, indent: str = "  ") -> None:
    print(textwrap.fill(text, width=WIDTH, initial_indent=indent, subsequent_indent=indent))


def main() -> None:
    data = json.loads(Path("config/releases.json").read_text(encoding="utf-8"))
    releases = data.get("releases", {})
    default = variant_name(data.get("default_release", ""))
    branches, alias_versions = canonical_branches(releases if isinstance(releases, dict) else {})

    print("Configured Firmware Variants")
    print()
    paragraph(
        "A firmware variant is the chosen combination of EDK2, Radxa, CIX, "
        "and local project sources."
    )
    paragraph(
        "All listed variants are rendered as ordinary files before building, "
        "without the use of git submodules."
    )
    paragraph(
        "The names below are the recommended RELEASE= values. A full branch "
        "name such as source/release/custom/... is also accepted when copying "
        "an existing branch name from git branch output."
    )
    print()
    print(f"Default variant: {default}")
    print()
    print("Name components:")
    print("  edk2-YYYYMM[.NN]    selects the upstream EDK2 release")
    print("  radxa-X.Y.Z[-R]     adds the Radxa EDK2 vendor layer")
    print("  cix-X.Y             adds CIX TF-A and OP-TEE component sources")
    print("  local               adds this project's local firmware changes")
    print()
    print("Available variants:")
    print_variant_list(branches)
    if alias_versions:
        print()
        print("Versioned local aliases:")
        indented(
            "Any listed /local variant also accepts a versioned alias of the "
            f"form /local-<version>; currently configured version(s): "
            f"{', '.join(alias_versions)}."
        )


if __name__ == "__main__":
    main()
