#!/usr/bin/env python3
"""Read, refresh, and verify cached Makefile help data."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import time
from pathlib import Path
from typing import Any

from list_configured_variants import render_help
from reconstruction_common import (
    ReconstructionError,
    default_release,
    format_duration,
    git,
    main_wrapper,
    repo_root,
    variant_name,
    write_json,
)

CACHE_PATH = "config/help-cache.json"
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
    actions.add_argument("--print-variants", action="store_true")
    actions.add_argument("--refresh", action="store_true")
    actions.add_argument("--verify", action="store_true")
    p.add_argument("--v", default="0")
    return p


def tracked_dependency_files(repo: Path) -> list[str]:
    result = git(repo, "ls-files", "-z", *FILE_PATTERNS)
    files = [path for path in result.stdout.split("\0") if path]
    return sorted(path for path in files if path != CACHE_PATH)


def file_signature(repo: Path) -> list[dict[str, str]]:
    records: list[dict[str, str]] = []
    for rel in tracked_dependency_files(repo):
        path = repo / rel
        if not path.is_file():
            continue
        oid = git(repo, "hash-object", rel).stdout.strip()
        records.append({"path": rel, "object_id": oid})
    return records


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
        "files": file_signature(repo),
        "refs": ref_signature(repo),
    }
    digest = hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()
    return {"sha256": digest, **payload}


def build_cache(repo: Path) -> dict[str, Any]:
    return {
        "generated": {
            "default_release": variant_name(default_release(repo)),
            "variants_help": render_help(repo),
        },
        "signature": dependency_signature(repo),
    }


def read_cache(repo: Path) -> dict[str, Any] | None:
    path = repo / CACHE_PATH
    if not path.exists():
        return None
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def cache_current(repo: Path, cache: dict[str, Any] | None) -> bool:
    if not cache:
        return False
    return cache.get("signature", {}).get("sha256") == dependency_signature(repo)["sha256"]


def cached_or_generated(repo: Path) -> tuple[dict[str, Any], bool]:
    cache = read_cache(repo)
    if cache:
        return cache, True
    return build_cache(repo), False


def print_stale_notice() -> None:
    print("[help-cache] cached help data is stale; run make refresh-help-cache", file=sys.stderr)


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))

    if args.refresh:
        cache = build_cache(repo)
        write_json(repo / CACHE_PATH, cache)
        print(f"refreshed help cache in {format_duration(time.monotonic() - started)}")
        return

    if args.verify:
        cache = read_cache(repo)
        if not cache:
            raise ReconstructionError(f"{CACHE_PATH} is missing; run make refresh-help-cache")
        actual = dependency_signature(repo)
        expected = cache.get("signature", {})
        if expected.get("sha256") != actual["sha256"]:
            raise ReconstructionError(
                f"{CACHE_PATH} is stale; run make refresh-help-cache\n"
                f"  cached:  {expected.get('sha256', '<missing>')}\n"
                f"  current: {actual['sha256']}"
            )
        print(f"help cache is current in {format_duration(time.monotonic() - started)}")
        return

    cache, current = cached_or_generated(repo)
    if not current:
        print_stale_notice()
    generated = cache.get("generated", {})
    if args.print_default_release:
        print(str(generated.get("default_release", "<unavailable>")))
    elif args.print_variants:
        print(str(generated.get("variants_help", "")), end="")


if __name__ == "__main__":
    main_wrapper(main)
