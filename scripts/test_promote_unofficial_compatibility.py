#!/usr/bin/env python3
"""Regression tests for retained EDK2 compatibility refs."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

from reconstruction_common import ReconstructionError
from promote_unofficial_compatibility import (
    record_compatibility_ref,
    repair_or_report_existing,
)
from test_support import load_json


def git(repo: Path, *args: str) -> str:
    return subprocess.run(
        ["git", *args],
        cwd=repo,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    ).stdout.strip()


def fixture_repo(directory: str) -> tuple[Path, str, str]:
    repo = Path(directory)
    git(repo, "init", "-q")
    git(repo, "config", "user.name", "Compatibility Test")
    git(repo, "config", "user.email", "compatibility-test")
    (repo / "firmware.txt").write_text("202608\n", encoding="utf-8")
    git(repo, "add", "firmware.txt")
    git(repo, "commit", "-q", "-m", "202608 compatibility")
    target = "source/unofficial/edk2-stable202608"
    git(repo, "branch", target)
    oid = git(repo, "rev-parse", "HEAD")
    return repo, target, oid


class PromoteUnofficialCompatibilityTests(unittest.TestCase):
    def test_compatibility_ref_is_recorded_as_mutable_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, target, oid = fixture_repo(directory)

            record_compatibility_ref(repo, target, "edk2-stable202608")

            record = load_json(repo / "config/refs-unofficial.json")["refs"][0]
            self.assertEqual(record["ref"], target)
            self.assertEqual(record["object_id"], oid)
            self.assertEqual(record["edk2_base"], "edk2-stable202608")
            self.assertFalse(record["immutable"])
            self.assertEqual(record["type"], "unofficial-edk2-compatibility")

    def test_existing_branch_repairs_only_its_missing_tag(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, target, oid = fixture_repo(directory)
            tag = "source/unofficial/edk2/stable-202608"

            handled = repair_or_report_existing(
                repo,
                target_ref=target,
                tag=tag,
                write=True,
                verbose=False,
            )

            self.assertTrue(handled)
            self.assertEqual(git(repo, "rev-parse", tag), oid)
            self.assertEqual(git(repo, "rev-parse", target), oid)

    def test_existing_branch_rejects_a_mismatched_tag(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, target, _oid = fixture_repo(directory)
            tag = "source/unofficial/edk2/stable-202608"
            (repo / "firmware.txt").write_text("different\n", encoding="utf-8")
            git(repo, "commit", "-q", "-a", "-m", "different")
            git(repo, "tag", tag)

            with self.assertRaisesRegex(ReconstructionError, "review the mismatch"):
                repair_or_report_existing(
                    repo,
                    target_ref=target,
                    tag=tag,
                    write=True,
                    verbose=False,
                )


if __name__ == "__main__":
    unittest.main()
