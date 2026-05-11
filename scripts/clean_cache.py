#!/usr/bin/env python3
"""Remove generated filesystem caches without touching Git refs."""

from __future__ import annotations

import argparse
import os
import shutil
from pathlib import Path

from reconstruction_common import (
    ReconstructionError,
    cache_dir,
    git,
    main_wrapper,
    release_entries,
    repo_root,
    safe_name,
    truthy,
)


HELP = """clean-cache

Modes:
  stale  Remove rendered worktree cache entries that no longer match any
         current firmware source-target tree ID, plus ephemeral bytecode caches.
  all    Remove all edk2-cix filesystem cache directories.

Variables:
  FORCE=0|1  With mode=all, remove dirty cached worktrees too.
  V=0|1      Print retained cache entries.

This command removes only files under .cache/edk2-cix. It never deletes Git refs.
"""

EPHEMERAL_CACHE_SUBDIRS = ("pycache", "docs/pycache")
CACHE_SUBDIRS = ("tmp", "reports", "signing-certs", "docs", "firmware", "help", *EPHEMERAL_CACHE_SUBDIRS)


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--mode", choices=("stale", "all"), default="stale")
    p.add_argument("--force", default=os.environ.get("FORCE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def expected_worktree_trees(repo: Path) -> dict[str, set[str]]:
    expected: dict[str, set[str]] = {}
    for branch, entry in release_entries(repo).items():
        tree = entry.get("tree_id")
        if not tree:
            continue
        expected.setdefault(safe_name(branch), set()).add(str(tree))
    return expected


def cached_worktrees(repo: Path) -> list[Path]:
    root = cache_dir(repo, "worktrees")
    if not root.exists():
        return []
    return sorted(path for path in root.iterdir() if path.is_dir())


def worktree_dirty(path: Path) -> bool:
    result = git(path, "status", "--porcelain", check=False)
    if result.returncode != 0:
        return False
    return bool(result.stdout.strip())


def worktree_tree(path: Path) -> str | None:
    result = git(path, "rev-parse", "--verify", "HEAD^{tree}", check=False)
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def cache_key(path: Path) -> str:
    name = path.name
    suffix = name.rsplit("-", 1)[-1]
    if len(suffix) == 12 and all(ch in "0123456789abcdef" for ch in suffix.lower()):
        return name[: -(len(suffix) + 1)]
    return name


def remove_path(repo: Path, path: Path) -> None:
    print(f"removing {path}", flush=True)
    result = git(repo, "worktree", "remove", "--force", str(path), check=False)
    if result.returncode != 0 and path.exists():
        shutil.rmtree(path)
    print(f"removed {path}")


def remove_cache_tree(path: Path) -> None:
    print(f"removing {path}", flush=True)
    shutil.rmtree(path)
    print(f"removed {path}")


def clean_ephemeral_caches(repo: Path) -> int:
    root = cache_dir(repo)
    removed = 0
    for name in EPHEMERAL_CACHE_SUBDIRS:
        path = root / name
        if path.exists():
            remove_cache_tree(path)
            removed += 1
    return removed


def clean_stale(repo: Path, verbose: bool) -> None:
    expected = expected_worktree_trees(repo)
    removed = 0
    blocked: list[str] = []
    retained = 0
    for path in cached_worktrees(repo):
        key = cache_key(path)
        current_tree = worktree_tree(path)
        reason = ""
        if key not in expected:
            reason = "no current source target uses this cache key"
        elif current_tree not in expected[key]:
            reason = "tree no longer matches the current source-target manifest"

        if not reason:
            retained += 1
            if verbose:
                print(f"kept {path}")
            continue
        if worktree_dirty(path):
            blocked.append(f"{path}: dirty cached worktree ({reason})")
            continue
        remove_path(repo, path)
        removed += 1
    if blocked:
        details = "\n".join(f"  - {item}" for item in blocked)
        raise ReconstructionError(f"stale cache clean refused dirty entries:\n{details}")
    ephemeral_removed = clean_ephemeral_caches(repo)
    print(
        "stale filesystem cache clean complete: "
        f"{removed} stale worktree(s) removed, {retained} retained, "
        f"{ephemeral_removed} ephemeral cache path(s) removed; Git refs were not touched"
    )


def clean_all(repo: Path, force: bool) -> None:
    blocked = [str(path) for path in cached_worktrees(repo) if worktree_dirty(path)]
    if blocked and not force:
        details = "\n".join(f"  - {item}" for item in blocked)
        raise ReconstructionError(
            "realclean refused dirty cached worktrees; rerun with FORCE=1 if removal is intentional:\n"
            + details
        )
    root = cache_dir(repo)
    removed = 0
    for name in ("worktrees", *CACHE_SUBDIRS):
        path = root / name
        if path.exists():
            remove_path(repo, path)
            removed += 1
    print(f"filesystem cache realclean complete: {removed} cache paths removed; Git refs were not touched")


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    if args.mode == "all":
        clean_all(repo, truthy(args.force))
    else:
        clean_stale(repo, truthy(args.v))


if __name__ == "__main__":
    main_wrapper(main)
