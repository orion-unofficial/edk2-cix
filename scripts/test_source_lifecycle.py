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
    normalise_overlay_lifecycle,
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


def test_normalise_exact_mirror_rename_retargets_symlink() -> None:
    repo = make_repo()
    try:
        write_file(repo, "src/component/new.c", "same content\n")
        symlink(repo, "../../../src/component/new.c", "custom/overlay/component/new.c")
        commit_all(repo, "current")

        switch_orphan(repo, "older")
        write_file(repo, "src/component/old.c", "same content\n")
        commit_all(repo, "older")

        git(repo, "switch", "-c", "scratch", "older")
        symlink(repo, "../../../src/component/new.c", "custom/overlay/component/new.c")
        git(repo, "add", "custom/overlay/component/new.c")

        normalise_overlay_lifecycle(
            repo,
            source_repo=repo,
            from_ref="current",
            to_ref="older",
            paths=["custom/overlay/component/new.c"],
            mode="exact",
        )
        require(
            os.readlink(repo / "custom/overlay/component/old.c") == "../../../src/component/old.c",
            "mirror symlink was not retargeted to the older source path",
        )
        require(not (repo / "custom/overlay/component/new.c").exists(), "old overlay path was not removed")
    finally:
        shutil.rmtree(repo)


def test_normalise_validate_reports_required_changes_without_mutating() -> None:
    repo = make_repo()
    try:
        write_file(repo, "src/component/new.c", "same content\n")
        symlink(repo, "../../../src/component/new.c", "custom/overlay/component/new.c")
        commit_all(repo, "current")

        switch_orphan(repo, "older")
        write_file(repo, "src/component/old.c", "same content\n")
        commit_all(repo, "older")

        git(repo, "switch", "-c", "scratch", "older")
        symlink(repo, "../../../src/component/new.c", "custom/overlay/component/new.c")
        git(repo, "add", "custom/overlay/component/new.c")

        try:
            normalise_overlay_lifecycle(
                repo,
                source_repo=repo,
                from_ref="current",
                to_ref="older",
                paths=["custom/overlay/component/new.c"],
                mode="validate",
            )
        except Exception as exc:
            require("source lifecycle normalisation is required" in str(exc), str(exc))
        else:
            raise AssertionError("validate mode should report required normalisation")
        require((repo / "custom/overlay/component/new.c").is_symlink(), "validate mode mutated the scratch tree")
    finally:
        shutil.rmtree(repo)


def test_normalise_exact_regular_overlay_rename() -> None:
    repo = make_repo()
    try:
        write_file(repo, "src/component/new.c", "same content\n")
        write_file(repo, "custom/overlay/component/new.c", "patched content\n")
        commit_all(repo, "current")

        switch_orphan(repo, "older")
        write_file(repo, "src/component/old.c", "same content\n")
        commit_all(repo, "older")

        git(repo, "switch", "-c", "scratch", "older")
        write_file(repo, "custom/overlay/component/new.c", "patched content\n")
        git(repo, "add", "custom/overlay/component/new.c")

        normalise_overlay_lifecycle(
            repo,
            source_repo=repo,
            from_ref="current",
            to_ref="older",
            paths=["custom/overlay/component/new.c"],
            mode="exact",
        )
        require(git(repo, "show", ":custom/overlay/component/old.c").stdout == "patched content\n", "regular overlay was not renamed")
        require(git(repo, "ls-files", "--error-unmatch", "custom/overlay/component/new.c", check=False).returncode != 0, "old regular overlay path is still staged")
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
    test_normalise_exact_mirror_rename_retargets_symlink()
    test_normalise_validate_reports_required_changes_without_mutating()
    test_normalise_exact_regular_overlay_rename()
    print("source_lifecycle tests passed")


if __name__ == "__main__":
    main()
