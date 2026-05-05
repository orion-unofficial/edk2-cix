#!/usr/bin/env python3
"""Unit checks for check_upstream_versions.py."""

from __future__ import annotations

from check_upstream_versions import LocalState, RemoteRef, compare_head, compare_tag, comparison_items, latest_remote_tag
from test_support import require


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


def main() -> None:
    test_latest_remote_tag_prefers_peeled_commit()
    test_compare_tag_statuses()
    test_compare_head_statuses()
    test_grouped_comparison_items()
    print("check_upstream_versions tests passed")


if __name__ == "__main__":
    main()
