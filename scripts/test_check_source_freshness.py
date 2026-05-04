#!/usr/bin/env python3
"""Unit checks for check_source_freshness.py."""

from __future__ import annotations

from check_source_freshness import LocalState, RemoteRef, compare_head, compare_tag, latest_remote_tag


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


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


def main() -> None:
    test_latest_remote_tag_prefers_peeled_commit()
    test_compare_tag_statuses()
    test_compare_head_statuses()
    print("check_source_freshness tests passed")


if __name__ == "__main__":
    main()
