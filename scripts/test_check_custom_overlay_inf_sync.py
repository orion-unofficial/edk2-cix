#!/usr/bin/env python3

from __future__ import annotations

import tempfile
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from check_custom_overlay_inf_sync import compare_overlay_inf


class CheckCustomOverlayInfSyncTest(unittest.TestCase):
    def write_file(self, path: Path, contents: str) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")

    def test_overlay_may_add_entries_but_not_drop_source_dependencies(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            overlay_root = root / "overlay"
            source_root = root / "source"
            overlay_inf = overlay_root / "Pkg/Module/Module.inf"
            source_inf = source_root / "Pkg/Module/Module.inf"

            self.write_file(
                source_inf,
                """
[LibraryClasses]
  BaseLib
  PcdLib

[Pcd]
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString
""".strip(),
            )
            self.write_file(
                overlay_inf,
                """
[LibraryClasses]
  BaseLib
  PcdLib
  DebugLib

[Pcd]
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString
""".strip(),
            )

            self.assertEqual(compare_overlay_inf(overlay_inf, source_inf, overlay_root), [])

    def test_overlay_reports_missing_source_dependency_entries(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            overlay_root = root / "overlay"
            source_root = root / "source"
            overlay_inf = overlay_root / "Pkg/Module/Module.inf"
            source_inf = source_root / "Pkg/Module/Module.inf"

            self.write_file(
                source_inf,
                """
[LibraryClasses]
  BaseLib
  PcdLib

[Pcd]
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString
""".strip(),
            )
            self.write_file(
                overlay_inf,
                """
[LibraryClasses]
  BaseLib
""".strip(),
            )

            self.assertEqual(
                compare_overlay_inf(overlay_inf, source_inf, overlay_root),
                [
                    "Pkg/Module/Module.inf is missing [LibraryClasses] entry: PcdLib",
                    "Pkg/Module/Module.inf is missing [Pcd] entry: gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString",
                ],
            )


if __name__ == "__main__":
    unittest.main()
