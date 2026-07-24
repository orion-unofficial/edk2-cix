#!/usr/bin/env python3
"""Regression tests for reviewed vendor workflow baseline refreshes."""

from __future__ import annotations

import shutil
import tempfile
from pathlib import Path

from test_support import commit_all, git, load_function_tests, require, switch_orphan, write_file

from check_vendor_workflow_drift import refresh_missing_refs, workflow_snapshot


def create_vendor_ref(repo: Path, release: str, workflow: str) -> str:
    ref = f"source/vendor/radxa/{release}/edk2-stable202208"
    switch_orphan(repo, ref)
    write_file(repo, ".github/workflows/release.yaml", workflow)
    commit_all(repo, f"vendor {release}")
    return ref


def test_refresh_groups_identical_workflows_and_preserves_review_boundaries() -> None:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-vendor-workflow-test."))
    try:
        git(repo, "init", "-b", "build")
        git(repo, "config", "user.name", "Vendor Workflow Test")
        git(repo, "config", "user.email", "vendor-workflow-test")
        write_file(repo, "README.md", "build branch\n")
        commit_all(repo, "build root")

        first = create_vendor_ref(repo, "1.0.0", "name: release v1\n")
        second = create_vendor_ref(repo, "1.0.1", "name: release v1\n")
        third = create_vendor_ref(repo, "1.1.0", "name: release v2\n")
        git(repo, "switch", "build")

        tree, paths = workflow_snapshot(repo, first, ".github/workflows")
        data = {
            "checks": [
                {
                    "baseline_ref": first,
                    "paths": paths,
                    "refs": [first],
                    "vendor": "radxa",
                    "workflow_dir": ".github/workflows",
                    "workflow_tree_id": tree,
                }
            ],
            "description": "test",
        }

        updated, additions = refresh_missing_refs(repo, data)
        require(len(additions) == 2, "refresh did not cover both missing vendor refs")
        require(not additions[0]["new_snapshot"], "identical workflow was treated as a new review boundary")
        require(additions[1]["new_snapshot"], "changed workflow did not create a new review boundary")

        entries = updated["checks"]
        require(entries[0]["refs"] == [first, second], "identical workflow refs were not grouped")
        require(entries[1]["baseline_ref"] == third, "changed workflow used the wrong baseline ref")
        require(entries[1]["refs"] == [third], "changed workflow baseline contains unexpected refs")

        rerun, rerun_additions = refresh_missing_refs(repo, updated)
        require(not rerun_additions, "baseline refresh was not idempotent")
        require(rerun == updated, "idempotent refresh changed baseline metadata")
    finally:
        shutil.rmtree(repo)


def main() -> None:
    test_refresh_groups_identical_workflows_and_preserves_review_boundaries()
    print("vendor workflow drift tests passed")


def load_tests(loader, tests, pattern):
    return load_function_tests(globals())


if __name__ == "__main__":
    main()
