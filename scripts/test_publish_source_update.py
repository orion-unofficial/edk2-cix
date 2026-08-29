#!/usr/bin/env python3
"""Regression tests for guarded build/source publication."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from publish_source_update import metadata_ref, mutable_ref, verify_metadata
from reconstruction_common import ReconstructionError
from test_support import commit_all, git, write_file


class PublishSourceUpdateTests(unittest.TestCase):
    def test_mutable_unofficial_refs_have_direct_metadata_mappings(self) -> None:
        current = "refs/heads/source/unofficial/1.3/current"
        compatibility = "refs/heads/source/unofficial/edk2-stable202608"
        tag = "refs/tags/source/unofficial/edk2/stable-202608"

        self.assertEqual(metadata_ref(current), "source/unofficial/1.3/current")
        self.assertEqual(
            metadata_ref(compatibility),
            "source/unofficial/edk2-stable202608",
        )
        self.assertEqual(
            metadata_ref(tag),
            "source/unofficial/edk2-stable202608",
        )
        self.assertTrue(mutable_ref(current))
        self.assertTrue(mutable_ref(compatibility))
        self.assertTrue(mutable_ref(tag))

    def test_mutable_ref_metadata_must_match_selected_object_and_tree(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            git(repo, "init", "-q", "-b", "build")
            git(repo, "config", "user.name", "Publication Test")
            git(repo, "config", "user.email", "publication-test")
            write_file(repo, "firmware.txt", "current\n")
            commit = commit_all(repo, "current")
            branch = "source/unofficial/1.3/current"
            git(repo, "branch", branch, commit)
            tree = git(repo, "rev-parse", f"{branch}^{{tree}}").stdout.strip()
            record = {
                "manifest": "config/refs-unofficial.json",
                "object_id": commit,
                "ref": branch,
                "tree_id": tree,
            }

            self.assertEqual(
                verify_metadata(repo, f"refs/heads/{branch}", [record]),
                "config/refs-unofficial.json",
            )

            write_file(repo, "firmware.txt", "moved\n")
            moved = commit_all(repo, "moved")
            git(repo, "branch", "-f", branch, moved)
            with self.assertRaisesRegex(ReconstructionError, "does not match"):
                verify_metadata(repo, f"refs/heads/{branch}", [record])

    def test_compatibility_tag_must_match_its_recorded_branch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            git(repo, "init", "-q", "-b", "build")
            git(repo, "config", "user.name", "Publication Test")
            git(repo, "config", "user.email", "publication-test")
            write_file(repo, "firmware.txt", "tagged\n")
            tagged = commit_all(repo, "tagged")
            branch = "source/unofficial/edk2-stable202608"
            tag = "source/unofficial/edk2/stable-202608"
            git(repo, "branch", branch, tagged)
            git(repo, "tag", tag, tagged)
            write_file(repo, "firmware.txt", "branch moved\n")
            moved = commit_all(repo, "branch moved")
            git(repo, "branch", "-f", branch, moved)
            record = {
                "manifest": "config/refs-unofficial.json",
                "object_id": tagged,
                "ref": branch,
                "tree_id": git(
                    repo,
                    "rev-parse",
                    f"{tag}^{{tree}}",
                ).stdout.strip(),
            }

            with self.assertRaisesRegex(
                ReconstructionError,
                "tag does not identify its recorded compatibility branch",
            ):
                verify_metadata(repo, f"refs/tags/{tag}", [record])


if __name__ == "__main__":
    unittest.main()
