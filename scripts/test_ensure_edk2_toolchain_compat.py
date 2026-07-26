#!/usr/bin/env python3

from pathlib import Path
import tempfile
import unittest

from ensure_edk2_toolchain_compat import add_compatibility_tag


GCC_DEFINITIONS = """\
*_GCC_*_FAMILY = GCC
*_GCC_AARCH64_CC_PATH = ENV(GCC_AARCH64_PREFIX)gcc
DEBUG_GCC_AARCH64_CC_FLAGS = DEF(GCC_AARCH64_CC_FLAGS) -flto
DEBUG_GCC_AARCH64_DLINK_FLAGS = DEF(GCC_AARCH64_DLINK_FLAGS) -flto
"""


class Edk2ToolchainCompatTest(unittest.TestCase):
    def test_adds_compatibility_keys_without_rewriting_values(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tools_def.txt"
            path.write_text(GCC_DEFINITIONS, encoding="utf-8")

            count = add_compatibility_tag(path, "GCC", "GCC5")
            result = path.read_text(encoding="utf-8")

        self.assertEqual(count, 4)
        self.assertIn("*_GCC5_*_FAMILY = GCC", result)
        self.assertIn("*_GCC5_AARCH64_CC_PATH = ENV(GCC_AARCH64_PREFIX)gcc", result)
        self.assertIn(
            "DEBUG_GCC5_AARCH64_CC_FLAGS = DEF(GCC_AARCH64_CC_FLAGS) -flto",
            result,
        )

    def test_existing_compatibility_tag_is_unchanged(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tools_def.txt"
            path.write_text(GCC_DEFINITIONS, encoding="utf-8")
            add_compatibility_tag(path, "GCC", "GCC5")
            original = path.read_text(encoding="utf-8")

            count = add_compatibility_tag(path, "GCC", "GCC5")

            self.assertEqual(count, 0)
            self.assertEqual(path.read_text(encoding="utf-8"), original)

    def test_rejects_partial_existing_compatibility_tag(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tools_def.txt"
            path.write_text(
                GCC_DEFINITIONS + "*_GCC5_*_FAMILY = GCC\n",
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "existing GCC5 tag is incomplete"):
                add_compatibility_tag(path, "GCC", "GCC5")

    def test_rejects_incomplete_source_toolchain(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tools_def.txt"
            path.write_text("*_GCC_*_FAMILY = GCC\n", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "required compatibility keys"):
                add_compatibility_tag(path, "GCC", "GCC5")


if __name__ == "__main__":
    unittest.main()
