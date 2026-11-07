#!/usr/bin/env python3

from pathlib import Path
import json
import os
import shlex
import sys
import tempfile

from reconstruction_common import unofficial_firmware_refs, show_file
import subprocess
import unittest

from test_render_release_branch import CUSTOM_TARGET, UPSTREAM_TARGET, fixture_source_state, rendering_fixture


REPO_ROOT = Path(__file__).resolve().parents[1]


class BuildMakefileTests(unittest.TestCase):
    def run_rendered_fixture(self, repo, target, verbose, *, exit_code=0):
        # Keep the public Make recipe and real renderer/preparer. Intercept only
        # artifact validators/mirroring/installation: this fixture has no firmware.
        dispatcher = repo / ".cache" / "fixture-python.py"
        dispatcher.parent.mkdir(exist_ok=True)
        dispatcher.write_text("""import os, pathlib, sys
script = sys.argv[1]
if pathlib.Path(script).name in {'render_release_branch.py', 'prepare_release_worktree.py'}:
    os.execv(sys.executable, [sys.executable, *sys.argv[1:]])
if pathlib.Path(script).name == 'validate_build_variables.py':
    sys.exit(0)
assert pathlib.Path(script).name in {'validate_bootloader1.py', 'validate_firmware_chain.py', 'mirror_build_outputs.py', 'install_release_payload.py'}, script
worktree = pathlib.Path(sys.argv[sys.argv.index('--worktree') + 1])
assert worktree.is_dir() and (worktree / 'VERSION').read_text() == '1.3.1\\n', repr(str(worktree))
if pathlib.Path(script).name == 'install_release_payload.py':
    pathlib.Path(os.environ['FIXTURE_INSTALL_RECEIPT']).write_text(str(worktree) + '\\n')
print('fixture helper: ' + script, file=sys.stderr)
""")
        env = os.environ.copy()
        env.update(EDK2_CIX_TMP_ROOT=str(repo / ".worktrees" / "scratch"),
                   EDK2_CIX_WORKTREE_NAMESPACE="", FIXTURE_RESULT=str(repo / ".cache/delegated"),
                   FIXTURE_INSTALL_RECEIPT=str(repo / ".cache/install-intercepted"))
        runner = repo / ".cache/buildbox-runner.sh"
        runner.write_text('#!/bin/bash\nexec "$1" -C "$EDK2_CIX_FIRMWARE_ROOT" "${@:2}"\n')
        runner.chmod(0o755)
        certs = repo / ".cache/certs"
        certs.mkdir()
        for name in ("trusted_key_no.crt", "nt_fw_cert.crt", "nt_fw_key.crt"):
            (certs / name).write_text("fixture certificate\n")
        return subprocess.run(
            ["make", "--no-print-directory", target, f"V={verbose}", "FIRST_OUTPUT_PROBE=0",
             f"PYTHON={shlex.join([sys.executable, str(dispatcher)])}",
             f"RELEASE={CUSTOM_TARGET}", f"REPLAY_SOURCE_TARGET={UPSTREAM_TARGET}",
             "REPLAY_DOWNLOAD=0", "REPLAY_INPUT=", f"FIXTURE_EXIT={exit_code}",
             f"BUILDBOX_RUNNER={runner}", f"SIGNING_CERT_SOURCE_DIR={certs}", f"INSTALL_ROOT={repo / '.cache/never-install'}"],
            cwd=repo, env=env, capture_output=True, text=True,
        )

    def test_public_make_render_capture_paths_at_both_verbosity_levels(self):
        goals = {"build": "firmware-build", "build-all": "build-all",
                 "zip": "zip", "targz": "targz",
                 "buildbox-firmware-build": "firmware-build",
                 "buildbox-firmware-stage": "firmware-stage",
                 "install": "firmware-stage", "deterministic-replay": "deterministic-replay"}
        for verbose in ("0", "1"):
            for target, delegated_goal in goals.items():
                with self.subTest(target=target, verbose=verbose), rendering_fixture() as repo:
                    before = fixture_source_state(repo)
                    result = self.run_rendered_fixture(repo, target, verbose)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    lines = (repo / ".cache/delegated").read_text().splitlines()
                    worktree = Path(lines[0])
                    self.assertTrue(worktree.is_dir())
                    self.assertEqual(lines[1:3], [delegated_goal, verbose])
                    stage = "upstream" if target == "deterministic-replay" else "custom"
                    self.assertIn(f"release_{stage}_edk2-202208_radxa-1.3.1", worktree.name)
                    self.assertIn("fixture delegate stderr", result.stderr)
                    if stage == "custom":
                        self.assertTrue((worktree / lines[3] / "nt_fw_cert.crt").is_file())
                    if target == "install":
                        self.assertEqual((repo / ".cache/install-intercepted").read_text(), str(worktree) + "\n")
                        self.assertFalse((repo / ".cache/never-install").exists())
                    if verbose == "1":
                        self.assertIn("rm '", result.stderr)
                    self.assertEqual(fixture_source_state(repo), before)

    def test_make_preserves_delegated_failure_diagnostics(self):
        for verbose in ("0", "1"):
            for target in ("build", "deterministic-replay"):
                with self.subTest(target=target, verbose=verbose), rendering_fixture() as repo:
                    before = fixture_source_state(repo)
                    result = self.run_rendered_fixture(repo, target, verbose, exit_code=37)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("Error 37", result.stderr)
                    self.assertIn("fixture delegate stderr", result.stderr)
                    self.assertNotIn("fixture helper: scripts/mirror_build_outputs.py", result.stderr)
                    self.assertEqual(fixture_source_state(repo), before)

    def test_make_stops_before_delegation_on_render_failure(self):
        for verbose in ("0", "1"):
            for target in ("build", "deterministic-replay"):
                with self.subTest(target=target, verbose=verbose), rendering_fixture() as repo:
                    before = fixture_source_state(repo)
                    hook = repo / ".git/hooks/pre-commit"
                    hook.write_text("#!/bin/sh\necho fixture-render-failure >&2\nexit 1\n")
                    hook.chmod(0o755)
                    result = self.run_rendered_fixture(repo, target, verbose)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("fixture-render-failure", result.stderr)
                    self.assertFalse((repo / ".cache/delegated").exists())
                    self.assertEqual(fixture_source_state(repo), before)

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
                if candidates else unofficial_firmware_refs(REPO_ROOT))
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

    def test_replay_defaults_to_radxa_1_3_2(self) -> None:
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
            "Preparing replay-capable source target: edk2-202208/radxa-1.3.2",
            result.stderr,
        )


if __name__ == "__main__":
    unittest.main()
