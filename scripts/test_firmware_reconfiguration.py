#!/usr/bin/env python3
"""Exercise repeated configuration changes through the real firmware Makefile."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from reconstruction_common import show_file

ROOT = Path(__file__).resolve().parents[1]
FILES = ("src/Makefile", ".github/local/Makefile.local",
         "scripts/validate_make_inputs.py", "scripts/firmware_layout.py",
         "src/edk2/MdePkg/Include/Library/DebugLib.h")


class FirmwareReconfigurationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="firmware-reconfiguration-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for relative in FILES:
            local = os.environ.get("SOURCE_TEST_ROOT")
            data = (Path(local) / relative).read_bytes() if local else show_file(
                ROOT, os.environ.get("SOURCE_TEST_REF", "source/unofficial/1.3/current"), relative
            )
            dest = self.root / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
        (self.root / "src/edk2-platforms/Platform/Radxa").mkdir(parents=True)
        self.make = shutil.which("gmake") or shutil.which("make")
        self.base = {
            "ARTEFACT_MODE": "custom", "FIRMWARE_TARGET": "RELEASE",
            "ENABLE_FIRMWARE_FIXES": "true", "ENABLE_CORE_ORDER": "cix",
            "ENABLE_EXPERIMENTAL_UEFI_SETTINGS": "false", "DEBUG_VERBOSE": "false",
            "DEBUG_ON_UART3": "false", "UART3_ENABLE": "false",
            "DEBUG_PRINT_ERROR_LEVEL": "0x80000040", "CIX_RELEASE": "",
            "ENABLE_TF_A_FIXES": "false", "EDK2_CIX_REPO_LOCK_HELD": "1",
            "BUILD_METADATA_SCRIPT": "true", "BUILD_DATE": "2026-09-17T00:00:00Z",
            "SOURCE_COMMIT_HASH": "source", "EDK2_COMMIT_HASH": "edk2",
            "EDK2_NON_OSI_COMMIT_HASH": "nonosi", "EDK2_PLATFORMS_COMMIT_HASH": "platforms",
            "FIRMWARE_VERSION": "1.3.1", "UEFI_FW_VERSION": "1.3.1+fixes",
            "PREFERRED_TMP_ROOT": str(self.root / "tmp"),
        }

    def test_custom_payload_metadata_changes_invalidate_cached_images(self):
        for key, value in (("MEM_CFG_MEMFREQ", "2750"), ("SOURCE_DATE_EPOCH", "1700000000"),
                           ("PM_CONFIG_SOURCE_DATE_EPOCH", "1700000001")):
            output, _ = self.configure()
            stale = output / "cix_flash_all.bin"
            stale.write_bytes(b"old image")
            _, config = self.configure({key: value})
            self.assertFalse(stale.exists(), key)
            self.assertEqual(config[key], value)

    def configure(self, changes=None, board="O6"):
        settings = {**self.base, **(changes or {})}
        output = self.root / "src/Build" / board / (settings["FIRMWARE_TARGET"] + "_GCC")
        stamp = output / ".edk2-cix-build-config"
        result = subprocess.run(
            [self.make, "--no-print-directory", str(stamp.relative_to(self.root / "src")),
             *(f"{key}={value}" for key, value in settings.items())],
            cwd=self.root / "src", capture_output=True, text=True, timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        values = dict(line.split("=", 1) for line in stamp.read_text().splitlines())
        return output, values

    def test_each_firmware_switch_invalidates_previous_outputs_in_both_directions(self):
        cases = {
            "ENABLE_FIRMWARE_FIXES": ("false", "FALSE"),
            "ENABLE_CORE_ORDER": ("performance", "performance"),
            "ENABLE_EXPERIMENTAL_UEFI_SETTINGS": ("true", "TRUE"),
            "DEBUG_VERBOSE": ("true", "TRUE"),
            "DEBUG_ON_UART3": ("true", "TRUE"),
            "UART3_ENABLE": ("true", "TRUE"),
            "DEBUG_PRINT_ERROR_LEVEL": ("2147483649", "0x80000001"),
            "CIX_RELEASE": ("v1.2", "1.2"),
            "ENABLE_TF_A_FIXES": ("true", "TRUE"),
        }
        for key, (argument, expected) in cases.items():
            with self.subTest(option=key):
                output, before = self.configure()
                stage = output.with_name(output.name + ".build")
                for changes, value in (({key: argument}, expected), ({}, before[key])):
                    output.mkdir(parents=True, exist_ok=True)
                    stage.mkdir(parents=True, exist_ok=True)
                    old_output = output / "cix_flash_all.bin"
                    old_payload = stage / "old-payload"
                    old_output.write_bytes(b"previous configuration")
                    old_payload.write_bytes(b"previous configuration")
                    _, actual = self.configure(changes)
                    self.assertEqual(actual[key], value)
                    self.assertFalse(old_output.exists(), key)
                    self.assertFalse(old_payload.exists(), key)

    def test_identical_configuration_preserves_outputs_and_board_target_isolation(self):
        output, _ = self.configure()
        image = output / "cix_flash_all.bin"
        image.write_bytes(b"existing output")
        self.configure()
        self.assertTrue(image.exists())
        self.configure(board="O6N")
        self.configure({"FIRMWARE_TARGET": "DEBUG"})
        self.assertEqual(image.read_bytes(), b"existing output")

    def test_public_and_buildbox_arguments_follow_each_new_invocation(self):
        probe = self.root / "probe.mk"
        probe.write_text(".PHONY: capture\ncapture:\n\t@printf '%s\\n' $(CAPTURE_ARGS)\n")
        changed = {
            "ENABLE_FIRMWARE_FIXES": "false", "ENABLE_CORE_ORDER": "performance",
            "ENABLE_EXPERIMENTAL_UEFI_SETTINGS": "true", "DEBUG_VERBOSE": "true",
            "DEBUG_ON_UART3": "true", "UART3_ENABLE": "true",
            "DEBUG_PRINT_ERROR_LEVEL": "0x80000001", "CIX_RELEASE": "1.2",
        }
        for changes in ({}, changed, {}):
            settings = {**self.base, "FIRMWARE_BOARD": "O6", "FIRMWARE_DISTRO": "trixie", **changes}
            for makefile, variable, normalized in (
                (ROOT / "Makefile", "DELEGATED_BUILD_ARGS", False),
                (self.root / ".github/local/Makefile.local", "BUILDBOX_FIRMWARE_ARGS", True),
            ):
                result = subprocess.run(
                    [self.make, "--no-print-directory", "-f", str(makefile), "-f", str(probe),
                     "capture", f"CAPTURE_ARGS=$({variable})",
                     *(f"{key}={value}" for key, value in settings.items())],
                    cwd=self.root, capture_output=True, text=True, timeout=30,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                actual = dict(line.split("=", 1) for line in result.stdout.splitlines())
                for key in (*changed, "FIRMWARE_BOARD", "FIRMWARE_TARGET", "FIRMWARE_DISTRO"):
                    expected = settings[key]
                    if normalized and expected in ("true", "false"):
                        expected = expected.upper()
                    self.assertEqual(actual.get(key, ""), expected, (variable, key))


if __name__ == "__main__":
    unittest.main()
