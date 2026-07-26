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
)
from uplift_radxa_release import (
    ensure_checkpoint_record,
    release_line,
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


def test_release_line_validation() -> None:
    require(release_line("1.3.1") == "1.3", "release line was derived incorrectly")
    try:
        release_line("next")
    except Exception as exc:
        require("cannot derive" in str(exc), "invalid release did not explain line derivation")
    else:
        raise AssertionError("invalid release unexpectedly produced a line")


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
    test_active_line_tip_can_advance_beyond_immutable_checkpoint()
    test_release_line_validation()
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
