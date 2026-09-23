#!/usr/bin/env python3
"""Exercise actual retained headers/layouts and public preflight failures."""
import os
from pathlib import Path
import subprocess
import unittest

from debug_build_policy import preflight, fd_size
from reconstruction_common import for_each_ref, show_file
from validate_release_inputs import source_fdf

ROOT = Path(__file__).resolve().parents[1]


class DebugPreflightTests(unittest.TestCase):
    def check(self, ref="source/unofficial/1.3/current", **values):
        board = values.pop("board", "O6")
        experimental = values.pop("experimental", False)
        return preflight(lambda p: show_file(ROOT, ref, p).decode(),
                         board=board, target=values.pop("target", "RELEASE"),
                         fdf_override=source_fdf(ROOT, ref, board, experimental), **values)

    def test_all_retained_layouts_and_default_masks(self):
        for ref in for_each_ref(ROOT, "source/unofficial/"):
            for board in ("O6", "O6N"):
                for experimental in (False, True):
                    with self.subTest(ref=ref, board=board, experimental=experimental):
                        report = self.check(ref, board=board, experimental=experimental)
                        self.assertEqual(report["effective_mask"], "0x80000040")
                        self.assertLessEqual(report["fd_size"], report["bootloader3_slot"])

    def test_force_is_not_a_mask_validation_bypass(self):
        for mask in ("0x200", "0xFFFFFFFF", "-1", "0x100000000"):
            with self.subTest(mask=mask), self.assertRaisesRegex(ValueError, "invalid bits"):
                self.check(mask=mask, force="1")

    def test_custom_release_uses_existing_slot_headroom_without_moving_flash(self):
        ref = "source/unofficial/1.3/current"
        for board in ("O6", "O6N"):
            imported = show_file(ROOT, ref, f"src/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.fdf").decode().replace("\r\n", "\n")
            self.assertEqual(source_fdf(ROOT, ref, board),
                             imported.replace("0x001f2000", "0x001f4000").replace("0x1f2\n", "0x1f4\n"))
            report = self.check(ref, board=board)
            self.assertEqual(report["fd_size"], 0x1F4000)
            self.assertEqual(report["bootloader3_slot"], 0x1F9000)
            self.assertEqual(source_fdf(ROOT, ref, board, True), source_fdf(ROOT, ref, board))
            self.assertEqual(self.check(ref, board=board, experimental=True)["fd_size"], 0x1F4000)

    def test_masks_compile_without_size_predictions_or_force(self):
        self.assertEqual(self.check(target="DEBUG")["experimental_reasons"], [])
        self.assertEqual(self.check(verbose="true")["effective_mask"], "0x83FB55FF")
        self.assertEqual(self.check(mask="0x80000001")["experimental_reasons"], [])
        self.assertFalse(self.check(force="1")["allow_large"])
        self.assertTrue(self.check(allow_large="1")["allow_large"])
        with self.assertRaisesRegex(ValueError, "DEBUG_ALLOW_LARGE_IMAGE"):
            self.check(allow_large="yes")

    def test_124_experimental_menus_retain_the_vendor_volume_capacity(self):
        for edk2 in ("202208", "202605", "202608"):
            ref = f"source/unofficial/1.2.4/edk2-stable{edk2}"
            for board in ("O6", "O6N"):
                with self.subTest(edk2=edk2, board=board):
                    imported = show_file(ROOT, ref,
                                         f"src/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.fdf").decode()
                    self.assertEqual(fd_size(imported, "RELEASE"), 0x200000)
                    self.assertEqual(fd_size(source_fdf(ROOT, ref, board, True), "RELEASE"),
                                     fd_size(imported, "RELEASE"))
                    self.assertEqual(self.check(ref, board=board, experimental=True)["bootloader3_slot"],
                                     self.check(ref, board=board)["bootloader3_slot"])

    def test_public_make_rejects_before_rendering_or_downloading(self):
        for value in ("DEBUG_PRINT_ERROR_LEVEL=0x200", "DEBUG_ALLOW_LARGE_IMAGE=2"):
            result = subprocess.run(["make", "build", "RELEASE=edk2-202608/radxa-1.3.1/unofficial", value],
                                    cwd=ROOT, env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("build variable validation failed", result.stderr)
            self.assertNotIn("[render]", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
