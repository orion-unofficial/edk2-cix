#!/usr/bin/env python3

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

import buildbox_fiptool_state


class BuildboxFiptoolStateTests(unittest.TestCase):
    def test_no_cleanup_when_build_dir_missing(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir_text:
            tmpdir = Path(tmpdir_text)
            build_dir = tmpdir / "build"
            stamp_path = tmpdir / "stamp"
            self.assertIsNone(
                buildbox_fiptool_state.determine_cleanup_reason(
                    build_dir=build_dir,
                    stamp_path=stamp_path,
                    requested_distro="bookworm",
                )
            )

    def test_cleanup_when_stamp_mismatches_requested_distro(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir_text:
            tmpdir = Path(tmpdir_text)
            build_dir = tmpdir / "build"
            build_dir.mkdir()
            stamp_path = tmpdir / "stamp"
            stamp_path.write_text("trixie\n", encoding="utf-8")
            self.assertEqual(
                buildbox_fiptool_state.determine_cleanup_reason(
                    build_dir=build_dir,
                    stamp_path=stamp_path,
                    requested_distro="bookworm",
                ),
                "built for trixie",
            )

    def test_cleanup_when_binary_exists_without_stamp(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir_text:
            tmpdir = Path(tmpdir_text)
            build_dir = tmpdir / "build" / "aarch64"
            build_dir.mkdir(parents=True)
            (build_dir / "fiptool").write_text("", encoding="utf-8")
            self.assertEqual(
                buildbox_fiptool_state.determine_cleanup_reason(
                    build_dir=tmpdir / "build",
                    stamp_path=tmpdir / "stamp",
                    requested_distro="bookworm",
                ),
                "existing build predates distro stamp",
            )

    def test_prepare_build_dir_removes_stale_tree(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir_text:
            tmpdir = Path(tmpdir_text)
            build_dir = tmpdir / "build"
            build_dir.mkdir()
            (build_dir / "aarch64").mkdir()
            stamp_path = tmpdir / "stamp"
            stamp_path.write_text("trixie\n", encoding="utf-8")
            reason = buildbox_fiptool_state.prepare_build_dir(
                build_dir=build_dir,
                stamp_path=stamp_path,
                requested_distro="bookworm",
            )
            self.assertEqual(reason, "built for trixie")
            self.assertFalse(build_dir.exists())


if __name__ == "__main__":
    unittest.main()
