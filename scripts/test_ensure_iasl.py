#!/usr/bin/env python3

from __future__ import annotations

import os
import pathlib
import subprocess
import tempfile
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "scripts" / "ensure_iasl.sh"


def write_fake_iasl(path: pathlib.Path, version: str) -> None:
    path.write_text(
        "#!/usr/bin/env bash\n"
        f"printf '%s\\n' 'ASL+ Optimizing Compiler/Disassembler version {version}'\n",
        encoding="utf-8",
    )
    path.chmod(0o755)


class EnsureIaslTests(unittest.TestCase):
    def test_accepts_the_pinned_2026_release(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            iasl = pathlib.Path(tempdir) / "iasl"
            write_fake_iasl(iasl, "20260408")
            result = subprocess.run(
                [str(SCRIPT), "--verify", str(iasl)],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(pathlib.Path(result.stdout.strip()), iasl)

    def test_rejects_the_hosts_2020_release(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            iasl = pathlib.Path(tempdir) / "iasl"
            write_fake_iasl(iasl, "20200925")
            result = subprocess.run(
                [str(SCRIPT), "--verify", str(iasl)],
                check=False,
                capture_output=True,
                text=True,
                env={**os.environ, "IASL": str(iasl)},
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("ACPICA 20200925", result.stderr)
            self.assertIn("requires 20260408", result.stderr)


if __name__ == "__main__":
    unittest.main()
