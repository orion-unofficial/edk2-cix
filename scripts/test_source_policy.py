#!/usr/bin/env python3
"""Tests for shared source policy checks."""

from __future__ import annotations

import os
import shutil
import tempfile
from pathlib import Path

from reconstruction_common import ReconstructionError
from source_policy import enforce_source_tree_policy
from test_support import git, require, write_file


def make_repo() -> Path:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-policy-test."))
    git(repo, "init", "-b", "test")
    git(repo, "config", "user.name", "Source Policy Test")
    git(repo, "config", "user.email", "source-policy-test")
    write_file(repo, "src/component/file.c", "source\n")
    write_file(repo, "custom/overlay/component/file.c", "custom\n")
    git(repo, "add", ".")
    git(repo, "commit", "-m", "initial")
    return repo


def test_changed_overlay_file_is_allowed() -> None:
    repo = make_repo()
    try:
        enforce_source_tree_policy(repo, ref="HEAD")
    finally:
        shutil.rmtree(repo)


def test_identical_overlay_file_is_rejected_from_ref() -> None:
    repo = make_repo()
    try:
        write_file(repo, "custom/overlay/component/file.c", "source\n")
        git(repo, "add", ".")
        git(repo, "commit", "-m", "make overlay identical")
        try:
            enforce_source_tree_policy(repo, ref="HEAD")
        except ReconstructionError as exc:
            require("byte-identical" in str(exc), str(exc))
        else:
            raise AssertionError("identical overlay file was accepted")
    finally:
        shutil.rmtree(repo)


def test_identical_overlay_file_is_rejected_from_index() -> None:
    repo = make_repo()
    try:
        write_file(repo, "custom/overlay/component/file.c", "source\n")
        git(repo, "add", "custom/overlay/component/file.c")
        try:
            enforce_source_tree_policy(repo, index=True)
        except ReconstructionError as exc:
            require("byte-identical" in str(exc), str(exc))
        else:
            raise AssertionError("identical staged overlay file was accepted")
    finally:
        shutil.rmtree(repo)


def test_matching_overlay_symlink_is_allowed() -> None:
    repo = make_repo()
    try:
        os.remove(repo / "custom/overlay/component/file.c")
        os.symlink("../../../src/component/file.c", repo / "custom/overlay/component/file.c")
        git(repo, "add", ".")
        git(repo, "commit", "-m", "replace overlay with symlink")
        enforce_source_tree_policy(repo, ref="HEAD")
    finally:
        shutil.rmtree(repo)


def test_bad_overlay_symlink_target_is_rejected() -> None:
    repo = make_repo()
    try:
        os.remove(repo / "custom/overlay/component/file.c")
        os.symlink("../wrong/file.c", repo / "custom/overlay/component/file.c")
        git(repo, "add", ".")
        git(repo, "commit", "-m", "bad symlink")
        try:
            enforce_source_tree_policy(repo, ref="HEAD")
        except ReconstructionError as exc:
            require("expected 'src/component/file.c'" in str(exc), str(exc))
        else:
            raise AssertionError("bad symlink target was accepted")
    finally:
        shutil.rmtree(repo)


def main() -> None:
    test_changed_overlay_file_is_allowed()
    test_identical_overlay_file_is_rejected_from_ref()
    test_identical_overlay_file_is_rejected_from_index()
    test_matching_overlay_symlink_is_allowed()
    test_bad_overlay_symlink_target_is_rejected()
    print("source_policy tests passed")


if __name__ == "__main__":
    main()
