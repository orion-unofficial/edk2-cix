#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import unittest


SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from audit_acpi_regression import compare_audits, compare_semantic_audits


TABLE_PATH = "AARCH64/Platform/CIX/Sky1/Drivers/AcpiSocTables/AcpiSocTables/OUTPUT/Fadt.acpi"


class AuditAcpiRegressionTests(unittest.TestCase):
    def test_compare_audits_flags_table_sha_changes(self) -> None:
        expected = {
            "board": "O6",
            "target": "RELEASE_GCC5",
            "tables": {
                TABLE_PATH: {
                    "path": TABLE_PATH,
                    "size": 448,
                    "sha256": "baseline",
                }
            },
            "iasl": {},
        }
        actual = {
            "board": "O6",
            "target": "RELEASE_GCC5",
            "tables": {
                TABLE_PATH: {
                    "path": TABLE_PATH,
                    "size": 448,
                    "sha256": "current",
                }
            },
            "iasl": {},
        }

        self.assertEqual(
            compare_audits(expected, actual),
            [f"Changed ACPI tables: {TABLE_PATH}"],
        )

    def test_compare_audits_flags_iasl_summary_changes(self) -> None:
        expected = {
            "board": "O6",
            "target": "RELEASE_GCC5",
            "tables": {},
            "iasl": {
                "soc_dsdt": {
                    "path": "AARCH64/.../Dsdt.iiii",
                    "status": "match",
                    "summary": {
                        "errors": 0,
                        "warnings": 39,
                        "remarks": 56,
                        "optimizations": 2649,
                        "constants_folded": 156,
                    },
                    "warning_codes": {"3175": 1},
                    "remark_codes": {"2173": 1},
                    "error_codes": {},
                    "warning_messages": {"3175": "warning"},
                    "remark_messages": {"2173": "remark"},
                    "error_messages": {},
                }
            },
        }
        actual = {
            "board": "O6",
            "target": "RELEASE_GCC5",
            "tables": {},
            "iasl": {
                "soc_dsdt": {
                    "path": "AARCH64/.../Dsdt.iiii",
                    "status": "match",
                    "summary": {
                        "errors": 0,
                        "warnings": 39,
                        "remarks": 55,
                        "optimizations": 2645,
                        "constants_folded": 156,
                    },
                    "warning_codes": {"3175": 1},
                    "remark_codes": {"2173": 1},
                    "error_codes": {},
                    "warning_messages": {"3175": "warning"},
                    "remark_messages": {"2173": "remark"},
                    "error_messages": {},
                }
            },
        }

        self.assertEqual(
            compare_audits(expected, actual),
            ["Changed IASL audits: soc_dsdt"],
        )

    def test_semantic_compare_allows_compiler_output_changes(self) -> None:
        expected = {
            "tables": {
                TABLE_PATH: {
                    "path": TABLE_PATH,
                    "size": 448,
                    "sha256": "old-compiler",
                }
            },
            "iasl": {
                "soc_dsdt": {
                    "status": "match",
                    "summary": {"errors": 0, "warnings": 39},
                    "error_codes": {},
                }
            },
        }
        actual = {
            "tables": {
                TABLE_PATH: {
                    "path": TABLE_PATH,
                    "size": 452,
                    "sha256": "new-compiler",
                }
            },
            "iasl": {
                "soc_dsdt": {
                    "status": "match",
                    "summary": {"errors": 0, "warnings": 2},
                    "error_codes": {},
                }
            },
        }

        self.assertEqual(compare_semantic_audits(expected, actual), [])

    def test_semantic_compare_rejects_missing_tables_and_compiler_errors(self) -> None:
        expected = {
            "tables": {TABLE_PATH: {}},
            "iasl": {"soc_dsdt": {}},
        }
        actual = {
            "tables": {},
            "iasl": {
                "soc_dsdt": {
                    "status": "compiler-error",
                    "summary": {"errors": 1},
                    "error_codes": {"6126": 1},
                }
            },
        }

        self.assertEqual(
            compare_semantic_audits(expected, actual),
            [
                f"Missing ACPI tables: {TABLE_PATH}",
                "Failed IASL audits: soc_dsdt",
            ],
        )


if __name__ == "__main__":
    unittest.main()
