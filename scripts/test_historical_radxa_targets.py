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
    ReconstructionError,
    clear_metadata_caches,
    default_release,
    matrix_release_branches,
    release_entry,
)
from integrate_source_release import (  # noqa: E402
    cix_component_records,
    cix_record_remote,
    commit_radxa_source_snapshot,
    existing_immutable_target,
    manifest_path_for,
    materialise_existing_target_local_head,
    operation_manifest_metadata,
)
from source_porting import apply_source_delta_to_base, unchanged_ours_conflicts  # noqa: E402
from render_release_branch import coupled_persistent_refs, render_from_plan  # noqa: E402
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
                    "default_line": "1.2",
                    "lines": {
                        "1.2": {
                            "current_ref": "source/unofficial/1.2/current",
                            "current_edk2_release": edk2,
                            "current_cix_release": "1.2",
                            "current_radxa_release": radxa,
                        }
                    },
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
    create_branch(repo, "source/vendor/cix/1.2/tf-a", {"tf-a.txt": "tf-a\n"}, "cix tf-a")
    create_branch(repo, "source/vendor/cix/1.2/op-tee", {"op-tee.txt": "op-tee\n"}, "cix op-tee")
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
        "source/unofficial/1.2/current",
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
        create_branch(
            repo,
            "source/unofficial/1.2.1/edk2-stable202602",
            {"src.txt": "unofficial checkpoint 1.2.1\n"},
            "unofficial checkpoint 1.2.1",
        )
        create_branch(
            repo,
            "source/unofficial/1.2.2/edk2-stable202602",
            {"src.txt": "unofficial checkpoint 1.2.2\n"},
            "unofficial checkpoint 1.2.2",
        )
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


def test_unofficial_cache_aliases_are_coupled_for_persistent_refresh() -> None:
    repo = make_repo()
    try:
        canonical = f"{CACHE_RELEASE_PREFIX}custom/edk2-202602/cix-1.2/radxa-1.2.1/unofficial"
        versioned = f"{canonical}-1.2.1"
        expected = sorted([canonical, versioned])

        require(coupled_persistent_refs(repo, canonical) == expected, "canonical target lost its versioned alias")
        require(coupled_persistent_refs(repo, versioned) == expected, "versioned alias lost its canonical target")
    finally:
        shutil.rmtree(repo)


def test_unofficial_source_policy_requires_selected_exact_checkpoint() -> None:
    repo = make_repo()
    try:
        create_branch(repo, "source/base/edk2/edk2-stable202605", {"base.txt": "202605\n"}, "edk2 202605")
        create_branch(repo, "source/port/radxa/1.2.1/edk2-stable202605", {"radxa.txt": "1.2.1\n"}, "radxa 1.2 202605")
        git(repo, "switch", "build")

        write_unofficial_source_policy(repo, edk2="202605")
        branches, _aliases = matrix_release_branches(repo)
        stale = require_unofficial_source_policy(repo, branches)
        require(
            any("unavailable unofficial checkpoint" in problem for problem in stale),
            "missing exact checkpoint was not reported",
        )

        create_branch(
            repo,
            "source/unofficial/1.2.1/edk2-stable202605",
            {"src.txt": "unofficial checkpoint 1.2.1 202605\n"},
            "unofficial checkpoint 1.2.1 202605",
        )
        git(repo, "switch", "build")
        clear_metadata_caches()
        write_unofficial_source_policy(repo, edk2="202605")
        branches, _aliases = matrix_release_branches(repo)
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


def test_source_delta_porting_canonicalises_merge_preimages() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-port-normalisation-test."))
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Source Port Test")
        git(repo, "config", "user.email", "source-port-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")

        create_branch(
            repo,
            "old-base",
            {"src/shared.txt": "vendor base\r\ncontext one\r\ncontext two\r\ncontext three\r\nupstream base\r\n"},
            "old base",
        )
        create_branch(
            repo,
            "new-base",
            {"src/shared.txt": "vendor base\r\ncontext one\r\ncontext two\r\ncontext three\r\nupstream change\r\n"},
            "new base",
        )
        create_branch(
            repo,
            "source-tree",
            {
                "src/shared.txt": "vendor change\ncontext one\ncontext two\ncontext three\nupstream base\n",
                "custom/project.txt": "project delta\n",
            },
            "normalised source tree",
        )
        git(repo, "switch", "build")

        commit = apply_source_delta_to_base(
            repo,
            old_base_ref="old-base",
            source_ref="source-tree",
            new_base_ref="new-base",
            message="port normalised source tree",
            label="source-port-normalisation-test",
            normalise_source=True,
            verbose=False,
        )
        require(
            git(repo, "show", f"{commit}:src/shared.txt").stdout
            == "vendor change\ncontext one\ncontext two\ncontext three\nupstream change\n",
            "preimage canonicalisation did not preserve both semantic changes",
        )
        require(
            git(repo, "show", f"{commit}:custom/project.txt").stdout == "project delta\n",
            "project delta was not replayed after preimage canonicalisation",
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


def test_source_delta_porting_does_not_infer_cross_path_binary_renames() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-port-rename-test."))
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
                "obsolete/vendor.bin": "same binary payload\n",
                "src/shared.c": "old shared\n",
            },
            "old base",
        )
        create_branch(
            repo,
            "new-base",
            {
                "unrelated/testdata.bin": "same binary payload\n",
                "src/shared.c": "new shared\n",
            },
            "new base",
        )
        create_branch(
            repo,
            "source-tree",
            {
                "src/shared.c": "old shared\n",
                "vendor/release.txt": "new vendor release\n",
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
            label="source-port-rename-test",
            verbose=False,
        )
        require(
            git(repo, "show", f"{commit}:unrelated/testdata.bin").stdout
            == "same binary payload\n",
            "unrelated equal-content file was consumed as a false rename",
        )
        require(
            git(repo, "cat-file", "-e", f"{commit}:obsolete/vendor.bin", check=False).returncode
            != 0,
            "obsolete vendor file was retained",
        )
        require(
            git(repo, "show", f"{commit}:vendor/release.txt").stdout
            == "new vendor release\n",
            "vendor delta was not replayed",
        )
    finally:
        shutil.rmtree(repo)


def test_only_identical_base_and_ours_stages_are_safe_false_renames() -> None:
    identical = "9d037dcbe232329f7ad1d10095262b47c52907d7"
    changed = "7f3114f519be1d64246d97a65252303cca31c59d"
    output = (
        f"100644 {identical} 1\tunrelated/testdata.bin\n"
        f"100644 {identical} 2\tunrelated/testdata.bin\n"
        f"100644 {identical} 1\tstill-conflicted.bin\n"
        f"100644 {changed} 2\tstill-conflicted.bin\n"
        f"100644 {identical} 3\tstill-conflicted.bin\n"
    )

    require(
        unchanged_ours_conflicts(output) == {"unrelated/testdata.bin"},
        "false-rename filter accepted a genuine three-way conflict",
    )


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
                resume_variable="UNOFFICIAL_REF",
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
        handoff = (worktree.parent / "README.md").read_text(encoding="utf-8")
        require("UNOFFICIAL_REF=$(git -C" in handoff, "conflict handoff used the wrong resume variable")
    finally:
        if old_tmp_root is None:
            os.environ.pop("EDK2_CIX_TMP_ROOT", None)
        else:
            os.environ["EDK2_CIX_TMP_ROOT"] = old_tmp_root
        shutil.rmtree(repo)


def test_source_delta_porting_resolves_policy_owned_paths_from_source() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-port-policy-test."))
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Source Port Policy Test")
        git(repo, "config", "user.email", "source-port-policy-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")
        create_branch(
            repo,
            "old-base",
            {
                ".github/workflow.yml": "old vendor workflow\n",
                "src/firmware.c": "old firmware\n",
            },
            "old base",
        )
        create_branch(
            repo,
            "new-base",
            {
                ".github/workflow.yml": "new vendor workflow\n",
                "src/firmware.c": "new firmware\n",
            },
            "new base",
        )
        create_branch(
            repo,
            "source-tree",
            {
                ".github/workflow.yml": "unofficial workflow\n",
                "custom/project.txt": "project change\n",
                "src/firmware.c": "old firmware\n",
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
            label="source-port-policy-test",
            source_owned_paths=(".github",),
            verbose=False,
        )
        require(
            git(repo, "show", f"{commit}:.github/workflow.yml").stdout == "unofficial workflow\n",
            "policy-owned path did not preserve the unofficial source",
        )
        require(
            git(repo, "show", f"{commit}:src/firmware.c").stdout == "new firmware\n",
            "vendor firmware change was overwritten by the policy overlay",
        )
        require(
            git(repo, "show", f"{commit}:custom/project.txt").stdout == "project change\n",
            "unofficial source delta was not carried",
        )
    finally:
        shutil.rmtree(repo)


def test_source_delta_porting_drops_mirror_for_deleted_source_path() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-port-deleted-mirror-test."))
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Source Port Lifecycle Test")
        git(repo, "config", "user.email", "source-port-lifecycle-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")
        create_branch(
            repo,
            "old-base",
            {"src/component/obsolete.c": "old source\n"},
            "old base",
        )
        create_branch(
            repo,
            "new-base",
            {"src/component/current.c": "new source\n"},
            "new base",
        )

        switch_orphan(repo, "source-tree")
        write_file(repo, "src/component/obsolete.c", "old source\n")
        overlay = repo / "custom/overlay/component/obsolete.c"
        overlay.parent.mkdir(parents=True, exist_ok=True)
        os.symlink("../../../src/component/obsolete.c", overlay)
        commit_all(repo, "source tree with mirror")
        git(repo, "switch", "build")

        commit = apply_source_delta_to_base(
            repo,
            old_base_ref="old-base",
            source_ref="source-tree",
            new_base_ref="new-base",
            message="port source tree",
            label="source-port-deleted-mirror-test",
            verbose=False,
        )
        require(
            git(
                repo,
                "cat-file",
                "-e",
                f"{commit}:custom/overlay/component/obsolete.c",
                check=False,
            ).returncode
            != 0,
            "mirror symlink for a proven-deleted source path was retained",
        )
        require(
            git(repo, "show", f"{commit}:src/component/current.c").stdout == "new source\n",
            "new source tree was not preserved while dropping the stale mirror",
        )
    finally:
        shutil.rmtree(repo)


def test_source_delta_porting_rebases_regular_overlay_content() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-port-overlay-rebase-test."))
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Source Port Overlay Test")
        git(repo, "config", "user.email", "source-port-overlay-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")
        old_source = "header\r\nvendor old\r\nmiddle\r\ncustom old\r\ntail\r\n"
        create_branch(
            repo,
            "old-base",
            {"src/component/module.inf": old_source},
            "old base",
        )
        create_branch(
            repo,
            "new-base",
            {
                "src/component/module.inf": (
                    "header\r\nvendor new\r\nmiddle\r\ncustom old\r\ntail\r\n"
                )
            },
            "new base",
        )
        create_branch(
            repo,
            "source-tree",
            {
                "src/component/module.inf": old_source,
                "custom/overlay/component/module.inf": (
                    "header\nvendor old\nmiddle\ncustom unofficial\ntail\n"
                ),
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
            label="source-port-overlay-rebase-test",
            verbose=False,
        )
        merged = git(
            repo,
            "show",
            f"{commit}:custom/overlay/component/module.inf",
        ).stdout
        require(
            merged == "header\nvendor new\nmiddle\ncustom unofficial\ntail\n",
            "regular overlay did not combine new source and unofficial changes",
        )
    finally:
        shutil.rmtree(repo)


def test_source_delta_porting_retires_overlay_absorbed_by_vendor() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-port-absorbed-overlay-test."))
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Source Port Overlay Test")
        git(repo, "config", "user.email", "source-port-overlay-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")
        old_source = "header\nvendor old\nmiddle\ncustom old\ntail\n"
        create_branch(
            repo,
            "old-base",
            {"src/component/module.inf": old_source},
            "old base",
        )
        create_branch(
            repo,
            "new-base",
            {
                "src/component/module.inf": (
                    "header\nvendor new\nmiddle\ncustom unofficial\ntail\n"
                )
            },
            "new base",
        )
        create_branch(
            repo,
            "source-tree",
            {
                "src/component/module.inf": old_source,
                "custom/overlay/component/module.inf": (
                    "header\nvendor old\nmiddle\ncustom unofficial\ntail\n"
                ),
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
            label="source-port-absorbed-overlay-test",
            verbose=False,
        )
        mirror = "custom/overlay/component/module.inf"
        require(
            git(repo, "ls-tree", commit, "--", mirror).stdout.startswith("120000 "),
            "overlay absorbed by the vendor source was not retired to a mirror",
        )
        require(
            git(repo, "show", f"{commit}:{mirror}").stdout
            == "../../../src/component/module.inf",
            "retired overlay does not mirror the updated vendor source",
        )
        require(
            git(repo, "show", f"{commit}:src/component/module.inf").stdout
            == "header\nvendor new\nmiddle\ncustom unofficial\ntail\n",
            "vendor source did not retain its additional update",
        )
    finally:
        shutil.rmtree(repo)


def test_source_delta_porting_mirrors_new_file_in_complete_overlay() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-port-complete-overlay-test."))
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Source Port Complete Overlay Test")
        git(repo, "config", "user.email", "source-port-complete-overlay-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")
        create_branch(
            repo,
            "old-base",
            {"src/component/module/a.c": "source a\n"},
            "old base",
        )
        create_branch(
            repo,
            "new-base",
            {
                "src/component/module/a.c": "source a\n",
                "src/component/module/b.c": "source b\n",
            },
            "new base",
        )
        create_branch(
            repo,
            "source-tree",
            {
                "src/component/module/a.c": "source a\n",
                "custom/overlay/component/module/a.c": "custom a\n",
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
            label="source-port-complete-overlay-test",
            verbose=False,
        )
        mirror = "custom/overlay/component/module/b.c"
        require(
            git(repo, "show", f"{commit}:{mirror}").stdout
            == "../../../../src/component/module/b.c",
            "new source file in a complete overlay directory was not mirrored",
        )
        require(
            git(repo, "ls-tree", commit, "--", mirror).stdout.startswith("120000 "),
            "new complete-overlay mirror was not recorded as a symlink",
        )
    finally:
        shutil.rmtree(repo)


def test_source_delta_porting_resolves_policy_paths_before_mixed_handoff() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-port-mixed-policy-test."))
    old_tmp_root = os.environ.get("EDK2_CIX_TMP_ROOT")
    os.environ["EDK2_CIX_TMP_ROOT"] = str(repo / ".cache" / "test-tmp")
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Source Port Policy Test")
        git(repo, "config", "user.email", "source-port-policy-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")
        create_branch(
            repo,
            "old-base",
            {
                ".github/workflow.yml": "old vendor workflow\n",
                "src/conflict.c": "old firmware\n",
            },
            "old base",
        )
        create_branch(
            repo,
            "new-base",
            {
                ".github/workflow.yml": "new vendor workflow\n",
                "src/conflict.c": "new vendor firmware\n",
            },
            "new base",
        )
        create_branch(
            repo,
            "source-tree",
            {
                ".github/workflow.yml": "unofficial workflow\n",
                "src/conflict.c": "unofficial firmware\n",
            },
            "source tree",
        )
        git(repo, "switch", "build")

        try:
            apply_source_delta_to_base(
                repo,
                old_base_ref="old-base",
                source_ref="source-tree",
                new_base_ref="new-base",
                message="port source tree",
                label="source-port-mixed-policy-test",
                source_owned_paths=(".github",),
                resume_variable="UNOFFICIAL_REF",
                verbose=False,
            )
        except Exception as exc:
            message = str(exc)
        else:
            raise AssertionError("mixed source port conflict unexpectedly resolved automatically")

        worktree_line = [
            line
            for line in message.splitlines()
            if "source-port conflict worktree preserved at:" in line
        ][0]
        worktree = Path(worktree_line.split(":", 1)[1].strip())
        require(
            (worktree / ".github" / "workflow.yml").read_text(encoding="utf-8")
            == "unofficial workflow\n",
            "policy-owned path retained conflict markers in mixed handoff",
        )
        require(
            "<<<<<<<" in (worktree / "src" / "conflict.c").read_text(encoding="utf-8"),
            "genuine firmware conflict was not preserved",
        )
        handoff = (worktree.parent / "README.md").read_text(encoding="utf-8")
        require(
            "  - src/conflict.c" in handoff,
            "genuine firmware conflict was omitted from handoff",
        )
        require(
            "  - .github/workflow.yml" not in handoff,
            "resolved policy conflict remained in handoff conflict list",
        )
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


def test_resolved_radxa_port_snapshot_preserves_exact_tree() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-resolved-radxa-port-test."))
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Resolved Radxa Port Test")
        git(repo, "config", "user.email", "resolved-radxa-port-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")
        create_branch(
            repo,
            "source/cache/base/edk2/edk2-stable202608",
            {"src/upstream.c": "upstream\r\n"},
            "202608 base",
        )
        create_branch(
            repo,
            "resolved-port",
            {"src/upstream.c": "reviewed change\r\n"},
            "reviewed CRLF source-port resolution",
        )
        git(repo, "switch", "build")

        snapshot = commit_radxa_source_snapshot(
            repo,
            "resolved-port",
            "1.3.1",
            "edk2-stable202608",
            "ported-vendor-source",
        )
        require(
            git(repo, "rev-parse", f"{snapshot}^{{tree}}").stdout.strip()
            == git(repo, "rev-parse", "resolved-port^{tree}").stdout.strip(),
            "resolved Radxa source-port commit was normalised after review",
        )
    finally:
        shutil.rmtree(repo)


def test_radxa_vendor_integration_records_raw_upstream_ref() -> None:
    repo = make_repo()
    try:
        source = "source/vendor/radxa/0.2.0-1/edk2-stable202208"
        raw_source = git(repo, "rev-parse", f"{source}^{{commit}}").stdout.strip()
        metadata = operation_manifest_metadata(
            repo,
            source,
            {
                "type": "vendor-source",
                "vendor": "radxa",
                "radxa_release": "0.2.0-1",
                "_internal": "ignored",
            },
        )
        require(metadata["upstream_ref"] == raw_source, "Radxa vendor records should store the raw source commit")
        require("_internal" not in metadata, "internal operation metadata should not be written to manifests")

        port_metadata = operation_manifest_metadata(
            repo,
            source,
            {
                "type": "ported-vendor-source",
                "vendor": "radxa",
                "radxa_release": "0.2.0-1",
            },
        )
        require("upstream_ref" not in port_metadata, "Radxa port records should not invent vendor upstream refs")
    finally:
        shutil.rmtree(repo)


def test_cix_vendor_payload_and_port_namespaces() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-cix-namespace-test."))
    try:
        write_file(
            repo,
            "config/refs-cix.json",
            json.dumps(
                {
                    "refs": [
                        {
                            "component": "bios",
                            "ref": "source/vendor/cix/1.2/bios",
                            "remote": "cix-bios",
                            "type": "vendor-bundle",
                        },
                        {
                            "component": "bootloader1",
                            "ref": "source/vendor/cix/1.2/bootloader1",
                            "remote": "cix-edk2-non-osi-release",
                            "type": "vendor-payload",
                        },
                        {
                            "component": "tf-a",
                            "ref": "source/port/cix/1.2/tf-a/v2.12",
                            "type": "ported-vendor-component",
                        },
                    ]
                }
            )
            + "\n",
        )
        write_file(
            repo,
            "config/remotes.json",
            json.dumps(
                {
                    "remotes": {
                        "cix-bios": {"url": "https://example.invalid/cix-bios.git"},
                        "cix-edk2-non-osi-release": {
                            "url": "https://example.invalid/cix-edk2-non-osi.git"
                        },
                    }
                }
            )
            + "\n",
        )

        records = cix_component_records(repo, "1.2")
        require(
            [record["component"] for record in records] == ["bios", "bootloader1"],
            "CIX release integration should include raw vendor payloads and exclude ports",
        )
        require(
            cix_record_remote(repo, records[1]) == "https://example.invalid/cix-edk2-non-osi.git",
            "CIX vendor payload did not use its recorded provenance remote",
        )
        require(
            manifest_path_for("source/vendor/cix/1.2/tf-a") == "config/refs-cix.json"
            and manifest_path_for("source/port/cix/1.2/tf-a/v2.12") == "config/refs-cix.json",
            "CIX vendor and port refs should share the CIX manifest",
        )
    finally:
        shutil.rmtree(repo)


def test_integrate_source_release_recognises_remote_tracking_target() -> None:
    repo = make_repo()
    try:
        target = "source/vendor/radxa/0.2.0-1/edk2-stable202208"
        object_id = git(repo, "rev-parse", f"{target}^{{commit}}").stdout.strip()
        tree_id = git(repo, "rev-parse", f"{target}^{{tree}}").stdout.strip()
        write_file(
            repo,
            "config/refs-radxa.json",
            json.dumps(
                {
                    "defaults": {
                        "immutable": True,
                        "type": "vendor-source",
                        "vendor": "radxa",
                    },
                    "refs": [
                        {
                            "object_id": object_id,
                            "ref": target,
                            "tree_id": tree_id,
                            "upstream_ref": object_id,
                        }
                    ],
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
        )
        git(repo, "update-ref", f"refs/remotes/origin/{target}", object_id)
        git(repo, "branch", "-D", target)

        state = existing_immutable_target(
            repo,
            target,
            object_id,
            {"type": "vendor-source"},
        )
        require(state is not None, "remote-tracking immutable target was not detected")
        require(not state["has_local_head"], "remote-only target was reported as a local branch")
        require(
            state["copies"] == [(f"refs/remotes/origin/{target}", object_id)],
            "remote-tracking target location was not reported exactly",
        )
        require(
            materialise_existing_target_local_head(repo, state, verbose=False),
            "remote-only immutable target was not materialised locally",
        )
        require(
            git(repo, "rev-parse", f"refs/heads/{target}^{{commit}}").stdout.strip() == object_id,
            "local immutable branch did not preserve the recorded commit",
        )
        require(
            not materialise_existing_target_local_head(repo, state, verbose=False),
            "materialising an existing local immutable branch was not idempotent",
        )

        wrong_source = "source/base/edk2/edk2-stable202602"
        try:
            existing_immutable_target(
                repo,
                target,
                wrong_source,
                {"type": "vendor-source"},
            )
        except ReconstructionError as exc:
            require(
                "already integrated from upstream_ref" in str(exc),
                "mismatched requested provenance did not explain the existing integration",
            )
        else:
            raise AssertionError("mismatched requested provenance was accepted")
    finally:
        shutil.rmtree(repo)


def main() -> None:
    test_custom_targets_allow_historical_radxa_releases_on_later_edk2()
    test_default_source_target_follows_unofficial_source_policy()
    test_unofficial_cache_aliases_are_coupled_for_persistent_refresh()
    test_unofficial_source_policy_requires_selected_exact_checkpoint()
    test_source_delta_porting_replays_only_project_delta()
    test_source_delta_porting_canonicalises_merge_preimages()
    test_source_delta_porting_ignores_deletes_already_absent_upstream()
    test_source_delta_porting_preserves_conflict_worktree_for_manual_resume()
    test_source_delta_porting_resolves_policy_owned_paths_from_source()
    test_source_delta_porting_drops_mirror_for_deleted_source_path()
    test_source_delta_porting_rebases_regular_overlay_content()
    test_source_delta_porting_retires_overlay_absorbed_by_vendor()
    test_source_delta_porting_mirrors_new_file_in_complete_overlay()
    test_source_delta_porting_resolves_policy_paths_before_mixed_handoff()
    test_historical_upstream_target_overlays_build_infrastructure_only()
    test_integrate_source_release_make_target_preserves_materialise_default()
    test_resolved_radxa_port_snapshot_preserves_exact_tree()
    test_radxa_vendor_integration_records_raw_upstream_ref()
    test_cix_vendor_payload_and_port_namespaces()
    test_integrate_source_release_recognises_remote_tracking_target()
    print("historical Radxa target tests passed")


def load_tests(loader, tests, pattern):
    return load_function_tests(globals())


if __name__ == "__main__":
    main()
