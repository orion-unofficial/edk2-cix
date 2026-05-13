#!/usr/bin/env python3
"""Unit checks for check_upstream_versions.py."""

from __future__ import annotations

import tempfile
from pathlib import Path

from check_upstream_versions import (
    LocalState,
    RemoteRef,
    UpstreamVersionResult,
    compare_head,
    compare_tag,
    comparison_items,
    latest_docker_tag_from_snapshot,
    latest_remote_subject_from_snapshot,
    latest_remote_tag,
    local_file_regex,
    local_workflow_action_ref,
    write_github_summary,
)
from test_support import load_function_tests, require


def test_latest_remote_tag_prefers_peeled_commit() -> None:
    refs = [
        RemoteRef("tag-object", "refs/tags/v2.7"),
        RemoteRef("commit-object", "refs/tags/v2.7^{}"),
        RemoteRef("newer-commit", "refs/tags/v2.8"),
    ]
    latest = latest_remote_tag(refs, r"^refs/tags/v(?P<version>\d+\.\d+)$")
    require(latest is not None, "expected a latest tag")
    require(latest.label == "v2.8", "expected the newest version")
    require(latest.object_id == "newer-commit", "expected direct tag object for lightweight tag")

    only_annotated = latest_remote_tag(refs[:2], r"^refs/tags/v(?P<version>\d+\.\d+)$")
    require(only_annotated is not None, "expected annotated tag")
    require(only_annotated.object_id == "commit-object", "expected peeled commit object")


def test_compare_tag_statuses() -> None:
    current = compare_tag(
        LocalState(label="v2.7", version="2.7", object_id="abc"),
        LocalState(label="v2.7", version="2.7", object_id="abc"),
    )
    require(current[0] == "current", "same tag and object should be current")

    stale = compare_tag(
        LocalState(label="v2.7", version="2.7", object_id="abc"),
        LocalState(label="v2.8", version="2.8", object_id="def"),
    )
    require(stale[0] == "stale", "newer remote tag should be stale")

    changed = compare_tag(
        LocalState(label="v2.7", version="2.7", object_id="abc"),
        LocalState(label="v2.7", version="2.7", object_id="def"),
    )
    require(changed[0] == "changed", "same tag with different object should be changed")


def test_compare_head_statuses() -> None:
    current = compare_head(
        LocalState(label="cix-1.2/bios", object_id="abc"),
        LocalState(label="refs/heads/main", object_id="abc"),
    )
    require(current[0] == "current", "same branch head should be current")

    stale = compare_head(
        LocalState(label="cix-1.2/bios", object_id="abc"),
        LocalState(label="refs/heads/main", object_id="def"),
    )
    require(stale[0] == "stale", "different branch head should be stale")


def test_grouped_comparison_items() -> None:
    check = {
        "id": "edk2",
        "local": {"type": "edk2-release"},
        "release": {"tag_pattern": "tag-regex"},
        "commits": {"ref": "refs/heads/master", "mode": "advisory"},
    }
    items = comparison_items(check)
    require([label for label, _item in items] == ["release", "commits"], "expected release then commits checks")
    require(items[0][1]["kind"] == "latest_tag", "release comparison should default to latest_tag")
    require(items[1][1]["kind"] == "branch_head", "commits comparison should default to branch_head")
    require(items[1][1]["local"] == {"type": "edk2-release"}, "grouped comparison should inherit local state")


def test_grouped_release_subject_comparison_items() -> None:
    check = {
        "id": "cix-bios",
        "local": {"component": "bios", "type": "cix-component-ref"},
        "release": {
            "kind": "release_subject",
            "ref": "refs/heads/cix_p1_community_dev",
            "subject_pattern": "release-regex",
        },
    }
    items = comparison_items(check)
    require([label for label, _item in items] == ["release"], "release_subject should still be a release check")
    require(items[0][1]["kind"] == "release_subject", "release_subject kind should be preserved")


def test_latest_remote_subject_snapshot() -> None:
    refs = [
        RemoteRef("daily", "refs/heads/cix_p1_dev", "daily development"),
        RemoteRef("release-2", "refs/heads/cix_p1_dev~1", "P1 GA 2026Q2 [COMMUNITY] RELEASE"),
        RemoteRef("release-1", "refs/heads/cix_p1_dev~2", "P1 GA 2026Q1 [COMMUNITY] RELEASE"),
    ]
    latest = latest_remote_subject_from_snapshot(refs, "refs/heads/cix_p1_dev", r"^P1 GA .* \[COMMUNITY\] RELEASE$")
    require(latest is not None, "expected a matching release subject")
    require(latest.object_id == "release-2", "expected the newest matching release subject")
    require(latest.label == "P1 GA 2026Q2 [COMMUNITY] RELEASE", "expected release subject label")


def test_github_summary_table() -> None:
    result = UpstreamVersionResult(
        source_id="cix-bootloader1",
        check_id="cix-bootloader1:release",
        kind="release",
        description="Latest CIX bootloader1 release commit",
        mode="advisory",
        status="current",
        local="cix-1.2/bootloader1",
        remote="P1 GA 2026Q2 [COMMUNITY] RELEASE",
        detail="local record matches release commit",
    )
    with tempfile.TemporaryDirectory(prefix="check-upstream-versions-test-") as tmp:
        path = Path(tmp) / "summary.md"
        write_github_summary([result], path)
        summary = path.read_text(encoding="utf-8")
    require("| Source | Check | Status | Policy | Local | Remote | Detail |" in summary, "expected summary table header")
    require("cix-bootloader1" in summary, "expected source ID in summary")
    require("P1 GA 2026Q2 [COMMUNITY] RELEASE" in summary, "expected release subject in summary")


def test_local_file_regex() -> None:
    with tempfile.TemporaryDirectory(prefix="check-upstream-versions-test-") as tmp:
        root = Path(tmp)
        path = root / "scripts"
        path.mkdir()
        (path / "ensure_act.sh").write_text(
            'default_runner_image="${ACT_RUNNER_IMAGE:-${EDK2_CIX_ACT_RUNNER_IMAGE:-catthehacker/ubuntu:act-24.04-20260508}}"\n'
            'act_version="${EDK2_CIX_ACT_VERSION:-0.2.88}"\n',
            encoding="utf-8",
        )
        state = local_file_regex(
            root,
            {
                "path": "scripts/ensure_act.sh",
                "pattern": r'act_version="\$\{EDK2_CIX_ACT_VERSION:-(?P<version>\d+\.\d+\.\d+)\}"',
                "label_template": "v{version}",
            },
        )
        docker_state = local_file_regex(
            root,
            {
                "path": "scripts/ensure_act.sh",
                "pattern": r"(?P<repository>catthehacker/ubuntu):(?P<version>act-24\.04-\d{8})",
                "label_template": "{repository}:{version}",
            },
        )
    require(state.label == "v0.2.88", "expected formatted local label")
    require(state.version == "0.2.88", "expected normalized local version")
    require(docker_state.label == "catthehacker/ubuntu:act-24.04-20260508", "expected image label")
    require(docker_state.version == "act-24.04-20260508", "expected image tag version")


def test_local_workflow_action_ref() -> None:
    with tempfile.TemporaryDirectory(prefix="check-upstream-versions-test-") as tmp:
        root = Path(tmp)
        workflow_root = root / ".github" / "workflows"
        workflow_root.mkdir(parents=True)
        (workflow_root / "ci.yaml").write_text(
            "steps:\n"
            "  - uses: actions/checkout@v6\n"
            "  - uses: actions/upload-artifact@v7\n",
            encoding="utf-8",
        )
        state = local_workflow_action_ref(root, {"action": "actions/checkout", "type": "workflow-action-ref"})
    require(state.label == "v6", "expected workflow action ref label")
    require(state.version == "6", "expected normalized workflow action version")


def test_docker_latest_tag_snapshot() -> None:
    latest = latest_docker_tag_from_snapshot(
        [
            RemoteRef("", "docker://act-24.04-20260429"),
            RemoteRef("", "docker://act-24.04-20260508"),
            RemoteRef("", "docker://act-22.04-20260508"),
        ],
        r"^(?P<version>act-24\.04-\d{8})$",
    )
    require(latest is not None, "expected a matching docker snapshot tag")
    require(latest.label == "act-24.04-20260508", "expected newest matching docker tag")
    require(latest.version == "act-24.04-20260508", "expected docker snapshot version")


def main() -> None:
    test_latest_remote_tag_prefers_peeled_commit()
    test_compare_tag_statuses()
    test_compare_head_statuses()
    test_grouped_comparison_items()
    test_grouped_release_subject_comparison_items()
    test_latest_remote_subject_snapshot()
    test_github_summary_table()
    test_local_file_regex()
    test_local_workflow_action_ref()
    test_docker_latest_tag_snapshot()
    print("check_upstream_versions tests passed")


def load_tests(loader, tests, pattern):
    return load_function_tests(globals())


if __name__ == "__main__":
    main()
