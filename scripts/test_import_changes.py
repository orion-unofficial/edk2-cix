#!/usr/bin/env python3
"""Integration tests for import_changes.py."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCRIPT_FILES = [
    "import_changes.py",
    "import_unofficial_commits.py",
    "reconstruction_common.py",
]


def run(cmd: list[str], cwd: Path, *, check: bool = True, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    full_env = os.environ.copy()
    if env:
        full_env.update(env)
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=check,
        env=full_env,
    )


def git(repo: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return run(["git", *args], repo, check=check)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def write_file(repo: Path, relative: str, text: str) -> None:
    path = repo / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def commit_all(repo: Path, message: str) -> str:
    git(repo, "add", ".", ":!.cache")
    git(repo, "commit", "-m", message)
    return rev_parse(repo, "HEAD")


def rev_parse(repo: Path, ref: str) -> str:
    return git(repo, "rev-parse", f"{ref}^{{commit}}").stdout.strip()


def show(repo: Path, ref: str, relative: str) -> str:
    return git(repo, "show", f"{ref}:{relative}").stdout


def switch_orphan(repo: Path, branch: str) -> None:
    git(repo, "switch", "--orphan", branch)
    for path in repo.iterdir():
        if path.name == ".git":
            continue
        if path.is_dir():
            shutil.rmtree(path)
        else:
            path.unlink()


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

    git(tmp, "switch", "-c", "mainline-source", "legacy-base")
    write_file(tmp, "firmware.txt", "mainline-source base\n")
    commit_all(tmp, "mainline-source base")

    git(tmp, "switch", "build")
    return tmp


def run_import_changes(repo: Path, **env: str) -> subprocess.CompletedProcess[str]:
    return run(["python3", "scripts/import_changes.py"], repo, check=False, env=env)


def make_materialised_topic(repo: Path, name: str = "materialised-topic", text: str = "from materialised\n") -> str:
    git(repo, "switch", "-c", name, "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial")
    write_file(repo, "firmware.txt", text)
    commit_all(repo, "materialised topic change")
    git(repo, "switch", "build")
    return name


def conflicted_scratch(op_dir: Path) -> Path:
    with (op_dir / "state.json").open("r", encoding="utf-8") as f:
        state = json.load(f)
    for target in state["targets"]:
        if target.get("status") == "conflict":
            return Path(target["scratch"])
    raise AssertionError("operation has no conflicted scratch tree")


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
        git(repo, "switch", "-c", "legacy-topic", "mainline-source")
        write_file(repo, "overlay/new-driver.txt", "new broader-source file\n")
        commit_all(repo, "legacy topic change")
        git(repo, "switch", "build")

        dry_run = run_import_changes(repo, FROM_REF="legacy-topic")
        require(dry_run.returncode == 0, dry_run.stderr + dry_run.stdout)
        require("mainline-source" in dry_run.stdout, "dry-run base label was not retained in output")

        result = run_import_changes(repo, FROM_REF="legacy-topic", WRITE="1")
        require(result.returncode == 0, result.stderr + result.stdout)
        require(show(repo, "source/unofficial/current", "overlay/new-driver.txt") == "new broader-source file\n", "inferred legacy import failed")
        require(show(repo, "source/unofficial/current", "firmware.txt") == "base\n", "legacy base contents leaked into current")
    finally:
        shutil.rmtree(repo)


def test_import_infers_retained_legacy_fork_point_after_base_moves() -> None:
    repo = make_repo()
    try:
        fork_point = rev_parse(repo, "mainline-source")
        git(repo, "switch", "mainline-source")
        write_file(repo, "mainline-only.txt", "new mainline work\n")
        commit_all(repo, "advance mainline-source")
        git(repo, "switch", "-c", "legacy-topic", fork_point)
        write_file(repo, "overlay/forked-driver.txt", "forked broader-source file\n")
        commit_all(repo, "legacy forked topic change")
        git(repo, "switch", "build")

        dry_run = run_import_changes(repo, FROM_REF="legacy-topic")
        require(dry_run.returncode == 0, dry_run.stderr + dry_run.stdout)
        require("merge-base(mainline-source, FROM_REF)" in dry_run.stdout, "fork-point base was not reported")

        result = run_import_changes(repo, FROM_REF="legacy-topic", WRITE="1")
        require(result.returncode == 0, result.stderr + result.stdout)
        require(show(repo, "source/unofficial/current", "overlay/forked-driver.txt") == "forked broader-source file\n", "fork-point import failed")
        missing = git(repo, "show", "source/unofficial/current:mainline-only.txt", check=False)
        require(missing.returncode != 0, "post-fork mainline-source changes leaked into current")
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


def test_failed_apply_without_conflict_markers_pauses_for_manual_resolution() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "mainline-source")
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
    test_import_with_explicit_legacy_base()
    test_import_infers_retained_legacy_branch_base()
    test_import_infers_retained_legacy_fork_point_after_base_moves()
    test_missing_base_is_rejected_when_no_base_can_be_inferred()
    test_empty_diff_is_rejected()
    test_conflict_pauses_without_moving_refs_then_continue_finalises()
    test_failed_apply_without_conflict_markers_pauses_for_manual_resolution()
    test_propagation_updates_checkpoints_and_tags()
    print("import_changes tests passed")


if __name__ == "__main__":
    main()
