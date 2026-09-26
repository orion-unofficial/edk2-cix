#!/usr/bin/env python3
"""Regression checks for the reviewed O6 CPU reference conflict."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from dsdt_cpu_conflict import (
    ACCEPTED_GIT_BLOB, ACCEPTED_SHA256, DSDT_CPU_PATH, DsdtConflictError,
    resolve_dsdt_cpu_conflict,
)
from reconstruction_common import ReconstructionError
from release_expansion_source import resolve_dsdt_worktree, validated_dsdt_resolution


FIXTURE = Path(__file__).with_name("testdata") / "dsdt_cpu_202211_1_2_2_conflict.asl"


class DsdtCpuConflictTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.raw = FIXTURE.read_bytes()
        # This is the archived 202211/1.2.2 conflict file, not a fixture
        # synthesized from the resolver's own constants.
        assert hashlib.sha256(cls.raw).hexdigest() == (
            "83dda46ef0ceb4f0b1d682fe80a89b6b70f97f5b734ff5292fa3e02ec6b68c1f"
        )

    def test_real_conflict_resolves_to_reviewed_whole_file(self) -> None:
        resolved = resolve_dsdt_cpu_conflict(self.raw)
        self.assertEqual(hashlib.sha256(resolved).hexdigest(), ACCEPTED_SHA256)
        blob = hashlib.sha1(b"blob " + str(len(resolved)).encode() + b"\0" + resolved)
        self.assertEqual(blob.hexdigest(), ACCEPTED_GIT_BLOB)
        self.assertEqual(resolved.count(b"CPPC_PACKAGE_INIT"), 12)
        for first in range(0, 12, 2):
            self.assertEqual(resolved.count(
                f"CIX_CPU_{first}_{first + 1}_REF_PERF)".encode()), 2)

    def assert_rejected(self, raw: bytes, reason: str) -> None:
        with self.assertRaisesRegex(DsdtConflictError, reason):
            resolve_dsdt_cpu_conflict(raw)

    def test_missing_or_duplicate_cpu_group_is_rejected(self) -> None:
        start = self.raw.index(b"<<<<<<< ")
        end = self.raw.index(b">>>>>>> ", start)
        end = self.raw.index(b"\n", end) + 1
        group = self.raw[start:end]
        self.assert_rejected(self.raw[:start] + self.raw[end:], "exactly one")
        self.assert_rejected(self.raw[:end] + group + self.raw[end:], "duplicate")

    def test_marker_structure_is_rejected(self) -> None:
        self.assert_rejected(self.raw.replace(b"=======\n", b"======\n", 1), "conflict close")
        self.assert_rejected(self.raw.replace(b"=======\n", b"<<<<<<< nested\n=======\n", 1), "nested")
        self.assert_rejected(self.raw.replace(b">>>>>>> ", b"xxxxxxx ", 1), "nested conflict")

    def test_topology_and_vendor_references_are_rejected(self) -> None:
        self.assert_rejected(self.raw.replace(
            b"CIX_A520_REF_PERF)", b"CIX_A720_REF_PERF)", 1),
                             "reference mapping")
        self.assert_rejected(self.raw.replace(b"CORE_4_5_REF_PERF)", b"CORE_0_TO_3_REF_PERF)", 1),
                             "reference mapping")

    def test_macro_and_unrelated_file_mutations_are_rejected(self) -> None:
        self.assert_rejected(self.raw.replace(
            b"#define CIX_A720_REF_PERF  0x0C4E",
            b"#define CIX_A720_REF_PERF  0x0C4F", 1), "macro block")
        self.assert_rejected(self.raw.replace(b"Copyright 2024", b"Copyright 2025", 1),
                             "whole-file identity")
        self.assert_rejected(self.raw.replace(b"\n", b"\r\n", 1), "line ending")

    def test_private_resolution_binds_inputs_and_resumes_idempotently(self) -> None:
        def git(repo: Path, *args: str) -> str:
            return subprocess.run(["git", "-C", str(repo), *args], check=True,
                                  stdout=subprocess.PIPE, text=True).stdout.strip()

        with tempfile.TemporaryDirectory(prefix="dsdt-cpu-conflict-") as directory:
            state = Path(directory)
            repo = state / "repo"
            git(state, "init", "-q", str(repo))
            git(repo, "config", "user.name", "Test")
            git(repo, "config", "user.email", "test@example.invalid")
            source_ref = "source/unofficial/1.2.1/edk2-stable202211"
            base_ref = "source/port/radxa/1.2.2/edk2-stable202211"
            target = repo / DSDT_CPU_PATH
            target.parent.mkdir(parents=True)
            target.write_bytes(self.raw)
            git(repo, "add", DSDT_CPU_PATH)
            git(repo, "commit", "-q", "-m", "source-port: conflict tree for expansion-202211-1.2.2\n\n"
                f"Source-Port-Input: {source_ref}\nSource-Port-New-Base: {base_ref}\n"
                "Source-Port-Conflict-Stage: overlay\n")
            conflict = git(repo, "rev-parse", "HEAD")
            scratch = state / "tmp" / "port-expansion-202211-1.2.2-conflict-test"
            scratch.mkdir(parents=True)
            worktree = scratch / "worktree"
            git(repo, "worktree", "add", "--detach", str(worktree), conflict)
            (scratch / "README.md").write_text(
                "# Source Port Conflict: expansion-202211-1.2.2\n"
                "Conflict stage: overlay\nConflicted paths:\n"
                f"  - {DSDT_CPU_PATH}\n")
            error = ReconstructionError(f"source-port conflict worktree preserved at: {worktree}\n")
            resolved = resolve_dsdt_worktree(repo, state, "202211", "1.2.2",
                                             source_ref, base_ref, error)
            journal = state / "jobs" / "202211-1.2.2" / "dsdt-resolution.json"
            self.assertEqual(validated_dsdt_resolution(
                repo, journal, "202211", "1.2.2", source_ref, base_ref), resolved)
            ref = json.loads(journal.read_text())["resolution_ref"]
            git(repo, "update-ref", "-d", ref)
            self.assertEqual(validated_dsdt_resolution(
                repo, journal, "202211", "1.2.2", source_ref, base_ref), resolved)
            self.assertEqual(git(repo, "rev-parse", ref), resolved)
            with self.assertRaisesRegex(ReconstructionError, "metadata differs"):
                validated_dsdt_resolution(repo, journal, "202211", "1.2.2",
                                          source_ref, base_ref + "-wrong")
            row = json.loads(journal.read_text())
            row["resolution_commit"] = conflict
            journal.write_text(json.dumps(row))
            with self.assertRaisesRegex(ReconstructionError, "unexpected parent"):
                validated_dsdt_resolution(repo, journal, "202211", "1.2.2",
                                          source_ref, base_ref)


if __name__ == "__main__":
    unittest.main()
