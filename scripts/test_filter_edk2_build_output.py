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
    def run_filter(self, text: str) -> str:
        env = os.environ.copy()
        env["V"] = "0"
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
                    "--add-gnu-debuglink=/tmp/Build/O6/DEBUG_GCC5/AARCH64/Module/DEBUG/Module.debug /tmp/Build/O6/DEBUG_GCC5/AARCH64/Module/DEBUG/Module.dll",
                    '"echo" --add-gnu-debuglink=/tmp/Build/O6/DEBUG_GCC5/AARCH64/Module/DEBUG/Module.debug /tmp/Build/O6/DEBUG_GCC5/AARCH64/Module/DEBUG/Module.dll',
                    "error: real build failure",
                    "",
                ]
            )
        )

        self.assertNotIn("--add-gnu-debuglink", output)
        self.assertIn("Building edk2/MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf [AARCH64]", output)
        self.assertIn("error: real build failure", output)


if __name__ == "__main__":
    unittest.main()
