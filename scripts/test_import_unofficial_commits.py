#!/usr/bin/env python3
"""Integration tests for import_unofficial_commits.py."""

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
    "import_workflow.py",
    "import_unofficial_commits.py",
    "reconstruction_common.py",
    "source_lifecycle.py",
    "source_policy.py",
]


def make_repo() -> Path:
    tmp = Path(tempfile.mkdtemp(prefix="edk2-cix-import-test."))
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
    commit_all(tmp, "render cache")

    git(tmp, "switch", "build")
    return tmp


def run_import(repo: Path, **env: str) -> subprocess.CompletedProcess[str]:
    return run(["python3", "scripts/import_unofficial_commits.py"], repo, check=False, env=env)


def symlink(repo: Path, target: str, link: str) -> None:
    path = repo / link
    path.parent.mkdir(parents=True, exist_ok=True)
    os.symlink(target, path)


def make_topic(repo: Path, name: str = "topic", text: str = "topic change\n") -> tuple[str, str]:
    base = rev_parse(repo, "source/unofficial/current")
    git(repo, "switch", "-c", name, "source/unofficial/current")
    write_file(repo, "firmware.txt", text)
    commit_all(repo, f"{name} change")
    git(repo, "switch", "build")
    return base, name


def add_current_source_and_checkpoint_rename(repo: Path) -> None:
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
    git(repo, "switch", "build")


def make_overlay_symlink_topic(repo: Path, path: str = "new.c", name: str = "overlay-symlink-topic") -> tuple[str, str]:
    base = rev_parse(repo, "source/unofficial/current")
    git(repo, "switch", "-c", name, "source/unofficial/current")
    symlink(repo, f"../../../src/component/{path}", f"custom/overlay/component/{path}")
    commit_all(repo, f"{name} change")
    git(repo, "switch", "build")
    return base, name


def test_direct_import_dry_run_does_not_move_ref() -> None:
    repo = make_repo()
    try:
        _base, topic = make_topic(repo)
        old_current = rev_parse(repo, "source/unofficial/current")
        result = run_import(repo, FROM_REF=topic)
        require(result.returncode == 0, result.stderr)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "dry run moved source/unofficial/current")
        require("dry run" in result.stdout, "dry run output should explain no write occurred")
    finally:
        shutil.rmtree(repo)


def test_propagate_all_updates_current_checkpoints_and_tags() -> None:
    repo = make_repo()
    try:
        _base, topic = make_topic(repo)
        result = run_import(
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
            require(show(repo, ref, "firmware.txt") == "topic change\n", f"{ref} did not receive topic change")
        for release in ("202208", "202602"):
            branch = f"source/unofficial/edk2-stable{release}"
            tag = f"source/unofficial/edk2/stable-{release}"
            require(rev_parse(repo, tag) == rev_parse(repo, branch), f"{tag} was not updated to {branch}")
    finally:
        shutil.rmtree(repo)


def test_propagation_normalises_exact_mirror_rename() -> None:
    repo = make_repo()
    try:
        add_current_source_and_checkpoint_rename(repo)
        _base, topic = make_overlay_symlink_topic(repo)
        result = run_import(
            repo,
            FROM_REF=topic,
            PROPAGATE_CHECKPOINTS="all",
            UPDATE_COMPAT_TAGS="1",
            WRITE="1",
        )
        require(result.returncode == 0, result.stderr + result.stdout)
        require(
            show(repo, "source/unofficial/current", "custom/overlay/component/new.c") == "../../../src/component/new.c",
            "current did not keep the new mirror path",
        )
        require(
            show(repo, "source/unofficial/edk2-stable202208", "custom/overlay/component/old.c") == "../../../src/component/old.c",
            "older checkpoint did not receive the retargeted mirror path",
        )
        missing = git(repo, "show", "source/unofficial/edk2-stable202208:custom/overlay/component/new.c", check=False)
        require(missing.returncode != 0, "older checkpoint kept the broken new mirror path")
    finally:
        shutil.rmtree(repo)


def test_propagation_validate_mode_reports_required_normalisation() -> None:
    repo = make_repo()
    try:
        add_current_source_and_checkpoint_rename(repo)
        _base, topic = make_overlay_symlink_topic(repo)
        old_current = rev_parse(repo, "source/unofficial/current")
        old_checkpoint = rev_parse(repo, "source/unofficial/edk2-stable202208")
        result = run_import(
            repo,
            FROM_REF=topic,
            PROPAGATE_CHECKPOINTS="all",
            UPDATE_COMPAT_TAGS="1",
            SOURCE_LIFECYCLE_NORMALISE="validate",
            WRITE="1",
        )
        require(result.returncode != 0, "validate mode should reject required lifecycle rewrites")
        require("source lifecycle normalisation is required" in result.stderr, result.stderr)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "current moved despite validation failure")
        require(rev_parse(repo, "source/unofficial/edk2-stable202208") == old_checkpoint, "checkpoint moved despite validation failure")
    finally:
        shutil.rmtree(repo)


def test_propagation_drops_mirror_for_source_missing_in_checkpoint() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "source/unofficial/current")
        write_file(repo, "src/component/later-only.c", "later source\n")
        commit_all(repo, "current-only source")
        git(repo, "switch", "source/unofficial/edk2-stable202602")
        write_file(repo, "src/component/later-only.c", "later source\n")
        commit_all(repo, "newer-only source")
        git(repo, "switch", "build")

        _base, topic = make_overlay_symlink_topic(repo, path="later-only.c", name="drop-mirror-topic")
        result = run_import(
            repo,
            FROM_REF=topic,
            PROPAGATE_CHECKPOINTS="all",
            UPDATE_COMPAT_TAGS="1",
            WRITE="1",
        )
        require(result.returncode == 0, result.stderr + result.stdout)
        missing = git(repo, "show", "source/unofficial/edk2-stable202208:custom/overlay/component/later-only.c", check=False)
        require(missing.returncode != 0, "older checkpoint kept mirror for missing source path")
        require(
            show(repo, "source/unofficial/edk2-stable202602", "custom/overlay/component/later-only.c") == "../../../src/component/later-only.c",
            "newer checkpoint lost valid mirror path",
        )
    finally:
        shutil.rmtree(repo)


def test_direct_checkpoint_import_updates_matching_tag_when_requested() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "-c", "checkpoint-topic", "source/unofficial/edk2-stable202602")
        write_file(repo, "firmware.txt", "checkpoint topic\n")
        commit_all(repo, "checkpoint topic change")
        git(repo, "switch", "build")
        result = run_import(
            repo,
            FROM_REF="checkpoint-topic",
            SOURCE_UNOFFICIAL_REF="source/unofficial/edk2-stable202602",
            UPDATE_COMPAT_TAGS="1",
            WRITE="1",
        )
        require(result.returncode == 0, result.stderr + result.stdout)
        require(show(repo, "source/unofficial/edk2-stable202602", "firmware.txt") == "checkpoint topic\n", "checkpoint branch did not move")
        require(
            rev_parse(repo, "source/unofficial/edk2/stable-202602") == rev_parse(repo, "source/unofficial/edk2-stable202602"),
            "direct checkpoint import did not update compatibility tag",
        )
    finally:
        shutil.rmtree(repo)


def test_direct_import_rejects_identical_overlay_copy() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "-c", "topic", "source/unofficial/current")
        write_file(repo, "src/component/file.c", "same\n")
        write_file(repo, "custom/overlay/component/file.c", "same\n")
        commit_all(repo, "add bad overlay copy")
        git(repo, "switch", "build")
        old_current = rev_parse(repo, "source/unofficial/current")

        result = run_import(repo, FROM_REF="topic")
        require(result.returncode != 0, "identical overlay copy should be rejected during dry run")
        require("source-tree policy failed" in result.stderr, result.stderr)

        result = run_import(repo, FROM_REF="topic", WRITE="1")
        require(result.returncode != 0, "identical overlay copy should be rejected")
        require("source-tree policy failed" in result.stderr, result.stderr)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "current moved despite source policy failure")
    finally:
        shutil.rmtree(repo)


def test_source_unofficial_from_ref_is_rejected_for_propagation() -> None:
    repo = make_repo()
    try:
        result = run_import(
            repo,
            FROM_REF="source/unofficial/current",
            PROPAGATE_CHECKPOINTS="all",
            UPDATE_COMPAT_TAGS="1",
            WRITE="1",
        )
        require(result.returncode != 0, "source/unofficial/current should not be accepted as FROM_REF by default")
        require("ALLOW_SOURCE_REF_FROM=1" in result.stderr, result.stderr)
    finally:
        shutil.rmtree(repo)


def test_checkpoint_from_ref_is_rejected_for_propagation() -> None:
    repo = make_repo()
    try:
        result = run_import(
            repo,
            FROM_REF="source/unofficial/edk2-stable202208",
            PROPAGATE_CHECKPOINTS="all",
            UPDATE_COMPAT_TAGS="1",
            WRITE="1",
        )
        require(result.returncode != 0, "checkpoint source refs should not be accepted as FROM_REF by default")
        require("ALLOW_SOURCE_REF_FROM=1" in result.stderr, result.stderr)
    finally:
        shutil.rmtree(repo)


def test_cache_based_from_ref_is_rejected() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "-c", "cache-topic", "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial")
        write_file(repo, "firmware.txt", "cache topic\n")
        commit_all(repo, "cache topic change")
        git(repo, "switch", "build")
        result = run_import(repo, FROM_REF="cache-topic", WRITE="1")
        require(result.returncode != 0, "cache-based topic should not be accepted by import-unofficial-commits")
        require("make import-changes" in result.stderr, result.stderr)
    finally:
        shutil.rmtree(repo)


def test_unrelated_from_ref_is_rejected() -> None:
    repo = make_repo()
    try:
        switch_orphan(repo, "legacy-topic")
        write_file(repo, "firmware.txt", "legacy topic\n")
        commit_all(repo, "legacy topic")
        git(repo, "switch", "build")
        result = run_import(repo, FROM_REF="legacy-topic", WRITE="1")
        require(result.returncode != 0, "unrelated topic should not be accepted by import-unofficial-commits")
        require("Use make import-changes" in result.stderr, result.stderr)
    finally:
        shutil.rmtree(repo)


def test_propagation_dry_run_does_not_move_refs_or_tags() -> None:
    repo = make_repo()
    try:
        _base, topic = make_topic(repo)
        old_values = {
            ref: rev_parse(repo, ref)
            for ref in (
                "source/unofficial/current",
                "source/unofficial/edk2-stable202208",
                "source/unofficial/edk2-stable202602",
                "source/unofficial/edk2/stable-202208",
                "source/unofficial/edk2/stable-202602",
            )
        }
        result = run_import(
            repo,
            FROM_REF=topic,
            PROPAGATE_CHECKPOINTS="all",
            UPDATE_COMPAT_TAGS="1",
        )
        require(result.returncode == 0, result.stderr)
        require("dry run" in result.stdout, "propagation dry run should explain no write occurred")
        require("source/unofficial/edk2/stable-202208" in result.stdout, "dry run should list compatibility tags")
        for ref, old_oid in old_values.items():
            require(rev_parse(repo, ref) == old_oid, f"dry run moved {ref}")
    finally:
        shutil.rmtree(repo)


def test_empty_replay_range_is_rejected() -> None:
    repo = make_repo()
    try:
        base = rev_parse(repo, "source/unofficial/current")
        result = run_import(
            repo,
            FROM_REF="source/unofficial/current",
            BASE_REF=base,
            ALLOW_SOURCE_REF_FROM="1",
            PROPAGATE_CHECKPOINTS="all",
            WRITE="1",
        )
        require(result.returncode != 0, "empty replay range should be rejected")
        require("replay range is empty" in result.stderr, result.stderr)
    finally:
        shutil.rmtree(repo)


def test_conflict_leaves_permanent_refs_unchanged_then_continue_finalises() -> None:
    repo = make_repo()
    try:
        base, topic = make_topic(repo, text="topic\n")
        git(repo, "switch", "source/unofficial/edk2-stable202208")
        write_file(repo, "firmware.txt", "checkpoint conflict\n")
        commit_all(repo, "make checkpoint conflict")
        git(repo, "tag", "-f", "source/unofficial/edk2/stable-202208")
        git(repo, "switch", "build")

        old_current = rev_parse(repo, "source/unofficial/current")
        old_checkpoint = rev_parse(repo, "source/unofficial/edk2-stable202208")
        result = run_import(
            repo,
            FROM_REF=topic,
            BASE_REF=base,
            PROPAGATE_CHECKPOINTS="all",
            UPDATE_COMPAT_TAGS="1",
            WRITE="1",
        )
        require(result.returncode != 0, "conflicting replay should pause")
        require("Import paused due to conflicts" in result.stderr, result.stderr)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "current moved despite conflict")
        require(rev_parse(repo, "source/unofficial/edk2-stable202208") == old_checkpoint, "checkpoint moved despite conflict")

        op_dirs = sorted((repo / ".cache" / "edk2-cix" / "operations" / "import-unofficial").iterdir())
        require(len(op_dirs) == 1, "expected one paused operation")
        op_id = op_dirs[0].name
        scratch = conflicted_scratch(op_dirs[0])
        write_file(scratch, "firmware.txt", "resolved\n")
        git(scratch, "add", "firmware.txt")

        continued = run_import(repo, CONTINUE="1", OP_ID=op_id, WRITE="1")
        require(continued.returncode == 0, continued.stderr + continued.stdout)
        require(show(repo, "source/unofficial/current", "firmware.txt") == "topic\n", "current did not receive topic after continue")
        require(show(repo, "source/unofficial/edk2-stable202208", "firmware.txt") == "resolved\n", "checkpoint resolution was not finalised")
        require(
            rev_parse(repo, "source/unofficial/edk2/stable-202208") == rev_parse(repo, "source/unofficial/edk2-stable202208"),
            "checkpoint tag was not finalised",
        )
    finally:
        shutil.rmtree(repo)


def test_abort_removes_paused_operation_without_moving_refs() -> None:
    repo = make_repo()
    try:
        base, topic = make_topic(repo, text="topic\n")
        git(repo, "switch", "source/unofficial/edk2-stable202208")
        write_file(repo, "firmware.txt", "checkpoint conflict\n")
        commit_all(repo, "make checkpoint conflict")
        git(repo, "switch", "build")
        old_current = rev_parse(repo, "source/unofficial/current")

        result = run_import(repo, FROM_REF=topic, BASE_REF=base, PROPAGATE_CHECKPOINTS="all", WRITE="1")
        require(result.returncode != 0, "conflicting replay should pause")
        op_id = next((repo / ".cache" / "edk2-cix" / "operations" / "import-unofficial").iterdir()).name
        aborted = run_import(repo, ABORT="1", OP_ID=op_id)
        require(aborted.returncode == 0, aborted.stderr)
        require(not (repo / ".cache" / "edk2-cix" / "operations" / "import-unofficial" / op_id).exists(), "abort left operation state")
        require(rev_parse(repo, "source/unofficial/current") == old_current, "abort moved current")
    finally:
        shutil.rmtree(repo)


def test_concurrent_ref_movement_aborts_finalise() -> None:
    repo = make_repo()
    try:
        base, topic = make_topic(repo, text="topic\n")
        git(repo, "switch", "source/unofficial/edk2-stable202208")
        write_file(repo, "firmware.txt", "checkpoint conflict\n")
        commit_all(repo, "make checkpoint conflict")
        git(repo, "switch", "build")
        result = run_import(repo, FROM_REF=topic, BASE_REF=base, PROPAGATE_CHECKPOINTS="all", WRITE="1")
        require(result.returncode != 0, "conflicting replay should pause")
        op_id = next((repo / ".cache" / "edk2-cix" / "operations" / "import-unofficial").iterdir()).name
        scratch = conflicted_scratch(repo / ".cache" / "edk2-cix" / "operations" / "import-unofficial" / op_id)
        write_file(scratch, "firmware.txt", "resolved\n")
        git(scratch, "add", "firmware.txt")

        git(repo, "switch", "source/unofficial/current")
        write_file(repo, "unrelated.txt", "movement\n")
        commit_all(repo, "move current concurrently")
        git(repo, "switch", "build")
        moved_current = rev_parse(repo, "source/unofficial/current")

        continued = run_import(repo, CONTINUE="1", OP_ID=op_id, WRITE="1")
        require(continued.returncode != 0, "concurrent movement should abort finalise")
        require("changed during import" in continued.stderr, continued.stderr)
        require(rev_parse(repo, "source/unofficial/current") == moved_current, "concurrent current movement was overwritten")
    finally:
        shutil.rmtree(repo)


def main() -> None:
    test_direct_import_dry_run_does_not_move_ref()
    test_propagate_all_updates_current_checkpoints_and_tags()
    test_propagation_normalises_exact_mirror_rename()
    test_propagation_validate_mode_reports_required_normalisation()
    test_propagation_drops_mirror_for_source_missing_in_checkpoint()
    test_direct_checkpoint_import_updates_matching_tag_when_requested()
    test_direct_import_rejects_identical_overlay_copy()
    test_source_unofficial_from_ref_is_rejected_for_propagation()
    test_checkpoint_from_ref_is_rejected_for_propagation()
    test_cache_based_from_ref_is_rejected()
    test_unrelated_from_ref_is_rejected()
    test_propagation_dry_run_does_not_move_refs_or_tags()
    test_empty_replay_range_is_rejected()
    test_conflict_leaves_permanent_refs_unchanged_then_continue_finalises()
    test_abort_removes_paused_operation_without_moving_refs()
    test_concurrent_ref_movement_aborts_finalise()
    print("import_unofficial_commits tests passed")


if __name__ == "__main__":
    main()
