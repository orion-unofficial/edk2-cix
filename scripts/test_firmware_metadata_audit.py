#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from firmware_metadata_audit import audit_targets
from normalize_build_options import normalize_build_options_text


class FirmwareMetadataAuditTests(unittest.TestCase):
    def test_ascii_breadcrumbs_are_detected(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir_text:
            payload = Path(tempdir_text) / "BurnImage.efi"
            payload.write_bytes(
                b"NB10 /data/devops/jenkins/workspace/job/Build/Sky1UnitTest/BurnImage.dll"
            )
            findings = audit_targets([(payload.name, payload)])

        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].path, "BurnImage.efi")
        self.assertIn("NB10", findings[0].reasons)
        self.assertIn("Jenkins workspace", findings[0].reasons)

    def test_utf16_workspace_path_is_detected(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir_text:
            payload = Path(tempdir_text) / "Shell.efi"
            payload.write_bytes("/workspaces/edk2-cix".encode("utf-16le"))
            findings = audit_targets([(payload.name, payload)])

        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].text, "/workspaces/edk2-cix")
        self.assertIn("/workspaces/", findings[0].reasons)

    def test_windows_workspace_path_is_detected(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir_text:
            payload = Path(tempdir_text) / "FlashUpdate.efi"
            payload.write_bytes(b"C:\\Users\\builder\\workspace\\FlashUpdate.pdb")
            findings = audit_targets([(payload.name, payload)])

        self.assertEqual(len(findings), 1)
        self.assertIn("Windows drive path", findings[0].reasons)
        self.assertIn(".pdb", findings[0].reasons)

    def test_shell_help_example_is_ignored(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir_text:
            payload = Path(tempdir_text) / "Shell.efi"
            payload.write_bytes(b"    Shell> set -v EFI_SOURCE c:\\project\\EFI1.1")
            findings = audit_targets([(payload.name, payload)])

        self.assertEqual(findings, [])

    def test_short_binary_noise_is_not_mistaken_for_windows_path(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir_text:
            payload = Path(tempdir_text) / "cix_flash_all.bin"
            payload.write_bytes(b"v:/+p q:\\{6 g93k:\\( g1a:/_P h:/K")
            findings = audit_targets([(payload.name, payload)])

        self.assertEqual(findings, [])

    def test_build_options_paths_are_normalized(self) -> None:
        original = "\n".join(
            (
                "Active Platform: /tmp/edk2-cix-custom-workspace/edk2-platforms/Platform/Radxa/Orion/O6/O6.dsc",
                "Flash Image Definition: /tmp/edk2-cix-custom-workspace/edk2-platforms/Platform/Radxa/Orion/O6/O6.fdf",
                "gCommandLineDefines: {'BUILD_DATE': '2026-03-31T00:00:00+00:00'}",
            )
        )
        normalized = normalize_build_options_text(
            original,
            "src/edk2-platforms/Platform/Radxa/Orion/O6/O6.dsc",
            "src/edk2-platforms/Platform/Radxa/Orion/O6/O6.fdf",
        )

        self.assertIn(
            "Active Platform: src/edk2-platforms/Platform/Radxa/Orion/O6/O6.dsc",
            normalized,
        )
        self.assertIn(
            "Flash Image Definition: src/edk2-platforms/Platform/Radxa/Orion/O6/O6.fdf",
            normalized,
        )
        self.assertNotIn("/tmp/edk2-cix-custom-workspace", normalized)


if __name__ == "__main__":
    unittest.main()
