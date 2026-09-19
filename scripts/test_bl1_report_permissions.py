#!/usr/bin/env python3
"""Exercise host validation against firmware outputs the host cannot modify."""

import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from mirror_build_outputs import mirror_raw_outputs
from reconstruction_common import ReconstructionError, bootloader1_report_path
from test_validate_bootloader1 import flash
import validate_bootloader1 as bl1


class ReadOnlyOutputTests(unittest.TestCase):
    @unittest.skipIf(os.geteuid() == 0, "run this permission regression as an unprivileged user")
    def test_read_only_outputs_validate_and_export_without_being_modified(self):
        with tempfile.TemporaryDirectory(prefix="bl1-read-only-") as temporary:
            root = Path(temporary)
            host = root / "host"
            worktree = root / "rendered"
            output = worktree / "src/Build/O6/RELEASE_GCC"
            output.mkdir(parents=True)
            payload = b"fixture approved vendor BL1"
            digest = hashlib.sha256(payload).hexdigest()
            catalog = {digest: {"size": len(payload), "provenance": [{"ref": "fixture"}]}}
            image = output / "cix_flash_all.bin"
            image.write_bytes(flash(payload))
            legacy_report = output / "bootloader1-validation.json"
            legacy_report.write_text("stale container-owned report")
            before = {p.name: p.read_bytes() for p in output.iterdir()}
            for path in output.iterdir():
                path.chmod(0o444)
            output.chmod(0o555)
            report = bootloader1_report_path(host, worktree, "O6", "RELEASE")
            try:
                self.assertFalse(os.access(output, os.W_OK))
                with patch.object(bl1, "ROOT", host), \
                        patch.object(bl1, "verify_or_warn", return_value={"status": "verified"}) as verify:
                    records = bl1.check_outputs(worktree, "O6", "RELEASE", "buildbox-firmware-build",
                                                catalog, {digest})
                verify.assert_called_once_with({digest: payload})
                self.assertEqual(len(records), 1)
                self.assertEqual(json.loads(report.read_text())["acceptance_basis"],
                                 "approved-vendor-hash-and-signature")
                copied = mirror_raw_outputs(worktree, host / "dist", "fixture", "custom", "O6", "RELEASE",
                                            validation_report=report)
                exported_report = next(p for p in copied if p.name == "bootloader1-validation.json")
                self.assertEqual(exported_report.read_bytes(), report.read_bytes())
                self.assertEqual({p.name: p.read_bytes() for p in output.iterdir()}, before)

                # A failed check must invalidate host evidence without touching
                # the old report or any other container-owned output.
                with patch.object(bl1, "ROOT", host), patch.object(bl1, "verify_or_warn") as verify:
                    with self.assertRaisesRegex(ReconstructionError, "not an unchanged qualified vendor"):
                        bl1.check_outputs(worktree, "O6", "RELEASE", "buildbox-firmware-build", {}, {digest})
                    verify.assert_not_called()
                self.assertFalse(report.exists())
                self.assertEqual({p.name: p.read_bytes() for p in output.iterdir()}, before)
            finally:
                output.chmod(0o755)

    def test_parallel_worktrees_boards_and_targets_have_separate_reports(self):
        root = Path("fixture")
        reports = {bootloader1_report_path(root, root / tree, board, target)
                   for tree in ("first", "second") for board in ("O6", "O6N") for target in ("RELEASE", "DEBUG")}
        self.assertEqual(len(reports), 8)
        self.assertEqual(bootloader1_report_path(root, root / "first", "O6", "release"),
                         bootloader1_report_path(root, root / "first", "O6", "RELEASE"))


if __name__ == "__main__":
    unittest.main()
