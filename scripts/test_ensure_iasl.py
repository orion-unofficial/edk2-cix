#!/usr/bin/env python3

from __future__ import annotations

import os
import pathlib
import subprocess
import tempfile
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "scripts" / "ensure_iasl.sh"
REPLAY_PACKAGE_LISTS = (
    REPO_ROOT / "containers" / "replay-bookworm" / "packages.bookworm-amd64.txt",
    REPO_ROOT / "containers" / "replay-bookworm" / "packages.bookworm-arm64.txt",
)


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

    def test_accepts_2020_release_for_historical_replay(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            iasl = pathlib.Path(tempdir) / "iasl"
            write_fake_iasl(iasl, "20200925")
            result = subprocess.run(
                [str(SCRIPT), "--verify", str(iasl)],
                check=False,
                capture_output=True,
                text=True,
                env={
                    **os.environ,
                    "EDK2_CIX_IASL_RELEASE": "20200925",
                    "IASL": str(iasl),
                },
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(pathlib.Path(result.stdout.strip()), iasl)

    def test_replay_image_installs_the_historical_compiler(self) -> None:
        for package_list in REPLAY_PACKAGE_LISTS:
            with self.subTest(package_list=package_list.name):
                packages = package_list.read_text(encoding="utf-8")
                self.assertIn("ca-certificates=20230311+deb12u1", packages)
                self.assertIn("acpica-tools=20200925-8", packages)
                self.assertIn("python-is-python3=3.11.2-1+deb12u1", packages)


if __name__ == "__main__":
    unittest.main()
