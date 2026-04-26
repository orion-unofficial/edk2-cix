#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import unittest


SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from validate_make_inputs import build_parser, emit_c_string_literal, run_validate


class ValidateMakeInputsTests(unittest.TestCase):
    def setUp(self) -> None:
        self.parser = build_parser()

    def parse(self, *args: str):
        return self.parser.parse_args(["validate", *args])

    def test_upstream_allows_unset_core_order(self) -> None:
        args = self.parse("--artefact-mode", "upstream", "--firmware-board", "O6")
        self.assertEqual(run_validate(args), 0)

    def test_upstream_allows_no_op_cix_core_order(self) -> None:
        args = self.parse(
            "--artefact-mode",
            "upstream",
            "--firmware-board",
            "O6",
            "--enable-core-order",
            "cix",
        )
        self.assertEqual(run_validate(args), 0)

    def test_upstream_rejects_non_default_core_order(self) -> None:
        args = self.parse(
            "--artefact-mode",
            "upstream",
            "--firmware-board",
            "O6",
            "--enable-core-order",
            "conventional",
        )
        self.assertEqual(run_validate(args), 2)

    def test_custom_o6_allows_shared_o6_smbios_asset_tag(self) -> None:
        args = self.parse(
            "--artefact-mode",
            "custom",
            "--firmware-board",
            "O6",
            "--o6-smbios-asset-tag",
            "Lab Asset 42",
        )
        self.assertEqual(run_validate(args), 0)

    def test_upstream_rejects_o6_smbios_asset_tag(self) -> None:
        args = self.parse(
            "--artefact-mode",
            "upstream",
            "--firmware-board",
            "O6",
            "--o6-smbios-asset-tag",
            "Lab Asset 42",
        )
        self.assertEqual(run_validate(args), 2)

    def test_non_o6_rejects_o6_smbios_asset_tag(self) -> None:
        args = self.parse(
            "--artefact-mode",
            "custom",
            "--firmware-board",
            "O6N",
            "--o6-smbios-asset-tag",
            "Lab Asset 42",
        )
        self.assertEqual(run_validate(args), 2)

    def test_emit_c_string_literal_escapes_spaces(self) -> None:
        self.assertEqual(emit_c_string_literal("Lab Asset 42"), '"Lab Asset 42"')

    def test_emit_c_string_literal_rejects_shell_sensitive_quotes(self) -> None:
        with self.assertRaises(ValueError):
            emit_c_string_literal('bad"value')


if __name__ == "__main__":
    unittest.main()
