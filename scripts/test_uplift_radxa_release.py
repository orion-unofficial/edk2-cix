#!/usr/bin/env python3
"""Regression tests for adjacent Radxa release uplift orchestration."""

from __future__ import annotations

import json
import shutil
import tempfile
from pathlib import Path

from test_support import commit_all, git, load_function_tests, require, rev_parse, switch_orphan, write_file
from import_workflow import ZERO_OID, transaction_update_refs
from reconstruction_common import (
    active_unofficial_source_ref,
    branch_to_ref,
    check_immutable_refs,
    ref_exists,
    selected_unofficial_current_ref,
    source_target_ref_records,
    synthesise_release_entry,
    tree_id,
)
from source_porting import (
    apply_source_delta_to_base,
    git_blob_bytes,
    normalise_source_tree,
    normalise_overlay_tree,
    overlay_paths_from_source,
    resolved_source_port_stage,
)
from uplift_radxa_release import (
    align_release_metadata,
    ensure_checkpoint_record,
    release_line,
    resolve_from_unofficial_ref,
    run_script,
    unofficial_candidate,
    update_policy,
    write_uplift_report,
)


def create_branch(repo: Path, ref: str, files: dict[str, str], message: str) -> str:
    switch_orphan(repo, ref)
    for path, content in files.items():
        write_file(repo, path, content)
    return commit_all(repo, message)


def make_repo() -> Path:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-radxa-uplift-test."))
    git(repo, "init", "-b", "build")
    git(repo, "config", "user.name", "Radxa Uplift Test")
    git(repo, "config", "user.email", "radxa-uplift-test")
    write_file(repo, "README.md", "build branch\n")
    commit_all(repo, "build root")
    return repo


def test_overlap_report_separates_policy_owned_paths() -> None:
    repo = make_repo()
    try:
        old_vendor = create_branch(
            repo,
            "old-vendor",
            {".github/workflow.yml": "vendor old\n", "src/firmware.c": "old\n"},
            "old vendor",
        )
        new_vendor = create_branch(
            repo,
            "new-vendor",
            {".github/workflow.yml": "vendor new\n", "src/firmware.c": "new\n"},
            "new vendor",
        )
        old_port = create_branch(
            repo,
            "old-port",
            {".github/workflow.yml": "vendor old\n", "src/firmware.c": "old\n"},
            "old port",
        )
        unofficial = create_branch(
            repo,
            "unofficial",
            {".github/workflow.yml": "project\n", "src/firmware.c": "project\n"},
            "unofficial",
        )
        git(repo, "switch", "build")

        report_path = write_uplift_report(
            repo,
            from_release="1.2.1",
            to_release="1.2.2",
            edk2_base="edk2-stable202605",
            old_vendor=old_vendor,
            new_vendor=new_vendor,
            old_port=old_port,
            from_unofficial_ref=unofficial,
        )
        report = json.loads(report_path.read_text(encoding="utf-8"))
        require(
            report["policy_owned_overlap_paths"] == [".github/workflow.yml"],
            "project-owned overlap was not classified by the shared build-infrastructure policy",
        )
        require(
            report["review_overlap_paths"] == ["src/firmware.c"],
            "firmware overlap should remain visible for merge review",
        )
    finally:
        shutil.rmtree(repo)


def test_policy_overlay_batch_restores_files_directories_and_deletions() -> None:
    repo = make_repo()
    try:
        source = create_branch(
            repo,
            "source",
            {
                "policy/one.txt": "source one\n",
                "policy/nested/two.txt": "source two\n",
                "single.txt": "source single\n",
            },
            "source",
        )
        candidate = create_branch(
            repo,
            "candidate",
            {
                "policy/one.txt": "candidate one\n",
                "policy/extra.txt": "candidate extra\n",
                "single.txt": "candidate single\n",
            },
            "candidate",
        )
        git(repo, "switch", "build")

        tree = overlay_paths_from_source(
            repo,
            tree=tree_id(repo, candidate),
            source_ref=source,
            paths=("policy", "single.txt", "absent.txt"),
            label="batched-policy-overlay",
            verbose=False,
        )

        require(
            git(repo, "show", f"{tree}:policy/one.txt").stdout == "source one\n",
            "directory overlay did not restore a source file",
        )
        require(
            git(repo, "show", f"{tree}:policy/nested/two.txt").stdout
            == "source two\n",
            "directory overlay did not restore a nested source file",
        )
        require(
            git(repo, "show", f"{tree}:single.txt").stdout == "source single\n",
            "file overlay did not restore the source file",
        )
        require(
            git(
                repo,
                "cat-file",
                "-e",
                f"{tree}:policy/extra.txt",
                check=False,
            ).returncode
            != 0,
            "directory overlay retained a candidate-only file",
        )
    finally:
        shutil.rmtree(repo)


def test_selected_source_paths_are_normalised_without_a_checkout() -> None:
    repo = make_repo()
    try:
        switch_orphan(repo, "candidate")
        selected = repo / "selected.txt"
        selected.write_bytes(b"selected \r\n")
        executable = repo / "executable.txt"
        executable.write_bytes(b"not a script\r\n")
        executable.chmod(0o755)
        untouched = repo / "untouched.txt"
        untouched.write_bytes(b"untouched\r\n")
        binary = repo / "firmware.bin"
        binary.write_bytes(b"binary\r\n\0payload")
        candidate = commit_all(repo, "candidate source")
        git(repo, "switch", "build")

        tree, result = normalise_source_tree(
            repo,
            tree=tree_id(repo, candidate),
            label="selected-index-normalisation",
            verbose=False,
            paths=("selected.txt", "executable.txt", "firmware.bin"),
            include_worktree_drift=False,
        )

        require(result.line_endings == 2, "selected CRLF files were not counted")
        require(result.trailing_whitespace == 1, "selected trailing whitespace was not counted")
        require(result.file_modes == 1, "selected non-script executable mode was not counted")
        require(
            git_blob_bytes(repo, git(repo, "rev-parse", f"{tree}:selected.txt").stdout.strip())
            == b"selected\n",
            "selected text was not canonicalised",
        )
        executable_entry = git(repo, "ls-tree", tree, "executable.txt").stdout
        require(executable_entry.startswith("100644 blob "), "executable mode was not canonicalised")
        require(
            git_blob_bytes(repo, git(repo, "rev-parse", f"{tree}:untouched.txt").stdout.strip())
            == b"untouched\r\n",
            "unselected text was changed",
        )
        require(
            git_blob_bytes(repo, git(repo, "rev-parse", f"{tree}:firmware.bin").stdout.strip())
            == b"binary\r\n\0payload",
            "selected binary data was changed",
        )
    finally:
        shutil.rmtree(repo)


def test_update_policy_migrates_legacy_current_to_line_records() -> None:
    repo = make_repo()
    try:
        write_file(
            repo,
            "config/policies.json",
            json.dumps(
                {
                    "unofficial_source_policy": {
                        "current_cix_release": "1.2",
                        "current_edk2_release": "202605",
                        "current_radxa_release": "1.2.1",
                        "current_ref": "source/unofficial/current",
                        "prefer_versioned_default_alias": True,
                    }
                }
            )
            + "\n",
        )
        update_policy(
            repo,
            line="1.2",
            line_ref="source/unofficial/1.2/current",
            radxa_release="1.2.4",
            edk2_base="edk2-stable202605",
            cix_release="1.2",
            make_default=False,
        )
        update_policy(
            repo,
            line="1.3",
            line_ref="source/unofficial/1.3/current",
            radxa_release="1.3.1",
            edk2_base="edk2-stable202605",
            cix_release="1.2",
            make_default=True,
        )

        policy = json.loads((repo / "config/policies.json").read_text(encoding="utf-8"))[
            "unofficial_source_policy"
        ]
        require(policy["default_line"] == "1.3", "requested default line was not selected")
        require(
            policy["lines"]["1.2"]["current_radxa_release"] == "1.2.4",
            "updating line 1.3 overwrote the independent line 1.2 tip",
        )
        require(
            policy["lines"]["1.3"]["current_ref"] == "source/unofficial/1.3/current",
            "line 1.3 did not receive its explicit current ref",
        )
    finally:
        shutil.rmtree(repo)


def test_resolved_unofficial_tree_gets_canonical_provenance_commit() -> None:
    repo = make_repo()
    try:
        old_port_ref = "source/port/radxa/1.2.1/edk2-stable202605"
        old_port = create_branch(repo, old_port_ref, {"src/firmware.c": "old\n"}, "old port")
        new_port = create_branch(repo, "new-port", {"src/firmware.c": "new\n"}, "new port")
        resolved = create_branch(repo, "resolved", {"src/firmware.c": "resolved\n"}, "resolved")
        git(repo, "switch", "build")

        candidate = unofficial_candidate(
            repo,
            from_release="1.2.1",
            to_release="1.2.2",
            edk2_base="edk2-stable202605",
            from_unofficial_ref=old_port,
            new_port_ref=new_port,
            resolved_ref=resolved,
            verbose=False,
        )
        require(
            git(repo, "rev-parse", f"{candidate}^{{tree}}").stdout.strip()
            == git(repo, "rev-parse", f"{resolved}^{{tree}}").stdout.strip(),
            "resolved unofficial tree changed while recording provenance",
        )
        message = git(repo, "show", "-s", "--format=%B", candidate).stdout
        require(
            f"Source-Port-To: {new_port}" in message,
            "canonical checkpoint provenance did not record the actual new port",
        )
    finally:
        shutil.rmtree(repo)


def test_final_unofficial_candidate_is_promoted_without_rewriting() -> None:
    repo = make_repo()
    try:
        create_branch(
            repo,
            "source/port/radxa/1.2.1/edk2-stable202605",
            {"src/firmware.c": "old\n"},
            "old port",
        )
        new_port = create_branch(
            repo,
            "new-port",
            {"src/firmware.c": "new\n"},
            "new port",
        )
        final = create_branch(
            repo,
            "tested-final",
            {"src/firmware.c": "tested\n"},
            "tested final",
        )
        git(repo, "switch", "build")

        candidate = unofficial_candidate(
            repo,
            from_release="1.2.1",
            to_release="1.2.2",
            edk2_base="edk2-stable202605",
            from_unofficial_ref="source/port/radxa/1.2.1/edk2-stable202605",
            new_port_ref=new_port,
            resolved_ref=final,
            resolved_stage="final",
            verbose=False,
        )
        require(candidate == final, "tested final candidate was rewritten during promotion")
    finally:
        shutil.rmtree(repo)


def test_release_metadata_comes_from_new_radxa_port() -> None:
    repo = make_repo()
    try:
        new_changelog = (
            "edk2-cix (1.3.1) main; urgency=medium\n\n"
            "  * Radxa 1.3.1\n\n"
            " -- Radxa  Thu, 16 Jul 2026 11:43:01 +0000\n"
        )
        new_port = create_branch(
            repo,
            "new-port",
            {"debian/changelog": new_changelog, "src/firmware.c": "vendor\n"},
            "new port",
        )
        candidate = create_branch(
            repo,
            "candidate",
            {
                "VERSION": "1.2.1\n",
                "debian/changelog": "edk2-cix (1.2.1) main; urgency=medium\n",
                "src/firmware.c": "project\n",
            },
            "unofficial candidate",
        )
        git(repo, "switch", "build")

        aligned = align_release_metadata(
            repo,
            candidate=candidate,
            new_port_ref=new_port,
            to_release="1.3.1",
            verbose=False,
        )
        require(git(repo, "show", f"{aligned}:VERSION").stdout == "1.3.1\n", "VERSION was stale")
        require(
            git(repo, "show", f"{aligned}:debian/changelog").stdout == new_changelog,
            "new Radxa changelog was not preserved exactly",
        )
        require(
            git(repo, "show", f"{aligned}:src/firmware.c").stdout == "project\n",
            "release metadata alignment replaced unrelated unofficial source",
        )
        require(
            "Source-Release-Metadata: Radxa 1.3.1"
            in git(repo, "show", "-s", "--format=%B", aligned).stdout,
            "release metadata provenance was not recorded",
        )
    finally:
        shutil.rmtree(repo)


def test_source_stage_resume_flags_changed_overlay_when_vendor_source_is_unchanged() -> None:
    repo = make_repo()
    try:
        unofficial = create_branch(
            repo,
            "unofficial",
            {
                "src/component/file.c": "vendor\n",
                "custom/overlay/component/file.c": "custom policy\n",
            },
            "unofficial",
        )
        switch_orphan(repo, "resolved")
        write_file(repo, "src/component/file.c", "vendor\n")
        lost_overlay = repo / "custom/overlay/component/file.c"
        lost_overlay.parent.mkdir(parents=True, exist_ok=True)
        lost_overlay.symlink_to("../../../src/component/file.c")
        resolved = commit_all(repo, "resolved source conflicts with lost overlay")
        git(repo, "switch", "build")

        _tree, conflicts, detail = normalise_overlay_tree(
            repo,
            tree=tree_id(repo, resolved),
            source_ref=unofficial,
            label="unchanged-source-overlay-state",
            verbose=False,
        )
        require(
            conflicts == {"custom/overlay/component/file.c"},
            "changed overlay state over unchanged source was not flagged for review",
        )
        require(
            "intentionally absorbed by related source changes" in detail,
            "overlay-state conflict did not explain the cross-file decision",
        )
    finally:
        shutil.rmtree(repo)


def test_unchanged_source_accepts_canonical_overlay_normalisation() -> None:
    repo = make_repo()
    try:
        switch_orphan(repo, "unofficial")
        write_file(repo, "src/component/file.c", "vendor\n")
        overlay = repo / "custom/overlay/component/file.c"
        overlay.parent.mkdir(parents=True, exist_ok=True)
        overlay.write_bytes(b"custom policy\r\n")
        unofficial = commit_all(repo, "unofficial with noncanonical overlay")
        git(repo, "switch", "build")

        tree, conflicts, detail = normalise_overlay_tree(
            repo,
            tree=tree_id(repo, unofficial),
            source_ref=unofficial,
            label="unchanged-source-canonical-overlay",
            verbose=False,
        )
        require(not conflicts, f"canonical overlay normalisation was treated as semantic: {detail}")
        result = git(repo, "show", f"{tree}:custom/overlay/component/file.c").stdout
        require(result == "custom policy\n", "overlay was not canonicalised")
    finally:
        shutil.rmtree(repo)


def test_changed_source_absorbs_matching_overlay() -> None:
    repo = make_repo()
    try:
        unofficial = create_branch(
            repo,
            "unofficial",
            {
                "src/component/file.c": "vendor\n",
                "custom/overlay/component/file.c": "custom policy\n",
            },
            "unofficial with source override",
        )
        switch_orphan(repo, "candidate")
        write_file(repo, "src/component/file.c", "custom policy\n")
        write_file(repo, "custom/overlay/component/file.c", "custom policy\n")
        candidate = commit_all(repo, "upstream absorbed source override")
        git(repo, "switch", "build")

        tree, conflicts, detail = normalise_overlay_tree(
            repo,
            tree=tree_id(repo, candidate),
            source_ref=unofficial,
            label="changed-source-absorbed-overlay",
            verbose=False,
        )
        require(not conflicts, f"absorbed source override was treated as a conflict: {detail}")
        entry = git(
            repo,
            "ls-tree",
            tree,
            "custom/overlay/component/file.c",
        ).stdout.strip()
        require(entry.startswith("120000 blob "), "absorbed overlay was not replaced by a symlink")
        target = git(repo, "show", f"{tree}:custom/overlay/component/file.c").stdout
        require(target == "../../../src/component/file.c", "absorbed overlay symlink target changed")
    finally:
        shutil.rmtree(repo)


def test_source_port_preserves_upstream_crlf_for_non_replayed_source_deltas() -> None:
    repo = make_repo()
    try:
        git(repo, "config", "core.autocrlf", "false")
        switch_orphan(repo, "old-base")
        write_file(repo, "src/changed.txt", "old\n")
        unchanged = repo / "src/unchanged.vcproj"
        unchanged.parent.mkdir(parents=True, exist_ok=True)
        unchanged.write_bytes(b"upstream\r\n")
        absorbed = repo / "src/absorbed.vcproj"
        absorbed.write_bytes(b"old\r\n")
        old_base = commit_all(repo, "old base")

        switch_orphan(repo, "source")
        write_file(repo, "src/changed.txt", "custom\n")
        write_file(repo, "src/unchanged.vcproj", "upstream\n")
        write_file(repo, "src/absorbed.vcproj", "upstreamed\n")
        source = commit_all(repo, "source with historical line-ending drift")

        switch_orphan(repo, "new-base")
        write_file(repo, "src/changed.txt", "old\n")
        unchanged = repo / "src/unchanged.vcproj"
        unchanged.parent.mkdir(parents=True, exist_ok=True)
        unchanged.write_bytes(b"upstream\r\n")
        absorbed = repo / "src/absorbed.vcproj"
        absorbed.write_bytes(b"upstreamed\r\n")
        new_base = commit_all(repo, "new base")
        git(repo, "switch", "build")

        candidate = apply_source_delta_to_base(
            repo,
            old_base_ref=old_base,
            source_ref=source,
            new_base_ref=new_base,
            message="ported source",
            label="preserve-upstream-crlf",
            normalise_source=True,
            verbose=False,
        )
        unchanged_blob = git(repo, "rev-parse", f"{candidate}:src/unchanged.vcproj").stdout.strip()
        absorbed_blob = git(repo, "rev-parse", f"{candidate}:src/absorbed.vcproj").stdout.strip()
        changed_blob = git(repo, "rev-parse", f"{candidate}:src/changed.txt").stdout.strip()
        require(
            git_blob_bytes(repo, unchanged_blob) == b"upstream\r\n",
            "normalisation-only source history changed upstream CRLF bytes",
        )
        require(
            git_blob_bytes(repo, absorbed_blob) == b"upstreamed\r\n",
            "upstream-absorbed source history changed upstream CRLF bytes",
        )
        require(
            git_blob_bytes(repo, changed_blob) == b"custom\n",
            "semantic source change was not replayed",
        )
    finally:
        shutil.rmtree(repo)


def test_resolved_unofficial_stage_detects_overlay_handoff() -> None:
    repo = make_repo()
    try:
        create_branch(
            repo,
            "overlay-conflict",
            {
                "custom/overlay/component/file.c": (
                    "<<<<<<< unofficial overlay\ncustom\n=======\nnew\n>>>>>>> new source\n"
                ),
                "src/component/file.c": "new\n",
            },
            "source-port: conflict tree\n\nSource-Port-Conflict-Stage: overlay",
        )
        git(repo, "switch", "-c", "overlay-resolved")
        write_file(repo, "custom/overlay/component/file.c", "resolved\n")
        overlay_resolved = commit_all(repo, "resolve overlay")
        require(
            resolved_source_port_stage(
                repo,
                overlay_resolved,
                "auto",
                stage_variable="UNOFFICIAL_REF_STAGE",
            )
            == "overlay",
            "overlay handoff stage was not detected from its parent metadata",
        )
        require(
            resolved_source_port_stage(
                repo,
                overlay_resolved,
                "final",
                stage_variable="UNOFFICIAL_REF_STAGE",
            )
            == "final",
            "explicit final-stage override was ignored",
        )

        create_branch(
            repo,
            "legacy-source-conflict",
            {
                "src/component/file.c": (
                    "<<<<<<< new source\nnew\n=======\ncustom\n>>>>>>> unofficial\n"
                )
            },
            "legacy source conflict",
        )
        git(repo, "switch", "-c", "legacy-source-resolved")
        write_file(repo, "src/component/file.c", "resolved\n")
        source_resolved = commit_all(repo, "resolve source")
        require(
            resolved_source_port_stage(
                repo,
                source_resolved,
                "auto",
                stage_variable="UNOFFICIAL_REF_STAGE",
            )
            == "source",
            "legacy source handoff stage was not inferred from marker paths",
        )
    finally:
        shutil.rmtree(repo)


def test_active_line_tip_can_advance_beyond_immutable_checkpoint() -> None:
    repo = make_repo()
    try:
        create_branch(
            repo,
            "source/unofficial/1.2.4/edk2-stable202605",
            {"src/firmware.c": "release\n"},
            "release checkpoint",
        )
        create_branch(
            repo,
            "source/unofficial/1.2/current",
            {"src/firmware.c": "active development\n"},
            "active line",
        )
        git(repo, "switch", "build")
        write_file(
            repo,
            "config/policies.json",
            json.dumps(
                {
                    "unofficial_source_policy": {
                        "default_line": "1.2",
                        "lines": {
                            "1.2": {
                                "current_cix_release": "1.2",
                                "current_edk2_release": "202605",
                                "current_radxa_release": "1.2.4",
                                "current_ref": "source/unofficial/1.2/current",
                            }
                        },
                    }
                }
            )
            + "\n",
        )

        require(
            active_unofficial_source_ref(repo, "1.2.4", "edk2-stable202605")
            == "source/unofficial/1.2/current",
            "active source target did not select the mutable line tip",
        )
        require(
            selected_unofficial_current_ref(repo) == "source/unofficial/1.2/current",
            "default mutable line ref did not follow policy",
        )
    finally:
        shutil.rmtree(repo)


def test_retained_historical_target_is_not_rebound_to_old_checkpoint() -> None:
    repo = make_repo()
    try:
        create_branch(
            repo,
            "source/unofficial/1.2.4/edk2-stable202605",
            {"src/firmware.c": "release checkpoint\n"},
            "release checkpoint",
        )
        current = create_branch(
            repo,
            "source/unofficial/1.2/current",
            {"src/firmware.c": "advanced line tip\n"},
            "advanced line tip",
        )
        old_target = (
            "source/cache/release/custom/edk2-202605/cix-1.2/"
            "radxa-1.2.4/unofficial"
        )
        old_cache = create_branch(
            repo,
            old_target,
            {"src/firmware.c": "old rendered line tip\n"},
            "retained old rendered line tip",
        )
        current_target = (
            "source/cache/release/custom/edk2-202608/cix-1.2/"
            "radxa-1.2.4/unofficial"
        )
        git(repo, "switch", "build")
        write_file(
            repo,
            "config/policies.json",
            json.dumps(
                {
                    "unofficial_source_policy": {
                        "default_line": "1.2",
                        "lines": {
                            "1.2": {
                                "current_cix_release": "1.2",
                                "current_edk2_release": "202608",
                                "current_radxa_release": "1.2.4",
                                "current_ref": "source/unofficial/1.2/current",
                            }
                        },
                    }
                }
            )
            + "\n",
        )
        write_file(
            repo,
            "config/refs-source-target-cache.json",
            json.dumps(
                {
                    "refs": [
                        {"ref": old_target, "tree_id": tree_id(repo, old_cache)},
                        {"ref": current_target, "tree_id": "0" * 40},
                    ]
                }
            )
            + "\n",
        )

        records = source_target_ref_records(repo)
        require(
            records[old_target]["tree_id"] == tree_id(repo, old_cache),
            "inactive retained target was rebound to a different checkpoint",
        )
        require(
            records[current_target]["tree_id"] == tree_id(repo, current),
            "active target did not follow the mutable line tip",
        )
        require(
            synthesise_release_entry(repo, old_target)["tree_id"]
            == tree_id(repo, old_cache),
            "inactive retained render plan ignored its manifested tree",
        )
        require(
            synthesise_release_entry(repo, current_target)["tree_id"]
            == tree_id(repo, current),
            "active render plan ignored its mutable line tip",
        )
    finally:
        shutil.rmtree(repo)


def test_release_line_validation() -> None:
    require(release_line("1.3.1") == "1.3", "release line was derived incorrectly")
    try:
        release_line("next")
    except Exception as exc:
        require("cannot derive" in str(exc), "invalid release did not explain line derivation")
    else:
        raise AssertionError("invalid release unexpectedly produced a line")


def test_explicit_unofficial_source_overrides_exact_checkpoint() -> None:
    repo = make_repo()
    try:
        exact = "source/unofficial/1.2.4/edk2-stable202605"
        explicit = "source/unofficial/1.2/current"
        create_branch(repo, exact, {"firmware.txt": "checkpoint\n"}, "checkpoint")
        create_branch(repo, explicit, {"firmware.txt": "active line\n"}, "active line")
        git(repo, "switch", "build")

        selected = resolve_from_unofficial_ref(
            repo,
            from_release="1.2.4",
            edk2_base="edk2-stable202605",
            line="1.3",
            explicit_ref=explicit,
            policy={},
        )
        require(
            selected == explicit,
            "explicit cross-line source did not override the exact checkpoint",
        )
    finally:
        shutil.rmtree(repo)


def test_delegated_ref_creation_invalidates_parent_metadata_cache() -> None:
    repo = make_repo()
    try:
        script = repo / "scripts" / "create_ref.py"
        write_file(
            repo,
            "scripts/create_ref.py",
            """#!/usr/bin/env python3
import subprocess
subprocess.run(
    ["git", "branch", "source/port/radxa/1.2.2/edk2-stable202605", "HEAD"],
    check=True,
)
""",
        )
        require(
            not ref_exists(repo, "source/port/radxa/1.2.2/edk2-stable202605"),
            "fixture unexpectedly started with the delegated ref",
        )
        run_script(repo, script.name, {}, verbose=False)
        require(
            ref_exists(repo, "source/port/radxa/1.2.2/edk2-stable202605"),
            "parent metadata cache hid a ref created by a delegated integration script",
        )
    finally:
        shutil.rmtree(repo)


def test_atomic_ref_creation_invalidates_parent_metadata_cache() -> None:
    repo = make_repo()
    try:
        ref = "source/unofficial/1.2.1/edk2-stable202605"
        require(not ref_exists(repo, ref), "fixture unexpectedly started with the checkpoint")
        transaction_update_refs(
            repo,
            [(branch_to_ref(ref), rev_parse(repo, "HEAD"), ZERO_OID)],
        )
        require(
            ref_exists(repo, ref),
            "parent metadata cache hid a ref created by an atomic ref transaction",
        )
    finally:
        shutil.rmtree(repo)


def test_generated_cache_refresh_does_not_relax_source_immutability() -> None:
    repo = make_repo()
    try:
        generated_ref = "source/cache/release/custom/test"
        source_ref = "source/vendor/radxa/1.2.1/edk2-stable202208"
        original = create_branch(repo, generated_ref, {"generated.txt": "old\n"}, "old cache")
        source = create_branch(repo, source_ref, {"source.txt": "source\n"}, "source")
        original_tree = git(repo, "rev-parse", f"{original}^{{tree}}").stdout.strip()
        source_tree = git(repo, "rev-parse", f"{source}^{{tree}}").stdout.strip()
        git(repo, "switch", generated_ref)
        write_file(repo, "generated.txt", "new\n")
        commit_all(repo, "new cache")
        git(repo, "switch", "build")
        write_file(
            repo,
            "config/refs-test.json",
            json.dumps(
                {
                    "refs": [
                        {
                            "immutable": True,
                            "object_id": original,
                            "ref": generated_ref,
                            "tree_id": original_tree,
                            "type": "rendered-release",
                        },
                        {
                            "immutable": True,
                            "object_id": source,
                            "ref": source_ref,
                            "tree_id": source_tree,
                            "type": "vendor-source",
                        },
                    ]
                }
            )
            + "\n",
        )

        try:
            check_immutable_refs(repo)
        except Exception as exc:
            require("tree moved" in str(exc), "stale generated cache was not detected")
        else:
            raise AssertionError("stale generated cache unexpectedly passed strict validation")
        check_immutable_refs(repo, allow_generated_refresh=True)

        transaction_update_refs(
            repo,
            [(branch_to_ref(source_ref), rev_parse(repo, generated_ref), source)],
        )
        try:
            check_immutable_refs(repo, allow_generated_refresh=True)
        except Exception as exc:
            require(
                source_ref in str(exc),
                "generated-cache refresh hid the wrong immutable source failure",
            )
        else:
            raise AssertionError("generated-cache refresh relaxed source immutability")
    finally:
        shutil.rmtree(repo)


def test_checkpoint_record_preserves_valid_provenance_and_repairs_self_reference() -> None:
    repo = make_repo()
    try:
        previous_ref = "source/unofficial/1.2.1/edk2-stable202605"
        current_ref = "source/unofficial/1.2.2/edk2-stable202605"
        previous = create_branch(repo, previous_ref, {"firmware.txt": "old\n"}, "old")
        current = create_branch(repo, current_ref, {"firmware.txt": "new\n"}, "new")
        replacement = create_branch(
            repo,
            "replacement",
            {"firmware.txt": "repaired\n"},
            "replacement",
        )
        git(repo, "switch", "build")
        write_file(
            repo,
            "config/refs-unofficial.json",
            json.dumps(
                {
                    "refs": [
                        {
                            "edk2_base": "edk2-stable202605",
                            "immutable": True,
                            "line": "1.2",
                            "object_id": previous,
                            "previous_unofficial_ref": "source/unofficial/current",
                            "radxa_release": "1.2.1",
                            "radxa_source_ref": "old-port",
                            "ref": previous_ref,
                            "tree_id": git(
                                repo, "rev-parse", f"{previous}^{{tree}}"
                            ).stdout.strip(),
                            "type": "unofficial-release-checkpoint",
                        },
                        {
                            "edk2_base": "edk2-stable202605",
                            "immutable": True,
                            "line": "1.2",
                            "object_id": current,
                            "previous_unofficial_ref": current_ref,
                            "radxa_release": "1.2.2",
                            "radxa_source_ref": "new-port",
                            "ref": current_ref,
                            "tree_id": git(
                                repo, "rev-parse", f"{current}^{{tree}}"
                            ).stdout.strip(),
                            "type": "unofficial-release-checkpoint",
                        },
                    ]
                }
            )
            + "\n",
        )

        ensure_checkpoint_record(
            repo,
            ref=previous_ref,
            source_oid=previous,
            line="1.2",
            radxa_release="1.2.1",
            edk2_base="edk2-stable202605",
            radxa_ref="old-port",
            preferred_previous_ref=previous_ref,
        )
        ensure_checkpoint_record(
            repo,
            ref=current_ref,
            source_oid=current,
            line="1.2",
            radxa_release="1.2.2",
            edk2_base="edk2-stable202605",
            radxa_ref="new-port",
            preferred_previous_ref=current_ref,
        )
        git(repo, "update-ref", f"refs/heads/{current_ref}", replacement, current)
        ensure_checkpoint_record(
            repo,
            ref=current_ref,
            source_oid=replacement,
            line="1.2",
            radxa_release="1.2.2",
            edk2_base="edk2-stable202605",
            radxa_ref="new-port",
            preferred_previous_ref=current_ref,
        )
        records = json.loads(
            (repo / "config/refs-unofficial.json").read_text(encoding="utf-8")
        )["refs"]
        by_ref = {record["ref"]: record for record in records}
        require(
            by_ref[previous_ref]["previous_unofficial_ref"] == "source/unofficial/current",
            "valid existing checkpoint provenance was overwritten",
        )
        require(
            by_ref[current_ref]["previous_unofficial_ref"] == previous_ref,
            "self-referential checkpoint provenance was not repaired from history",
        )
        require(
            by_ref[current_ref]["object_id"] == replacement,
            "checkpoint metadata did not follow an explicit recovered ref replacement",
        )
        require(
            by_ref[current_ref]["tree_id"]
            == git(repo, "rev-parse", f"{replacement}^{{tree}}").stdout.strip(),
            "checkpoint tree metadata was stale after an explicit recovered ref replacement",
        )
    finally:
        shutil.rmtree(repo)


def test_first_checkpoint_on_new_line_uses_explicit_cross_line_source() -> None:
    repo = make_repo()
    try:
        old_ref = "source/unofficial/1.2.4/edk2-stable202605"
        new_ref = "source/unofficial/1.3.0/edk2-stable202605"
        old = create_branch(repo, old_ref, {"firmware.txt": "1.2.4\n"}, "old line")
        new = create_branch(repo, new_ref, {"firmware.txt": "1.3.0\n"}, "new line")
        git(repo, "switch", "build")
        write_file(
            repo,
            "config/refs-unofficial.json",
            json.dumps(
                {
                    "refs": [
                        {
                            "edk2_base": "edk2-stable202605",
                            "immutable": True,
                            "line": "1.2",
                            "object_id": old,
                            "previous_unofficial_ref": "source/unofficial/1.2.3/edk2-stable202605",
                            "radxa_release": "1.2.4",
                            "radxa_source_ref": "old-port",
                            "ref": old_ref,
                            "tree_id": git(
                                repo, "rev-parse", f"{old}^{{tree}}"
                            ).stdout.strip(),
                            "type": "unofficial-release-checkpoint",
                        }
                    ]
                }
            )
            + "\n",
        )

        cross_line_source = "source/unofficial/1.2/current"
        ensure_checkpoint_record(
            repo,
            ref=new_ref,
            source_oid=new,
            line="1.3",
            radxa_release="1.3.0",
            edk2_base="edk2-stable202605",
            radxa_ref="new-port",
            preferred_previous_ref=cross_line_source,
        )
        records = json.loads(
            (repo / "config/refs-unofficial.json").read_text(encoding="utf-8")
        )["refs"]
        by_ref = {record["ref"]: record for record in records}
        require(
            by_ref[new_ref]["previous_unofficial_ref"] == cross_line_source,
            "first checkpoint on a new line inherited predecessor from another line",
        )
    finally:
        shutil.rmtree(repo)


def main() -> None:
    test_overlap_report_separates_policy_owned_paths()
    test_update_policy_migrates_legacy_current_to_line_records()
    test_resolved_unofficial_tree_gets_canonical_provenance_commit()
    test_final_unofficial_candidate_is_promoted_without_rewriting()
    test_source_stage_resume_flags_changed_overlay_when_vendor_source_is_unchanged()
    test_unchanged_source_accepts_canonical_overlay_normalisation()
    test_changed_source_absorbs_matching_overlay()
    test_source_port_preserves_upstream_crlf_for_non_replayed_source_deltas()
    test_resolved_unofficial_stage_detects_overlay_handoff()
    test_active_line_tip_can_advance_beyond_immutable_checkpoint()
    test_retained_historical_target_is_not_rebound_to_old_checkpoint()
    test_release_line_validation()
    test_explicit_unofficial_source_overrides_exact_checkpoint()
    test_delegated_ref_creation_invalidates_parent_metadata_cache()
    test_atomic_ref_creation_invalidates_parent_metadata_cache()
    test_generated_cache_refresh_does_not_relax_source_immutability()
    test_checkpoint_record_preserves_valid_provenance_and_repairs_self_reference()
    test_first_checkpoint_on_new_line_uses_explicit_cross_line_source()
    print("Radxa uplift tests passed")


def load_tests(loader, tests, pattern):
    return load_function_tests(globals())


if __name__ == "__main__":
    main()
