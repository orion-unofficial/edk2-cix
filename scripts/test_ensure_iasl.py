#!/usr/bin/env python3

from __future__ import annotations

import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys
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
    def test_provisioning_keeps_stdout_machine_readable(self) -> None:
        script = SCRIPT.read_text(encoding="utf-8")
        self.assertRegex(script, r'make -C "\$\{source_root\}/generate/unix" iasl \\\n.*NOWERROR=FALSE >&2')

    @unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("bison"),
                         "Provisioning uses Bison from the Linux buildbox")
    def test_bison_extension_is_allowed_but_other_warnings_fail(self) -> None:
        script = SCRIPT.read_text(encoding="utf-8")
        match = re.search(r"'YFLAGS=([^']+)'", script)
        self.assertIsNotNone(match)
        flags = shlex.split(match.group(1))
        with tempfile.TemporaryDirectory() as tempdir:
            grammar = pathlib.Path(tempdir) / "parser.y"
            grammar.write_text("%expect 0\n%%\nstart: 'x';\n", encoding="utf-8")
            command = ["bison", *flags, "-d", str(grammar)]
            result = subprocess.run(command, cwd=tempdir, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, "")
            self.assertTrue((pathlib.Path(tempdir) / "y.tab.c").is_file())
            self.assertTrue((pathlib.Path(tempdir) / "y.tab.h").is_file())
            grammar.write_text(
                "%expect 0\n%%\nstart: 'x';\nunused: 'y';\n", encoding="utf-8",
            )
            result = subprocess.run(command, cwd=tempdir, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0, result.stderr)
            self.assertIn("error", result.stderr)
            self.assertIn("useless", result.stderr)

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

    def test_replay_image_builds_the_pinned_compiler_from_source(self) -> None:
        for package_list in REPLAY_PACKAGE_LISTS:
            with self.subTest(package_list=package_list.name):
                packages = package_list.read_text(encoding="utf-8")
                self.assertIn("ca-certificates=20230311+deb12u1", packages)
                self.assertNotIn("acpica-tools=", packages)


if __name__ == "__main__":
    unittest.main()
