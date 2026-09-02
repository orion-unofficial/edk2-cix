#!/usr/bin/env python3
"""Regression tests for published source-ref coherence checks."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from check_remote_source_coherence import (
    coherence_errors,
    expected_remote_refs,
    prepare_local_refs,
)
from test_support import commit_all, git, write_file


class RemoteSourceCoherenceTests(unittest.TestCase):
    def test_expected_refs_include_compatibility_tags_but_not_cache_refs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            (repo / "config").mkdir()
            records = {
                "refs": [
                    {
                        "object_id": "1" * 40,
                        "ref": "source/unofficial/1.3/current",
                    },
                    {
                        "object_id": "2" * 40,
                        "ref": "source/unofficial/edk2-stable202608",
                    },
                    {
                        "object_id": "3" * 40,
                        "ref": "source/cache/release/example",
                    },
                ]
            }
            (repo / "config" / "refs-unofficial.json").write_text(
                json.dumps(records), encoding="utf-8"
            )

            self.assertEqual(
                expected_remote_refs(repo),
                {
                    "refs/heads/source/unofficial/1.3/current": "1" * 40,
                    "refs/heads/source/unofficial/edk2-stable202608": "2" * 40,
                    "refs/tags/source/unofficial/edk2/stable-202608": "2" * 40,
                },
            )

    def test_coherence_errors_report_missing_and_mismatched_refs(self) -> None:
        expected = {
            "refs/heads/source/base/example": "1" * 40,
            "refs/heads/source/vendor/example": "2" * 40,
        }
        errors = coherence_errors(
            expected,
            {"refs/heads/source/base/example": "3" * 40},
        )

        self.assertEqual(len(errors), 2)
        self.assertIn("remote " + "3" * 40, errors[0])
        self.assertIn("missing", errors[1])

    def test_prepare_local_refs_uses_only_exact_objects_already_present(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            git(repo, "init", "-q", "-b", "build")
            git(repo, "config", "user.name", "Coherence Test")
            git(repo, "config", "user.email", "coherence-test")
            write_file(repo, "source.txt", "old\n")
            old = commit_all(repo, "old")
            ref = "refs/heads/source/unofficial/1.3/current"
            git(repo, "update-ref", ref, old)
            write_file(repo, "source.txt", "new\n")
            new = commit_all(repo, "new")
            missing = "f" * 40

            updates, unavailable = prepare_local_refs(
                repo,
                {ref: new, "refs/heads/source/vendor/missing": missing},
                write=False,
            )
            self.assertEqual(updates, [ref])
            self.assertEqual(len(unavailable), 1)
            self.assertEqual(git(repo, "rev-parse", ref).stdout.strip(), old)

            updates, unavailable = prepare_local_refs(
                repo,
                {ref: new},
                write=True,
            )
            self.assertEqual(updates, [ref])
            self.assertEqual(unavailable, [])
            self.assertEqual(git(repo, "rev-parse", ref).stdout.strip(), new)


if __name__ == "__main__":
    unittest.main()
