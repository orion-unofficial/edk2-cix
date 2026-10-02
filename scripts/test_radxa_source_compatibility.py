#!/usr/bin/env python3
"""Regression coverage for the AArch64 exception-library source adaptation."""

from pathlib import Path
import tempfile
import unittest

from radxa_source_compatibility import (
    DEPENDENCY_BINDINGS, DESCRIPTORS, DXE_EXCEPTION, LEGACY_EXCEPTION, adapt_source_libraries,
    inf_library_classes, validate_source_libraries,
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

    def fixture(self, *, historical=False, aarch64=True, mirror=False, duplicate=False,
                modern=False, dependencies=True, selected=True, missing_provider=False):
        descriptor = ("# retained\r\n[LibraryClasses]\r\n"
                      "  DefaultExceptionHandlerLib|Silicon/CIX/Default.inf\r\n"
                      f"  CpuExceptionHandlerLib|{LEGACY_EXCEPTION}\r\n"
                      "# tail\r\n").encode()
        if duplicate:
            descriptor += f"  CpuExceptionHandlerLib|{LEGACY_EXCEPTION}\r\n".encode()
        if modern:
            descriptor = descriptor.replace(b"[LibraryClasses]", b"[LibraryClasses.common]")
            descriptor += (
                "[LibraryClasses.common]\r\n"
                "  ArmSmcLib|MdePkg/Library/ArmSmcLib.inf\r\n"
                "  NetLib|NetworkPkg/Library/NetLib.inf\r\n"
                "[LibraryClasses.common.DXE_RUNTIME_DRIVER]\r\n"
                "  ArmSmcLib|MdePkg/Library/ArmSmcLib.inf\r\n"
                "[Components]\r\n"
            ).encode()
            for name, consumer, provider, _ in DEPENDENCY_BINDINGS:
                descriptor += (("  " if selected else "# ") + consumer + "\r\n").encode()
                module = self.repo / ("src/edk2/" + consumer)
                module.parent.mkdir(parents=True, exist_ok=True)
                module.write_bytes(("[LibraryClasses]\r\n" + (name if dependencies else "BaseLib") + "\r\n").encode())
                if not missing_provider:
                    library = self.repo / ("src/edk2/" + provider)
                    library.parent.mkdir(parents=True, exist_ok=True)
                    library.write_bytes(("[Defines]\r\n  LIBRARY_CLASS = " + name + "\r\n").encode())
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
            validate_source_libraries(self.repo, before)
        after = adapt_source_libraries(self.repo, before)
        validate_source_libraries(self.repo, after)
        self.assertEqual(git(self.repo, "rev-parse", after + "^").stdout.strip(), before)
        self.assertEqual(git(self.repo, "rev-parse", "build").stdout.strip(), before)
        self.assertEqual(git(self.repo, "diff", "--name-only", before, after).stdout.splitlines(),
                         sorted(DESCRIPTORS))
        tree = GitTree(self.repo, after)
        for path in DESCRIPTORS:
            self.assertEqual(tree.blob(path), descriptor.replace(LEGACY_EXCEPTION.encode(), DXE_EXCEPTION.encode()))
        self.assertEqual(adapt_source_libraries(self.repo, after), after)
        self.assertIn("Source-Compatibility-Input: " + before,
                      git(self.repo, "show", "-s", "--format=%B", after).stdout)

    def test_mirror_symlink_is_preserved_and_only_real_source_is_changed(self):
        before, _ = self.fixture(mirror=True)
        after = adapt_source_libraries(self.repo, before)
        self.assertEqual(git(self.repo, "diff", "--name-only", before, after).stdout.splitlines(), [DESCRIPTORS[0]])
        self.assertEqual(GitTree(self.repo, before).entries[DESCRIPTORS[1]],
                         GitTree(self.repo, after).entries[DESCRIPTORS[1]])

    def test_historical_library_is_left_unchanged(self):
        before, _ = self.fixture(historical=True)
        self.assertEqual(adapt_source_libraries(self.repo, before), before)

    def test_replacement_without_aarch64_is_rejected(self):
        before, _ = self.fixture(aarch64=False)
        with self.assertRaisesRegex(ReconstructionError, "lacks AArch64"):
            adapt_source_libraries(self.repo, before)

    def test_unexpected_duplicate_binding_is_rejected(self):
        before, _ = self.fixture(duplicate=True)
        with self.assertRaisesRegex(ReconstructionError, "unexpected obsolete"):
            adapt_source_libraries(self.repo, before)

    def test_modern_dependencies_are_corrected_together_in_matching_scopes(self):
        before, descriptor = self.fixture(modern=True)
        after = adapt_source_libraries(self.repo, before)
        validate_source_libraries(self.repo, after)
        expected = descriptor.replace(LEGACY_EXCEPTION.encode(), DXE_EXCEPTION.encode())
        for name, _, provider, anchor in DEPENDENCY_BINDINGS:
            prefix = ("  " + anchor + "|").encode()
            lines = expected.splitlines(keepends=True)
            addition = ("  " + name + "|" + provider + "\r\n").encode()
            expected = b"".join(line + (addition if line.startswith(prefix) else b"")
                                for line in lines)
        tree = GitTree(self.repo, after)
        for path in DESCRIPTORS:
            self.assertEqual(tree.blob(path), expected)
        self.assertEqual(adapt_source_libraries(self.repo, after), after)
        self.assertEqual(git(self.repo, "diff", "--name-only", before, after).stdout.splitlines(), sorted(DESCRIPTORS))

    def test_old_module_dependencies_do_not_add_modern_bindings(self):
        before, _ = self.fixture(modern=True, dependencies=False, historical=True)
        self.assertEqual(adapt_source_libraries(self.repo, before), before)

    def test_unselected_modern_modules_do_not_add_bindings(self):
        before, _ = self.fixture(modern=True, selected=False, historical=True)
        self.assertEqual(adapt_source_libraries(self.repo, before), before)

    def test_missing_required_provider_is_rejected(self):
        before, _ = self.fixture(modern=True, missing_provider=True)
        with self.assertRaisesRegex(ReconstructionError, "missing selected dependency provider"):
            adapt_source_libraries(self.repo, before)

    def test_inf_dependencies_exclude_other_architectures(self):
        self.assertEqual(inf_library_classes(
            b"[LibraryClasses]\r\nGptLib # comment\r\n"
            b"[LibraryClasses.AARCH64]\r\nArmSmcccSocIdLib\r\n"
            b"[LibraryClasses.X64]\r\nOtherLib\r\n"
            b"[Sources]\r\nUnrelatedName\r\n"), {"GptLib", "ArmSmcccSocIdLib"})

    def test_provider_class_must_match_selected_requirement(self):
        before, _ = self.fixture(modern=True)
        provider = self.repo / ("src/edk2/" + DEPENDENCY_BINDINGS[0][2])
        provider.write_bytes(b"[Defines]\n  LIBRARY_CLASS = WrongLib\n")
        git(self.repo, "add", ".")
        git(self.repo, "-c", "commit.gpgsign=false", "commit", "-m", "bad provider")
        before = git(self.repo, "rev-parse", "HEAD").stdout.strip()
        with self.assertRaisesRegex(ReconstructionError, "does not declare GptLib"):
            adapt_source_libraries(self.repo, before)

    def test_modern_mirror_and_crlf_bytes_are_preserved(self):
        before, _ = self.fixture(modern=True, mirror=True)
        after = adapt_source_libraries(self.repo, before)
        self.assertEqual(git(self.repo, "diff", "--name-only", before, after).stdout.splitlines(), [DESCRIPTORS[0]])
        old, new = GitTree(self.repo, before), GitTree(self.repo, after)
        self.assertEqual(old.entries[DESCRIPTORS[1]], new.entries[DESCRIPTORS[1]])
        self.assertNotIn(b"\n", new.blob(DESCRIPTORS[0]).replace(b"\r\n", b""))


if __name__ == "__main__":
    unittest.main()
