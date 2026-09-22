#!/usr/bin/env python3

from pathlib import Path
import json
import os
import tempfile

from reconstruction_common import for_each_ref, show_file
import subprocess
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]


class BuildMakefileTests(unittest.TestCase):
    def test_targetless_make_dispatches_the_upstream_profile(self) -> None:
        result = subprocess.run(
            ["make", "--no-print-directory", "FIRST_OUTPUT_PROBE=1"],
            cwd=REPO_ROOT,
            check=True,
            capture_output=True,
            text=True,
        )

        self.assertIn("Resolving firmware profile: <default>", result.stderr)

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

    def test_product_cannot_disagree_with_board(self) -> None:
        for mode in ("custom", "upstream"):
            result = subprocess.run(
                ["make", "--no-print-directory", "build", "FIRST_OUTPUT_PROBE=1",
                 "FIRMWARE_BOARD=O6", "FIRMWARE_PRODUCT=orion-o6n", f"ARTEFACT_MODE={mode}"],
                cwd=REPO_ROOT, capture_output=True, text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Select only FIRMWARE_BOARD", result.stderr)
            self.assertNotIn("[build]", result.stderr)

    def test_help_has_one_board_selector_and_no_replacement_cix_advice(self) -> None:
        result = subprocess.run(["make", "--no-print-directory", "help-vars"],
                                cwd=REPO_ROOT, check=True, capture_output=True, text=True)
        self.assertIn("FIRMWARE_BOARD=O6|O6N", result.stdout)
        self.assertNotIn("FIRMWARE_PRODUCT=", result.stdout)
        self.assertNotIn("CIX early-boot", result.stdout)
        readme = (REPO_ROOT / 'README.md').read_text()
        self.assertNotIn('FIRMWARE_PRODUCT=<name>', readme)
        self.assertNotIn('RELEASE=edk2-202608/cix-', readme)
        self.assertNotIn('optional CIX early-boot', readme)

    def test_retained_source_wrappers_derive_product_and_reject_mismatch(self):
        candidates = os.environ.get('SOURCE_BOARD_CANDIDATES')
        refs = ([entry['new'] for entry in json.loads(Path(candidates).read_text())]
                if candidates else for_each_ref(REPO_ROOT, 'source/unofficial/'))
        snippets = set()
        for ref in refs:
            text = show_file(REPO_ROOT, ref, '.github/local/Makefile.local').decode()
            snippet = text[text.index('FIRMWARE_BOARD ?='):text.index('DEFAULT_FIRMWARE_TARGET :=')]
            if snippet in snippets:
                continue
            snippets.add(snippet)
            with tempfile.TemporaryDirectory(prefix='board-selector-') as tmp:
                root = Path(tmp)
                (root / 'Makefile').write_text(snippet + '\nall:\n\t@echo $(FIRMWARE_PRODUCT)\n')
                for board, product in (('O6', 'orion-o6'), ('O6N', 'orion-o6n')):
                    for explicit in ([], [f'FIRMWARE_PRODUCT={product}']):
                        result = subprocess.run(['make', '--no-print-directory', f'FIRMWARE_BOARD={board}', *explicit],
                                                cwd=root, capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertEqual(result.stdout.strip(), product)
                result = subprocess.run(['make', 'FIRMWARE_BOARD=O6N', 'FIRMWARE_PRODUCT=orion-o6'],
                                        cwd=root, capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('Select only FIRMWARE_BOARD', result.stderr)

    def test_replay_source_target_follows_release_version(self) -> None:
        result = subprocess.run(
            [
                "make",
                "--no-print-directory",
                "deterministic-replay",
                "FIRST_OUTPUT_PROBE=1",
                "REPLAY_VERSION=1.3.1",
            ],
            cwd=REPO_ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
        self.assertIn(
            "Preparing replay-capable source target: edk2-202208/radxa-1.3.1",
            result.stderr,
        )
        self.assertNotIn("unofficial", result.stderr)

    def test_replay_defaults_to_radxa_1_3_1(self) -> None:
        result = subprocess.run(
            [
                "make",
                "--no-print-directory",
                "deterministic-replay",
                "FIRST_OUTPUT_PROBE=1",
            ],
            cwd=REPO_ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
        self.assertIn(
            "Preparing replay-capable source target: edk2-202208/radxa-1.3.1",
            result.stderr,
        )


if __name__ == "__main__":
    unittest.main()
