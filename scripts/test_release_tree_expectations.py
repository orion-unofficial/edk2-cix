#!/usr/bin/env python3
"""Exercise source rendering with stale metadata and no generated cache refs."""

from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from reconstruction_common import (
    ReconstructionError,
    clear_metadata_caches,
    matrix_release_branches,
    release_entries,
    release_entry,
    synthesise_release_entry,
    tree_id,
)
from render_release_branch import apply_release_metadata, render_from_plan, validate_release_metadata
from test_support import commit_all, git, run, write_file


ROOT = Path(__file__).resolve().parents[1]
PREFIX = "source/cache/release/custom/"


class ReleaseTreeExpectationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="edk2-cix-tree-expectations.")
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name) / "origin"
        self.repo.mkdir()
        git(self.repo, "init", "-b", "build")
        git(self.repo, "config", "user.name", "Render Test")
        git(self.repo, "config", "user.email", "render-test")
        write_file(self.repo, "VERSION", "1.2.1\n")
        write_file(self.repo, "debian/changelog", "edk2-cix (1.2.1) main; urgency=medium\n")
        write_file(self.repo, "debian/control", "preserve packaging\n")
        write_file(self.repo, "src/payload", "preserve firmware\n")
        # Stop at the firmware-build boundary: exercise the real top-level
        # Makefile, renderer, worktree preparation and output mirroring.
        write_file(self.repo, "Makefile", (
            "buildbox-firmware-build deterministic-replay:\n"
            "\t@mkdir -p src/Build/$(FIRMWARE_BOARD)/$(FIRMWARE_TARGET)_GCC\n"
            "\t@printf '%s\\n' 'ARTEFACT_MODE=$(ARTEFACT_MODE)' 'ENABLE_FIRMWARE_FIXES=$(ENABLE_FIRMWARE_FIXES)' 'CIX_RELEASE=$(CIX_RELEASE)' > src/Build/$(FIRMWARE_BOARD)/$(FIRMWARE_TARGET)_GCC/BuildOptions\n"
        ))
        write_file(self.repo, "src/Makefile", "# -vw 6084 -vw 6161 -vw 6033 -vw 6049 -vw 6050\n")
        write_file(self.repo, "scripts/ensure_build_deps.sh", "common_packages=(\n    python3\n)\n")
        self.source = commit_all(self.repo, "source with stale package metadata")
        for edk2 in ("202605", "202608"):
            git(self.repo, "branch", f"source/base/edk2/edk2-stable{edk2}", self.source)
            for radxa in ("1.2.4", "1.3.1"):
                git(self.repo, "branch", f"source/unofficial/{radxa}/edk2-stable{edk2}", self.source)
        for component in ("tf-a", "op-tee"):
            git(self.repo, "branch", f"source/vendor/cix/1.2/{component}", self.source)
        for radxa in ("1.2.4", "1.3.1"):
            write_file(self.repo, "debian/changelog", f"edk2-cix ({radxa}) main; urgency=medium\n")
            vendor = commit_all(self.repo, f"Radxa {radxa} metadata")
            git(self.repo, "branch", f"source/vendor/radxa/{radxa}/edk2-stable202208", vendor)
        write_file(self.repo, "config/policies.json", "{}\n")
        write_file(self.repo, "config/refs-source-target-cache.json", '{"refs": []}\n')
        clear_metadata_caches()
        self.addCleanup(clear_metadata_caches)
        self.environment = patch.dict(os.environ, {"EDK2_CIX_TMP_ROOT": str(Path(self.temp.name) / "tmp")})
        self.environment.start()
        self.addCleanup(self.environment.stop)

    def assert_render(self, repo: Path, target: str) -> str:
        branch, entry = release_entry(repo, target, require=True)
        before_index = git(repo, "write-tree").stdout
        before_refs = git(repo, "show-ref").stdout
        rendered = render_from_plan(repo, branch, entry, verbose=False)
        self.assertEqual(tree_id(repo, rendered), entry["tree_id"])
        validate_release_metadata(repo, rendered, entry, branch)
        self.assertEqual(git(repo, "show", f"{rendered}:src/payload").stdout, "preserve firmware\n")
        self.assertEqual(git(repo, "show", f"{rendered}:debian/control").stdout, "preserve packaging\n")
        self.assertEqual(git(repo, "write-tree").stdout, before_index)
        self.assertEqual(git(repo, "show-ref").stdout, before_refs)
        return tree_id(repo, rendered)

    def test_every_custom_tuple_and_alias_renders_without_cache_records(self) -> None:
        branches, _ = matrix_release_branches(self.repo)
        custom = sorted(branch for branch in branches if branch.startswith(PREFIX))
        self.assertEqual(len(custom), 16)
        for branch in custom:
            with self.subTest(target=branch):
                self.assert_render(self.repo, branch)

    def test_fresh_clone_uses_remote_refs_and_does_not_need_generated_branches(self) -> None:
        commit_all(self.repo, "build metadata")
        clone = Path(self.temp.name) / "clone"
        run(["git", "clone", "--no-local", str(self.repo), str(clone)], cwd=self.repo)
        self.assertEqual(git(clone, "branch", "--format=%(refname:short)").stdout.strip(), "build")
        self.assertEqual(git(clone, "for-each-ref", "refs/remotes/origin/source/cache").stdout, "")
        self.assert_render(clone, "edk2-202605/radxa-1.3.1/unofficial")

    def test_make_build_from_fresh_clone_preserves_the_reported_arguments(self) -> None:
        shutil.copy2(ROOT / "Makefile", self.repo / "Makefile")
        shutil.copytree(ROOT / "scripts", self.repo / "scripts", dirs_exist_ok=True, ignore=shutil.ignore_patterns("__pycache__"))
        commit_all(self.repo, "real build orchestration")
        clone = Path(self.temp.name) / "make-clone"
        run(["git", "clone", "--no-local", str(self.repo), str(clone)], cwd=self.repo)
        result = run([
            "make", "build",
            "RELEASE=edk2-202605/radxa-1.3.1/unofficial",
            "ARTEFACT_MODE=custom", "FIRMWARE_BOARD=O6", "FIRMWARE_TARGET=RELEASE",
            "FIRMWARE_DISTRO=trixie", "ENABLE_FIRMWARE_FIXES=false", "ENABLE_CORE_ORDER=cix",
            "ENABLE_EXPERIMENTAL_UEFI_SETTINGS=false", "DEBUG_VERBOSE=false", "CIX_RELEASE=",
        ], cwd=clone, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        outputs = list((clone / "dist").rglob("BuildOptions"))
        self.assertEqual(len(outputs), 1)
        self.assertEqual(outputs[0].read_text(), "ARTEFACT_MODE=custom\nENABLE_FIRMWARE_FIXES=false\nCIX_RELEASE=\n")

    def test_all_user_profiles_reach_firmware_delegation_from_fresh_clone(self) -> None:
        for ref in ("source/base/edk2/edk2-stable202208", "source/unofficial/edk2-stable202208", "source/unofficial/1.3/current"):
            git(self.repo, "branch", ref, self.source)
        policy = json.loads((ROOT / "config/policies.json").read_text())
        policy["unofficial_source_policy"] = {
            "default_line": "1.3",
            "lines": {"1.3": {
                "current_ref": "source/unofficial/1.3/current",
                "current_edk2_release": "202608",
                "current_radxa_release": "1.3.1",
                "current_cix_release": "1.2",
            }},
        }
        write_file(self.repo, "config/policies.json", json.dumps(policy))
        shutil.copy2(ROOT / "Makefile", self.repo / "Makefile")
        shutil.copytree(ROOT / "scripts", self.repo / "scripts", dirs_exist_ok=True, ignore=shutil.ignore_patterns("__pycache__"))
        commit_all(self.repo, "real profile orchestration")
        for profile in ("", *policy["firmware_profile_policy"]["profiles"]):
            with self.subTest(profile=profile or "default"):
                clone = Path(self.temp.name) / f"profile-{profile or 'default'}"
                run(["git", "clone", "--no-local", str(self.repo), str(clone)], cwd=self.repo)
                args = ["make", "FIRMWARE_BOARD=O6", "REPLAY_DOWNLOAD=0"]
                if profile:
                    args.append(f"PROFILE={profile}")
                result = run(args, cwd=clone, check=False)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(len(list((clone / "dist").rglob("BuildOptions"))), 1)

    def test_aligned_metadata_and_missing_metadata_files_render(self) -> None:
        target = PREFIX + "edk2-202605/radxa-1.3.1/unofficial"
        source_ref = "source/unofficial/1.3.1/edk2-stable202605"
        for missing in (False, True):
            with self.subTest(missing=missing):
                if missing:
                    git(self.repo, "rm", "VERSION", "debian/changelog")
                else:
                    write_file(self.repo, "VERSION", "1.3.1\n")
                source = commit_all(self.repo, "metadata fixture")
                git(self.repo, "update-ref", f"refs/heads/{source_ref}", source)
                clear_metadata_caches()
                rendered_tree = self.assert_render(self.repo, target)
                if not missing:
                    self.assertEqual(rendered_tree, tree_id(self.repo, source_ref))

    def test_current_line_manifest_uses_metadata_adjusted_tree(self) -> None:
        git(self.repo, "branch", "source/unofficial/1.3/current", self.source)
        write_file(self.repo, "config/policies.json", json.dumps({
            "unofficial_source_policy": {
                "default_line": "1.3",
                "lines": {"1.3": {
                    "current_ref": "source/unofficial/1.3/current",
                    "current_edk2_release": "202608",
                    "current_radxa_release": "1.3.1",
                    "current_cix_release": "1.2",
                }},
            },
        }))
        target = PREFIX + "edk2-202608/cix-1.2/radxa-1.3.1/unofficial"
        write_file(self.repo, "config/refs-source-target-cache.json", json.dumps({
            "refs": [{"ref": target, "tree_id": "0" * 40}],
        }))
        clear_metadata_caches()
        for suffix in ("", "-1.3.1"):
            with self.subTest(alias=suffix):
                self.assert_render(self.repo, target + suffix)

    def test_aligned_crlf_version_matches_the_expected_tree(self) -> None:
        (self.repo / "VERSION").write_bytes(b"1.3.1\r\n")
        source = commit_all(self.repo, "aligned version with CRLF")
        source_ref = "source/unofficial/1.3.1/edk2-stable202605"
        git(self.repo, "update-ref", f"refs/heads/{source_ref}", source)
        clear_metadata_caches()
        self.assert_render(self.repo, PREFIX + "edk2-202605/radxa-1.3.1/unofficial")

    def test_manifest_integrity_check_still_rejects_an_incorrect_expected_tree(self) -> None:
        target = PREFIX + "edk2-202605/cix-1.2/radxa-1.3.1/unofficial"
        write_file(self.repo, "config/refs-source-target-cache.json", json.dumps({
            "refs": [{"ref": target, "tree_id": "0" * 40}],
        }))
        clear_metadata_caches()
        entry = synthesise_release_entry(self.repo, target)
        with self.assertRaisesRegex(ReconstructionError, "does not match manifest"):
            render_from_plan(self.repo, target, entry, verbose=False)

    def test_unexpected_firmware_changes_still_fail_the_tree_check(self) -> None:
        target = PREFIX + "edk2-202605/radxa-1.3.1/unofficial"
        entry = synthesise_release_entry(self.repo, target)

        def unexpected_change(repo, worktree, ref, release, verbose):
            apply_release_metadata(repo, worktree, ref, release, verbose)
            write_file(worktree, "src/payload", "unexpected firmware change\n")
            git(worktree, "add", "src/payload")

        with patch("render_release_branch.apply_release_metadata", side_effect=unexpected_change):
            with self.assertRaisesRegex(ReconstructionError, "does not match manifest"):
                render_from_plan(self.repo, target, entry, verbose=False)


class SupportedReleaseTreeTests(unittest.TestCase):
    def test_all_supported_custom_expectations_match_git_index_rendering(self) -> None:
        """Check real manifests, including targets with no retained cache ref.

        Use an isolated Git index as an independent oracle for the tree hash;
        only the two metadata paths are changed, without firmware checkouts.
        """
        entries = release_entries(ROOT)
        custom = {branch: entry for branch, entry in entries.items() if entry.get("unofficial_delta")}
        self.assertTrue(custom)
        with tempfile.TemporaryDirectory(prefix="edk2-cix-matrix-trees.") as tmp:
            repo = Path(tmp)
            git(repo, "init", "-b", "test")
            objects = git(ROOT, "rev-parse", "--path-format=absolute", "--git-path", "objects").stdout.strip()
            write_file(repo, ".git/objects/info/alternates", objects + "\n")
            rendered = {}
            versions = {}
            loaded_tree = None
            for branch, entry in custom.items():
                with self.subTest(target=branch):
                    steps = entry["render"]["steps"]
                    self.assertEqual([list(step) for step in steps], [["release_metadata"]])
                    metadata = steps[0]["release_metadata"]
                    source_tree = tree_id(ROOT, entry["source_ref"])
                    metadata_tree = tree_id(ROOT, metadata["ref"])
                    release = metadata["release"]
                    key = (source_tree, metadata_tree, release)
                    if key not in rendered:
                        if loaded_tree != source_tree:
                            git(repo, "read-tree", source_tree)
                            git(repo, "update-index", "--force-remove", ".gitmodules")
                            loaded_tree = source_tree
                        if release not in versions:
                            versions[release] = subprocess.run(
                                ["git", "-C", str(repo), "hash-object", "-w", "--stdin"],
                                input=f"{release}\n", text=True, capture_output=True, check=True,
                            ).stdout.strip()
                        version_info = git(repo, "ls-files", "-s", "--", "VERSION").stdout
                        mode = version_info.split()[0] if version_info else "100644"
                        changelog = git(repo, "ls-tree", metadata_tree, "--", "debian/changelog").stdout
                        changelog_mode, _, changelog_oid = changelog.split("\t", 1)[0].split()
                        git(repo, "update-index", "--add", "--cacheinfo", f"{mode},{versions[release]},VERSION")
                        git(repo, "update-index", "--add", "--cacheinfo", f"{changelog_mode},{changelog_oid},debian/changelog")
                        rendered[key] = git(repo, "write-tree").stdout.strip()
                    self.assertEqual(entry.get("tree_id"), rendered[key])


if __name__ == "__main__":
    unittest.main()
