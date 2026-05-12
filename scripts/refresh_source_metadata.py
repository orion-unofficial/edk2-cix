#!/usr/bin/env python3
"""Refresh source-ref manifests and rendered source-target tree records."""

from __future__ import annotations

import argparse
import os
import time
from copy import deepcopy
from pathlib import Path
from typing import Any

from reconstruction_common import (
    SOURCE_TARGET_CACHE_MANIFEST,
    ReconstructionError,
    alias_target_for,
    clear_metadata_caches,
    format_duration,
    format_manifest_value,
    git,
    load_json,
    main_wrapper,
    manifest_record_sort_key,
    ref_exists,
    release_branch_parts,
    release_entries,
    repo_root,
    rev_parse,
    tree_id,
    truthy,
    unofficial_release_branch_for_tag,
    unofficial_release_branches,
    unofficial_release_tag_for_branch,
    write_json,
)
from render_release_branch import render_from_plan


HELP = """refresh-source-metadata

Optional variables:
  WRITE=0|1
      Write refreshed config/refs-*.json metadata and, when requested, update
      unofficial release tags. Without WRITE=1, the command reports drift only.
  CHECK=0|1
      Fail if any refreshable metadata is stale. make check-source-metadata
      sets this automatically.
  RENDER_GENERATED=0|1
      Re-render generated source/cache/release entries whose tree cannot be
      derived directly from a retained source ref. This is slower, but is the
      deterministic full-cache mode needed after history rewrites.
  UPDATE_RELEASE_TAGS=0|1
      Check or update refs/tags/source/unofficial/edk2/stable-* so they match
      the corresponding source/unofficial/edk2-stable* branch heads.
  V=0|1
      Print unchanged records and render progress.

The command never deletes refs. Generated cache tree refreshes create temporary
commits only unless a source/cache/** ref already exists and is explicitly
updated by another workflow.
"""

ZERO_OID = "0" * 40
SOURCE_REF_MANIFESTS = (
    "refs-arm.json",
    "refs-cix.json",
    "refs-edk2.json",
    "refs-radxa.json",
)


class Change:
    def __init__(self, subject: str, field: str, old: str | None, new: str) -> None:
        self.subject = subject
        self.field = field
        self.old = old
        self.new = new

    def describe(self) -> str:
        old = "<missing>" if self.old is None else self.old
        return f"{self.subject}: {self.field} {old} -> {self.new}"


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--write", nargs="?", const="1", default=os.environ.get("WRITE", "0"))
    p.add_argument("--check", nargs="?", const="1", default=os.environ.get("CHECK", "0"))
    p.add_argument("--render-generated", nargs="?", const="1", default=os.environ.get("RENDER_GENERATED", "0"))
    p.add_argument("--update-release-tags", nargs="?", const="1", default=os.environ.get("UPDATE_RELEASE_TAGS", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def update_field(record: dict[str, Any], subject: str, field: str, value: str, changes: list[Change]) -> None:
    old = record.get(field)
    if old == value:
        return
    record[field] = value
    changes.append(Change(subject, field, str(old) if old is not None else None, value))


def ref_ids(repo: Path, ref: str) -> tuple[str, str]:
    if not ref_exists(repo, ref):
        raise ReconstructionError(f"required source ref is unavailable locally: {ref}")
    return rev_parse(repo, ref), tree_id(repo, ref)


def item_refs(item: dict[str, Any]) -> list[str]:
    refs = item.get("refs")
    if isinstance(refs, list):
        return [str(ref) for ref in refs]
    ref = item.get("ref")
    return [str(ref)] if ref else []


def refresh_ref_item(repo: Path, item: dict[str, Any], subject_prefix: str, changes: list[Change]) -> None:
    refs = item_refs(item)
    if not refs:
        return
    values = [ref_ids(repo, ref) for ref in refs]
    object_ids = {object_id for object_id, _tree in values}
    tree_ids = {tree for _object_id, tree in values}
    subject = item.get("ref") or ",".join(refs)
    if len(object_ids) != 1 or len(tree_ids) != 1:
        raise ReconstructionError(f"{subject_prefix}: grouped refs do not share one commit/tree: {subject}")
    update_field(item, f"{subject_prefix}:{subject}", "object_id", object_ids.pop(), changes)
    update_field(item, f"{subject_prefix}:{subject}", "tree_id", tree_ids.pop(), changes)


def refresh_simple_ref_manifest(repo: Path, manifest_name: str, changes: list[Change]) -> dict[str, Any]:
    data = load_json(repo, f"config/{manifest_name}")
    for item in data.get("refs", []):
        refresh_ref_item(repo, item, manifest_name, changes)
    return data


def release_context(release: dict[str, Any], component: str) -> dict[str, Any]:
    context = {key: value for key, value in release.items() if key != "components"}
    context["component"] = component
    return context


def refresh_release_ref_manifest(repo: Path, manifest_name: str, changes: list[Change]) -> dict[str, Any]:
    data = load_json(repo, f"config/{manifest_name}")
    templates = data.get("component_templates", {})
    if not isinstance(templates, dict):
        raise ReconstructionError(f"config/{manifest_name}: releases require component_templates")
    for release in data.get("releases", []):
        components = release.get("components", {})
        if not isinstance(components, dict):
            raise ReconstructionError(f"config/{manifest_name}: release components must be an object")
        for component, component_data in components.items():
            template = templates.get(component)
            if not isinstance(template, dict):
                raise ReconstructionError(f"config/{manifest_name}: missing template for component {component}")
            formatted = format_manifest_value(template, release_context(release, component))
            ref = formatted.get("ref")
            if not isinstance(ref, str) or not ref:
                raise ReconstructionError(f"config/{manifest_name}: component {component} has no ref template")
            object_id, ref_tree = ref_ids(repo, ref)
            subject = f"{manifest_name}:{ref}"
            update_field(component_data, subject, "object_id", object_id, changes)
            update_field(component_data, subject, "tree_id", ref_tree, changes)
    return data


def refresh_source_ref_manifests(repo: Path) -> tuple[dict[str, dict[str, Any]], list[Change]]:
    refreshed: dict[str, dict[str, Any]] = {}
    changes: list[Change] = []
    for manifest_name in SOURCE_REF_MANIFESTS:
        path = repo / "config" / manifest_name
        if not path.exists():
            continue
        if "releases" in load_json(repo, f"config/{manifest_name}"):
            refreshed[manifest_name] = refresh_release_ref_manifest(repo, manifest_name, changes)
        else:
            refreshed[manifest_name] = refresh_simple_ref_manifest(repo, manifest_name, changes)
    return refreshed, changes


def source_ref_tree(repo: Path, entry: dict[str, Any]) -> str | None:
    source_ref = entry.get("source_ref")
    if isinstance(source_ref, str) and source_ref and ref_exists(repo, source_ref):
        return tree_id(repo, source_ref)
    return None


def computed_source_target_tree(
    repo: Path,
    ref: str,
    entry: dict[str, Any],
    *,
    render_generated: bool,
    verbose: bool,
) -> tuple[str | None, str]:
    parts = release_branch_parts(ref)
    stage = parts["stage"]
    if stage in {"custom", "upstream"}:
        direct = source_ref_tree(repo, entry)
        if direct:
            return direct, "source-ref"

    if render_generated:
        if verbose:
            print(f"rendering generated source target for metadata refresh: {ref}")
        commit = render_from_plan(repo, ref, entry, verbose, allow_manifest_refresh=True)
        return tree_id(repo, commit), "rendered"

    if ref_exists(repo, ref):
        return tree_id(repo, ref), "persisted-cache-ref"

    return None, "skipped-render"


def refresh_source_target_cache_manifest(
    repo: Path,
    *,
    render_generated: bool,
    verbose: bool,
) -> tuple[dict[str, Any], list[Change], list[str]]:
    manifest_name = SOURCE_TARGET_CACHE_MANIFEST
    data = load_json(repo, f"config/{manifest_name}")
    data = deepcopy(data)
    entries = release_entries(repo)
    changes: list[Change] = []
    skipped: list[str] = []

    for item in data.get("refs", []):
        refs = item_refs(item)
        if not refs:
            continue
        values: dict[str, str] = {}
        for ref in refs:
            try:
                parts = release_branch_parts(ref)
            except ReconstructionError as exc:
                raise ReconstructionError(f"config/{manifest_name}: {exc}") from exc
            entry_ref = alias_target_for(ref, parts) or ref
            entry = entries.get(entry_ref) or entries.get(ref)
            if not entry:
                raise ReconstructionError(f"config/{manifest_name}: source target is no longer derivable: {ref}")
            value, method = computed_source_target_tree(
                repo,
                entry_ref,
                entry,
                render_generated=render_generated,
                verbose=verbose,
            )
            if value is None:
                skipped.append(f"{ref} ({method})")
                continue
            values[ref] = value
            if verbose:
                print(f"{ref}: {value} ({method})")
        if not values:
            continue
        unique_values = set(values.values())
        if len(unique_values) != 1:
            details = "\n".join(f"  - {ref}: {value}" for ref, value in sorted(values.items()))
            raise ReconstructionError(f"config/{manifest_name}: grouped source-target refs do not share one tree:\n{details}")
        subject = item.get("ref") or ",".join(refs)
        update_field(item, f"{manifest_name}:{subject}", "tree_id", unique_values.pop(), changes)

    data["refs"] = sorted(data.get("refs", []), key=manifest_record_sort_key)
    return data, changes, skipped


def tag_oid(repo: Path, tag: str) -> str | None:
    result = git(repo, "rev-parse", "--verify", "--quiet", f"refs/tags/{tag}^{{commit}}", check=False)
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def release_tag_changes(repo: Path) -> list[Change]:
    changes: list[Change] = []
    for branch in unofficial_release_branches(repo):
        branch_oid = rev_parse(repo, branch)
        tag = unofficial_release_tag_for_branch(branch)
        old = tag_oid(repo, tag)
        if old != branch_oid:
            changes.append(Change(f"refs/tags/{tag}", "target", old or ZERO_OID, branch_oid))
    return changes


def write_release_tags(repo: Path, changes: list[Change]) -> None:
    for change in changes:
        tag = change.subject.removeprefix("refs/tags/")
        branch = unofficial_release_branch_for_tag(tag)
        git(repo, "update-ref", f"refs/tags/{tag}", rev_parse(repo, branch), change.old if change.old != ZERO_OID else ZERO_OID)


def write_refreshed_manifests(repo: Path, manifests: dict[str, dict[str, Any]], changes: list[Change]) -> None:
    changed_manifests = {
        change.subject.split(":", 1)[0]
        for change in changes
        if change.subject.startswith("refs-")
    }
    for manifest_name, data in manifests.items():
        if manifest_name not in changed_manifests:
            continue
        write_json(repo / "config" / manifest_name, data)
    clear_metadata_caches()


def print_changes(title: str, changes: list[Change]) -> None:
    if not changes:
        print(f"{title}: current")
        return
    print(f"{title}: {len(changes)} update(s)")
    for change in changes:
        print(f"  - {change.describe()}")


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    write = truthy(args.write)
    check = truthy(args.check)
    render_generated = truthy(args.render_generated)
    update_tags = truthy(args.update_release_tags)
    verbose = truthy(args.v)

    source_manifests, source_changes = refresh_source_ref_manifests(repo)
    target_manifest, target_changes, skipped = refresh_source_target_cache_manifest(
        repo,
        render_generated=render_generated,
        verbose=verbose,
    )
    manifests = dict(source_manifests)
    manifests[SOURCE_TARGET_CACHE_MANIFEST] = target_manifest
    tag_changes = release_tag_changes(repo) if update_tags else []
    all_changes = source_changes + target_changes + tag_changes

    print_changes("source ref manifest metadata", source_changes)
    print_changes("source-target cache metadata", target_changes)
    if update_tags:
        print_changes("unofficial release tags", tag_changes)
    if skipped:
        print(
            f"source-target cache entries skipped: {len(skipped)} "
            "(set RENDER_GENERATED=1 for a full rendered-cache refresh)"
        )
        if verbose:
            for item in skipped:
                print(f"  - {item}")

    if all_changes and write:
        write_refreshed_manifests(repo, manifests, source_changes + target_changes)
        if update_tags:
            write_release_tags(repo, tag_changes)
        print(f"wrote refreshed source metadata in {format_duration(time.monotonic() - started)}")
        return

    if all_changes and check:
        raise ReconstructionError(
            "source metadata is stale; run make refresh-source-metadata WRITE=1 "
            "after reviewing the reported updates"
        )

    if all_changes:
        print("dry run; set WRITE=1 to update source metadata")
    else:
        print(f"source metadata already current in {format_duration(time.monotonic() - started)}")


if __name__ == "__main__":
    main_wrapper(main)
