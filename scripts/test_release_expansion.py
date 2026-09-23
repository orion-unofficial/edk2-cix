#!/usr/bin/env python3
"""Regression coverage for resumable batch isolation and artifact acceptance."""

import argparse
import hashlib
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import release_expansion as batch
from release_expansion_source import finish_registration, register_new
from reconstruction_common import clear_metadata_caches, load_ref_records


class ExpansionTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="release-expansion-test-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.repo = self.root / "input"
        self.repo.mkdir()
        batch.git(self.repo, "init", "-b", "build")
        batch.git(self.repo, "config", "user.name", "Expansion Test")
        batch.git(self.repo, "config", "user.email", "expansion@example.invalid")
        (self.repo / "scripts").mkdir()
        for name in batch.RUNNERS:
            (self.repo / "scripts" / name).write_text("# test runner\n")
        (self.repo / "config").mkdir()
        (self.repo / "config/policies.json").write_text('{}\n')
        (self.repo / "config/refs-unofficial.json").write_text('{"refs": []}\n')
        batch.git(self.repo, "add", ".")
        batch.git(self.repo, "-c", "commit.gpgsign=false", "commit", "-m", "fixture")
        batch.git(self.repo, "branch", "source/vendor/radxa/1.2.1/edk2-stable202208")
        batch.git(self.repo, "tag", "source/unofficial/edk2/stable-202208")
        clear_metadata_caches()

    def init(self):
        state = self.root / "batch"
        state.mkdir()
        args = argparse.Namespace(edk2="all", radxa="1.2.1", boards="O6", fixes="all",
                                  settings="all", platform="linux/amd64", min_free_gib=1)
        with patch.object(batch, "ROOT", self.repo), patch(
                "reconstruction_common.matrix_release_values", return_value=["202208"]):
            plan = batch.initialise(state, args)
        return state, plan, args

    def test_clone_isolates_refs_and_rejects_protected_changes(self):
        original = batch.refs(self.repo)
        state, plan, _ = self.init()
        private = state / "repo"
        self.assertNotEqual(batch.git(private, "rev-parse", "--git-common-dir"),
                            str(self.repo / ".git"))
        batch.git(private, "branch", "source/unofficial/new-candidate")
        batch.guard(state, plan)
        self.assertEqual(batch.refs(self.repo), original)
        batch.git(private, "tag", "-d", "source/unofficial/edk2/stable-202208")
        with self.assertRaisesRegex(ValueError, "protected input changed"):
            batch.guard(state, plan)
        self.assertEqual(batch.refs(self.repo), original)

    def test_resume_refuses_new_configuration(self):
        state, _, args = self.init()
        args.boards = "O6N"
        with self.assertRaisesRegex(ValueError, "frozen plan"):
            batch.initialise(state, args)

    def test_registration_recovers_crash_between_ref_and_manifest(self):
        ref = "source/unofficial/1.2.1/edk2-stable202208"
        oid = batch.git(self.repo, "rev-parse", "HEAD")
        journal = self.root / "journal.json"
        metadata = {"type": "unofficial-release-checkpoint", "radxa_release": "1.2.1"}
        with patch("release_expansion_source.update_ref_record", side_effect=RuntimeError("crash")):
            with self.assertRaisesRegex(RuntimeError, "crash"):
                register_new(self.repo, journal, ref, oid, metadata)
        self.assertTrue(journal.exists())
        self.assertEqual(batch.git(self.repo, "rev-parse", ref), oid)
        (self.repo / "config/refs-unofficial.json").write_text('{"refs": [')
        finish_registration(self.repo, journal)
        self.assertFalse(journal.exists())
        records = load_ref_records(self.repo)
        record = next(r for r in records if r["ref"] == ref)
        self.assertEqual(record["manifest"], "config/refs-unofficial.json")
        self.assertEqual(record["object_id"], oid)

    def test_registration_never_replaces_existing_ref(self):
        ref = "source/unofficial/1.2.1/edk2-stable202208"
        oid = batch.git(self.repo, "rev-parse", "HEAD")
        batch.git(self.repo, "branch", ref)
        (self.repo / "change").write_text("different\n")
        batch.git(self.repo, "add", "change")
        batch.git(self.repo, "-c", "commit.gpgsign=false", "commit", "-m", "changed")
        from reconstruction_common import ReconstructionError
        with self.assertRaisesRegex(ReconstructionError, "existing ref"):
            register_new(self.repo, self.root / "journal.json", ref,
                         batch.git(self.repo, "rev-parse", "HEAD"), {})
        self.assertEqual(batch.git(self.repo, "rev-parse", ref), oid)

    def test_environment_cannot_enable_vendor_mode_or_replace_inputs(self):
        with patch.dict(os.environ, {"ARTEFACT_MODE": "upstream", "CIX_RELEASE": "1.2",
                                     "MAKEFLAGS": "-e", "FASTBOOT_LOAD": "nvme",
                                     "DEBUG_VERBOSE": "true", "GIT_DIR": "/wrong"}):
            env = batch.environment(self.root, {"id": "test"})
        for name in ("ARTEFACT_MODE", "CIX_RELEASE", "MAKEFLAGS", "FASTBOOT_LOAD", "DEBUG_VERBOSE", "GIT_DIR"):
            self.assertNotIn(name, env)

    def output(self, out=None, fixes="true", settings="false", radxa="1.3.1"):
        out = out or self.root / "output"
        out.mkdir()
        image = out / "cix_flash_all.bin"
        with image.open("wb") as stream:
            stream.truncate(8 * 1024**2)
        digest = hashlib.sha256(image.read_bytes()).hexdigest()
        batch.save(out / "firmware-chain-validation.json",
                   {"status": "verified", "images": [{"image_sha256": digest}]})
        batch.save(out / "bootloader1-validation.json", {"acceptance_basis": "approved-vendor-hash-fallback"})
        suffixes = {("false", "false"): "+custom", ("true", "false"): "+fixes",
                    ("false", "true"): "+experimental", ("true", "true"): "+fixes+experimental"}
        defines = {"ENABLE_FIRMWARE_FIXES": fixes.upper(), "ENABLE_EXPERIMENTAL_UEFI_SETTINGS": settings.upper(),
                   "DEBUG_VERBOSE": "FALSE", "UEFI_FW_VERSION": radxa + suffixes[(fixes, settings)]}
        (out / "BuildOptions").write_text(f"gCommandLineDefines: {defines!r}\nActive Platform: src/Platform/O6/O6.dsc\n")
        return out

    def test_acceptance_binds_options_and_chain_to_actual_image(self):
        out = self.output()
        batch.verify_output(out, "O6", "true", "false", "1.3.1")
        with self.assertRaisesRegex(ValueError, "BuildOptions mismatch"):
            batch.verify_output(out, "O6", "false", "false", "1.3.1")
        with (out / "cix_flash_all.bin").open("r+b") as stream:
            stream.write(b"wrong image")
        with self.assertRaisesRegex(ValueError, "does not verify this image"):
            batch.verify_output(out, "O6", "true", "false", "1.3.1")

    def test_missing_report_and_duplicate_outputs_are_rejected(self):
        out = self.output()
        (out / "nested").mkdir()
        (out / "nested/BuildOptions").write_text("stale")
        with self.assertRaisesRegex(ValueError, "expected one BuildOptions"):
            batch.verify_output(out, "O6", "true", "false", "1.3.1")

        (out / "nested/BuildOptions").unlink()
        (out / "firmware-chain-validation.json").unlink()
        with self.assertRaisesRegex(ValueError, "expected one firmware-chain"):
            batch.verify_output(out, "O6", "true", "false", "1.3.1")

    def test_experimental_build_uses_experimental_version(self):
        out = self.output(settings="true")
        batch.verify_output(out, "O6", "true", "true", "1.3.1")

    def test_conflict_archive_retains_commit_and_notes_without_checkout(self):
        state, _, _ = self.init()
        repo = state / "repo"
        job = state / "jobs/pair"
        job.mkdir(parents=True)
        parent = state / "tmp/port-pair-conflict-test"
        parent.mkdir(parents=True)
        (parent / "README.md").write_text("Resolve source-stage conflict\n")
        batch.git(repo, "worktree", "add", "--detach", str(parent / "worktree"), "HEAD")
        rows = batch.archive_conflicts(state, job)
        self.assertEqual(len(rows), 1)
        self.assertEqual(batch.git(repo, "rev-parse", rows[0]["ref"]), rows[0]["commit"])
        self.assertTrue((state / rows[0]["notes"]).exists())
        self.assertFalse(parent.exists())

    def test_build_recipe_has_no_force_bypass_and_unique_output(self):
        plan = {"build_date": "2026-09-22T00:00:00+00:00", "platform": "linux/arm64"}
        cmd = batch.build_command(plan, "202608", "1.3.1", "O6", "true", "false", self.root,
                                  self.root / "unique/output")
        for value in ("ARTEFACT_MODE=custom", "CIX_RELEASE=", "FORCE_DEBUG_BUILD=0", "DEBUG_VERBOSE=false"):
            self.assertIn(value, cmd)
        self.assertIn(f"BUILD_DIST_ROOT={self.root / 'unique/output'}", cmd)

    def test_failed_build_does_not_stop_batch_and_retry_skips_verified_passes(self):
        state, plan, _ = self.init()
        plan["settings"] = ["false"]
        batch.save(state / "plan.json", plan)
        repo = state / "repo"
        source_ref = "source/vendor/radxa/1.2.1/edk2-stable202208"
        source = {"source_ref": source_ref, "source_commit": batch.git(repo, "rev-parse", source_ref)}
        job = state / "jobs/202208-1.2.1"
        job.mkdir(parents=True)
        batch.save(job / "receipt.json", {"status": "prepared", "source": source})
        calls = []

        def execute(_state, _plan, command, log, label):
            opts = dict(arg.split("=", 1) for arg in command if "=" in arg)
            calls.append(opts)
            log.write_text("fixture build\n")
            if len(calls) == 1:
                return 2
            self.output(Path(opts["BUILD_DIST_ROOT"]), fixes=opts["ENABLE_FIRMWARE_FIXES"], radxa="1.2.1")
            return 0

        with patch.object(batch, "execute", side_effect=execute):
            self.assertEqual(batch.run(state, plan, False, False), 1)
            self.assertEqual(len(calls), 2)
            self.assertEqual(batch.run(state, plan, False, False), 1)
            self.assertEqual(len(calls), 2)
            self.assertEqual(batch.run(state, plan, False, True), 0)
            self.assertEqual(len(calls), 3)
        summary = batch.status(state)
        self.assertEqual(summary["counts"], {"prepared": 1, "passed": 2})
        self.assertEqual(len(list(job.glob("*/attempt-*/receipt.json"))), 1)


if __name__ == "__main__":
    unittest.main()
