#!/usr/bin/env python3
"""Regression coverage for the AArch64 exception-library source adaptation."""

from pathlib import Path
import tempfile
import unittest

from radxa_source_compatibility import (
    DESCRIPTORS, DXE_EXCEPTION, LEGACY_EXCEPTION, adapt_exception_library,
    validate_exception_library,
)
from reconstruction_common import ReconstructionError, git
from validate_radxa13_source import GitTree


class ExceptionLibraryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="radxa-source-compatibility-")
        self.addCleanup(self.tmp.cleanup)
        self.repo = Path(self.tmp.name)
        git(self.repo, "init", "-b", "build")
        git(self.repo, "config", "user.name", "Compatibility Test")
        git(self.repo, "config", "user.email", "compatibility@example.invalid")

    def fixture(self, *, historical=False, aarch64=True, mirror=False, duplicate=False):
        descriptor = ("# retained\r\n[LibraryClasses]\r\n"
                      "  DefaultExceptionHandlerLib|Silicon/CIX/Default.inf\r\n"
                      f"  CpuExceptionHandlerLib|{LEGACY_EXCEPTION}\r\n"
                      "# tail\r\n").encode()
        if duplicate:
            descriptor += f"  CpuExceptionHandlerLib|{LEGACY_EXCEPTION}\r\n".encode()
        for path in DESCRIPTORS:
            dest = self.repo / path
            dest.parent.mkdir(parents=True, exist_ok=True)
            if mirror and path.startswith("custom/"):
                import os
                dest.symlink_to(os.path.relpath(self.repo / DESCRIPTORS[0], dest.parent))
            else:
                dest.write_bytes(descriptor)
        inf = self.repo / ("src/edk2/" + DXE_EXCEPTION)
        inf.parent.mkdir(parents=True, exist_ok=True)
        inf.write_bytes(b"[Sources.AARCH64]\r\nAArch64/ArmExceptionLib.c\r\n" if aarch64 else b"[Sources.X64]\n")
        if historical:
            old = self.repo / ("src/edk2/" + LEGACY_EXCEPTION)
            old.parent.mkdir(parents=True, exist_ok=True)
            old.write_bytes(b"[Sources.AARCH64]\n")
        git(self.repo, "add", ".")
        git(self.repo, "-c", "commit.gpgsign=false", "commit", "-m", "preimage")
        return git(self.repo, "rev-parse", "HEAD").stdout.strip(), descriptor

    def test_modern_source_has_only_two_byte_preserving_edits_and_child_commit(self):
        before, descriptor = self.fixture()
        with self.assertRaisesRegex(ReconstructionError, "obsolete Sky1"):
            validate_exception_library(self.repo, before)
        after = adapt_exception_library(self.repo, before)
        validate_exception_library(self.repo, after)
        self.assertEqual(git(self.repo, "rev-parse", after + "^").stdout.strip(), before)
        self.assertEqual(git(self.repo, "rev-parse", "build").stdout.strip(), before)
        self.assertEqual(git(self.repo, "diff", "--name-only", before, after).stdout.splitlines(),
                         sorted(DESCRIPTORS))
        tree = GitTree(self.repo, after)
        for path in DESCRIPTORS:
            self.assertEqual(tree.blob(path), descriptor.replace(LEGACY_EXCEPTION.encode(), DXE_EXCEPTION.encode()))
        self.assertEqual(adapt_exception_library(self.repo, after), after)
        self.assertIn("Source-Compatibility-Input: " + before,
                      git(self.repo, "show", "-s", "--format=%B", after).stdout)

    def test_mirror_symlink_is_preserved_and_only_real_source_is_changed(self):
        before, _ = self.fixture(mirror=True)
        after = adapt_exception_library(self.repo, before)
        self.assertEqual(git(self.repo, "diff", "--name-only", before, after).stdout.splitlines(), [DESCRIPTORS[0]])
        self.assertEqual(GitTree(self.repo, before).entries[DESCRIPTORS[1]],
                         GitTree(self.repo, after).entries[DESCRIPTORS[1]])

    def test_historical_library_is_left_unchanged(self):
        before, _ = self.fixture(historical=True)
        self.assertEqual(adapt_exception_library(self.repo, before), before)

    def test_replacement_without_aarch64_is_rejected(self):
        before, _ = self.fixture(aarch64=False)
        with self.assertRaisesRegex(ReconstructionError, "lacks AArch64"):
            adapt_exception_library(self.repo, before)

    def test_unexpected_duplicate_binding_is_rejected(self):
        before, _ = self.fixture(duplicate=True)
        with self.assertRaisesRegex(ReconstructionError, "unexpected obsolete"):
            adapt_exception_library(self.repo, before)


if __name__ == "__main__":
    unittest.main()
