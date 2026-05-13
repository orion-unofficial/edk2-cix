#!/usr/bin/env python3
"""Regression tests for historical Radxa source-target coverage."""

from __future__ import annotations

import shutil
import sys
import tempfile
from pathlib import Path

from test_support import commit_all, git, require, switch_orphan, write_file


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from reconstruction_common import (  # noqa: E402
    CACHE_RELEASE_PREFIX,
    matrix_release_branches,
    release_entry,
)
from render_release_branch import render_from_plan  # noqa: E402


def create_branch(repo: Path, ref: str, files: dict[str, str], message: str) -> None:
    switch_orphan(repo, ref)
    for path, content in files.items():
        write_file(repo, path, content)
    commit_all(repo, message)


def make_repo() -> Path:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-historical-radxa-test."))
    git(repo, "init", "-b", "build")
    git(repo, "config", "user.name", "Historical Radxa Test")
    git(repo, "config", "user.email", "historical-radxa-test")
    write_file(repo, "README.md", "build branch\n")
    commit_all(repo, "build root")

    create_branch(repo, "source/base/edk2/edk2-stable202208", {"base.txt": "202208\n"}, "edk2 202208")
    create_branch(repo, "source/base/edk2/edk2-stable202602", {"base.txt": "202602\n"}, "edk2 202602")
    create_branch(
        repo,
        "source/vendor/radxa/0.2.0-1/edk2-stable202208",
        {
            "Makefile": "old build entry\n",
            "radxa.txt": "0.2.0\n",
            "src/vendor.c": "vendor source\n",
        },
        "radxa 0.2",
    )
    create_branch(repo, "source/port/radxa/1.2.1/edk2-stable202602", {"radxa.txt": "1.2.1\n"}, "radxa 1.2")
    create_branch(repo, "source/component/cix/1.2/tf-a", {"tf-a.txt": "tf-a\n"}, "cix tf-a")
    create_branch(repo, "source/component/cix/1.2/op-tee", {"op-tee.txt": "op-tee\n"}, "cix op-tee")
    create_branch(
        repo,
        "source/unofficial/edk2-stable202208",
        {
            ".github/local/Makefile.local": "buildbox-firmware-build:\n\t@true\n",
            "Makefile": "modern build entry\n",
            "custom/overlay.c": "custom source change\n",
            "scripts/build.py": "print('modern build helper')\n",
            "scripts/ensure_build_deps.sh": (
                "common_packages=(\n"
                "    python3\n"
                "    python3-cryptography\n"
                ")\n"
            ),
            "src/Makefile": (
                "modern src build entry\n"
                "PATCHED_EDK2_SOURCE_INPUTS := \\\n"
                "\t$(abspath missing-custom-source.c)\n"
                "\t\t\tupstream) \\\n"
                "\t\t\t\t;; \\\n"
            ),
            "src/scripts/helper.sh": "#!/bin/sh\n",
            "src/unofficial.c": "unofficial source change\n",
        },
        "unofficial 202208",
    )
    create_branch(
        repo,
        "source/unofficial/edk2-stable202602",
        {
            ".github/local/Makefile.local": "buildbox-firmware-build:\n\t@true\n",
            "Makefile": "modern build entry\n",
            "scripts/build.py": "print('modern build helper')\n",
            "scripts/ensure_build_deps.sh": (
                "common_packages=(\n"
                "    python3\n"
                "    python3-cryptography\n"
                ")\n"
            ),
            "src/Makefile": (
                "modern src build entry\n"
                "PATCHED_EDK2_SOURCE_INPUTS := \\\n"
                "\t$(abspath missing-custom-source.c)\n"
                "\t\t\tupstream) \\\n"
                "\t\t\t\t;; \\\n"
            ),
            "src/scripts/helper.sh": "#!/bin/sh\n",
            "src.txt": "unofficial 202602\n",
        },
        "unofficial 202602",
    )
    git(repo, "switch", "build")
    return repo


def test_custom_targets_allow_historical_radxa_releases_on_later_edk2() -> None:
    repo = make_repo()
    try:
        branches, _aliases = matrix_release_branches(repo)
        old_custom = f"{CACHE_RELEASE_PREFIX}custom/edk2-202602/cix-1.2/radxa-0.2.0-1/unofficial"
        old_upstream = f"{CACHE_RELEASE_PREFIX}upstream/edk2-202602/radxa-0.2.0-1"

        require(old_custom in branches, "historical Radxa release is blocked for later custom target")
        require(old_upstream not in branches, "upstream target should still require a matching Radxa source ref")

        branch, entry = release_entry(repo, "edk2-202602/cix-1.2/radxa-0.2.0-1/unofficial", require=True)
        require(branch == old_custom, "custom historical target resolved to the wrong branch")
        require(
            entry.get("source_ref") == "source/unofficial/edk2-stable202602",
            "custom historical target should use the selected unofficial EDK2 tree",
        )
    finally:
        shutil.rmtree(repo)


def test_historical_upstream_target_overlays_build_infrastructure_only() -> None:
    repo = make_repo()
    try:
        branch, entry = release_entry(repo, "edk2-202208/radxa-0.2.0-1", require=True)
        commit = render_from_plan(repo, branch, entry, verbose=False, allow_manifest_refresh=True)

        require(
            git(repo, "show", f"{commit}:Makefile").stdout == "modern build entry\n",
            "historical upstream target did not receive the modern build Makefile",
        )
        require(
            "buildbox-firmware-build" in git(repo, "show", f"{commit}:.github/local/Makefile.local").stdout,
            "historical upstream target did not receive buildbox make rules",
        )
        require(
            git(repo, "show", f"{commit}:src/vendor.c").stdout == "vendor source\n",
            "historical upstream target should retain vendor firmware source",
        )
        require(
            "modern src build entry\n" in git(repo, "show", f"{commit}:src/Makefile").stdout,
            "historical upstream target did not receive modern src build rules",
        )
        require(
            "$(wildcard $(abspath missing-custom-source.c))" in git(repo, "show", f"{commit}:src/Makefile").stdout,
            "historical upstream target should relax custom-only Makefile prerequisites",
        )
        require(
            "-vw 6084 -vw 6161 -vw 6033 -vw 6049 -vw 6050"
            in git(repo, "show", f"{commit}:src/Makefile").stdout,
            "historical upstream target should add ASL compatibility flags",
        )
        require(
            "python-is-python3" in git(repo, "show", f"{commit}:scripts/ensure_build_deps.sh").stdout,
            "historical upstream target should install python compatibility package",
        )
        require(
            git(repo, "show", f"{commit}:src/scripts/helper.sh").stdout == "#!/bin/sh\n",
            "historical upstream target did not receive modern src helper scripts",
        )
        require(
            git(repo, "cat-file", "-e", f"{commit}:src/unofficial.c", check=False).returncode != 0,
            "historical upstream target must not take unofficial firmware source",
        )
        require(
            git(repo, "cat-file", "-e", f"{commit}:custom/overlay.c", check=False).returncode != 0,
            "historical upstream target must not take custom overlay source",
        )
    finally:
        shutil.rmtree(repo)


def test_integrate_source_release_make_target_preserves_materialise_default() -> None:
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    require(
        'MATERIALISE="$(or $(MATERIALISE),1)"' in makefile,
        "make integrate-source-release must not pass an empty MATERIALISE value over the script default",
    )


def main() -> None:
    test_custom_targets_allow_historical_radxa_releases_on_later_edk2()
    test_historical_upstream_target_overlays_build_infrastructure_only()
    test_integrate_source_release_make_target_preserves_materialise_default()
    print("historical Radxa target tests passed")


if __name__ == "__main__":
    main()
