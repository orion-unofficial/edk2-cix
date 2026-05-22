#!/usr/bin/env python3
"""Regression tests for historical Radxa source-target coverage."""

from __future__ import annotations

import json
import os
import shutil
import sys
import tempfile
from pathlib import Path

from test_support import commit_all, git, load_function_tests, require, switch_orphan, write_file


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from reconstruction_common import (  # noqa: E402
    CACHE_RELEASE_PREFIX,
    default_release,
    matrix_release_branches,
    release_entry,
)
from source_porting import apply_source_delta_to_base  # noqa: E402
from render_release_branch import render_from_plan  # noqa: E402
from verify_build_matrix import require_unofficial_source_policy  # noqa: E402


def create_branch(repo: Path, ref: str, files: dict[str, str], message: str) -> None:
    switch_orphan(repo, ref)
    for path, content in files.items():
        write_file(repo, path, content)
    commit_all(repo, message)


def write_unofficial_source_policy(repo: Path, *, edk2: str = "202602", radxa: str = "1.2.1") -> None:
    write_file(
        repo,
        "config/policies.json",
        json.dumps(
            {
                "unofficial_source_policy": {
                    "current_ref": "source/unofficial/current",
                    "current_edk2_release": edk2,
                    "current_cix_release": "1.2",
                    "current_radxa_release": radxa,
                    "prefer_versioned_default_alias": True,
                }
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
    )


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
    create_branch(
        repo,
        "source/unofficial/current",
        {
            ".github/local/Makefile.local": "buildbox-firmware-build:\n\t@true\n",
            "Makefile": "modern build entry\n",
            "src.txt": "current unofficial source\n",
        },
        "current unofficial",
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


def test_default_source_target_follows_unofficial_source_policy() -> None:
    repo = make_repo()
    try:
        create_branch(repo, "source/port/radxa/1.2.2/edk2-stable202602", {"radxa.txt": "1.2.2\n"}, "radxa 1.2.2")
        git(repo, "switch", "build")
        write_unofficial_source_policy(repo, radxa="1.2.1")

        branches, _aliases = matrix_release_branches(repo)
        new_vendor_target = f"{CACHE_RELEASE_PREFIX}custom/edk2-202602/cix-1.2/radxa-1.2.2/unofficial-1.2.2"
        require(new_vendor_target in branches, "newer Radxa release should still be available as an explicit target")
        require(
            default_release(repo) == "edk2-202602/cix-1.2/radxa-1.2.1/unofficial-1.2.1",
            "default source target should not silently advance to a newer Radxa release",
        )
    finally:
        shutil.rmtree(repo)


def test_unofficial_source_policy_tracks_latest_unofficial_release_branch() -> None:
    repo = make_repo()
    try:
        create_branch(repo, "source/base/edk2/edk2-stable202605", {"base.txt": "202605\n"}, "edk2 202605")
        create_branch(repo, "source/port/radxa/1.2.1/edk2-stable202605", {"radxa.txt": "1.2.1\n"}, "radxa 1.2 202605")
        create_branch(repo, "source/unofficial/edk2-stable202605", {"src.txt": "unofficial 202605\n"}, "unofficial 202605")
        git(repo, "switch", "build")

        write_unofficial_source_policy(repo, edk2="202602")
        branches, _aliases = matrix_release_branches(repo)
        stale = require_unofficial_source_policy(repo, branches)
        require(
            any("latest source/unofficial release branch is 202605" in problem for problem in stale),
            "stale unofficial source policy was not reported",
        )

        write_unofficial_source_policy(repo, edk2="202605")
        fresh = require_unofficial_source_policy(repo, branches)
        require(not fresh, "fresh unofficial source policy was reported stale: " + "\n".join(fresh))
    finally:
        shutil.rmtree(repo)


def test_source_delta_porting_replays_only_project_delta() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-port-test."))
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Source Port Test")
        git(repo, "config", "user.email", "source-port-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")
        create_branch(repo, "old-base", {"src/base.txt": "old\n", "stable.txt": "same\n"}, "old base")
        create_branch(repo, "new-base", {"src/base.txt": "new\n", "stable.txt": "same\n"}, "new base")
        create_branch(
            repo,
            "source-tree",
            {
                "src/base.txt": "old\n",
                "stable.txt": "same\n",
                "custom/project.txt": "project delta\n",
            },
            "source tree",
        )
        git(repo, "switch", "build")

        commit = apply_source_delta_to_base(
            repo,
            old_base_ref="old-base",
            source_ref="source-tree",
            new_base_ref="new-base",
            message="port source tree",
            label="source-port-test",
            verbose=False,
        )
        require(git(repo, "show", f"{commit}:src/base.txt").stdout == "new\n", "new base content was overwritten")
        require(
            git(repo, "show", f"{commit}:custom/project.txt").stdout == "project delta\n",
            "project delta was not replayed",
        )
    finally:
        shutil.rmtree(repo)


def test_source_delta_porting_ignores_deletes_already_absent_upstream() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-port-delete-test."))
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Source Port Test")
        git(repo, "config", "user.email", "source-port-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")
        create_branch(
            repo,
            "old-base",
            {
                "src/obsolete-upstream.c": "old upstream\n",
                "src/shared.c": "old shared\n",
            },
            "old base",
        )
        create_branch(repo, "new-base", {"src/shared.c": "new shared\n"}, "new base")
        create_branch(
            repo,
            "source-tree",
            {
                "src/shared.c": "old shared\n",
                "custom/project.txt": "project delta\n",
            },
            "source tree",
        )
        git(repo, "switch", "build")

        commit = apply_source_delta_to_base(
            repo,
            old_base_ref="old-base",
            source_ref="source-tree",
            new_base_ref="new-base",
            message="port source tree",
            label="source-port-delete-test",
            verbose=False,
        )
        require(
            git(repo, "cat-file", "-e", f"{commit}:src/obsolete-upstream.c", check=False).returncode != 0,
            "obsolete upstream file was reintroduced",
        )
        require(git(repo, "show", f"{commit}:src/shared.c").stdout == "new shared\n", "new base update was lost")
        require(
            git(repo, "show", f"{commit}:custom/project.txt").stdout == "project delta\n",
            "project delta was not replayed",
        )
    finally:
        shutil.rmtree(repo)


def test_source_delta_porting_preserves_conflict_worktree_for_manual_resume() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-port-conflict-test."))
    old_tmp_root = os.environ.get("EDK2_CIX_TMP_ROOT")
    os.environ["EDK2_CIX_TMP_ROOT"] = str(repo / ".cache" / "test-tmp")
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Source Port Test")
        git(repo, "config", "user.email", "source-port-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")
        create_branch(repo, "old-base", {"src/conflict.txt": "base\n"}, "old base")
        create_branch(repo, "new-base", {"src/conflict.txt": "new upstream\n"}, "new base")
        create_branch(repo, "source-tree", {"src/conflict.txt": "old source\n"}, "source tree")
        git(repo, "switch", "build")

        try:
            apply_source_delta_to_base(
                repo,
                old_base_ref="old-base",
                source_ref="source-tree",
                new_base_ref="new-base",
                message="port source tree",
                label="source-port-conflict-test",
                verbose=False,
            )
        except Exception as exc:
            message = str(exc)
        else:
            raise AssertionError("source port conflict unexpectedly resolved automatically")

        require("source-port conflict worktree preserved at:" in message, "conflict worktree path was not reported")
        worktree_line = [line for line in message.splitlines() if "source-port conflict worktree preserved at:" in line][0]
        worktree = Path(worktree_line.split(":", 1)[1].strip())
        require(worktree.exists(), "reported conflict worktree does not exist")
        conflict_text = (worktree / "src" / "conflict.txt").read_text(encoding="utf-8")
        require("<<<<<<<" in conflict_text, "conflict worktree did not contain conflict markers")
        require((worktree.parent / "README.md").exists(), "conflict handoff README was not written")
    finally:
        if old_tmp_root is None:
            os.environ.pop("EDK2_CIX_TMP_ROOT", None)
        else:
            os.environ["EDK2_CIX_TMP_ROOT"] = old_tmp_root
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
    require(
        "promote-unofficial-release:" in makefile,
        "make promote-unofficial-release target is missing",
    )
    require(
        "uplift-edk2-release:" in makefile,
        "make uplift-edk2-release target is missing",
    )
    require(
        "uplift-edk2-release-help:" in makefile,
        "make uplift-edk2-release-help target is missing",
    )
    require(
        'FROM_EDK2_BASE="$(FROM_EDK2_BASE)"' in makefile,
        "make integrate-source-release must pass FROM_EDK2_BASE through to the script",
    )
    require(
        'RESOLVED_REF="$(or $(RESOLVED_REF),$(REF))"' in makefile,
        "make promote-unofficial-release must pass RESOLVED_REF/REF through to the script",
    )
    require(
        'RADXA_REF="$(RADXA_REF)"' in makefile and 'UNOFFICIAL_REF="$(UNOFFICIAL_REF)"' in makefile,
        "make uplift-edk2-release must expose separate Radxa and unofficial conflict-resume refs",
    )
    uplift_script = ROOT / "scripts" / "uplift_edk2_release.py"
    require(uplift_script.exists(), "uplift-edk2-release script is missing")
    uplift_text = uplift_script.read_text(encoding="utf-8")
    require(
        "already exists; skipping" in uplift_text,
        "uplift-edk2-release should skip completed stages for conflict-resume reruns",
    )
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    require(
        "The primitive targets remain supported" in readme
        and "make integrate-source-release" in readme
        and "make promote-unofficial-release" in readme,
        "README should document how older source-uplift primitives fit the current process",
    )


def main() -> None:
    test_custom_targets_allow_historical_radxa_releases_on_later_edk2()
    test_default_source_target_follows_unofficial_source_policy()
    test_unofficial_source_policy_tracks_latest_unofficial_release_branch()
    test_source_delta_porting_replays_only_project_delta()
    test_source_delta_porting_ignores_deletes_already_absent_upstream()
    test_source_delta_porting_preserves_conflict_worktree_for_manual_resume()
    test_historical_upstream_target_overlays_build_infrastructure_only()
    test_integrate_source_release_make_target_preserves_materialise_default()
    print("historical Radxa target tests passed")


def load_tests(loader, tests, pattern):
    return load_function_tests(globals())


if __name__ == "__main__":
    main()
