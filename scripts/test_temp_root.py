#!/usr/bin/env python3
"""Regression tests for repository-local temporary workspace placement."""

from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from reconstruction_common import temp_root


class TempRootTests(unittest.TestCase):
    def test_default_uses_repository_root_worktrees_directory(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            subprocess.run(
                ["git", "init", "-q"],
                cwd=repo,
                check=True,
            )
            with mock.patch.dict(os.environ, {"EDK2_CIX_TMP_ROOT": ""}):
                selected = temp_root(repo)

            self.assertEqual(
                selected,
                repo / ".worktrees" / "edk2-cix-tmp",
            )
            self.assertTrue(selected.is_dir())


if __name__ == "__main__":
    unittest.main()
