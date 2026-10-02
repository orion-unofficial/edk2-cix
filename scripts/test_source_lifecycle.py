#!/usr/bin/env python3
"""Tests for deterministic source lifecycle projection."""

from __future__ import annotations

import json
import os
import shutil
import tempfile
from pathlib import Path

from source_lifecycle import (
    SourceLifecycle,
    lifecycle_errors,
    normalisation_blockers,
    normalise_overlay_lifecycle,
    project_overlay_tree,
)
from reconstruction_common import ReconstructionError, clear_metadata_caches, tree_id
from test_support import commit_all, git, load_function_tests, require, switch_orphan, write_file


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


def test_remote_tracking_source_ref_is_resolved() -> None:
    repo = make_repo()
    try:
        write_file(repo, "src/component/file.c", "source\n")
        symlink(repo, "../../../src/component/file.c", "custom/overlay/component/file.c")
        current = commit_all(repo, "current")
        git(repo, "update-ref", "refs/remotes/origin/source/unofficial/1.3/current", current)
        git(repo, "switch", "-c", "older")

        projections = project_overlay_tree(repo, "source/unofficial/1.3/current", "older")
        require(not lifecycle_errors(projections), "remote-tracking source ref should resolve")
        require(projections[0].action == "keep", projections[0].action)
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


def test_replay_keeps_new_source_and_mirror_but_drops_missing_counterpart() -> None:
    repo = make_repo()
    try:
        source = "src/component/new.h"
        overlay = "custom/overlay/component/new.h"
        write_file(repo, source, "new interface\n")
        symlink(repo, "../../../src/component/new.h", overlay)
        commit_all(repo, "current")
        switch_orphan(repo, "older")
        write_file(repo, "README.md", "older\n")
        commit_all(repo, "older")
        git(repo, "checkout", "current", "--", source, overlay)
        projections = normalise_overlay_lifecycle(
            repo, source_repo=repo, from_ref="current", to_ref="older", paths=[overlay], mode="exact",
        )
        require(projections[0].action == "keep", str(projections))
        require((repo / overlay).read_text() == "new interface\n", "new source mirror was removed")
        git(repo, "rm", "-f", source)
        projections = normalise_overlay_lifecycle(
            repo, source_repo=repo, from_ref="current", to_ref="older", paths=[overlay], mode="exact",
        )
        require(projections[0].action == "drop-mirror", str(projections))
        require(not (repo / overlay).is_symlink(), "orphaned mirror was retained")
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


def test_normalisation_blockers_respect_modes() -> None:
    repo = make_repo()
    try:
        write_file(repo, "src/component/new.c", "same content\n")
        write_file(repo, "custom/overlay/component/new.c", "patched content\n")
        commit_all(repo, "current")

        switch_orphan(repo, "older")
        write_file(repo, "src/component/old.c", "same content\n")
        commit_all(repo, "older")

        projections = project_overlay_tree(repo, "current", "older")
        require(not normalisation_blockers(projections, "exact"), "exact mode should accept exact regular overlay renames")
        mirror_blockers = normalisation_blockers(projections, "mirror")
        require(len(mirror_blockers) == 1, f"expected one mirror-mode blocker, got {mirror_blockers}")
        require(mirror_blockers[0].action == "rename", mirror_blockers[0].action)
        validate_blockers = normalisation_blockers(projections, "validate")
        require(len(validate_blockers) == 1, f"expected one validate-mode blocker, got {validate_blockers}")
        require(validate_blockers[0].action == "rename", validate_blockers[0].action)
    finally:
        shutil.rmtree(repo)


def ownership_fixture():
    repo = make_repo()
    base = "source/port/radxa/1.3.1/edk2-stable202608"
    source = "source/unofficial/1.3.1/edk2-stable202608"
    write_file(repo, "src/component/vendor.c", "vendor source\n")
    original = commit_all(repo, "vendor port")
    git(repo, "branch", base, original)
    git(repo, "switch", "-c", source)
    write_file(repo, "src/component/custom.c", "custom src addition\n")
    write_file(repo, "custom/overlay/component/custom.c", "custom override\n")
    write_file(repo, "custom/overlay/component/vendor.c", "modified vendor source\n")
    custom = commit_all(repo, "custom source additions")
    write_file(repo, "config/refs-unofficial.json", json.dumps({"refs": [{
        "ref": source, "type": "unofficial-release-checkpoint", "radxa_source_ref": base,
        "object_id": custom, "tree_id": tree_id(repo, custom),
    }]}))
    write_file(repo, "config/refs-radxa.json", json.dumps({"refs": [{
        "ref": base, "type": "ported-vendor-source",
        "object_id": original, "tree_id": tree_id(repo, original),
    }]}))
    commit_all(repo, "record ownership provenance")
    # Keep source identity pinned to the pre-metadata firmware tree.
    git(repo, "branch", "metadata", "HEAD")
    git(repo, "switch", "metadata")
    git(repo, "branch", "-f", source, custom)
    clear_metadata_caches()
    return repo, base, source


def test_recorded_custom_src_addition_is_not_a_vendor_deletion():
    repo, base, source = ownership_fixture()
    try:
        default = project_overlay_tree(repo, source, base)
        require(any(row.action == "non-mirror-source-deleted" for row in default), "default deletion guard weakened")
        proven = project_overlay_tree(repo, source, base, source_base_ref=base)
        require(not lifecycle_errors(proven), str(proven))
        require(any(row.action == "keep-custom-source-addition" for row in proven), "custom ownership not recognized")
    finally:
        clear_metadata_caches()
        shutil.rmtree(repo)


def test_custom_addition_collision_rejects_even_whitespace_only_destination_change():
    repo, base, source = ownership_fixture()
    try:
        git(repo, "switch", "-c", "destination", base)
        write_file(repo, "src/component/custom.c", "custom src addition\r\n")
        commit_all(repo, "destination custom path collision")
        git(repo, "switch", "metadata")
        proven = project_overlay_tree(repo, source, "destination", source_base_ref=base)
        require(any(row.action == "custom-source-collision" for row in proven), "byte-different collision accepted")
    finally:
        clear_metadata_caches()
        shutil.rmtree(repo)


def test_recorded_baseline_does_not_allow_vendor_source_deletion():
    repo, base, source = ownership_fixture()
    try:
        git(repo, "switch", "-c", "destination", base)
        git(repo, "rm", "src/component/vendor.c")
        commit_all(repo, "vendor removes original source")
        git(repo, "switch", "metadata")
        proven = project_overlay_tree(repo, source, "destination", source_base_ref=base)
        require(any(row.action == "non-mirror-source-deleted" for row in proven), "genuine vendor deletion accepted")
    finally:
        clear_metadata_caches()
        shutil.rmtree(repo)


def test_false_or_rebound_ownership_baseline_fails_closed():
    repo, base, source = ownership_fixture()
    try:
        try:
            project_overlay_tree(repo, source, base, source_base_ref=source)
        except ReconstructionError:
            pass
        else:
            raise AssertionError("false baseline accepted")
        git(repo, "branch", "-f", base, source)
        clear_metadata_caches()
        try:
            project_overlay_tree(repo, source, base, source_base_ref=base)
        except ReconstructionError:
            pass
        else:
            raise AssertionError("moved vendor baseline accepted")
    finally:
        clear_metadata_caches()
        shutil.rmtree(repo)


def test_custom_source_mirror_requires_exact_addition_carried_into_destination():
    repo, base, source = ownership_fixture()
    try:
        git(repo, "switch", "source/unofficial/1.3.1/edk2-stable202608")
        git(repo, "rm", "custom/overlay/component/custom.c")
        symlink(repo, "../../../src/component/custom.c", "custom/overlay/component/custom.c")
        mirrored = commit_all(repo, "mirror custom source addition")
        git(repo, "switch", "metadata")
        data = json.loads((repo / "config/refs-unofficial.json").read_text())
        data["refs"][0].update(object_id=mirrored, tree_id=tree_id(repo, mirrored))
        write_file(repo, "config/refs-unofficial.json", json.dumps(data))
        clear_metadata_caches()
        missing = project_overlay_tree(repo, source, base, source_base_ref=base)
        require(any(row.action == "broken-custom-source-mirror" for row in missing), "dangling custom mirror accepted")
        carried = project_overlay_tree(repo, source, source, source_base_ref=base)
        require(not lifecycle_errors(carried), str(carried))
        require(any(row.mirror and row.action == "keep" for row in carried), "valid custom mirror dropped")
    finally:
        clear_metadata_caches()
        shutil.rmtree(repo)


def main() -> None:
    test_same_path_projection_keeps_overlay_path()
    test_remote_tracking_source_ref_is_resolved()
    test_exact_rename_projection_renames_overlay_path()
    test_ambiguous_exact_rename_is_an_error()
    test_deleted_mirror_symlink_can_be_dropped()
    test_replay_keeps_new_source_and_mirror_but_drops_missing_counterpart()
    test_deleted_non_mirror_overlay_is_an_error()
    test_custom_only_overlay_file_is_kept()
    test_broken_mirror_symlink_is_an_error()
    test_normalise_exact_mirror_rename_retargets_symlink()
    test_normalise_validate_reports_required_changes_without_mutating()
    test_normalise_exact_regular_overlay_rename()
    test_normalisation_blockers_respect_modes()
    print("source_lifecycle tests passed")


def load_tests(loader, tests, pattern):
    return load_function_tests(globals())


if __name__ == "__main__":
    main()
