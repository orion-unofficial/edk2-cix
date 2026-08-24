#!/usr/bin/env python3

from pathlib import Path
import subprocess
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]


class BuildMakefileTests(unittest.TestCase):
    def test_default_product_follows_board(self) -> None:
        for board, product in (("O6", "orion-o6"), ("O6N", "orion-o6n")):
            with self.subTest(board=board):
                result = subprocess.run(
                    [
                        "make",
                        "--no-print-directory",
                        "-n",
                        "buildbox-firmware-build",
                        "FIRST_OUTPUT_PROBE=1",
                        f"FIRMWARE_BOARD={board}",
                    ],
                    cwd=REPO_ROOT,
                    check=True,
                    capture_output=True,
                    text=True,
                )
                self.assertIn(f'FIRMWARE_PRODUCT="{product}"', result.stdout)


if __name__ == "__main__":
    unittest.main()
