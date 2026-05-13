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
    load_function_tests,
    require,
    rev_parse,
    run,
    show,
    switch_orphan,
    write_file,
)
from import_changes import format_apply_output


ROOT = Path(__file__).resolve().parent.parent
SCRIPT_FILES = [
    "check_identity_integrity.py",
    "import_changes.py",
    "import_workflow.py",
    "import_unofficial_commits.py",
    "inspect_import_conflicts.py",
    "reconstruction_common.py",
    "resolve_import_conflicts.py",
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
        commit_all(tmp, f"release branch {release}")
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


def run_import_unofficial(repo: Path, **env: str) -> subprocess.CompletedProcess[str]:
    return run(["python3", "scripts/import_unofficial_commits.py"], repo, check=False, env=env)


def run_inspect_conflicts(repo: Path, **env: str) -> subprocess.CompletedProcess[str]:
    return run(["python3", str(ROOT / "scripts" / "inspect_import_conflicts.py")], repo, check=False, env=env)


def run_resolve_conflicts(repo: Path, **env: str) -> subprocess.CompletedProcess[str]:
    script = str(ROOT / "scripts" / "resolve_import_conflicts.py") if env.get("SCRATCH") else "scripts/resolve_import_conflicts.py"
    return run(["python3", script], repo, check=False, env=env)


def editor_script(repo: Path, name: str, body: str) -> Path:
    path = repo / name
    path.write_text(body, encoding="utf-8")
    path.chmod(0o755)
    return path


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


def make_materialised_topic_with_message(repo: Path, name: str, message_args: list[str]) -> str:
    git(repo, "switch", "-c", name, "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial")
    write_file(repo, "firmware.txt", f"from {name}\n")
    git(repo, "add", "firmware.txt")
    git(repo, "commit", *message_args)
    git(repo, "switch", "build")
    return name


def test_git_apply_trailing_whitespace_output_quotes_line_content() -> None:
    output = format_apply_output(
        "scratch/change.patch:15981: trailing whitespace.\n"
        "/** @file\n"
        "scratch/change.patch:15982: trailing whitespace.\n"
        "*   \n",
        limit=10,
    )
    require(output[0].endswith("warning: trailing whitespace in patch input"), "warning was not labelled")
    require(output[1] == "line content: '/** @file'", f"unexpected quoted line content: {output[1]}")
    require(output[3] == "line content: '*   '", f"unexpected whitespace-preserving line content: {output[3]}")


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


def add_materialised_typechange_fixture(repo: Path) -> None:
    for ref in (
        "source/unofficial/current",
        "source/unofficial/edk2-stable202602",
        "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial",
    ):
        git(repo, "switch", ref)
        write_file(repo, "src/component/typechange.h", "base\n")
        overlay = repo / "custom/overlay/component/typechange.h"
        if overlay.exists() or overlay.is_symlink():
            overlay.unlink()
        symlink(repo, "../../../src/component/typechange.h", "custom/overlay/component/typechange.h")
        commit_all(repo, f"{ref} typechange fixture")
    git(repo, "switch", "source/unofficial/edk2-stable202208")
    write_file(repo, "src/component/typechange.h", "base\n")
    commit_all(repo, "source/unofficial/edk2-stable202208 typechange source-only fixture")
    git(repo, "switch", "build")


def make_materialised_typechange_topic(repo: Path) -> str:
    git(repo, "switch", "-c", "materialised-typechange-topic", "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial")
    overlay = repo / "custom/overlay/component/typechange.h"
    overlay.unlink()
    write_file(repo, "custom/overlay/component/typechange.h", "base\nextra\n")
    commit_all(repo, "materialised typechange overlay")
    git(repo, "switch", "build")
    return "materialised-typechange-topic"


def add_symlink_file_conflict_fixture(repo: Path) -> None:
    for ref in (
        "source/unofficial/current",
        "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial",
    ):
        git(repo, "switch", ref)
        write_file(repo, "src/component/a.h", "source A\n")
        write_file(repo, "src/component/b.h", "source B\n")
        overlay = repo / "custom/overlay/component/mode.h"
        if overlay.exists() or overlay.is_symlink():
            overlay.unlink()
        symlink(repo, "../../../src/component/a.h", "custom/overlay/component/mode.h")
        commit_all(repo, f"{ref} symlink-file conflict base")

    git(repo, "switch", "source/unofficial/current")
    overlay = repo / "custom/overlay/component/mode.h"
    overlay.unlink()
    symlink(repo, "../../../src/component/b.h", "custom/overlay/component/mode.h")
    commit_all(repo, "current retargets overlay symlink")
    git(repo, "switch", "build")


def make_materialised_symlink_file_topic(repo: Path) -> str:
    git(repo, "switch", "-c", "materialised-symlink-file-topic", "source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial")
    overlay = repo / "custom/overlay/component/mode.h"
    overlay.unlink()
    write_file(repo, "custom/overlay/component/mode.h", "topic regular overlay\n")
    commit_all(repo, "materialise overlay mode change")
    git(repo, "switch", "build")
    return "materialised-symlink-file-topic"


def remove_reject_files(repo: Path) -> None:
    for path in repo.rglob("*.rej"):
        path.unlink()


def test_dry_run_infers_materialised_base_without_moving_refs() -> None:
    repo = make_repo()
    try:
        topic = make_materialised_topic(repo)
        old_current = rev_parse(repo, "source/unofficial/current")
        result = run_import_changes(repo, FROM_REF=topic)
        require(result.returncode == 0, result.stderr + result.stdout)
        require("source/cache/release/custom/edk2-202602/radxa-1.2.1/unofficial" in result.stdout, "dry run did not report inferred cache base")
        require("M\tfirmware.txt" in result.stdout, "dry run did not report changed path")
        require("status legend: M=modified." in result.stdout, "dry run did not explain changed-path status codes")
        require(
            "status legend: M=modified.\n\n  source lifecycle normalise: exact" in result.stdout,
            "dry-run summary should have exactly one blank line after the status legend",
        )
        require(
            "status legend: M=modified.\n\n\n  source lifecycle normalise: exact" not in result.stdout,
            "dry-run summary had more than one blank line after the status legend",
        )
        require("Dry-run succeeded. To apply this change permanently" in result.stdout, "dry run did not print apply guidance")
        require(f"FROM_REF={topic}" in result.stdout and "WRITE=1" in result.stdout, "dry run did not print write command")
        require("BASE_REF=" not in result.stdout, "dry run printed inferred BASE_REF in write command")
        require("commit message source: from-ref" in result.stdout, "dry run did not report inherited message source")
        require("materialised topic change" in result.stdout, "dry run did not print inherited commit message")
        require("make test" in result.stdout, "dry run did not print qualification guidance")
        require("firmware qualification" not in result.stdout, "dry run printed overly broad qualification guidance")
        require(rev_parse(repo, "source/unofficial/current") == old_current, "dry run moved source/unofficial/current")
        operations = repo / ".cache" / "edk2-cix" / "operations" / "import-changes"
        require(not operations.exists() or not any(operations.iterdir()), "dry run left operation state")
    finally:
        shutil.rmtree(repo)


def test_import_inherits_multiline_from_ref_commit_message() -> None:
    repo = make_repo()
    try:
        topic = make_materialised_topic_with_message(repo, "message-topic", ["-m", "topic subject", "-m", "topic body"])
        result = run_import_changes(repo, FROM_REF=topic, WRITE="1")
        require(result.returncode == 0, result.stderr + result.stdout)
        message = git(repo, "log", "-1", "--format=%B", "source/unofficial/current").stdout.rstrip("\n")
        require(message == "topic subject\n\ntopic body", f"unexpected inherited message: {message!r}")
    finally:
        shutil.rmtree(repo)


def test_import_commit_message_literal_newlines_use_m_parameters() -> None:
    repo = make_repo()
    try:
        topic = make_materialised_topic(repo, name="literal-message-topic")
        result = run_import_changes(repo, FROM_REF=topic, COMMIT_MESSAGE=r"explicit subject\nexplicit body", WRITE="1")
        require(result.returncode == 0, result.stderr + result.stdout)
        message = git(repo, "log", "-1", "--format=%B", "source/unofficial/current").stdout.rstrip("\n")
        require(message == "explicit subject\n\nexplicit body", f"unexpected explicit message: {message!r}")
    finally:
        shutil.rmtree(repo)


def test_import_commit_message_file_and_signoff() -> None:
    repo = make_repo()
    try:
        topic = make_materialised_topic(repo, name="file-message-topic")
        message_file = repo / "message.txt"
        message_file.write_text("file subject\n\nfile body line 1\nfile body line 2\n", encoding="utf-8")
        result = run_import_changes(repo, FROM_REF=topic, COMMIT_MESSAGE_FILE="message.txt", SIGNOFF="1", WRITE="1")
        require(result.returncode == 0, result.stderr + result.stdout)
        message = git(repo, "log", "-1", "--format=%B", "source/unofficial/current").stdout.rstrip("\n")
        require(message.startswith("file subject\n\nfile body line 1\nfile body line 2"), f"unexpected file message: {message!r}")
        require("Signed-off-by: Import Test <import-test>" in message, f"missing signoff: {message!r}")
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


def test_import_preserves_crlf_patch_context() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "source/unofficial/current")
        (repo / "crlf-source.c").write_bytes(b"one\r\ntwo\r\nthree\r\n")
        commit_all(repo, "add crlf source")
        git(repo, "switch", "-c", "crlf-topic")
        (repo / "crlf-source.c").write_bytes(b"one\r\ntwo\r\ninserted\r\nthree\r\n")
        commit_all(repo, "modify crlf source")
        git(repo, "switch", "build")

        result = run_import_changes(repo, FROM_REF="crlf-topic", WRITE="1")
        require(result.returncode == 0, result.stderr + result.stdout)
        content = git(repo, "cat-file", "-p", "source/unofficial/current:crlf-source.c").stdout
        require(content == "one\ntwo\ninserted\nthree\n", "CRLF import content not visible through text helper")
        raw = subprocess.run(
            ["git", "-C", str(repo), "cat-file", "-p", "source/unofficial/current:crlf-source.c"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        require(raw.stdout == b"one\r\ntwo\r\ninserted\r\nthree\r\n", "CRLF line endings were not preserved")
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
            PROPAGATE_RELEASE_BRANCHES="all",
            UPDATE_RELEASE_TAGS="1",
            WRITE="1",
        )
        require(result.returncode == 0, result.stderr + result.stdout)
        require(
            show(repo, "source/unofficial/current", "custom/overlay/component/new.c") == "../../../src/component/new.c",
            "current did not receive materialised overlay change",
        )
        require(
            show(repo, "source/unofficial/edk2-stable202208", "custom/overlay/component/old.c") == "../../../src/component/old.c",
            "older release branch did not receive normalised overlay path",
        )
        missing = git(repo, "show", "source/unofficial/edk2-stable202208:custom/overlay/component/new.c", check=False)
        require(missing.returncode != 0, "older release branch kept the unnormalised overlay path")
    finally:
        shutil.rmtree(repo)


def test_import_changes_can_propagate_after_current_already_applied() -> None:
    repo = make_repo()
    try:
        topic = make_materialised_topic(repo)
        old_current = rev_parse(repo, "source/unofficial/current")

        current_only = run_import_changes(repo, FROM_REF=topic, WRITE="1")
        require(current_only.returncode == 0, current_only.stderr + current_only.stdout)
        updated_current = rev_parse(repo, "source/unofficial/current")
        require(updated_current != old_current, "current-only import did not move current")

        propagated = run_import_changes(
            repo,
            FROM_REF="source/unofficial/current",
            BASE_REF=old_current,
            PROPAGATE_RELEASE_BRANCHES="all",
            UPDATE_RELEASE_TAGS="1",
            WRITE="1",
        )
        require(propagated.returncode == 0, propagated.stderr + propagated.stdout)
        require(rev_parse(repo, "source/unofficial/current") == updated_current, "already-applied current target moved")
        for release in ("202208", "202602"):
            branch = f"source/unofficial/edk2-stable{release}"
            tag = f"source/unofficial/edk2/stable-{release}"
            require(show(repo, branch, "firmware.txt") == "from materialised\n", f"{branch} did not receive propagated change")
            require(rev_parse(repo, branch) == rev_parse(repo, tag), f"{tag} did not move with {branch}")
    finally:
        shutil.rmtree(repo)


def test_import_changes_drops_mirror_when_source_is_absent_from_release_branch() -> None:
    repo = make_repo()
    try:
        add_materialised_drop_fixture(repo)
        topic = make_materialised_drop_topic(repo)
        old_release_branch = rev_parse(repo, "source/unofficial/edk2-stable202208")
        result = run_import_changes(
            repo,
            FROM_REF=topic,
            PROPAGATE_RELEASE_BRANCHES="all",
            UPDATE_RELEASE_TAGS="1",
            WRITE="1",
        )
        require(result.returncode == 0, result.stderr + result.stdout)
        missing = git(repo, "show", "source/unofficial/edk2-stable202208:custom/overlay/component/later-only.c", check=False)
        require(missing.returncode != 0, "older release branch kept mirror for missing source path")
        require(rev_parse(repo, "source/unofficial/edk2-stable202208") == old_release_branch, "unchanged older release branch moved")
    finally:
        shutil.rmtree(repo)


def test_import_changes_accepts_clean_reject_fallback_typechange() -> None:
    repo = make_repo()
    try:
        add_materialised_typechange_fixture(repo)
        topic = make_materialised_typechange_topic(repo)
        result = run_import_changes(
            repo,
            FROM_REF=topic,
            PROPAGATE_RELEASE_BRANCHES="all",
            UPDATE_RELEASE_TAGS="1",
            WRITE="1",
        )
        require(result.returncode == 0, result.stderr + result.stdout)
        for ref in (
            "source/unofficial/current",
            "source/unofficial/edk2-stable202208",
            "source/unofficial/edk2-stable202602",
        ):
            path = "custom/overlay/component/typechange.h"
            require(show(repo, ref, path) == "base\nextra\n", f"{ref} did not receive materialised overlay")
            mode = git(repo, "ls-tree", ref, "--", path).stdout.split()[0]
            require(mode == "100644", f"{ref} kept symlink mode for {path}: {mode}")
        for release in ("202208", "202602"):
            branch = f"source/unofficial/edk2-stable{release}"
            tag = f"source/unofficial/edk2/stable-{release}"
            require(rev_parse(repo, branch) == rev_parse(repo, tag), f"{tag} did not move with {branch}")
    finally:
        shutil.rmtree(repo)


def test_import_with_explicit_legacy_base() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "-c", "legacy-topic", "legacy-base")
        write_file(repo, "overlay/new-driver.txt", "new broader-source file\n")
        commit_all(repo, "legacy topic change")
        git(repo, "switch", "build")

        dry_run = run_import_changes(repo, FROM_REF="legacy-topic", BASE_REF="legacy-base")
        require(dry_run.returncode == 0, dry_run.stderr + dry_run.stdout)
        require("BASE_REF=legacy-base" in dry_run.stdout, "explicit BASE_REF was not preserved in dry-run write command")

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
        require("BASE_REF is the patch-extraction base, not the destination branch" in dry_run.stdout, "dry-run did not explain non-target base")

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


def test_legacy_propagate_checkpoints_is_rejected() -> None:
    repo = make_repo()
    try:
        topic = make_materialised_topic(repo)
        result = run_import_changes(
            repo,
            FROM_REF=topic,
            PROPAGATE_CHECKPOINTS="all",
            UPDATE_RELEASE_TAGS="1",
        )
        require(result.returncode != 0, "legacy propagation variable should be rejected")
        require("PROPAGATE_CHECKPOINTS was renamed to PROPAGATE_RELEASE_BRANCHES" in result.stderr, result.stderr)
    finally:
        shutil.rmtree(repo)


def test_already_integrated_source_ref_does_not_infer_legacy_base() -> None:
    repo = make_repo()
    try:
        switch_orphan(repo, "legacy-root")
        write_file(repo, "legacy.txt", "root\n")
        commit_all(repo, "legacy root")
        git(repo, "switch", "-c", "legacy/ecc-dxe-fixes")
        write_file(repo, "legacy.txt", "ecc base\n")
        commit_all(repo, "ecc base")
        git(repo, "switch", "-c", "legacy/custom-certain-firmware-fixes")
        write_file(repo, "legacy.txt", "custom aggregate\n")
        commit_all(repo, "custom aggregate")
        aggregate_oid = rev_parse(repo, "legacy/custom-certain-firmware-fixes")
        git(repo, "switch", "-C", "source/unofficial/edk2-stable202208", aggregate_oid)
        write_file(repo, "release.txt", "release already has aggregate\n")
        commit_all(repo, "release contains aggregate")
        git(repo, "switch", "build")

        result = run_import_changes(repo, FROM_REF="legacy/custom-certain-firmware-fixes")
        require(result.returncode != 0, "already-integrated branch should not infer a legacy base")
        require("already contained by retained source/unofficial ref" in result.stderr, result.stderr)
        require("legacy/ecc-dxe-fixes" not in result.stderr, "rejected import should not select the legacy branch base")
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
        require("Dry-run still attempts every target" not in result.stderr, "conflict output kept obsolete dry-run commentary")
        require("Scratch trees have been kept for conflict resolution under:" in result.stderr, result.stderr)
        require("\n\nConflicting target(s):\n\n  - " in result.stderr, "conflicting targets should be visually separated")
        require("For mode conflicts involving symlinks" not in result.stderr, "conflict output used Git jargon")
        require(
            "For conflicts where one side is a symlink and the other is a regular file" in result.stderr,
            "conflict output did not describe symlink/file conflicts plainly",
        )
        require("inspect-import-conflicts OP_ID=" in result.stderr, result.stderr)
        require("\n\nRemove .rej files" in result.stderr, "inspect command should be separated from following guidance")
        abort_index = result.stderr.rfind("make import-changes ABORT=1 OP_ID=")
        require(abort_index != -1, result.stderr)
        require(result.stderr[abort_index:].endswith("\n\n"), "abort command should be followed by a blank line")
        require("Dry-run succeeded" not in result.stdout + result.stderr, "conflicting dry-run reported success")
        require("conflicting file(s):" in result.stderr, result.stderr)
        require("firmware.txt" in result.stderr, result.stderr)
        require("CONTINUE=1 OP_ID=" in result.stderr, result.stderr)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "dry-run conflict moved current")
        operations = repo / ".cache" / "edk2-cix" / "operations" / "import-changes"
        op_dir = next(operations.iterdir())
        short_op_id = op_dir.name.split("-", 1)[0]
        scratch = conflicted_scratch(op_dir)
        shortcut = repo / short_op_id
        require(shortcut.is_symlink(), "dry-run conflict did not create a root-level scratch shortcut")
        require(shortcut.resolve() == scratch.resolve(), "scratch shortcut does not point at the conflicted scratch tree")
        require(f"shortcut: {short_op_id}" in result.stderr, result.stderr)
        require(f"git -C {short_op_id} status" in result.stderr, result.stderr)
        require(f"git -C {short_op_id} diff --name-only --diff-filter=U" in result.stderr, result.stderr)
        write_file(scratch, "firmware.txt", "resolved from dry-run\n")
        git(scratch, "add", "firmware.txt")

        prepared = run_import_changes(repo, CONTINUE="1", OP_ID=short_op_id)
        require(prepared.returncode == 0, prepared.stderr + prepared.stdout)
        require("import candidates are ready" in prepared.stdout, prepared.stdout)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "continue without WRITE moved current")

        finalised = run_import_changes(repo, CONTINUE="1", OP_ID=short_op_id, WRITE="1")
        require(finalised.returncode == 0, finalised.stderr + finalised.stdout)
        require(show(repo, "source/unofficial/current", "firmware.txt") == "resolved from dry-run\n", "dry-run resolution was not finalised")
        require(not shortcut.exists() and not shortcut.is_symlink(), "scratch shortcut was not removed after finalising")
    finally:
        shutil.rmtree(repo)


def test_symlink_file_conflict_reports_expanded_context() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "-c", "symlink-conflict-base", "source/unofficial/current")
        write_file(repo, "src/component/a.h", "source A\n")
        write_file(repo, "src/component/b.h", "source B\n")
        symlink(repo, "../../../src/component/a.h", "custom/overlay/component/mode.h")
        commit_all(repo, "symlink conflict base")

        git(repo, "switch", "-c", "symlink-conflict-ours")
        (repo / "custom/overlay/component/mode.h").unlink()
        symlink(repo, "../../../src/component/b.h", "custom/overlay/component/mode.h")
        commit_all(repo, "retarget overlay symlink")

        git(repo, "switch", "-c", "symlink-conflict-theirs", "symlink-conflict-base")
        (repo / "custom/overlay/component/mode.h").unlink()
        write_file(repo, "custom/overlay/component/mode.h", "topic regular overlay\n")
        commit_all(repo, "materialise overlay mode change")

        git(repo, "switch", "symlink-conflict-ours")
        merge = git(repo, "merge", "symlink-conflict-theirs", check=False)
        require(merge.returncode != 0, "merge should create a symlink/file conflict")

        report_path = repo / "conflict-report.txt"
        inspect = run_inspect_conflicts(repo, SCRATCH=str(repo), REPORT=str(report_path))
        require(inspect.returncode == 0, inspect.stderr + inspect.stdout)
        require("symlink/file conflict" in inspect.stdout, inspect.stdout)
        report = report_path.read_text(encoding="utf-8")
        require("symlink/file conflict" in report, report)
        require("ours (target branch): symlink -> ../../../src/component/b.h" in report, report)
        require("theirs (incoming change): regular file" in report, report)
        require("recorded as: custom/overlay/component/mode.h~symlink-conflict-theirs" in report, report)
        require("vs theirs (incoming change) regular file: different" in report, report)
        expanded = report_path.parent / f"{report_path.stem}-expanded" / "custom_overlay_component_mode.h"
        require((expanded / "ours.symlink-target").read_text(encoding="utf-8") == "source B\n", "ours symlink target was not expanded")
        require((expanded / "theirs").read_text(encoding="utf-8") == "topic regular overlay\n", "theirs regular file was not expanded")
    finally:
        shutil.rmtree(repo)


def test_resolve_conflicts_edits_scratch_only_then_continue_finalises() -> None:
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
        op_dir = next((repo / ".cache" / "edk2-cix" / "operations" / "import-changes").iterdir())
        script = editor_script(
            repo,
            "resolve-editor.sh",
            "#!/usr/bin/env python3\n"
            "from pathlib import Path\n"
            "import sys\n"
            "Path(sys.argv[-1]).write_text('resolved by helper\\n', encoding='utf-8')\n",
        )

        resolved = run_resolve_conflicts(repo, OP_ID=op_dir.name, CONFLICT_EDITOR=str(script))
        require(resolved.returncode == 0, resolved.stderr + resolved.stdout)
        require("No source refs or tags were moved." in resolved.stdout, resolved.stdout)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "resolver moved current ref")

        prepared = run_import_changes(repo, CONTINUE="1", OP_ID=op_dir.name)
        require(prepared.returncode == 0, prepared.stderr + prepared.stdout)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "continue without WRITE moved current")

        finalised = run_import_changes(repo, CONTINUE="1", OP_ID=op_dir.name, WRITE="1")
        require(finalised.returncode == 0, finalised.stderr + finalised.stdout)
        require(show(repo, "source/unofficial/current", "firmware.txt") == "resolved by helper\n", "helper resolution was not finalised")
        require(script.exists(), "editor fixture should remain unrelated to import state")
    finally:
        shutil.rmtree(repo)


def test_resolve_conflicts_preserves_matching_symlink_resolution() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "-c", "symlink-conflict-base", "source/unofficial/current")
        write_file(repo, "src/component/a.h", "source A\n")
        write_file(repo, "src/component/b.h", "source B\n")
        symlink(repo, "../../../src/component/a.h", "custom/overlay/component/mode.h")
        commit_all(repo, "symlink conflict base")

        git(repo, "switch", "-c", "symlink-conflict-ours")
        (repo / "custom/overlay/component/mode.h").unlink()
        symlink(repo, "../../../src/component/b.h", "custom/overlay/component/mode.h")
        commit_all(repo, "retarget overlay symlink")

        git(repo, "switch", "-c", "symlink-conflict-theirs", "symlink-conflict-base")
        (repo / "custom/overlay/component/mode.h").unlink()
        write_file(repo, "custom/overlay/component/mode.h", "topic regular overlay\n")
        commit_all(repo, "materialise overlay mode change")

        git(repo, "switch", "symlink-conflict-ours")
        merge = git(repo, "merge", "symlink-conflict-theirs", check=False)
        require(merge.returncode != 0, "merge should create a symlink/file conflict")
        script = editor_script(repo, "noop-editor.sh", "#!/bin/sh\nexit 0\n")

        resolved = run_resolve_conflicts(repo, SCRATCH=str(repo), CONFLICT_EDITOR=str(script))
        require(resolved.returncode == 0, resolved.stderr + resolved.stdout)
        require(git(repo, "diff", "--name-only", "--diff-filter=U").stdout.strip() == "", "conflict remains unresolved")
        overlay = repo / "custom/overlay/component/mode.h"
        require(overlay.is_symlink(), "matching symlink target content should preserve the symlink")
        require(os.readlink(overlay) == "../../../src/component/b.h", "preserved symlink target changed")
    finally:
        shutil.rmtree(repo)


def test_continue_rejects_changed_operation_options() -> None:
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
        op_dir = next((repo / ".cache" / "edk2-cix" / "operations" / "import-changes").iterdir())

        changed_continue = run_import_changes(
            repo,
            CONTINUE="1",
            OP_ID=op_dir.name,
            PROPAGATE_RELEASE_BRANCHES="all",
            UPDATE_RELEASE_TAGS="1",
        )
        require(changed_continue.returncode != 0, "continue with changed options should fail")
        require("would be ignored" in changed_continue.stderr, changed_continue.stderr)
        require("PROPAGATE_RELEASE_BRANCHES='all'" in changed_continue.stderr, changed_continue.stderr)
        require("UPDATE_RELEASE_TAGS='1'" in changed_continue.stderr, changed_continue.stderr)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "changed continue moved current")
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
        remove_reject_files(scratch)
        git(scratch, "add", "legacy-only.txt")
        continued = run_import_changes(repo, CONTINUE="1", OP_ID=op_dir.name, WRITE="1")
        require(continued.returncode == 0, continued.stderr + continued.stdout)
        require(show(repo, "source/unofficial/current", "legacy-only.txt") == "legacy topic\n", "manual resolution was not finalised")
    finally:
        shutil.rmtree(repo)


def test_dry_run_failed_apply_without_conflict_markers_keeps_rejects() -> None:
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

        result = run_import_changes(repo, FROM_REF="legacy-topic")
        require(result.returncode != 0, "failed dry-run apply should pause")
        require("dry run detected conflicts" in result.stderr, result.stderr)
        require("Dry-run succeeded" not in result.stdout + result.stderr, "failed dry-run apply reported success")
        require("BASE_REF:" in result.stderr, result.stderr)
        require("extracted patch:" in result.stderr, result.stderr)
        require("reject apply output" in result.stderr, result.stderr)
        require("\n\n    symlink-aware conflict report:" in result.stderr, "conflict report should be visually separated")
        require("For mode conflicts involving symlinks" not in result.stderr, "reject output used Git jargon")
        require(rev_parse(repo, "source/unofficial/current") == old_current, "dry-run moved current")

        op_dir = next((repo / ".cache" / "edk2-cix" / "operations" / "import-changes").iterdir())
        scratch = conflicted_scratch(op_dir)
        clean_continue = run_import_changes(repo, CONTINUE="1", OP_ID=op_dir.name)
        require(clean_continue.returncode != 0, "clean conflicted scratch must not become ready")
        require("has no staged or unstaged changes" in clean_continue.stderr, clean_continue.stderr)

        write_file(scratch, "legacy-only.txt", "legacy topic\n")
        write_file(scratch, "legacy-only.txt.rej", "rejected hunk\n")
        git(scratch, "add", "legacy-only.txt")
        rejected_continue = run_import_changes(repo, CONTINUE="1", OP_ID=op_dir.name)
        require(rejected_continue.returncode != 0, "remaining .rej files should block continue")
        require("reject files remain" in rejected_continue.stderr, rejected_continue.stderr)
        remove_reject_files(scratch)
        prepared = run_import_changes(repo, CONTINUE="1", OP_ID=op_dir.name)
        require(prepared.returncode == 0, prepared.stderr + prepared.stdout)
        require("import candidates are ready" in prepared.stdout, prepared.stdout)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "continue without WRITE moved current")

        finalised = run_import_changes(repo, CONTINUE="1", OP_ID=op_dir.name, WRITE="1")
        require(finalised.returncode == 0, finalised.stderr + finalised.stdout)
        require(show(repo, "source/unofficial/current", "legacy-only.txt") == "legacy topic\n", "manual reject resolution was not finalised")
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


def test_import_rejects_legacy_branch_names_in_commit_message_on_write() -> None:
    repo = make_repo()
    try:
        legacy_branch = "main-" + "monorepo"
        topic = make_materialised_topic_with_message(
            repo,
            "legacy-message-topic",
            ["-m", "legacy source message", "-m", f"mentions {legacy_branch}"],
        )
        old_current = rev_parse(repo, "source/unofficial/current")
        result = run_import_changes(repo, FROM_REF=topic, WRITE="1")
        require(result.returncode != 0, "legacy branch name in commit message should fail")
        require("identity integrity check" in result.stderr, result.stderr)
        require(legacy_branch in result.stderr, result.stderr)
        require(rev_parse(repo, "source/unofficial/current") == old_current, "invalid-message import moved current")
    finally:
        shutil.rmtree(repo)


def test_propagation_updates_release_branches_and_tags() -> None:
    repo = make_repo()
    try:
        topic = make_materialised_topic(repo)
        result = run_import_changes(
            repo,
            FROM_REF=topic,
            PROPAGATE_RELEASE_BRANCHES="all",
            UPDATE_RELEASE_TAGS="1",
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


def test_abort_all_removes_all_paused_import_changes_operations() -> None:
    repo = make_repo()
    try:
        git(repo, "switch", "source/unofficial/current")
        write_file(repo, "firmware.txt", "current conflict\n")
        commit_all(repo, "current conflict")
        git(repo, "switch", "build")
        old_current = rev_parse(repo, "source/unofficial/current")
        topic = make_materialised_topic(repo, text="topic conflict\n")

        for op_id in ("first-paused-import", "second-paused-import"):
            result = run_import_changes(repo, FROM_REF=topic, OP_ID=op_id)
            require(result.returncode != 0, f"{op_id} should pause")

        operations = repo / ".cache" / "edk2-cix" / "operations" / "import-changes"
        require(sorted(path.name for path in operations.iterdir()) == ["first-paused-import", "second-paused-import"], "paused operations missing")
        aborted = run_import_changes(repo, ABORT_ALL="1")
        require(aborted.returncode == 0, aborted.stderr + aborted.stdout)
        require("aborted import-changes operation first-paused-import" in aborted.stdout, aborted.stdout)
        require("aborted import-changes operation second-paused-import" in aborted.stdout, aborted.stdout)
        require(not any(operations.iterdir()), "abort-all left paused operations")
        require(rev_parse(repo, "source/unofficial/current") == old_current, "abort-all moved refs")
    finally:
        shutil.rmtree(repo)


def test_numeric_op_id_prefix_must_be_unique() -> None:
    repo = make_repo()
    try:
        operations = repo / ".cache" / "edk2-cix" / "operations" / "import-changes"
        (operations / "123-first").mkdir(parents=True)
        (operations / "123-second").mkdir()
        result = run_import_changes(repo, CONTINUE="1", OP_ID="123")
        require(result.returncode != 0, "ambiguous numeric OP_ID prefix should fail")
        require("multiple paused import-changes operations match OP_ID=123" in result.stderr, result.stderr)
        require("123-first" in result.stderr and "123-second" in result.stderr, result.stderr)
    finally:
        shutil.rmtree(repo)


def test_current_import_can_be_propagated_later_without_base_ref() -> None:
    repo = make_repo()
    try:
        topic = make_materialised_topic(repo)
        import_result = run_import_changes(repo, FROM_REF=topic, WRITE="1")
        require(import_result.returncode == 0, import_result.stderr)

        dry_run = run_import_unofficial(
            repo,
            FROM_REF="source/unofficial/current",
            PROPAGATE_RELEASE_BRANCHES="all",
            ALLOW_SOURCE_REF_FROM="1",
        )
        require(dry_run.returncode == 0, dry_run.stderr)
        require("commits: 1" in dry_run.stdout, dry_run.stdout)
        require("source/unofficial/edk2-stable202208" in dry_run.stdout, dry_run.stdout)

        write_result = run_import_unofficial(
            repo,
            FROM_REF="source/unofficial/current",
            PROPAGATE_RELEASE_BRANCHES="all",
            ALLOW_SOURCE_REF_FROM="1",
            WRITE="1",
        )
        require(write_result.returncode == 0, write_result.stderr)
        require(
            show(repo, "source/unofficial/edk2-stable202208", "firmware.txt") == "from materialised\n",
            "older release branch did not receive propagated current change",
        )
        require(
            show(repo, "source/unofficial/edk2-stable202602", "firmware.txt") == "from materialised\n",
            "newer release branch did not receive propagated current change",
        )
    finally:
        shutil.rmtree(repo)


def main() -> None:
    test_git_apply_trailing_whitespace_output_quotes_line_content()
    test_dry_run_infers_materialised_base_without_moving_refs()
    test_import_inherits_multiline_from_ref_commit_message()
    test_import_commit_message_literal_newlines_use_m_parameters()
    test_import_commit_message_file_and_signoff()
    test_import_from_materialised_topic_creates_commit_on_current()
    test_import_preserves_crlf_patch_context()
    test_import_changes_normalises_overlay_lifecycle_when_propagating()
    test_import_changes_can_propagate_after_current_already_applied()
    test_import_changes_drops_mirror_when_source_is_absent_from_release_branch()
    test_import_changes_accepts_clean_reject_fallback_typechange()
    test_import_with_explicit_legacy_base()
    test_import_infers_retained_legacy_branch_base()
    test_import_infers_retained_legacy_fork_point_after_base_moves()
    test_missing_base_is_rejected_when_no_base_can_be_inferred()
    test_empty_diff_is_rejected()
    test_legacy_propagate_checkpoints_is_rejected()
    test_already_integrated_source_ref_does_not_infer_legacy_base()
    test_conflict_pauses_without_moving_refs_then_continue_finalises()
    test_dry_run_conflict_reports_paths_without_moving_refs()
    test_symlink_file_conflict_reports_expanded_context()
    test_resolve_conflicts_edits_scratch_only_then_continue_finalises()
    test_resolve_conflicts_preserves_matching_symlink_resolution()
    test_continue_rejects_changed_operation_options()
    test_failed_apply_without_conflict_markers_pauses_for_manual_resolution()
    test_dry_run_failed_apply_without_conflict_markers_keeps_rejects()
    test_import_rejects_identical_overlay_copy()
    test_import_rejects_legacy_branch_names_in_commit_message_on_write()
    test_propagation_updates_release_branches_and_tags()
    test_abort_all_removes_all_paused_import_changes_operations()
    test_numeric_op_id_prefix_must_be_unique()
    test_current_import_can_be_propagated_later_without_base_ref()
    print("import_changes tests passed")


def load_tests(loader, tests, pattern):
    return load_function_tests(globals())


if __name__ == "__main__":
    main()
