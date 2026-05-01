#!/usr/bin/env python3
"""Print configured firmware variants in the same layout as Makefile help."""

from __future__ import annotations

import json
from pathlib import Path

from reconstruction_common import variant_name


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
    for branch in branches:
        name = variant_name(branch)
        edk2 = edk2_key(name)
        if edk2 != current_edk2:
            current_edk2 = edk2
            print(f"\nEDK2 {edk2}")
        print(f"  {name}")


def main() -> None:
    data = json.loads(Path("config/releases.json").read_text(encoding="utf-8"))
    releases = data.get("releases", {})
    default = variant_name(data.get("default_release", ""))
    branches, alias_versions = canonical_branches(releases if isinstance(releases, dict) else {})

    print("Configured Firmware Variants")
    print()
    print("A firmware variant is the chosen combination of EDK2, Radxa, CIX, and local project sources.")
    print("All listed variants are rendered as ordinary files before building; active submodule gitlinks are not retained.")
    print("The names below omit the internal source/release/{upstream,vendor,custom}/ branch prefix.")
    print("Full source/release/... branch names are still accepted when needed.")
    print()
    print(f"Default variant: {default}")
    print()
    print("Name components:")
    print("  edk2-YYYYMM[.NN]  selects the upstream EDK2 release family")
    print("  radxa-X.Y.Z       adds the Radxa EDK2 vendor layer for that EDK2 base")
    print("  cix-X.Y           adds CIX TF-A and OP-TEE component sources")
    print("  local             adds this project's local firmware changes")
    print()
    print("Available variants:")
    print_variant_list(branches)
    if alias_versions:
        print()
        print("Versioned local aliases:")
        print(
            "  Any listed /local variant also accepts a versioned alias of the form "
            f"/local-<version>; currently configured version(s): {', '.join(alias_versions)}."
        )


if __name__ == "__main__":
    main()
