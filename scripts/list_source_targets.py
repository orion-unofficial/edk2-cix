#!/usr/bin/env python3
"""Print configured firmware source targets in the same layout as Makefile help."""

from __future__ import annotations

import argparse
import textwrap
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path


WIDTH = 80
STAGE_ORDER = {"upstream": 0, "custom-radxa": 1, "vendor": 2, "custom-cix": 3, "other": 9}
default_release = None
matrix_release_branches = None
release_branch_sort_key = None
repo_root = None
source_target_name = None


def load_reconstruction_common() -> None:
    global default_release
    global matrix_release_branches
    global release_branch_sort_key
    global repo_root
    global source_target_name

    if default_release is not None:
        return

    from reconstruction_common import (  # pylint: disable=import-outside-toplevel
        default_release as loaded_default_release,
    )
    from reconstruction_common import (  # pylint: disable=import-outside-toplevel
        matrix_release_branches as loaded_matrix_release_branches,
    )
    from reconstruction_common import (  # pylint: disable=import-outside-toplevel
        release_branch_sort_key as loaded_release_branch_sort_key,
    )
    from reconstruction_common import repo_root as loaded_repo_root  # pylint: disable=import-outside-toplevel
    from reconstruction_common import source_target_name as loaded_source_target_name  # pylint: disable=import-outside-toplevel

    default_release = loaded_default_release
    matrix_release_branches = loaded_matrix_release_branches
    release_branch_sort_key = loaded_release_branch_sort_key
    repo_root = loaded_repo_root
    source_target_name = loaded_source_target_name


def stage(branch: str) -> str:
    name = source_target_name(branch)
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


def sorted_source_targets(branches: list[str]) -> list[str]:
    def key(branch: str) -> tuple[object, ...]:
        name = source_target_name(branch)
        return (release_branch_sort_key(branch), STAGE_ORDER.get(stage(branch), 9), name)

    return sorted(branches, key=key)


def canonical_branches(branches: list[str] | set[str]) -> tuple[list[str], list[str]]:
    canonical: list[str] = []
    alias_versions: set[str] = set()
    for branch in branches:
        name = source_target_name(branch)
        if "/unofficial-" in name:
            alias_versions.add(name.rsplit("/unofficial-", 1)[1])
            continue
        canonical.append(branch)
    return sorted_source_targets(canonical), sorted(alias_versions)


def print_source_target_list(branches: list[str]) -> None:
    current_edk2 = ""
    first = True
    for branch in branches:
        name = source_target_name(branch)
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
    load_reconstruction_common()
    release_branches, _aliases = matrix_release_branches(repo)
    default = source_target_name(default_release(repo))
    branches, alias_versions = canonical_branches(release_branches)

    print("Configured Firmware Source Targets")
    print()
    paragraph(
        "A source target is the chosen combination of EDK2, Radxa, CIX, "
        "and unofficial project sources used to construct the firmware tree."
    )
    print()
    paragraph(
        "All listed source targets are rendered as ordinary files before building, "
        "without the use of git submodules."
    )
    print()
    paragraph(
        "The names below are the recommended RELEASE= values. A full branch "
        "name such as source/cache/release/custom/... is also accepted when copying "
        "an existing branch name from git branch output."
    )
    print()
    paragraph(
        "The default source target follows config/policies.json unofficial_source_policy "
        "so new vendor releases do not silently become the recommended target before "
        "the selected source/unofficial/<line>/current ref has intentionally migrated."
    )
    print()
    print("Name components:")
    print("  edk2-YYYYMM[.NN]    selects the upstream EDK2 release")
    print("  radxa-X.Y.Z[-R]     adds the Radxa EDK2 vendor layer")
    print("  cix-X.Y             adds CIX TF-A and OP-TEE component sources")
    print("  unofficial[-X.Y.Z]  adds this project's unofficial firmware changes")
    if alias_versions:
        indented(
            "Optional version suffixes select the matching unofficial source-target alias; "
            f"currently configured suffix(es): {', '.join(alias_versions)}.",
            indent="                      ",
        )
    print()
    print("Available source targets:")
    print_source_target_list(branches)
    print()
    print("Default source target:")
    print(f"  {default}")


def render_help(repo: Path) -> str:
    buffer = StringIO()
    with redirect_stdout(buffer):
        print_help(repo)
    return buffer.getvalue()


def main() -> None:
    argparse.ArgumentParser(description=__doc__).parse_args()
    load_reconstruction_common()
    repo = repo_root(Path(__file__))
    print(render_help(repo), end="")


if __name__ == "__main__":
    main()
