#!/usr/bin/env python3
"""Guard CIX_RELEASE parsing and custom-only profile selection."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from build_profiles import resolve_profile
from reconstruction_common import ReconstructionError, for_each_ref, show_file
from validate_build_variables import validate

ROOT = Path(__file__).resolve().parents[1]
ERROR = "CIX_RELEASE supports only 1.2"


class CixReleasePolicyTests(unittest.TestCase):
    def test_invalid_values_fail_during_make_parse_without_touching_outputs(self):
        with tempfile.TemporaryDirectory(prefix="cix-policy-") as tmp:
            root = Path(tmp)
            old = root / "cix_flash_all.bin"
            old.write_bytes(b"previous valid firmware")
            for value in ("v", "unknown", "0", "2.0"):
                for goal in ("build", "firmware", "help-vars"):
                    result = subprocess.run(
                        ["make", "--no-print-directory", "-f", str(ROOT / "Makefile"), goal,
                         "CIX_RELEASE=" + value, "PYTHON=/must-not-run", "SHELL=/must-not-run"],
                        cwd=root, capture_output=True, text=True, timeout=5)
                    self.assertNotEqual(result.returncode, 0, (value, goal))
                    self.assertIn(ERROR, result.stderr)
                    self.assertEqual(list(root.iterdir()), [old])
                    self.assertEqual(old.read_bytes(), b"previous valid firmware")

    def test_supported_values_are_advertised_and_upstream_is_rejected(self):
        for value in ("1.2", "v1.2", "V1.2"):
            result = subprocess.run(
                ["make", "--no-print-directory", "help-vars", f"CIX_RELEASE={value}",
                 "ARTEFACT_MODE=custom"], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("CIX_RELEASE=1.2|v1.2", result.stdout)
        result = subprocess.run(
            ["make", "help", "CIX_RELEASE=1.2", "ARTEFACT_MODE=upstream",
             "SHELL=/must-not-run"], cwd=ROOT, capture_output=True, text=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires ARTEFACT_MODE=custom", result.stderr)

    def test_retained_source_make_entry_points_reject_invalid_values(self):
        refs = for_each_ref(ROOT, "source/unofficial/")
        self.assertTrue(refs)
        with tempfile.TemporaryDirectory(prefix="cix-source-policy-") as tmp:
            root = Path(tmp)
            makefile = root / "Makefile"
            for ref in refs:
                try:
                    show_file(ROOT, ref, "custom/signing-keys/cix-1.2/cix_privatekey.pem")
                except ReconstructionError:
                    expected = "CIX_RELEASE must be empty"
                else:
                    expected = ERROR
                for name in ("Makefile", "src/Makefile", ".github/local/Makefile.local"):
                    makefile.write_bytes(show_file(ROOT, ref, name))
                    result = subprocess.run(
                        [shutil.which("gmake") or "make", "-f", str(makefile),
                         "help", "CIX_RELEASE=unknown", "SHELL=/must-not-run"],
                        cwd=root, capture_output=True, text=True, timeout=5)
                    self.assertNotEqual(result.returncode, 0, (ref, name))
                    self.assertIn(expected, result.stderr, (ref, name, result.stderr))

    def test_profile_and_direct_validator_accept_only_custom_v12(self):
        profile = resolve_profile(ROOT, requested_profile="latest", cix_release_override="v1.2")
        self.assertEqual(profile["cix_early_boot_release"], "1.2")
        with self.assertRaisesRegex(ReconstructionError, ERROR):
            resolve_profile(ROOT, requested_profile="latest", cix_release_override="v")
        for value, mode, success in (("1.2", "custom", True),
                                     ("v1.2", "custom", True),
                                     ("1.2", "upstream", False),
                                     ("v", "custom", False)):
            with patch.dict(os.environ, {"CIX_RELEASE": value, "ARTEFACT_MODE": mode}), \
                 patch("sys.argv", ["validate_build_variables.py", "--repo-root", str(ROOT)]), \
                 patch("validate_build_variables.validate_release"), \
                 patch("validate_build_variables.validate_signing_cert_source"):
                if success:
                    validate()
                else:
                    with self.assertRaises(ReconstructionError):
                        validate()


if __name__ == "__main__":
    unittest.main()
