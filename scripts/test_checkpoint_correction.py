#!/usr/bin/env python3
"""Checkpoint maintenance preserves ancestry and requires explicit selection."""

import argparse
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from integrate_source_release import integrate_checkpoint_correction
from publish_source_update import checkpoint_correction
from reconstruction_common import ReconstructionError, clear_metadata_caches
from test_support import commit_all, git, write_file


class CheckpointCorrectionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="checkpoint-correction.")
        self.addCleanup(self.temp.cleanup)
        self.addCleanup(clear_metadata_caches)
        self.repo = Path(self.temp.name)
        git(self.repo, "init", "-b", "build")
        git(self.repo, "config", "user.name", "Test")
        git(self.repo, "config", "user.email", "test")
        write_file(self.repo, "README", "old\n")
        self.old = commit_all(self.repo, "checkpoint")
        self.target = "source/unofficial/1.3.1/edk2-stable202605"
        git(self.repo, "branch", self.target, self.old)
        git(self.repo, "branch", "source/unofficial/1.3/current", self.old)
        write_file(self.repo, "README", "corrected\n")
        self.candidate = commit_all(self.repo, "focused correction")
        self.manifest = self.repo / "config/refs-unofficial.json"
        write_file(self.repo, "config/refs-unofficial.json", json.dumps({"refs": [{
            "ref": self.target, "type": "unofficial-release-checkpoint", "object_id": self.old,
            "tree_id": git(self.repo, "rev-parse", f"{self.old}^{{tree}}").stdout.strip(),
        }]}))
        self.args = argparse.Namespace(release="1.3.1", edk2_base="202605", ref=self.candidate, allow_replace="1", write="1")

    def test_updates_only_selected_checkpoint_and_preserves_old_ancestor(self) -> None:
        integrate_checkpoint_correction(self.repo, self.args)
        record = json.loads(self.manifest.read_text())["refs"][0]
        self.assertEqual(record["object_id"], self.candidate)
        self.assertEqual(record["maintenance_base_object_id"], self.old)
        self.assertEqual(git(self.repo, "rev-parse", self.target).stdout.strip(), self.candidate)
        self.assertEqual(git(self.repo, "rev-parse", "source/unofficial/1.3/current").stdout.strip(), self.old)
        git(self.repo, "merge-base", "--is-ancestor", self.old, self.candidate)

    def test_dry_run_changes_neither_manifest_nor_ref(self) -> None:
        before = self.manifest.read_bytes()
        self.args.write = "0"
        integrate_checkpoint_correction(self.repo, self.args)
        self.assertEqual(self.manifest.read_bytes(), before)
        self.assertEqual(git(self.repo, "rev-parse", self.target).stdout.strip(), self.old)

    def test_explicit_maintenance_authorisation_required(self) -> None:
        self.args.allow_replace = "0"
        with self.assertRaisesRegex(ReconstructionError, "ALLOW_REPLACE"):
            integrate_checkpoint_correction(self.repo, self.args)

    def test_unrelated_history_rejected(self) -> None:
        tree = git(self.repo, "rev-parse", "HEAD^{tree}").stdout.strip()
        self.args.ref = git(self.repo, "commit-tree", tree, "-m", "unrelated").stdout.strip()
        with self.assertRaisesRegex(ReconstructionError, "ancestor"):
            integrate_checkpoint_correction(self.repo, self.args)

    def test_remote_only_checkpoint_materialises_local_head(self) -> None:
        git(self.repo, "update-ref", f"refs/remotes/origin/{self.target}", self.old)
        git(self.repo, "branch", "-D", self.target)
        clear_metadata_caches()
        integrate_checkpoint_correction(self.repo, self.args)
        self.assertEqual(git(self.repo, "rev-parse", f"refs/heads/{self.target}").stdout.strip(), self.candidate)

    def test_checked_out_checkpoint_is_rejected(self) -> None:
        git(self.repo, "switch", self.target)
        with self.assertRaisesRegex(ReconstructionError, "checked out"):
            integrate_checkpoint_correction(self.repo, self.args)

    def test_concurrent_ref_change_is_preserved_and_manifest_restored(self) -> None:
        before = self.manifest.read_bytes()
        tree = git(self.repo, "rev-parse", "HEAD^{tree}").stdout.strip()
        other = git(self.repo, "commit-tree", tree, "-p", self.old, "-m", "concurrent correction").stdout.strip()
        with patch("integrate_source_release.enforce_source_tree_policy", side_effect=lambda *a, **kw: git(
            self.repo, "update-ref", f"refs/heads/{self.target}", other, self.old,
        )):
            with self.assertRaises(ReconstructionError):
                integrate_checkpoint_correction(self.repo, self.args)
        self.assertEqual(self.manifest.read_bytes(), before)
        self.assertEqual(git(self.repo, "rev-parse", self.target).stdout.strip(), other)

    def test_publication_accepts_only_recorded_descendant_corrections(self) -> None:
        ref = "refs/heads/" + self.target
        records = json.loads(self.manifest.read_text())["refs"]
        self.assertFalse(checkpoint_correction(self.repo, ref, self.old, self.candidate, records))
        integrate_checkpoint_correction(self.repo, self.args)
        records = json.loads(self.manifest.read_text())["refs"]
        self.assertTrue(checkpoint_correction(self.repo, ref, self.old, self.candidate, records))
        self.assertFalse(checkpoint_correction(self.repo, ref, self.candidate, self.old, records))
        self.assertFalse(checkpoint_correction(self.repo, ref.replace("unofficial", "vendor/radxa"), self.old, self.candidate, records))
        tree = git(self.repo, "rev-parse", "HEAD^{tree}").stdout.strip()
        unrelated = git(self.repo, "commit-tree", tree, "-m", "unrelated").stdout.strip()
        self.assertFalse(checkpoint_correction(self.repo, ref, unrelated, self.candidate, records))


if __name__ == "__main__":
    unittest.main()
