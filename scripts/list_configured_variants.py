#!/usr/bin/env python3
"""Print configured firmware variants in the same layout as Makefile help."""

from __future__ import annotations

import textwrap
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path

from reconstruction_common import default_release, release_branch_sort_key, release_entries, repo_root, variant_name


WIDTH = 80
STAGE_ORDER = {"upstream": 0, "custom-radxa": 1, "vendor": 2, "custom-cix": 3, "other": 9}


def stage(branch: str) -> str:
    name = variant_name(branch)
    if "/unofficial" in name and "/cix-" in name:
        return "custom-cix"
    if "/unofficial" in name:
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
    def key(branch: str) -> tuple[object, ...]:
        name = variant_name(branch)
        return (release_branch_sort_key(branch), STAGE_ORDER.get(stage(branch), 9), name)

    return sorted(branches, key=key)


def canonical_branches(releases: dict[str, object]) -> tuple[list[str], list[str]]:
    canonical: list[str] = []
    alias_versions: set[str] = set()
    for branch, entry in releases.items():
        entry = entry if isinstance(entry, dict) else {}
        name = variant_name(branch)
        if "/unofficial-" in name:
            alias_versions.add(name.rsplit("/unofficial-", 1)[1])
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


def print_help(repo: Path) -> None:
    releases = release_entries(repo)
    default = variant_name(default_release(repo))
    branches, alias_versions = canonical_branches(releases)

    print("Configured Firmware Variants")
    print()
    paragraph(
        "A firmware variant is the chosen combination of EDK2, Radxa, CIX, "
        "and unofficial project sources."
    )
    paragraph(
        "All listed variants are rendered as ordinary files before building, "
        "without the use of git submodules."
    )
    paragraph(
        "The names below are the recommended RELEASE= values. A full branch "
        "name such as source/cache/release/custom/... is also accepted when copying "
        "an existing branch name from git branch output."
    )
    print()
    print(f"Default variant: {default}")
    print()
    print("Name components:")
    print("  edk2-YYYYMM[.NN]    selects the upstream EDK2 release")
    print("  radxa-X.Y.Z[-R]     adds the Radxa EDK2 vendor layer")
    print("  cix-X.Y             adds CIX TF-A and OP-TEE component sources")
    print("  unofficial          adds this project's unofficial firmware changes")
    print()
    print("Available variants:")
    print_variant_list(branches)
    if alias_versions:
        print()
        print("Versioned unofficial aliases:")
        indented(
            "Any listed /unofficial variant also accepts a versioned alias of the "
            f"form /unofficial-<version>; currently configured version(s): "
            f"{', '.join(alias_versions)}."
        )


def render_help(repo: Path) -> str:
    buffer = StringIO()
    with redirect_stdout(buffer):
        print_help(repo)
    return buffer.getvalue()


def main() -> None:
    repo = repo_root(Path(__file__))
    print(render_help(repo), end="")


if __name__ == "__main__":
    main()
