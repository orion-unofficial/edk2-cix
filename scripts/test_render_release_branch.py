#!/usr/bin/env python3
"""Regression tests for release-worktree cache handling."""

from __future__ import annotations

import shutil
import sys
import tempfile
from pathlib import Path

from test_support import commit_all, git, load_function_tests, require, write_file


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from render_release_branch import cached_worktree_is_dirty  # noqa: E402


def make_repo() -> Path:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-render-release-test."))
    git(repo, "init", "-b", "build")
    git(repo, "config", "user.name", "Render Release Test")
    git(repo, "config", "user.email", "render-release-test")
    write_file(repo, ".gitattributes", "*.txt text eol=crlf\n")
    commit_all(repo, "configure line endings")
    return repo


def test_cache_ignores_only_raw_byte_identical_normalization_noise() -> None:
    repo = make_repo()
    try:
        path = repo / "legacy.txt"
        path.write_bytes(b"one\r\ntwo\r\n")
        object_id = git(repo, "hash-object", "-w", "--no-filters", "legacy.txt").stdout.strip()
        git(repo, "update-index", "--add", "--cacheinfo", f"100644,{object_id},legacy.txt")
        git(repo, "commit", "-m", "record legacy CRLF blob")

        require(git(repo, "status", "--porcelain").stdout, "fixture should trigger Git normalization noise")
        require(
            not cached_worktree_is_dirty(repo),
            "raw-byte-identical tracked file should not dirty the cache",
        )

        path.write_bytes(b"changed\r\n")
        require(cached_worktree_is_dirty(repo), "real tracked content change was ignored")
    finally:
        shutil.rmtree(repo)


def test_cache_rejects_staged_deleted_and_untracked_changes() -> None:
    repo = make_repo()
    try:
        write_file(repo, "tracked.txt", "tracked\n")
        commit_all(repo, "add tracked file")

        write_file(repo, "untracked.txt", "untracked\n")
        require(cached_worktree_is_dirty(repo), "untracked file was ignored")
        (repo / "untracked.txt").unlink()

        (repo / "tracked.txt").unlink()
        require(cached_worktree_is_dirty(repo), "deleted file was ignored")
        git(repo, "restore", "tracked.txt")

        write_file(repo, "tracked.txt", "staged\n")
        git(repo, "add", "tracked.txt")
        require(cached_worktree_is_dirty(repo), "staged change was ignored")
    finally:
        shutil.rmtree(repo)


def load_tests(_loader, _tests, _pattern):
    return load_function_tests(globals())


def main() -> None:
    test_cache_ignores_only_raw_byte_identical_normalization_noise()
    test_cache_rejects_staged_deleted_and_untracked_changes()
    print("release worktree cache regression tests passed")


if __name__ == "__main__":
    main()
