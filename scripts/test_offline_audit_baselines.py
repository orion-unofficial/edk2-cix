#!/usr/bin/env python3

from __future__ import annotations

import json
from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parent.parent
MANIFEST_BASELINE = REPO_ROOT / "validation" / "final-image-manifests.json"
ACPI_BASELINE = REPO_ROOT / "validation" / "acpi-audit-baselines.json"
REQUIRED_BOOKWORM_BOARDS = {"O6", "O6N"}


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class OfflineAuditBaselineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manifest_payload = load_json(MANIFEST_BASELINE)
        cls.acpi_payload = load_json(ACPI_BASELINE)

    def test_top_level_profiles_exist(self) -> None:
        for payload in (self.manifest_payload, self.acpi_payload):
            self.assertIsInstance(payload.get("profiles"), dict)
            self.assertTrue(payload["profiles"])

    def test_profile_and_board_coverage_match_between_files(self) -> None:
        manifest_profiles = {
            profile_name: set(profile["boards"])
            for profile_name, profile in self.manifest_payload["profiles"].items()
        }
        acpi_profiles = {
            profile_name: set(profile["boards"])
            for profile_name, profile in self.acpi_payload["profiles"].items()
        }
        self.assertEqual(manifest_profiles, acpi_profiles)

    def test_bookworm_baseline_covers_both_supported_boards(self) -> None:
        for payload in (self.manifest_payload, self.acpi_payload):
            profiles = payload["profiles"]
            self.assertIn("upstream-1.2.1-bookworm", profiles)
            boards = set(profiles["upstream-1.2.1-bookworm"]["boards"])
            self.assertTrue(REQUIRED_BOOKWORM_BOARDS.issubset(boards))

    def test_final_image_manifest_schema(self) -> None:
        for profile_name, profile in self.manifest_payload["profiles"].items():
            self.assertIsInstance(profile.get("boards"), dict, profile_name)
            for board, board_entry in profile["boards"].items():
                manifest = board_entry.get("manifest")
                self.assertIsInstance(manifest, dict, f"{profile_name}/{board}")
                self.assertEqual(manifest.get("board"), board)
                fd = manifest.get("fd")
                self.assertIsInstance(fd, dict, f"{profile_name}/{board}/fd")
                self.assertIsInstance(fd.get("path"), str)
                self.assertIsInstance(fd.get("size"), int)
                self.assertIsInstance(fd.get("sha256"), str)
                fvs = manifest.get("fvs")
                self.assertIsInstance(fvs, dict, f"{profile_name}/{board}/fvs")
                self.assertTrue(fvs, f"{profile_name}/{board}/fvs should not be empty")
                for fv_name, fv_entry in fvs.items():
                    self.assertIsInstance(fv_name, str)
                    self.assertIsInstance(fv_entry.get("entries"), list)
                    for entry in fv_entry["entries"]:
                        self.assertIsInstance(entry.get("guid"), str)
                        self.assertIsInstance(entry.get("offset"), str)

    def test_acpi_baseline_schema(self) -> None:
        for profile_name, profile in self.acpi_payload["profiles"].items():
            self.assertIsInstance(profile.get("boards"), dict, profile_name)
            for board, board_entry in profile["boards"].items():
                acpi = board_entry.get("acpi")
                self.assertIsInstance(acpi, dict, f"{profile_name}/{board}")
                self.assertEqual(acpi.get("board"), board)
                tables = acpi.get("tables")
                self.assertIsInstance(tables, dict, f"{profile_name}/{board}/tables")
                self.assertTrue(tables, f"{profile_name}/{board}/tables should not be empty")
                for table_name, table_entry in tables.items():
                    self.assertIsInstance(table_name, str)
                    self.assertIsInstance(table_entry.get("path"), str)
                    self.assertIsInstance(table_entry.get("size"), int)
                    self.assertIsInstance(table_entry.get("sha256"), str)
                iasl = acpi.get("iasl")
                self.assertIsInstance(iasl, dict, f"{profile_name}/{board}/iasl")
                self.assertTrue(iasl, f"{profile_name}/{board}/iasl should not be empty")
                for audit_name, audit_entry in iasl.items():
                    self.assertIsInstance(audit_name, str)
                    self.assertIsInstance(audit_entry.get("source"), str)
                    self.assertIsInstance(audit_entry.get("status"), str)
                    self.assertIsInstance(audit_entry.get("summary"), dict)
                    self.assertIsInstance(audit_entry.get("diagnostics"), list)
                    for field in (
                        "warning_codes",
                        "remark_codes",
                        "error_codes",
                        "warning_messages",
                        "remark_messages",
                        "error_messages",
                    ):
                        self.assertIsInstance(audit_entry.get(field), dict)


if __name__ == "__main__":
    unittest.main()
