#!/usr/bin/env python3
"""Shared source-tree policy checks for firmware source refs.

This module is intentionally a general policy layer. Individual checks, such as
overlay symlink integrity, are implemented as small rules and exposed through
the source-tree policy entry points below.
"""

from __future__ import annotations

import posixpath
from pathlib import Path
from typing import NamedTuple

from reconstruction_common import ReconstructionError, git


OVERLAY_PREFIX = "custom/overlay/"
SOURCE_PREFIX = "src/"
NORMAL_FILE_MODES = {"100644", "100755"}
SYMLINK_MODE = "120000"


class TreeEntry(NamedTuple):
    mode: str
    object_id: str
    path: str


def overlay_source_path(path: str) -> str:
    if not path.startswith(OVERLAY_PREFIX):
        raise ValueError(f"not an overlay path: {path}")
    return SOURCE_PREFIX + path[len(OVERLAY_PREFIX) :]


def parse_ls_tree_line(line: str) -> TreeEntry:
    metadata, path = line.split("\t", 1)
    mode, _kind, object_id = metadata.split()
    return TreeEntry(mode=mode, object_id=object_id, path=path)


def parse_ls_files_line(line: str) -> TreeEntry:
    metadata, path = line.split("\t", 1)
    mode, object_id, stage = metadata.split()
    if stage != "0":
        raise ReconstructionError(f"source policy cannot validate unmerged index entry: {path}")
    return TreeEntry(mode=mode, object_id=object_id, path=path)


def tree_entries(repo: Path, ref: str) -> dict[str, TreeEntry]:
    result = git(repo, "ls-tree", "-r", ref, "--", OVERLAY_PREFIX.rstrip("/"), SOURCE_PREFIX.rstrip("/"))
    return {
        entry.path: entry
        for entry in (parse_ls_tree_line(line) for line in result.stdout.splitlines() if line)
    }


def index_entries(repo: Path) -> dict[str, TreeEntry]:
    result = git(repo, "ls-files", "-s", "--", OVERLAY_PREFIX.rstrip("/"), SOURCE_PREFIX.rstrip("/"))
    return {
        entry.path: entry
        for entry in (parse_ls_files_line(line) for line in result.stdout.splitlines() if line)
    }


def blob_text(repo: Path, object_id: str) -> str:
    return git(repo, "cat-file", "-p", object_id).stdout


def expected_symlink_target_path(path: str) -> str:
    return overlay_source_path(path)


def resolved_symlink_path(path: str, target: str) -> str:
    if posixpath.isabs(target):
        return target
    return posixpath.normpath(posixpath.join(posixpath.dirname(path), target))


def collect_source_entries(repo: Path, *, ref: str | None = None, index: bool = False) -> dict[str, TreeEntry]:
    if index and ref:
        raise ValueError("pass exactly one of ref=... or index=True")
    return index_entries(repo) if index else tree_entries(repo, ref or "HEAD")


def overlay_symlink_policy_violations(repo: Path, entries: dict[str, TreeEntry]) -> list[str]:
    """Return overlay files that violate the overlay symlink policy."""

    violations: list[str] = []
    for path, entry in sorted(entries.items()):
        if not path.startswith(OVERLAY_PREFIX):
            continue
        source_path = overlay_source_path(path)
        source = entries.get(source_path)

        if entry.mode == SYMLINK_MODE:
            target = blob_text(repo, entry.object_id)
            if posixpath.isabs(target):
                violations.append(f"{path}: symlink target must be relative, found {target!r}")
                continue
            resolved = resolved_symlink_path(path, target)
            if resolved != source_path:
                violations.append(
                    f"{path}: symlink target resolves to {resolved!r}, expected {source_path!r}"
                )
            if source is None:
                violations.append(f"{path}: symlink target source path is missing: {source_path}")
            continue

        if entry.mode in NORMAL_FILE_MODES and source and source.mode in NORMAL_FILE_MODES:
            if entry.object_id == source.object_id:
                violations.append(
                    f"{path}: byte-identical to {source_path}; replace the overlay copy with a symlink"
                )

    return violations


def source_tree_policy_violations(repo: Path, *, ref: str | None = None, index: bool = False) -> list[str]:
    entries = collect_source_entries(repo, ref=ref, index=index)
    return overlay_symlink_policy_violations(repo, entries)


def enforce_source_tree_policy(
    repo: Path,
    *,
    ref: str | None = None,
    index: bool = False,
    label: str | None = None,
) -> None:
    violations = source_tree_policy_violations(repo, ref=ref, index=index)
    if not violations:
        return
    subject = label or ("index" if index else ref or "HEAD")
    sample = "\n".join(f"  - {item}" for item in violations[:20])
    extra = "" if len(violations) <= 20 else f"\n  ... and {len(violations) - 20} more"
    raise ReconstructionError(
        f"source-tree policy failed for {subject}:\n{sample}{extra}"
    )
