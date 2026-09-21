#!/usr/bin/env python3

from __future__ import annotations

import os
import subprocess
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
FILTER = REPO_ROOT / "src" / "scripts" / "filter_edk2_build_output.py"


class FilterEdk2BuildOutputTests(unittest.TestCase):
    def run_filter(self, text: str, *, verbose: bool = False) -> str:
        env = os.environ.copy()
        env["V"] = "1" if verbose else "0"
        result = subprocess.run(
            [sys.executable, str(FILTER)],
            input=text,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
            env=env,
        )
        return result.stdout

    def test_drops_debuglink_noop_output(self) -> None:
        output = self.run_filter(
            "\n".join(
                [
                    "Building edk2/MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf [AARCH64]",
                    "--add-gnu-debuglink=/tmp/Build/O6/DEBUG_GCC/AARCH64/Module/DEBUG/Module.debug /tmp/Build/O6/DEBUG_GCC/AARCH64/Module/DEBUG/Module.dll",
                    '"echo" --add-gnu-debuglink=/tmp/Build/O6/DEBUG_GCC/AARCH64/Module/DEBUG/Module.debug /tmp/Build/O6/DEBUG_GCC/AARCH64/Module/DEBUG/Module.dll',
                    "error: real build failure",
                    "",
                ]
            )
        )

        self.assertNotIn("--add-gnu-debuglink", output)
        self.assertIn("Building edk2/MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf [AARCH64]", output)
        self.assertIn("error: real build failure", output)

    def test_drops_optional_copy_misses_in_quiet_mode(self) -> None:
        text = "\n".join(
            [
                "Building edk2/MdeModulePkg/Core/Dxe/DxeMain.inf [AARCH64]",
                "cp: cannot stat '/var/tmp/workspace/Build/O6/RELEASE_GCC/AARCH64/MdeModulePkg/Core/Dxe/DxeMain/DEBUG/*.pdb': No such file or directory",
                "cp: cannot stat '/var/tmp/workspace/Build/O6/RELEASE_GCC/AARCH64/MdeModulePkg/Application/HelloWorld/HelloWorld/OUTPUT/HelloWorldhii.res': No such file or directory",
                "cp: cannot stat '/var/tmp/workspace/Build/O6/RELEASE_GCC/AARCH64/required.bin': No such file or directory",
                "error: real build failure",
                "",
            ]
        )

        output = self.run_filter(text)

        self.assertNotIn("*.pdb", output)
        self.assertNotIn("HelloWorldhii.res", output)
        self.assertIn("required.bin", output)
        self.assertIn("error: real build failure", output)

    def test_preserves_optional_copy_misses_in_verbose_mode(self) -> None:
        text = "\n".join(
            [
                "cp: cannot stat '/var/tmp/workspace/Build/O6/RELEASE_GCC/AARCH64/MdeModulePkg/Core/Dxe/DxeMain/DEBUG/*.pdb': No such file or directory",
                "",
            ]
        )

        output = self.run_filter(text, verbose=True)

        self.assertIn("*.pdb", output)


if __name__ == "__main__":
    unittest.main()
