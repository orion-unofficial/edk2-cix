#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import unittest


SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from validate_firmware_outputs import normalize_recorded_path


class ValidateFirmwareOutputsTests(unittest.TestCase):
    def test_normalize_recorded_path_strips_workspace_root_from_src_paths(self) -> None:
        repo_root = Path("/Users/example/src/edk2-cix")
        value = "/workspaces/edk2-cix/src/edk2-platforms/Platform/Radxa/Orion/O6N/O6N.dsc"
        self.assertEqual(
            normalize_recorded_path(value, repo_root),
            "src/edk2-platforms/Platform/Radxa/Orion/O6N/O6N.dsc",
        )

    def test_normalize_recorded_path_strips_workspace_root_from_build_paths(self) -> None:
        repo_root = Path("/Users/example/src/edk2-cix")
        value = "/workspaces/edk2-cix/src/Build/O6N/RELEASE_GCC/FV/SKY1_BL33_UEFI.fd"
        self.assertEqual(
            normalize_recorded_path(value, repo_root),
            "Build/O6N/RELEASE_GCC/FV/SKY1_BL33_UEFI.fd",
        )

    def test_normalize_recorded_path_preserves_external_absolute_paths(self) -> None:
        repo_root = Path("/Users/example/src/edk2-cix")
        value = "/opt/toolchains/aarch64-linux-gnu-gcc"
        self.assertEqual(normalize_recorded_path(value, repo_root), value)


if __name__ == "__main__":
    unittest.main()
