#!/usr/bin/env python3
"""Integration tests for import_changes.py."""

from __future__ import annotations

import shutil
import os
import subprocess
import tempfile
from pathlib import Path

from test_support import (
    commit_all,
    conflicted_scratch,
    git,
    require,
    rev_parse,
    run,
    show,
    switch_orphan,
    write_file,
)


ROOT = Path(__file__).resolve().parent.parent
SCRIPT_FILES = [
    "import_changes.py",
    "import_workflow.py",
    "import_unofficial_commits.py",
    "reconstruction_common.py",
    "source_lifecycle.py",
    "source_policy.py",
]


def make_repo() -> Path:
    tmp = Path(tempfile.mkdtemp(prefix="edk2-cix-import-changes-test."))
    git(tmp, "init", "-b", "build")
    git(tmp, "config", "user.name", "Import Test")
    git(tmp, "config", "user.email", "import-test")
    scripts = tmp / "scripts"
    scripts.mkdir()
    for name in SCRIPT_FILES:
        shutil.copy2(ROOT / "scripts" / name, scripts / name)
    commit_all(tmp, "build scripts")

    switch_orphan(tmp, "source/unofficial/current")
    write_file(tmp, "firmware.txt", "base\n")
    write_file(tmp, "release.txt", "current\n")
    commit_all(tmp, "current base")

    for release in ("202208", "202602"):
        git(tmp, "switch", "-c", f"source/unofficial/edk2-stable{release}", "source/unofficial/current")
        write_file(tmp, "release.txt", f"edk2-stable{release}\n")
        commit_all(tmp, f"checkpoint {release}")
        git(tmp, "tag", f"source/unofficial/edk2/stable-{release}")

    git(tmp, "switch", "--orphan", "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial")
    write_file(tmp, "firmware.txt", "base\n")
    write_file(tmp, "release.txt", "current\n")
    write_file(tmp, "render-only.txt", "materialised\n")
    commit_all(tmp, "render cache")

    switch_orphan(tmp, "legacy-base")
    write_file(tmp, "firmware.txt", "legacy base\n")
    commit_all(tmp, "legacy base")

    git(tmp, "switch", "-c", "retained-source", "legacy-base")
    write_file(tmp, "firmware.txt", "retained-source base\n")
    commit_all(tmp, "retained-source base")

    git(tmp, "switch", "build")
    return tmp


def run_import_changes(repo: Path, **env: str) -> subprocess.CompletedProcess[str]:
    return run(["python3", "scripts/import_changes.py"], repo, check=False, env=env)


def symlink(repo: Path, target: str, link: str) -> None:
    path = repo / link
    path.parent.mkdir(parents=True, exist_ok=True)
    os.symlink(target, path)


def make_materialised_topic(repo: Path, name: str = "materialised-topic", text: str = "from materialised\n") -> str:
    git(repo, "switch", "-c", name, "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial")
    write_file(repo, "firmware.txt", text)
    commit_all(repo, "materialised topic change")
    git(repo, "switch", "build")
    return name


def add_materialised_overlay_rename_fixture(repo: Path) -> None:
    git(repo, "switch", "source/unofficial/current")
    write_file(repo, "src/component/new.c", "renamed source\n")
    commit_all(repo, "current renamed source")

    git(repo, "switch", "source/unofficial/edk2-stable202208")
    write_file(repo, "src/component/old.c", "renamed source\n")
    commit_all(repo, "older source path")
    git(repo, "tag", "-f", "source/unofficial/edk2/stable-202208")

    git(repo, "switch", "source/unofficial/edk2-stable202602")
    write_file(repo, "src/component/new.c", "renamed source\n")
    commit_all(repo, "newer source path")
    git(repo, "tag", "-f", "source/unofficial/edk2/stable-202602")

    git(repo, "switch", "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial")
    write_file(repo, "src/component/new.c", "renamed source\n")
    commit_all(repo, "cache renamed source")
    git(repo, "switch", "build")


def make_materialised_overlay_topic(repo: Path) -> str:
    git(repo, "switch", "-c", "materialised-overlay-lifecycle-topic", "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial")
    symlink(repo, "../../../src/component/new.c", "custom/overlay/component/new.c")
    commit_all(repo, "materialised overlay lifecycle change")
    git(repo, "switch", "build")
    return "materialised-overlay-lifecycle-topic"


def add_materialised_drop_fixture(repo: Path) -> None:
    for ref in (
        "source/unofficial/current",
        "source/unofficial/edk2-stable202602",
        "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial",
    ):
        git(repo, "switch", ref)
        write_file(repo, "src/component/later-only.c", "later source\n")
        commit_all(repo, f"{ref} later-only source")
    git(repo, "switch", "build")


def make_materialised_drop_topic(repo: Path) -> str:
    git(repo, "switch", "-c", "materialised-drop-lifecycle-topic", "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial")
    symlink(repo, "../../../src/component/later-only.c", "custom/overlay/component/later-only.c")
    commit_all(repo, "materialised drop lifecycle change")
    git(repo, "switch", "build")
    return "materialised-drop-lifecycle-topic"


def test_dry_run_infers_materialised_base_without_moving_refs() -> None:
    repo = make_repo()
    try:
        topic = make_materialised_topic(repo)
        old_current = rev_parse(repo, "source/unofficial/current")
        result = run_import_changes(repo, FROM_REF=topic)
        require(result.returncode == 0, result.stderr + result.stdout)
        require("source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial" in result.stdout, "dry run did not report inferred cache base")
        require("M\tfirmware.txt" in result.stdout, "dry run did not report changed path")
        require(rev_parse(repo, "source/unofficial/current") == old_current, "dry run moved source/unofficial/current")
        operations = repo / ".cache" / "edk2-cix" / "operations" / "import-changes"
        require(not operations.exists() or not any(operations.iterdir()), "dry run left operation state")
    finally:
        shutil.rmtree(repo)


def test_import_from_materialised_topic_creates_commit_on_current() -> None:
    repo = make_repo()
    try:
        topic = make_materialised_topic(repo)
        old_current = rev_parse(repo, "source/unofficial/current")
        result = run_import_changes(repo, FROM_REF=topic, COMMIT_MESSAGE="import materialised change", WRITE="1")
        require(result.returncode == 0, result.stderr + result.stdout)
        require(show(repo, "source/unofficial/current", "firmware.txt") == "from materialised\n", "current did not receive extracted patch")
        require(git(repo, "show", "-s", "--format=%P", "source/unofficial/current").stdout.strip() == old_current, "import did not create a direct child of current")
        require(git(repo, "show", "-s", "--format=%s", "source/unofficial/current").stdout.strip() == "import materialised change", "commit message not used")
        missing = git(repo, "show", "source/unofficial/current:render-only.txt", check=False)
        require(missing.returncode != 0, "materialised-only base file leaked into current")
    finally:
        shutil.rmtree(repo)


def test_import_changes_normalises_overlay_lifecycle_when_propagating() -> None:
    repo = make_repo()
    try:
        add_materialised_overlay_rename_fixture(repo)
        topic = make_materialised_overlay_topic(repo)
        result = run_import_changes(
            repo,
            FROM_REF=topic,
            PROPAGATE_CHECKPOINTS="all",
            UPDATE_COMPAT_TAGS="1",
            WRITE="1",
        )
        require(result.returncode == 0, result.stderr + result.stdout)
        require(
            show(repo, "source/unofficial/current", "custom/overlay/component/new.c") == "../../../src/component/new.c",
            "current did not receive materialised overlay change",
        )
        require(
            show(repo, "source/unofficial/edk2-stable202208", "custom/overlay/component/old.c") == "../../../src/component/old.c",
            "older checkpoint did not receive normalised overlay path",
        )
        missing = git(repo, "show", "source/unofficial/edk2-stable202208:custom/overlay/component/new.c", check=False)
        require(missing.returncode != 0, "older checkpoint kept the unnormalised overlay path")
    finally:
        shutil.rmtree(repo)


def test_import_changes_drops_mirror_when_source_is_absent_from_checkpoint() -> None:
    repo = make_repo()
    try:
        add_materialised_drop_fixture(repo)
        topic = make_materialised_drop_topic(repo)
        old_checkpoint = rev_parse(repo, "source/unofficial/edk2-stable202208")
        result = run_import_changes(
            repo,
            FROM_REF=topic,
            PROPAGATE_CHECKPOINTS="all",
            UPDATE_COMPAT_TAGS="1",
            WRITE="1",
        )
        require(result.returncode == 0, result.stderr + result.stdout)
        missing = git(repo, "show", "source/unofficial/edk2-stable202208:custom/overlay/component/later-only.c", check=False)
        require(missing.returncode != 0, "older checkpoint kept mirror for missing source path")
        require(rev_parse(repo, "source/unofficial/edk2-stable202208") == old_checkpoint, "unchanged older checkpoint moved")
    finally:
        shutil.rmtree(repo)


def test_import_with_explicit_legacy_base() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "-c", "legacy-topic", "legacy-base")
        write_file(repo, "overlay/new-driver.txt", "new broader-source file\n")
        commit_all(repo, "legacy topic change")
        git(repo, "switch", "build")

        result = run_import_changes(repo, FROM_REF="legacy-topic", BASE_REF="legacy-base", WRITE="1")
        require(result.returncode == 0, result.stderr + result.stdout)
        require(show(repo, "source/unofficial/current", "overlay/new-driver.txt") == "new broader-source file\n", "explicit-base import failed")
    finally:
        shutil.rmtree(repo)


def test_import_infers_retained_legacy_branch_base() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "-c", "legacy-topic", "retained-source")
        write_file(repo, "overlay/new-driver.txt", "new broader-source file\n")
        commit_all(repo, "legacy topic change")
        git(repo, "switch", "build")

        dry_run = run_import_changes(repo, FROM_REF="legacy-topic")
        require(dry_run.returncode == 0, dry_run.stderr + dry_run.stdout)
        require("retained-source" in dry_run.stdout, "dry-run base label was not retained in output")

        result = run_import_changes(repo, FROM_REF="legacy-topic", WRITE="1")
        require(result.returncode == 0, result.stderr + result.stdout)
        require(show(repo, "source/unofficial/current", "overlay/new-driver.txt") == "new broader-source file\n", "inferred legacy import failed")
        require(show(repo, "source/unofficial/current", "firmware.txt") == "base\n", "legacy base contents leaked into current")
    finally:
        shutil.rmtree(repo)


def test_import_infers_retained_legacy_fork_point_after_base_moves() -> None:
    repo = make_repo()
    try:
        fork_point = rev_parse(repo, "retained-source")
        git(repo, "switch", "retained-source")
        write_file(repo, "retained-only.txt", "new retained-source work\n")
        commit_all(repo, "advance retained-source")
        git(repo, "switch", "-c", "legacy-topic", fork_point)
        write_file(repo, "overlay/forked-driver.txt", "forked broader-source file\n")
        commit_all(repo, "legacy forked topic change")
        git(repo, "switch", "build")

        dry_run = run_import_changes(repo, FROM_REF="legacy-topic")
        require(dry_run.returncode == 0, dry_run.stderr + dry_run.stdout)
        require("merge-base(retained-source, FROM_REF)" in dry_run.stdout, "fork-point base was not reported")

        result = run_import_changes(repo, FROM_REF="legacy-topic", WRITE="1")
        require(result.returncode == 0, result.stderr + result.stdout)
        require(show(repo, "source/unofficial/current", "overlay/forked-driver.txt") == "forked broader-source file\n", "fork-point import failed")
        missing = git(repo, "show", "source/unofficial/current:retained-only.txt", check=False)
        require(missing.returncode != 0, "post-fork retained-source changes leaked into current")
    finally:
        shutil.rmtree(repo)


def test_missing_base_is_rejected_when_no_base_can_be_inferred() -> None:
    repo = make_repo()
    try:
        switch_orphan(repo, "orphan-topic")
        write_file(repo, "firmware.txt", "orphan base\n")
        commit_all(repo, "orphan base")
        write_file(repo, "firmware.txt", "legacy topic\n")
        commit_all(repo, "orphan topic change")
        git(repo, "switch", "build")

        result = run_import_changes(repo, FROM_REF="orphan-topic")
        require(result.returncode != 0, "missing BASE_REF should be rejected for unrelated source")
        require("could not infer BASE_REF" in result.stderr, result.stderr)
    finally:
        shutil.rmtree(repo)


def test_empty_diff_is_rejected() -> None:
    repo = make_repo()
    try:
        result = run_import_changes(
            repo,
            FROM_REF="source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial",
            BASE_REF="source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial",
        )
        require(result.returncode != 0, "empty diff should be rejected")
        require("change diff is empty" in result.stderr, result.stderr)
    finally:
        shutil.rmtree(repo)


def test_conflict_pauses_without_moving_refs_then_continue_finalises() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "source/unofficial/current")
        write_file(repo, "firmware.txt", "current conflict\n")
        commit_all(repo, "current conflict")
        git(repo, "switch", "build")
        old_current = rev_parse(repo, "source/unofficial/current")

        topic = make_materialised_topic(repo, text="topic conflict\n")
        result = run_import_changes(repo, FROM_REF=topic, WRITE="1")
        require(result.returncode != 0, "conflicting import should pause")
        require("Import paused due to conflicts" in result.stderr, result.stderr)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "current moved despite conflict")

        op_dir = next((repo / ".cache" / "edk2-cix" / "operations" / "import-changes").iterdir())
        scratch = conflicted_scratch(op_dir)
        write_file(scratch, "firmware.txt", "resolved\n")
        git(scratch, "add", "firmware.txt")
        continued = run_import_changes(repo, CONTINUE="1", OP_ID=op_dir.name, WRITE="1")
        require(continued.returncode == 0, continued.stderr + continued.stdout)
        require(show(repo, "source/unofficial/current", "firmware.txt") == "resolved\n", "conflict resolution was not finalised")
    finally:
        shutil.rmtree(repo)


def test_dry_run_conflict_reports_paths_without_moving_refs() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "source/unofficial/current")
        write_file(repo, "firmware.txt", "current conflict\n")
        commit_all(repo, "current conflict")
        git(repo, "switch", "build")
        old_current = rev_parse(repo, "source/unofficial/current")

        topic = make_materialised_topic(repo, text="topic conflict\n")
        result = run_import_changes(repo, FROM_REF=topic)
        require(result.returncode != 0, "conflicting dry-run should fail")
        require("dry run detected conflicts" in result.stderr, result.stderr)
        require("conflicting file(s):" in result.stderr, result.stderr)
        require("firmware.txt" in result.stderr, result.stderr)
        require("CONTINUE=1 OP_ID=<operation-id> WRITE=1" in result.stderr, result.stderr)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "dry-run conflict moved current")
        operations = repo / ".cache" / "edk2-cix" / "operations" / "import-changes"
        require(not operations.exists() or not any(operations.iterdir()), "dry-run conflict left operation state")
    finally:
        shutil.rmtree(repo)


def test_failed_apply_without_conflict_markers_pauses_for_manual_resolution() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "retained-source")
        write_file(repo, "legacy-only.txt", "legacy base\n")
        commit_all(repo, "add legacy-only file")
        git(repo, "switch", "-c", "legacy-topic")
        write_file(repo, "legacy-only.txt", "legacy topic\n")
        commit_all(repo, "modify legacy-only file")
        git(repo, "switch", "build")
        old_current = rev_parse(repo, "source/unofficial/current")

        result = run_import_changes(repo, FROM_REF="legacy-topic", WRITE="1")
        require(result.returncode != 0, "failed apply should pause for manual resolution")
        require("Import paused due to conflicts" in result.stderr, result.stderr)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "current moved despite failed apply")

        op_dir = next((repo / ".cache" / "edk2-cix" / "operations" / "import-changes").iterdir())
        scratch = conflicted_scratch(op_dir)
        write_file(scratch, "legacy-only.txt", "legacy topic\n")
        git(scratch, "add", "legacy-only.txt")
        continued = run_import_changes(repo, CONTINUE="1", OP_ID=op_dir.name, WRITE="1")
        require(continued.returncode == 0, continued.stderr + continued.stdout)
        require(show(repo, "source/unofficial/current", "legacy-only.txt") == "legacy topic\n", "manual resolution was not finalised")
    finally:
        shutil.rmtree(repo)


def test_import_rejects_identical_overlay_copy() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "-c", "materialised-overlay-topic", "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial")
        write_file(repo, "src/component/file.c", "same\n")
        write_file(repo, "custom/overlay/component/file.c", "same\n")
        commit_all(repo, "add bad overlay copy")
        git(repo, "switch", "build")
        old_current = rev_parse(repo, "source/unofficial/current")

        result = run_import_changes(repo, FROM_REF="materialised-overlay-topic", WRITE="1")
        require(result.returncode != 0, "identical overlay copy should be rejected")
        require(
            "source-tree policy failed" in result.stderr or "source lifecycle projection failed" in result.stderr,
            result.stderr,
        )
        require(rev_parse(repo, "source/unofficial/current") == old_current, "current moved despite source policy failure")
        operations = repo / ".cache" / "edk2-cix" / "operations" / "import-changes"
        require(not operations.exists() or not any(operations.iterdir()), "policy failure left operation state")
    finally:
        shutil.rmtree(repo)


def test_propagation_updates_checkpoints_and_tags() -> None:
    repo = make_repo()
    try:
        topic = make_materialised_topic(repo)
        result = run_import_changes(
            repo,
            FROM_REF=topic,
            PROPAGATE_CHECKPOINTS="all",
            UPDATE_COMPAT_TAGS="1",
            WRITE="1",
        )
        require(result.returncode == 0, result.stderr + result.stdout)
        for ref in (
            "source/unofficial/current",
            "source/unofficial/edk2-stable202208",
            "source/unofficial/edk2-stable202602",
        ):
            require(show(repo, ref, "firmware.txt") == "from materialised\n", f"{ref} did not receive extracted change")
        for release in ("202208", "202602"):
            branch = f"source/unofficial/edk2-stable{release}"
            tag = f"source/unofficial/edk2/stable-{release}"
            require(rev_parse(repo, tag) == rev_parse(repo, branch), f"{tag} was not updated")
    finally:
        shutil.rmtree(repo)


def main() -> None:
    test_dry_run_infers_materialised_base_without_moving_refs()
    test_import_from_materialised_topic_creates_commit_on_current()
    test_import_changes_normalises_overlay_lifecycle_when_propagating()
    test_import_changes_drops_mirror_when_source_is_absent_from_checkpoint()
    test_import_with_explicit_legacy_base()
    test_import_infers_retained_legacy_branch_base()
    test_import_infers_retained_legacy_fork_point_after_base_moves()
    test_missing_base_is_rejected_when_no_base_can_be_inferred()
    test_empty_diff_is_rejected()
    test_conflict_pauses_without_moving_refs_then_continue_finalises()
    test_dry_run_conflict_reports_paths_without_moving_refs()
    test_failed_apply_without_conflict_markers_pauses_for_manual_resolution()
    test_import_rejects_identical_overlay_copy()
    test_propagation_updates_checkpoints_and_tags()
    print("import_changes tests passed")


if __name__ == "__main__":
    main()
