#!/usr/bin/env python3
"""Maintained component updates preserve ancestry, metadata and atomic publication."""
import argparse
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from integrate_source_release import integrate_checkpoint_correction, validate
from publish_source_update import component_correction, main
from reconstruction_common import ReconstructionError, clear_metadata_caches
from test_support import commit_all, git, write_file


class ComponentCorrectionTests(unittest.TestCase):
    component = "standalone-mm"
    target = "source/unofficial/stmm/edk2-stable202608"
    source_path = "CixStandaloneMmPkg/CixStandaloneMm.dsc"

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="component-correction.")
        self.addCleanup(self.temp.cleanup)
        self.addCleanup(clear_metadata_caches)
        self.root = Path(self.temp.name)
        self.repo = self.root / "repo"
        git(self.root, "init", "-q", "-b", "build", str(self.repo))
        git(self.repo, "config", "user.name", "Test")
        git(self.repo, "config", "user.email", "test")
        write_file(self.repo, self.source_path, "old binding\n")
        self.old = commit_all(self.repo, "old component")
        git(self.repo, "branch", self.target, self.old)
        write_file(self.repo, self.source_path, "corrected binding\n")
        self.candidate = commit_all(self.repo, "component correction")
        self.record = {"ref": self.target, "type": "unofficial-component", "component": self.component,
                       "immutable": False, "object_id": self.old,
                       "tree_id": git(self.repo, "rev-parse", self.old + "^{tree}").stdout.strip()}
        self.manifest = self.repo / "config/refs-cix.json"
        self.save()
        self.args = argparse.Namespace(type_="unofficial", component=self.component, release="1.2",
                                       vendor="cix", arm_base="3.17.0",
                                       edk2_base="202608", ref=self.candidate, allow_replace="1", write="1")

    def save(self):
        write_file(self.repo, "config/refs-cix.json", json.dumps({"refs": [self.record]}))

    def test_component_cli_requires_explicit_component_base_and_candidate(self):
        self.assertEqual(validate(self.args), [])
        self.args.ref = ""
        self.assertTrue(validate(self.args))
        self.args.ref = self.candidate
        self.args.component = "edk2"
        self.assertTrue(validate(self.args))

    def test_descendant_update_binds_metadata_without_checkpoint_ledger(self):
        integrate_checkpoint_correction(self.repo, self.args)
        record = json.loads(self.manifest.read_text())["refs"][0]
        self.assertEqual(record["object_id"], self.candidate)
        self.assertEqual(record["tree_id"], git(self.repo, "rev-parse", self.candidate + "^{tree}").stdout.strip())
        self.assertNotIn("maintenance_base_object_id", record)
        self.assertEqual(git(self.repo, "rev-parse", self.target).stdout.strip(), self.candidate)
        self.assertTrue(component_correction(self.repo, "refs/heads/" + self.target,
                                             self.old, self.candidate, [record]))

    def test_dry_run_preserves_manifest_and_ref(self):
        before = self.manifest.read_bytes()
        self.args.write = "0"
        integrate_checkpoint_correction(self.repo, self.args)
        self.assertEqual(self.manifest.read_bytes(), before)
        self.assertEqual(git(self.repo, "rev-parse", self.target).stdout.strip(), self.old)

    def test_immutable_or_unrecorded_component_is_rejected(self):
        for value in (True, None):
            self.record["immutable"] = value
            self.save()
            with self.assertRaisesRegex(ReconstructionError, "explicitly mutable"):
                integrate_checkpoint_correction(self.repo, self.args)
        write_file(self.repo, "config/refs-cix.json", '{"refs": []}')
        with self.assertRaisesRegex(ReconstructionError, "explicitly mutable"):
            integrate_checkpoint_correction(self.repo, self.args)

    def test_unrelated_history_and_missing_authorisation_are_rejected(self):
        self.args.allow_replace = "0"
        with self.assertRaisesRegex(ReconstructionError, "ALLOW_REPLACE"):
            integrate_checkpoint_correction(self.repo, self.args)
        self.args.allow_replace = "1"
        tree = git(self.repo, "rev-parse", "HEAD^{tree}").stdout.strip()
        self.args.ref = git(self.repo, "commit-tree", tree, "-m", "unrelated").stdout.strip()
        with self.assertRaisesRegex(ReconstructionError, "ancestor"):
            integrate_checkpoint_correction(self.repo, self.args)

    def test_publication_rejects_rewind_unrecorded_and_duplicate_components(self):
        record = dict(self.record, object_id=self.candidate)
        ref = "refs/heads/" + self.target
        self.assertFalse(component_correction(self.repo, ref, self.candidate, self.old, [record]))
        self.assertFalse(component_correction(self.repo, ref, self.old, self.candidate, [self.record]))
        self.assertFalse(component_correction(self.repo, ref, self.old, self.candidate, [record, record]))
        self.assertFalse(component_correction(self.repo, ref, self.old, self.candidate, [dict(record, immutable=True)]))
        self.assertFalse(component_correction(self.repo, ref.replace("unofficial", "base"),
                                              self.old, self.candidate, [record]))

    def test_stale_object_or_tree_and_duplicate_records_are_rejected(self):
        for key in ("object_id", "tree_id"):
            with self.subTest(key=key):
                saved = self.record[key]
                self.record[key] = "0" * 40
                self.save()
                before = self.manifest.read_bytes()
                with self.assertRaisesRegex(ReconstructionError, "recorded"):
                    integrate_checkpoint_correction(self.repo, self.args)
                self.assertEqual(self.manifest.read_bytes(), before)
                self.record[key] = saved
        self.manifest.write_text(json.dumps({"refs": [self.record, self.record]}))
        with self.assertRaisesRegex(ReconstructionError, "explicitly mutable"):
            integrate_checkpoint_correction(self.repo, self.args)

    def test_compare_and_swap_failure_restores_exact_manifest(self):
        before = self.manifest.read_bytes()
        from integrate_source_release import git as real_git

        def race(repo, *args, **kwargs):
            if args[0] == "update-ref":
                self.assertEqual(args[-1], self.old)
                raise ReconstructionError("simulated concurrent ref update")
            return real_git(repo, *args, **kwargs)

        with patch("integrate_source_release.git", side_effect=race):
            with self.assertRaisesRegex(ReconstructionError, "concurrent"):
                integrate_checkpoint_correction(self.repo, self.args)
        self.assertEqual(self.manifest.read_bytes(), before)
        self.assertEqual(git(self.repo, "rev-parse", self.target).stdout.strip(), self.old)

    def test_remote_only_component_and_idempotent_repetition(self):
        git(self.repo, "update-ref", "refs/remotes/origin/" + self.target, self.old)
        git(self.repo, "update-ref", "-d", "refs/heads/" + self.target)
        integrate_checkpoint_correction(self.repo, self.args)
        before = self.manifest.read_bytes()
        self.args.allow_replace = "0"
        integrate_checkpoint_correction(self.repo, self.args)
        self.assertEqual(self.manifest.read_bytes(), before)
        self.assertEqual(git(self.repo, "rev-parse", self.target).stdout.strip(), self.candidate)

    def test_checked_out_component_and_missing_source_are_rejected(self):
        git(self.repo, "checkout", "-q", self.target)
        with self.assertRaises(ReconstructionError):
            integrate_checkpoint_correction(self.repo, self.args)
        git(self.repo, "checkout", "-q", "build")
        git(self.repo, "rm", self.source_path)
        self.args.ref = commit_all(self.repo, "remove required source")
        before = self.manifest.read_bytes()
        with self.assertRaises(ReconstructionError):
            integrate_checkpoint_correction(self.repo, self.args)
        self.assertEqual(self.manifest.read_bytes(), before)

    def test_atomic_publication_updates_build_and_component_together(self):
        remote = self.root / "remote.git"
        git(self.root, "init", "-q", "--bare", str(remote))
        git(self.repo, "remote", "add", "origin", str(remote))
        commit_all(self.repo, "old metadata")
        git(self.repo, "push", "-q", "origin", "build", self.target)
        integrate_checkpoint_correction(self.repo, self.args)
        tip = commit_all(self.repo, "new component metadata")
        with patch("publish_source_update.repo_root", return_value=self.repo), patch(
                "sys.argv", ["publish_source_update.py", "--write", "1"]):
            main()
        self.assertEqual(git(self.repo, "ls-remote", "origin", "refs/heads/build").stdout.split()[0], tip)
        self.assertEqual(git(self.repo, "ls-remote", "origin", "refs/heads/" + self.target).stdout.split()[0],
                         self.candidate)


class OpteeCorrectionTests(ComponentCorrectionTests):
    component = "op-tee"
    target = "source/unofficial/cix/1.2/op-tee/3.17.0"
    source_path = "core/arch/arm/kernel/stmm_sp.c"

    def test_each_optee_selector_is_required(self):
        for key in ("vendor", "release", "arm_base", "ref"):
            with self.subTest(key=key):
                saved = getattr(self.args, key)
                setattr(self.args, key, "")
                self.assertTrue(validate(self.args))
                with self.assertRaisesRegex(ReconstructionError, "maintenance requires"):
                    integrate_checkpoint_correction(self.repo, self.args)
                setattr(self.args, key, saved)
        self.args.vendor = "radxa"
        self.assertTrue(validate(self.args))


if __name__ == "__main__":
    unittest.main()
