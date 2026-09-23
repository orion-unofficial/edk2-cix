#!/usr/bin/env python3
"""Ensure a diagnostic build cannot silently discard its BDS progress markers."""

from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

from check_release_debug import BDS_MARKERS, check
from reconstruction_common import show_file


class ReleaseDebugTests(unittest.TestCase):
    def test_source_build_hook_checks_the_selected_toolchain_directory(self):
        repo = Path(__file__).resolve().parents[1]
        for ref, toolchain in (("source/unofficial/edk2-stable202208", "GCC5"),
                               ("source/unofficial/1.3/current", "GCC")):
            with self.subTest(ref=ref), tempfile.TemporaryDirectory(prefix="debug-build-hook-") as directory:
                makefile = show_file(repo, ref, "src/Makefile").decode()
                command = re.search(r'python3 "\$\(REPO_ROOT\)/src/scripts/check_release_debug.py".*?;', makefile, re.S).group()
                command = command.replace("$(REPO_ROOT)/src/scripts", str(repo / "scripts"))
                command = command.replace("$(or $(DEBUG_PRINT_ERROR_LEVEL_EFFECTIVE),0x80000040)", "0x80000001")
                command = command.replace("$(UEFI_TARGET)", "RELEASE").replace("$*", "O6").replace("$$", "$")
                build = Path(directory) / ("Build/O6/RELEASE_" + toolchain)
                image = build / "AARCH64/BdsDxe.efi"
                image.parent.mkdir(parents=True)
                image.write_bytes(b"MZ\0" + b"\0".join(BDS_MARKERS))
                environment = {**os.environ, "build_options_path": str(build / "BuildOptions")}
                result = subprocess.run(["bash", "-c", command], cwd=directory, env=environment, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("verified", result.stdout)
                image.write_bytes(b"MZ\0")
                result = subprocess.run(["bash", "-c", command], cwd=directory, env=environment, capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("compiled out", result.stderr)

    def test_release_without_debug_messages_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="release-debug-") as directory:
            root = Path(directory)
            (root / "AARCH64").mkdir()
            image = root / "AARCH64/BdsDxe.efi"
            image.write_bytes(b"MZ\0PlatformBootManagerAfterConsole\0")
            with self.assertRaisesRegex(ValueError, "compiled out"):
                check(root, 0x80000001)
            with self.assertRaisesRegex(ValueError, "compiled out"):
                check(root, 0x400)
            # An explicitly error-only mask need not carry INIT messages.
            check(root, 0x80000000)
            image.write_bytes(b"MZ\0" + b"\0".join(BDS_MARKERS))
            check(root, 0x80000001)
            check(root, 0x400)
            image.write_bytes(b"MZ\0" + b"\0".join(BDS_MARKERS[:-1]))
            with self.assertRaisesRegex(ValueError, "HandleCapsules"):
                check(root, 0x80000001)


if __name__ == "__main__":
    unittest.main()
