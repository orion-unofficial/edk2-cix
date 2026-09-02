#!/usr/bin/env python3
"""Regression tests for minimised-clone ref selection."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from create_minimised_clone import required_refspecs  # noqa: E402
from test_support import commit_all, git, write_file  # noqa: E402


class MinimisedCloneRefTests(unittest.TestCase):
    def test_export_uses_checked_out_candidate_as_build(self) -> None:
        with tempfile.TemporaryDirectory(
            prefix="edk2-cix-minimised-build-ref-test."
        ) as directory:
            repo = Path(directory)
            git(repo, "init", "-q", "-b", "build")
            git(repo, "config", "user.name", "Test User")
            git(repo, "config", "user.email", "minimised-build-ref-test")
            write_file(repo, "fixture", "build\n")
            commit_all(repo, "build fixture")
            git(repo, "switch", "-q", "-c", "test")
            write_file(repo, "test-only", "must not be exported as build\n")
            commit_all(repo, "test fixture")

            selected = dict(required_refspecs(repo))

            self.assertEqual(
                selected["HEAD"],
                "refs/heads/build",
            )
            self.assertNotIn("refs/heads/build", selected)

    def test_export_omits_stale_remote_compatibility_refs(self) -> None:
        with tempfile.TemporaryDirectory(prefix="edk2-cix-minimised-refs-test.") as directory:
            repo = Path(directory)
            git(repo, "init", "-q", "-b", "build")
            git(repo, "config", "user.name", "Test User")
            git(repo, "config", "user.email", "minimised-refs-test")
            write_file(repo, "fixture", "source\n")
            commit = commit_all(repo, "fixture")
            refs = (
                "refs/heads/source/vendor/radxa/1.3.1/edk2-stable202208",
                "refs/heads/source/component/cix/1.2/bios",
                "refs/heads/source/unofficial/current",
                "refs/heads/source/cache/release/generated",
                "refs/remotes/origin/source/base/edk2/edk2-stable202608",
                "refs/remotes/origin/source/component/cix/1.2/op-tee",
                "refs/remotes/origin/source/unofficial/current",
            )
            for ref in refs:
                git(repo, "update-ref", ref, commit)
            git(repo, "tag", "source/unofficial/edk2/stable-202608", commit)

            selected = dict(required_refspecs(repo))

            self.assertEqual(selected["HEAD"], "refs/heads/build")
            self.assertIn("refs/heads/source/vendor/radxa/1.3.1/edk2-stable202208", selected)
            self.assertEqual(
                selected["refs/remotes/origin/source/base/edk2/edk2-stable202608"],
                "refs/heads/source/base/edk2/edk2-stable202608",
            )
            self.assertIn("refs/tags/source/unofficial/edk2/stable-202608", selected.values())
            self.assertNotIn("refs/heads/source/component/cix/1.2/bios", selected)
            self.assertNotIn("refs/remotes/origin/source/component/cix/1.2/op-tee", selected)
            self.assertNotIn("refs/heads/source/unofficial/current", selected)
            self.assertNotIn("refs/remotes/origin/source/unofficial/current", selected)
            self.assertNotIn("refs/heads/source/cache/release/generated", selected)


if __name__ == "__main__":
    unittest.main()
