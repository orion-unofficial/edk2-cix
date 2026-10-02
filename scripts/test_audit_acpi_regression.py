#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import unittest


SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from audit_acpi_regression import compare_audits


TABLE_PATH = "AARCH64/Platform/CIX/Sky1/Drivers/AcpiSocTables/AcpiSocTables/OUTPUT/Fadt.acpi"


class AuditAcpiRegressionTests(unittest.TestCase):
    def test_compare_audits_flags_table_sha_changes(self) -> None:
        expected = {
            "board": "O6",
            "target": "RELEASE_GCC",
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
            "target": "RELEASE_GCC",
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
            "target": "RELEASE_GCC",
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
            "target": "RELEASE_GCC",
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


if __name__ == "__main__":
    unittest.main()
