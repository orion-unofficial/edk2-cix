#!/usr/bin/env python3
"""Deterministic source path lifecycle helpers.

The lifecycle layer answers one narrow question for source import tooling:
given a source path in one materialised firmware tree, can we prove where that
path lives in another tree, or prove that it no longer exists?

Only exact, unambiguous mappings are accepted automatically. Similarity-based
renames are deliberately left to higher-level workflows to report as conflicts
or manual follow-up work.
"""

from __future__ import annotations

import posixpath
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from reconstruction_common import git
from source_policy import (
    NORMAL_FILE_MODES,
    OVERLAY_PREFIX,
    SOURCE_PREFIX,
    SYMLINK_MODE,
    blob_text,
    overlay_source_path,
    resolved_symlink_path,
)


@dataclass(frozen=True)
class TreeEntry:
    mode: str
    object_id: str
    path: str


@dataclass(frozen=True)
class SourcePathMapping:
    kind: str
    source_path: str
    target_path: str | None = None
    candidates: tuple[str, ...] = ()
    detail: str = ""


@dataclass(frozen=True)
class OverlayProjection:
    severity: str
    action: str
    overlay_path: str
    source_path: str | None
    target_overlay_path: str | None
    detail: str


def parse_ls_tree_record(record: str) -> TreeEntry:
    metadata, path = record.split("\t", 1)
    mode, _kind, object_id = metadata.split()
    return TreeEntry(mode=mode, object_id=object_id, path=path)


def tree_entries(repo: Path, ref: str, prefixes: Iterable[str]) -> dict[str, TreeEntry]:
    prefix_list = [prefix.rstrip("/") for prefix in prefixes]
    result = git(repo, "ls-tree", "-rz", ref, "--", *prefix_list)
    entries: dict[str, TreeEntry] = {}
    for record in result.stdout.split("\0"):
        if not record:
            continue
        entry = parse_ls_tree_record(record)
        entries[entry.path] = entry
    return entries


def source_entries(repo: Path, ref: str) -> dict[str, TreeEntry]:
    return tree_entries(repo, ref, [SOURCE_PREFIX.rstrip("/")])


def overlay_entries(repo: Path, ref: str) -> dict[str, TreeEntry]:
    return tree_entries(repo, ref, [OVERLAY_PREFIX.rstrip("/")])


def source_component_key(path: str) -> str:
    """Return a component bucket for exact rename searches.

    The key is intentionally path-derived so the same helper works for EDK2,
    CIX TF-A/OP-TEE, and future component roots without committed mapping data.
    """

    if not path.startswith(SOURCE_PREFIX):
        return ""
    parts = path.split("/")
    if len(parts) < 3:
        return path
    if parts[1].startswith("cix-") and len(parts) >= 4:
        component = "op-tee" if parts[2] == "tee" else parts[2]
        return f"{parts[1]}/{component}"
    return parts[1]


def exact_entry_key(entry: TreeEntry) -> tuple[str, str, str]:
    return (source_component_key(entry.path), entry.mode, entry.object_id)


def index_exact_entries(entries: dict[str, TreeEntry]) -> dict[tuple[str, str, str], list[str]]:
    indexed: dict[tuple[str, str, str], list[str]] = {}
    for entry in entries.values():
        indexed.setdefault(exact_entry_key(entry), []).append(entry.path)
    return {key: sorted(paths) for key, paths in indexed.items()}


class SourceLifecycle:
    def __init__(self, repo: Path, from_ref: str, to_ref: str):
        self.repo = repo
        self.from_ref = from_ref
        self.to_ref = to_ref
        self.from_entries = source_entries(repo, from_ref)
        self.to_entries = source_entries(repo, to_ref)
        self.to_exact = index_exact_entries(self.to_entries)

    def map_source_path(self, source_path: str) -> SourcePathMapping:
        source = self.from_entries.get(source_path)
        if source is None:
            return SourcePathMapping(
                kind="missing-from-source",
                source_path=source_path,
                detail=f"{source_path} is not present in {self.from_ref}",
            )

        target = self.to_entries.get(source_path)
        if target is not None:
            if target.mode != source.mode:
                return SourcePathMapping(
                    kind="same-path-type-change",
                    source_path=source_path,
                    target_path=source_path,
                    detail=f"{source_path} exists in {self.to_ref}, but mode changed from {source.mode} to {target.mode}",
                )
            return SourcePathMapping(kind="same-path", source_path=source_path, target_path=source_path)

        candidates = [path for path in self.to_exact.get(exact_entry_key(source), []) if path != source_path]
        if len(candidates) == 1:
            return SourcePathMapping(
                kind="exact-rename",
                source_path=source_path,
                target_path=candidates[0],
                candidates=tuple(candidates),
                detail=f"{source_path} maps exactly to {candidates[0]} in {self.to_ref}",
            )
        if len(candidates) > 1:
            return SourcePathMapping(
                kind="ambiguous-exact-rename",
                source_path=source_path,
                candidates=tuple(candidates),
                detail=f"{source_path} has {len(candidates)} exact candidates in {self.to_ref}",
            )
        return SourcePathMapping(
            kind="deleted",
            source_path=source_path,
            detail=f"{source_path} is absent from {self.to_ref}",
        )


def overlay_path_for_source(source_path: str) -> str:
    if not source_path.startswith(SOURCE_PREFIX):
        raise ValueError(f"not a source path: {source_path}")
    return OVERLAY_PREFIX + source_path[len(SOURCE_PREFIX) :]


def symlink_target(entry_repo: Path, entry: TreeEntry) -> str:
    return blob_text(entry_repo, entry.object_id)


def is_mirror_symlink(repo: Path, overlay_path: str, entry: TreeEntry) -> tuple[bool, str | None, str | None]:
    if entry.mode != SYMLINK_MODE:
        return False, None, None
    target = symlink_target(repo, entry)
    if posixpath.isabs(target):
        return False, None, f"{overlay_path}: symlink target must be relative, found {target!r}"
    resolved = resolved_symlink_path(overlay_path, target)
    expected = overlay_source_path(overlay_path)
    if resolved != expected:
        return False, resolved, f"{overlay_path}: symlink target resolves to {resolved!r}, expected {expected!r}"
    return True, resolved, None


def project_overlay_entry(repo: Path, lifecycle: SourceLifecycle, overlay_path: str, entry: TreeEntry) -> OverlayProjection:
    if not overlay_path.startswith(OVERLAY_PREFIX):
        raise ValueError(f"not an overlay path: {overlay_path}")

    mirror, resolved, error = is_mirror_symlink(repo, overlay_path, entry)
    if error:
        return OverlayProjection("error", "invalid-symlink", overlay_path, resolved, None, error)

    source_path = overlay_source_path(overlay_path)
    if entry.mode == SYMLINK_MODE and not mirror:
        return OverlayProjection("error", "invalid-symlink", overlay_path, resolved, None, f"{overlay_path}: symlink is not a source mirror")

    mapping = lifecycle.map_source_path(source_path)
    if mapping.kind == "missing-from-source":
        if mirror:
            return OverlayProjection("error", "broken-source-mirror", overlay_path, source_path, None, mapping.detail)
        return OverlayProjection("ok", "keep-custom-file", overlay_path, source_path, overlay_path, f"{overlay_path} has no source counterpart in {lifecycle.from_ref}")

    if mapping.kind in {"same-path", "same-path-type-change", "exact-rename"} and mapping.target_path:
        target_overlay = overlay_path_for_source(mapping.target_path)
        action = "keep" if target_overlay == overlay_path else "rename"
        return OverlayProjection("ok", action, overlay_path, source_path, target_overlay, mapping.detail or f"{overlay_path} maps to {target_overlay}")

    if mapping.kind == "deleted":
        if mirror:
            return OverlayProjection("ok", "drop-mirror", overlay_path, source_path, None, mapping.detail)
        if entry.mode in NORMAL_FILE_MODES:
            return OverlayProjection(
                "error",
                "non-mirror-source-deleted",
                overlay_path,
                source_path,
                None,
                f"{overlay_path} modifies a source path that is absent from {lifecycle.to_ref}",
            )

    if mapping.kind == "ambiguous-exact-rename":
        candidates = ", ".join(mapping.candidates[:10])
        extra = "" if len(mapping.candidates) <= 10 else f", ... and {len(mapping.candidates) - 10} more"
        return OverlayProjection(
            "error",
            "ambiguous-rename",
            overlay_path,
            source_path,
            None,
            f"{source_path} has ambiguous exact rename candidates in {lifecycle.to_ref}: {candidates}{extra}",
        )

    return OverlayProjection(
        "error",
        "unhandled-lifecycle",
        overlay_path,
        source_path,
        None,
        f"{overlay_path}: unhandled lifecycle mapping {mapping.kind}",
    )


def project_overlay_tree(repo: Path, from_ref: str, to_ref: str, paths: Iterable[str] | None = None) -> list[OverlayProjection]:
    lifecycle = SourceLifecycle(repo, from_ref, to_ref)
    entries = overlay_entries(repo, from_ref)
    selected = sorted(paths) if paths is not None else sorted(entries)
    projections: list[OverlayProjection] = []
    for path in selected:
        entry = entries.get(path)
        if entry is None:
            projections.append(
                OverlayProjection("error", "missing-overlay", path, None, None, f"{path} is not present in {from_ref}")
            )
            continue
        projections.append(project_overlay_entry(repo, lifecycle, path, entry))
    return projections


def lifecycle_errors(projections: Iterable[OverlayProjection]) -> list[OverlayProjection]:
    return [projection for projection in projections if projection.severity == "error"]


def summarise_projections(projections: Iterable[OverlayProjection]) -> dict[str, int]:
    summary: dict[str, int] = {}
    for projection in projections:
        summary[projection.action] = summary.get(projection.action, 0) + 1
    return dict(sorted(summary.items()))


def format_projection(projection: OverlayProjection) -> str:
    target = f" -> {projection.target_overlay_path}" if projection.target_overlay_path else ""
    return f"{projection.severity}: {projection.action}: {projection.overlay_path}{target}: {projection.detail}"
