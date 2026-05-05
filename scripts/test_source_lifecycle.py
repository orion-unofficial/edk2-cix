#!/usr/bin/env python3
"""Tests for deterministic source lifecycle projection."""

from __future__ import annotations

import os
import shutil
import tempfile
from pathlib import Path

from source_lifecycle import (
    SourceLifecycle,
    lifecycle_errors,
    project_overlay_tree,
)
from test_support import commit_all, git, require, switch_orphan, write_file


def make_repo() -> Path:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-source-lifecycle-test."))
    git(repo, "init", "-b", "current")
    git(repo, "config", "user.name", "Source Lifecycle Test")
    git(repo, "config", "user.email", "source-lifecycle-test")
    return repo


def symlink(repo: Path, target: str, link: str) -> None:
    path = repo / link
    path.parent.mkdir(parents=True, exist_ok=True)
    os.symlink(target, path)


def test_same_path_projection_keeps_overlay_path() -> None:
    repo = make_repo()
    try:
        write_file(repo, "src/component/file.c", "source\n")
        symlink(repo, "../../../src/component/file.c", "custom/overlay/component/file.c")
        commit_all(repo, "current")
        git(repo, "switch", "-c", "older")

        projections = project_overlay_tree(repo, "current", "older")
        require(not lifecycle_errors(projections), "same-path projection should not fail")
        require(projections[0].action == "keep", projections[0].action)
        require(projections[0].target_overlay_path == "custom/overlay/component/file.c", str(projections[0]))
    finally:
        shutil.rmtree(repo)


def test_exact_rename_projection_renames_overlay_path() -> None:
    repo = make_repo()
    try:
        write_file(repo, "src/component/new.c", "same content\n")
        symlink(repo, "../../../src/component/new.c", "custom/overlay/component/new.c")
        commit_all(repo, "current")

        git(repo, "switch", "--orphan", "older")
        for path in repo.iterdir():
            if path.name == ".git":
                continue
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()
        write_file(repo, "src/component/old.c", "same content\n")
        commit_all(repo, "older")

        mapping = SourceLifecycle(repo, "current", "older").map_source_path("src/component/new.c")
        require(mapping.kind == "exact-rename", mapping.kind)
        require(mapping.target_path == "src/component/old.c", str(mapping))
        projections = project_overlay_tree(repo, "current", "older")
        require(not lifecycle_errors(projections), "exact rename should project cleanly")
        require(projections[0].action == "rename", projections[0].action)
        require(projections[0].target_overlay_path == "custom/overlay/component/old.c", str(projections[0]))
    finally:
        shutil.rmtree(repo)


def test_ambiguous_exact_rename_is_an_error() -> None:
    repo = make_repo()
    try:
        write_file(repo, "src/component/new.c", "same content\n")
        symlink(repo, "../../../src/component/new.c", "custom/overlay/component/new.c")
        commit_all(repo, "current")

        switch_orphan(repo, "older")
        write_file(repo, "src/component/old-a.c", "same content\n")
        write_file(repo, "src/component/old-b.c", "same content\n")
        commit_all(repo, "older")

        projections = project_overlay_tree(repo, "current", "older")
        errors = lifecycle_errors(projections)
        require(len(errors) == 1, f"expected one error, got {errors}")
        require(errors[0].action == "ambiguous-rename", errors[0].action)
    finally:
        shutil.rmtree(repo)


def test_deleted_mirror_symlink_can_be_dropped() -> None:
    repo = make_repo()
    try:
        write_file(repo, "src/component/later.c", "source\n")
        symlink(repo, "../../../src/component/later.c", "custom/overlay/component/later.c")
        commit_all(repo, "current")

        switch_orphan(repo, "older")
        write_file(repo, "README.md", "older\n")
        commit_all(repo, "older")

        projections = project_overlay_tree(repo, "current", "older")
        require(not lifecycle_errors(projections), "deleted mirror should be safe to drop")
        require(projections[0].action == "drop-mirror", projections[0].action)
        require(projections[0].target_overlay_path is None, str(projections[0]))
    finally:
        shutil.rmtree(repo)


def test_deleted_non_mirror_overlay_is_an_error() -> None:
    repo = make_repo()
    try:
        write_file(repo, "src/component/later.c", "source\n")
        write_file(repo, "custom/overlay/component/later.c", "modified source\n")
        commit_all(repo, "current")

        switch_orphan(repo, "older")
        write_file(repo, "README.md", "older\n")
        commit_all(repo, "older")

        projections = project_overlay_tree(repo, "current", "older")
        errors = lifecycle_errors(projections)
        require(len(errors) == 1, f"expected one error, got {errors}")
        require(errors[0].action == "non-mirror-source-deleted", errors[0].action)
    finally:
        shutil.rmtree(repo)


def test_custom_only_overlay_file_is_kept() -> None:
    repo = make_repo()
    try:
        write_file(repo, "custom/overlay/component/generated.bin", "payload\n")
        commit_all(repo, "current")

        switch_orphan(repo, "older")
        write_file(repo, "README.md", "older\n")
        commit_all(repo, "older")

        projections = project_overlay_tree(repo, "current", "older")
        require(not lifecycle_errors(projections), "custom-only overlay should be retained")
        require(projections[0].action == "keep-custom-file", projections[0].action)
    finally:
        shutil.rmtree(repo)


def test_broken_mirror_symlink_is_an_error() -> None:
    repo = make_repo()
    try:
        symlink(repo, "../../../src/component/missing.c", "custom/overlay/component/missing.c")
        commit_all(repo, "current")

        switch_orphan(repo, "older")
        write_file(repo, "README.md", "older\n")
        commit_all(repo, "older")

        projections = project_overlay_tree(repo, "current", "older")
        errors = lifecycle_errors(projections)
        require(len(errors) == 1, f"expected one error, got {errors}")
        require(errors[0].action == "broken-source-mirror", errors[0].action)
    finally:
        shutil.rmtree(repo)


def main() -> None:
    test_same_path_projection_keeps_overlay_path()
    test_exact_rename_projection_renames_overlay_path()
    test_ambiguous_exact_rename_is_an_error()
    test_deleted_mirror_symlink_can_be_dropped()
    test_deleted_non_mirror_overlay_is_an_error()
    test_custom_only_overlay_file_is_kept()
    test_broken_mirror_symlink_is_an_error()
    print("source_lifecycle tests passed")


if __name__ == "__main__":
    main()
