#!/usr/bin/env python3
"""Regression coverage for resumable batch isolation and artifact acceptance."""

import argparse
import ast
import fcntl
import hashlib
import itertools
import json
import os
from pathlib import Path
import subprocess
import tempfile
import sys
import types
import unittest
from unittest.mock import patch

import release_expansion as batch
from release_expansion_source import finish_registration, register_new
from reconstruction_common import clear_metadata_caches, load_ref_records, show_file


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

    def test_only_pair_keeps_frozen_plan_and_reports_its_own_result(self):
        state, plan, _ = self.init()
        selected = batch.select_pair(plan, "202208/1.2.1")
        self.assertEqual(selected["edk2"], ["202208"])
        self.assertEqual(selected["radxa"], ["1.2.1"])
        self.assertIsNot(selected, plan)
        self.assertEqual(batch.read(state / "plan.json"), plan)
        with self.assertRaisesRegex(ValueError, "--only-pair"):
            batch.select_pair(plan, "202608/1.2.1")
        job = state / "jobs/202208-1.2.1"
        job.mkdir(parents=True)
        batch.save(job / "receipt.json", {"status": "prepared"})
        self.assertTrue(batch.selected_pair_passed(state, selected, True))
        self.assertFalse(batch.selected_pair_passed(state, selected, False))
        for fixes, settings in itertools.product(plan["fixes"], plan["settings"]):
            case = job / f"O6-fixes-{fixes}-settings-{settings}"
            case.mkdir()
            batch.save(case / "receipt.json", {"status": "passed"})
        self.assertTrue(batch.selected_pair_passed(state, selected, False))
        batch.save(job / "O6-fixes-true-settings-true/receipt.json", {"status": "failed"})
        self.assertFalse(batch.selected_pair_passed(state, selected, False))

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

    def output(self, out=None, fixes="true", settings="false", radxa="1.3.1", board="O6"):
        out = out or self.root / "output"
        out.mkdir()
        image = out / "cix_flash_all.bin"
        with image.open("wb") as stream:
            stream.truncate(8 * 1024**2)
        digest = hashlib.sha256(image.read_bytes()).hexdigest()
        batch.save(out / "firmware-chain-validation.json",
                   {"status": "verified", "images": [{"image_sha256": digest}]})
        batch.save(out / "bootloader1-validation.json", {"acceptance_basis": "approved-vendor-hash-fallback"})
        suffixes = {("false", "false"): "", ("true", "false"): "+fixes",
                    ("false", "true"): "+experimental", ("true", "true"): "+fixes+experimental"}
        defines = {"ENABLE_FIRMWARE_FIXES": fixes.upper(), "ENABLE_EXPERIMENTAL_UEFI_SETTINGS": settings.upper(),
                   "DEBUG_VERBOSE": "FALSE", "DEBUG_PRINT_ERROR_LEVEL": "0x80000001",
                   "UEFI_FW_VERSION": radxa + suffixes[(fixes, settings)] + "+mask80000001"}
        (out / "BuildOptions").write_text(f"gCommandLineDefines: {defines!r}\nActive Platform: src/Platform/{board}/{board}.dsc\n")
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

    def test_explicit_mask_versions_for_all_boards_releases_and_features(self):
        for radxa, board, fixes, settings in itertools.product(
                batch.RADXA, ("O6", "O6N"), ("false", "true"), ("false", "true")):
            with self.subTest(radxa=radxa, board=board, fixes=fixes, settings=settings):
                out = self.output(self.root / f"{radxa}-{board}-{fixes}-{settings}",
                                  fixes=fixes, settings=settings, radxa=radxa, board=board)
                batch.verify_output(out, board, fixes, settings, radxa)

    def test_accepts_versions_generated_by_retained_source_layout_helpers(self):
        # Do not rely solely on hand-written fixture versions: the original
        # bug was the same mistaken suffix in both verifier and fixture.
        records = [r for r in load_ref_records(batch.ROOT)
                   if r.get("type") == "unofficial-release-checkpoint"]
        self.assertTrue(records)
        out = self.output()
        options = out / "BuildOptions"
        defines = ast.literal_eval(options.read_text().split("gCommandLineDefines: ", 1)[1].splitlines()[0])
        for record in records:
            module = types.ModuleType("expansion_layout_probe")
            script = show_file(batch.ROOT, record["ref"], "scripts/firmware_layout.py")
            with patch.dict(sys.modules, {module.__name__: module}):
                exec(compile(script, record["ref"], "exec"), module.__dict__)
                for fixes, settings in itertools.product(("false", "true"), repeat=2):
                    with self.subTest(ref=record["ref"], fixes=fixes, settings=settings):
                        cmd = batch.build_command({"build_date": "fixed", "platform": "linux/arm64"},
                                                  "202608", record["radxa_release"], "O6", fixes, settings,
                                                  self.root, out)
                        mask = next(arg.split("=", 1)[1] for arg in cmd if arg.startswith("DEBUG_PRINT_ERROR_LEVEL="))
                        layout = module.FirmwareLayout(enable_firmware_fixes=fixes == "true",
                                                       enable_experimental_uefi_settings=settings == "true",
                                                       enable_core_order="cix", debug_print_error_level=mask)
                        version = module.display_version(record["radxa_release"], layout)
                        defines.update(ENABLE_FIRMWARE_FIXES=fixes.upper(),
                                       ENABLE_EXPERIMENTAL_UEFI_SETTINGS=settings.upper(),
                                       DEBUG_PRINT_ERROR_LEVEL=mask, UEFI_FW_VERSION=version)
                        options.write_text(f"gCommandLineDefines: {defines!r}\nActive Platform: src/Platform/O6/O6.dsc\n")
                        batch.verify_output(out, "O6", fixes, settings, record["radxa_release"])

    def test_incorrect_mask_version_or_board_remains_rejected(self):
        out = self.output()
        options = out / "BuildOptions"
        original = options.read_text()
        for wrong in (
                original.replace("0x80000001", "0x80000040"),
                original.replace("+mask80000001", ""),
                original.replace("+mask80000001", "+mask80000040"),
                original.replace("1.3.1+", "1.2.4+"),
                original.replace("/O6/O6.dsc", "/O6N/O6N.dsc")):
            with self.subTest(options=wrong):
                options.write_text(wrong)
                with self.assertRaisesRegex(ValueError, "BuildOptions.*mismatch"):
                    batch.verify_output(out, "O6", "true", "false", "1.3.1")

    def prepared_case(self):
        state, plan, _ = self.init()
        plan["fixes"], plan["settings"] = ["true"], ["false"]
        repo = state / "repo"
        ref = "source/vendor/radxa/1.2.1/edk2-stable202208"
        source = {"source_ref": ref, "source_commit": batch.git(repo, "rev-parse", ref)}
        job = state / "jobs/202208-1.2.1"
        case = job / "O6-fixes-true-settings-false"
        case.mkdir(parents=True)
        batch.save(job / "receipt.json", {"status": "prepared", "source": source})
        out = self.output(case / "output", radxa="1.2.1")
        row = {"status": "failed", "returncode": 0, "source": source,
               "command": batch.build_command(plan, "202208", "1.2.1", "O6", "true", "false", state, out),
               "error": "BuildOptions mismatch: UEFI_FW_VERSION='1.2.1+fixes+mask80000001', expected '1.2.1+fixes'"}
        batch.save(case / "receipt.json", row)
        return state, plan, case, row

    def test_revalidation_recovers_without_building_or_changing_inputs(self):
        state, plan, case, old = self.prepared_case()
        frozen_plan = (state / "plan.json").read_bytes()
        frozen_refs = batch.refs(state / "repo")
        image_before = (case / "output/cix_flash_all.bin").read_bytes()
        with patch.object(batch, "execute", side_effect=AssertionError("must not build")):
            self.assertEqual(batch.revalidate(state, plan, batch), 0)
            self.assertEqual(batch.revalidate(state, plan, batch), 0)
        row = batch.read(case / "receipt.json")
        self.assertEqual(row["status"], "passed")
        self.assertNotIn("error", row)
        self.assertEqual(batch.read(state / row["previous_receipt"]), old)
        self.assertEqual(len(list(case.glob("validation-attempt-*"))), 1)
        self.assertEqual(frozen_refs, batch.refs(state / "repo"))
        self.assertEqual(frozen_plan, (state / "plan.json").read_bytes())
        self.assertEqual(image_before, (case / "output/cix_flash_all.bin").read_bytes())
        self.assertEqual(batch.status(state)["counts"], {"prepared": 1, "passed": 1})

    def test_revalidation_does_not_accept_changed_images_options_recipes_or_sources(self):
        state, plan, case, row = self.prepared_case()
        output = case / "output"
        originals = {p: p.read_bytes() for p in output.iterdir()}
        for change in ("image", "options", "command", "source"):
            with self.subTest(change=change):
                candidate = dict(row)
                for path, data in originals.items():
                    path.write_bytes(data)
                if change == "image":
                    with (output / "cix_flash_all.bin").open("r+b") as stream:
                        stream.write(b"tampered")
                elif change == "options":
                    p = output / "BuildOptions"
                    p.write_text(p.read_text().replace("0x80000001", "0x80000040"))
                elif change == "command":
                    candidate["command"] = row["command"] + ["DEBUG_VERBOSE=true"]
                else:
                    candidate["source"] = {"source_ref": "wrong", "source_commit": "wrong"}
                batch.save(case / "receipt.json", candidate)
                self.assertEqual(batch.revalidate(state, plan, batch), 1)
                self.assertEqual(batch.read(case / "receipt.json"), candidate)
                self.assertFalse(list(case.glob("validation-attempt-*")))

    def test_revalidation_leaves_real_build_failures_and_interruptions_alone(self):
        state, plan, case, row = self.prepared_case()
        for delta in ({"returncode": 2}, {"status": "interrupted"},
                      {"error": "certificate-chain report does not verify this image"}):
            candidate = dict(row, **delta)
            batch.save(case / "receipt.json", candidate)
            self.assertEqual(batch.revalidate(state, plan, batch), 0)
            self.assertEqual(batch.read(case / "receipt.json"), candidate)

    def test_cleanup_accepts_raw_identical_crlf_but_retains_real_edit(self):
        state, plan, _ = self.init()
        repo = state / "repo"
        (repo / ".gitattributes").write_text("legacy.txt text eol=lf\n")
        batch.git(repo, "add", ".gitattributes")
        raw = b"vendor\r\nbytes\r\n"
        blob = subprocess.check_output(
            ["git", "-C", str(repo), "hash-object", "-w", "--stdin"], input=raw
        ).decode().strip()
        batch.git(repo, "update-index", "--add", "--cacheinfo", f"100644,{blob},legacy.txt")
        batch.git(repo, "-c", "commit.gpgsign=false", "commit", "-m", "legacy CRLF fixture")
        parent = repo / ".cache/edk2-cix/worktrees" / ("batch-" + plan["id"])

        identical = parent / "identical"
        batch.git(repo, "worktree", "add", "--detach", str(identical), "HEAD")
        (identical / "legacy.txt").write_bytes(raw)
        self.assertTrue(batch.git(identical, "status", "--porcelain"))
        batch.cleanup_rendered(state, plan)
        self.assertFalse(identical.exists())

        changed = parent / "changed"
        batch.git(repo, "worktree", "add", "--detach", str(changed), "HEAD")
        (changed / "legacy.txt").write_bytes(raw + b"real edit\r\n")
        with self.assertRaisesRegex(ValueError, "real changes"):
            batch.cleanup_rendered(state, plan)
        self.assertTrue(changed.exists())
        batch.git(repo, "worktree", "remove", "--force", str(changed))

    def test_cleanup_retires_only_its_own_worktree_buildbox_mount(self):
        state, plan, _ = self.init()
        name = "edk2-cix-buildbox-1234abcd"
        name_file = state / "cache/buildbox/buildbox-name"
        name_file.parent.mkdir(parents=True)
        name_file.write_text(name + "\n")
        worktree = state / "repo/rendered"
        mounts = [{"Type": "bind", "Source": str(worktree)},
                  {"Type": "bind", "Source": str(state / "cache/buildbox")}]
        inspect = subprocess.CompletedProcess([], 0, json.dumps([{"Mounts": mounts}]), "")
        removed = subprocess.CompletedProcess([], 0, name, "")
        with patch.object(batch.subprocess, "run", side_effect=[inspect, removed]) as run:
            batch.retire_buildbox_mount(state, worktree)
        self.assertEqual(run.call_args_list[0].args[0], ["docker", "inspect", name])
        self.assertEqual(run.call_args_list[1].args[0], ["docker", "rm", "-f", name])

        stale = state / "repo/.cache/edk2-cix/worktrees" / ("batch-" + plan["id"]) / "old"
        mounts[0]["Source"] = str(stale)
        inspect = subprocess.CompletedProcess([], 0, json.dumps([{"Mounts": mounts}]), "")
        with patch.object(batch.subprocess, "run", side_effect=[inspect, removed]) as run:
            batch.retire_buildbox_mount(state, None)
        self.assertEqual(run.call_count, 2)

        foreign = subprocess.CompletedProcess([], 0, json.dumps([{"Mounts": []}]), "")
        with patch.object(batch.subprocess, "run", return_value=foreign) as run:
            batch.retire_buildbox_mount(state, worktree)
        run.assert_called_once()

        mounts[0]["Source"] = str(worktree)
        mounts.pop()
        inspect = subprocess.CompletedProcess([], 0, json.dumps([{"Mounts": mounts}]), "")
        with patch.object(batch.subprocess, "run", return_value=inspect):
            with self.assertRaisesRegex(ValueError, "without this batch's cache"):
                batch.retire_buildbox_mount(state, worktree)

    def test_frozen_runner_uses_original_build_code_and_current_verifier(self):
        # A changed calling checkout must not replace the frozen recipe or
        # preparation code when correcting verification or cleanup.
        runner = self.repo / "scripts/release_expansion.py"
        runner.write_text("def build_command(*args):\n    return ['frozen']\n"
                          "def run(*args):\n    return 'original-run'\n"
                          "def verify_output(*args):\n    raise ValueError('old validator')\n")
        batch.git(self.repo, "add", ".")
        batch.git(self.repo, "-c", "commit.gpgsign=false", "commit", "-m", "snapshot runner")
        state, plan, _ = self.init()
        module = batch.frozen_runner(state, plan)
        self.assertEqual(module.build_command(), ["frozen"])
        self.assertIs(module.cleanup_rendered, batch.cleanup_rendered)
        self.assertEqual(module.run(), "original-run")
        self.assertIs(module.verify_output, batch.verify_output)
        hashes = dict(plan["runner_hashes"])
        batch.record_validator(state, plan)
        batch.record_validator(state, plan)
        self.assertEqual(plan["runner_hashes"], hashes)
        self.assertEqual(len(batch.read(state / "validator-history.json")["revisions"]), 1)
        (state / "repo/scripts/release_expansion.py").write_text("changed")
        with self.assertRaisesRegex(ValueError, "runner changed"):
            batch.frozen_runner(state, plan)

    def test_revalidation_cli_refuses_a_running_batch(self):
        state, _, case, row = self.prepared_case()
        with (state / "lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            result = subprocess.run([sys.executable, batch.__file__, "revalidate", "--state", str(state)],
                                    capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("this batch is already running", result.stderr)
        self.assertEqual(batch.read(case / "receipt.json"), row)
        self.assertFalse((state / "validator-history.json").exists())

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
