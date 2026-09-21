#!/usr/bin/env python3
"""Exercise the warning policy from the retained firmware source, including Bison."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
FILES = (
    "scripts/ensure_iasl.sh",
    "scripts/test_ensure_iasl.py",
    "scripts/test_filter_edk2_build_output.py",
    "src/scripts/filter_edk2_build_output.py",
    "containers/replay-bookworm/packages.bookworm-amd64.txt",
    "containers/replay-bookworm/packages.bookworm-arm64.txt",
)


class SourceWarningPolicyTests(unittest.TestCase):
    def test_real_source_warning_regressions(self):
        with tempfile.TemporaryDirectory(prefix="source-warning-policy-") as tmp:
            root = Path(tmp)
            for relative in FILES:
                source_root = os.environ.get("SOURCE_TEST_ROOT")
                if source_root:
                    data = (Path(source_root) / relative).read_bytes()
                else:
                    ref = os.environ.get("SOURCE_TEST_REF", "source/unofficial/1.3/current")
                    data = subprocess.check_output(
                        ["git", "-C", str(ROOT), "show", f"{ref}:{relative}"]
                    )
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
                path.chmod(0o755)
            regressions = {
                "test_ensure_iasl.py": "test_bison_extension_is_allowed_but_other_warnings_fail",
            }
            for name, method in regressions.items():
                self.assertIn("def " + method + "(", (root / "scripts" / name).read_text())
                result = subprocess.run(
                    [sys.executable, str(root / "scripts" / name)],
                    capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            # Custom diagnostics stay visible; upstream quiet-mode policy is
            # intentionally preserved for vendor replay.
            warning = "module.c:10: warning: an unexpected compiler warning\n"
            for mode in ("custom", "upstream"):
                result = subprocess.run(
                    [sys.executable, str(root / "src/scripts/filter_edk2_build_output.py")],
                    input=warning, capture_output=True, text=True,
                    env={**os.environ, "ARTEFACT_MODE": mode, "V": "0"},
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, warning if mode == "custom" else "")


if __name__ == "__main__":
    unittest.main()
