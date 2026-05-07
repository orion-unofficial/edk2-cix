#!/usr/bin/env python3
"""Read, refresh, and verify runtime-cached Makefile help data."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import time
from pathlib import Path
from typing import Any

from list_source_targets import render_help
from reconstruction_common import (
    ReconstructionError,
    default_release,
    format_duration,
    git,
    main_wrapper,
    repo_root,
    source_target_name,
    write_json,
)

CACHE_SCHEMA_VERSION = 2
CACHE_PATH = ".cache/edk2-cix/help/help-cache.json"
LEGACY_COMMITTED_CACHE_PATH = "config/help-cache.json"
FILE_PATTERNS = (
    "Makefile",
    "config",
    "scripts",
)
REF_PREFIXES = (
    "refs/heads/source/base",
    "refs/heads/source/vendor",
    "refs/heads/source/port",
    "refs/heads/source/component",
    "refs/heads/source/unofficial",
    "refs/remotes/origin/source/base",
    "refs/remotes/origin/source/vendor",
    "refs/remotes/origin/source/port",
    "refs/remotes/origin/source/component",
    "refs/remotes/origin/source/unofficial",
    "refs/tags/source/unofficial",
)


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    actions = p.add_mutually_exclusive_group(required=True)
    actions.add_argument("--print-default-release", action="store_true")
    actions.add_argument("--print-source-targets", action="store_true")
    actions.add_argument("--refresh", action="store_true")
    actions.add_argument("--verify", action="store_true")
    p.add_argument("--v", default="0")
    return p


def tracked_dependency_index_signature(repo: Path) -> list[str]:
    result = git(repo, "ls-files", "-s", "-z", "--", *FILE_PATTERNS)
    records = [line for line in result.stdout.split("\0") if line]
    return sorted(
        line
        for line in records
        if not line.endswith(f"\t{LEGACY_COMMITTED_CACHE_PATH}")
    )


def unstaged_dependency_signature(repo: Path) -> list[dict[str, str]]:
    result = git(repo, "diff", "--name-only", "-z", "--", *FILE_PATTERNS)
    records: list[dict[str, str]] = []
    for rel in sorted(path for path in result.stdout.split("\0") if path):
        if rel == LEGACY_COMMITTED_CACHE_PATH:
            continue
        path = repo / rel
        if path.exists() or path.is_symlink():
            object_id = git(repo, "hash-object", rel).stdout.strip()
        else:
            object_id = "<deleted>"
        records.append({"path": rel, "worktree_object_id": object_id})
    return records


def file_signature(repo: Path) -> dict[str, Any]:
    return {
        "tracked_index": tracked_dependency_index_signature(repo),
        "unstaged": unstaged_dependency_signature(repo),
    }


def ref_signature(repo: Path) -> list[dict[str, str]]:
    result = git(repo, "for-each-ref", "--format=%(refname) %(objectname)", *REF_PREFIXES, check=False)
    if result.returncode != 0:
        return []

    by_ref: dict[str, tuple[int, str]] = {}
    for line in result.stdout.splitlines():
        full_ref, _, oid = line.partition(" ")
        if not full_ref or "/source/cache/" in full_ref:
            continue
        if full_ref.startswith("refs/heads/"):
            priority = 0
            logical_ref = full_ref.removeprefix("refs/heads/")
        elif full_ref.startswith("refs/tags/"):
            priority = 0
            logical_ref = full_ref.removeprefix("refs/tags/")
        elif full_ref.startswith("refs/remotes/origin/"):
            priority = 1
            logical_ref = full_ref.removeprefix("refs/remotes/origin/")
        else:
            continue
        current = by_ref.get(logical_ref)
        if current is None or priority < current[0]:
            by_ref[logical_ref] = (priority, oid)

    return [
        {"ref": ref, "object_id": oid}
        for ref, (_priority, oid) in sorted(by_ref.items())
    ]


def dependency_signature(repo: Path) -> dict[str, Any]:
    payload = {
        "schema_version": CACHE_SCHEMA_VERSION,
        "files": file_signature(repo),
        "refs": ref_signature(repo),
    }
    digest = hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()
    return {"sha256": digest, **payload}


def build_cache(repo: Path) -> dict[str, Any]:
    return {
        "generated": {
            "default_release": source_target_name(default_release(repo)),
            "source_targets_help": render_help(repo),
        },
        "schema_version": CACHE_SCHEMA_VERSION,
        "signature": dependency_signature(repo),
    }


def read_cache(repo: Path) -> dict[str, Any] | None:
    path = repo / CACHE_PATH
    if not path.exists():
        return None
    try:
        with path.open("r", encoding="utf-8") as f:
            return json.load(f)
    except json.JSONDecodeError:
        return None


def cache_current(repo: Path, cache: dict[str, Any] | None) -> bool:
    if not cache:
        return False
    if cache.get("schema_version") != CACHE_SCHEMA_VERSION:
        return False
    return cache.get("signature", {}).get("sha256") == dependency_signature(repo)["sha256"]


def refresh_cache(repo: Path, started: float, reason: str) -> dict[str, Any]:
    print(
        f"[help-cache] Updating runtime help cache ({reason}); "
        "first generation may take several seconds in a cold clone.",
        file=sys.stderr,
        flush=True,
    )
    cache = build_cache(repo)
    write_json(repo / CACHE_PATH, cache)
    print(
        f"[help-cache] Runtime help cache updated in {format_duration(time.monotonic() - started)}.",
        file=sys.stderr,
        flush=True,
    )
    return cache


def cached_or_generated(repo: Path, started: float) -> tuple[dict[str, Any], bool]:
    cache = read_cache(repo)
    if cache_current(repo, cache):
        return cache, True
    reason = "missing" if cache is None else "stale"
    return refresh_cache(repo, started, reason), False


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))

    if args.refresh:
        refresh_cache(repo, started, "requested")
        return

    if args.verify:
        cache, _current = cached_or_generated(repo, started)
        actual = dependency_signature(repo)
        expected = cache.get("signature", {})
        if expected.get("sha256") != actual["sha256"]:
            raise ReconstructionError(
                f"{CACHE_PATH} could not be refreshed to match current help inputs\n"
                f"  cached:  {expected.get('sha256', '<missing>')}\n"
                f"  current: {actual['sha256']}"
            )
        print(f"runtime help cache is current in {format_duration(time.monotonic() - started)}")
        return

    cache, _current = cached_or_generated(repo, started)
    generated = cache.get("generated", {})
    if args.print_default_release:
        print(str(generated.get("default_release", "<unavailable>")))
    elif args.print_source_targets:
        print(str(generated.get("source_targets_help", "")), end="")


if __name__ == "__main__":
    main_wrapper(main)
