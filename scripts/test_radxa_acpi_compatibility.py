#!/usr/bin/env python3
"""Compile real Sky1 ACPI producers across EDK2's helper-header transition."""
from pathlib import Path
import io
import os
from platform import machine
import re
import subprocess
import tarfile
import tempfile
import unittest

from radxa_source_compatibility import (
    ACPI_HELPER_HEADER, ACPI_LIB_HEADER, ACPI_TABLE_DIRECTORY,
    acpi_helper_updates, adapt_source_libraries,
)
from reconstruction_common import ReconstructionError, git
from validate_radxa13_source import GitTree

ROOT = Path(__file__).resolve().parents[1]


class AcpiHelperTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='radxa-acpi-compatibility-')
        self.addCleanup(self.tmp.cleanup)
        self.repo = Path(self.tmp.name)
        git(self.repo, 'init', '-b', 'build')
        git(self.repo, 'config', 'user.name', 'Compatibility Test')
        git(self.repo, 'config', 'user.email', 'compatibility@example.invalid')

    def fixture(self, old=False, mirror=False, helper=True, macro=True, modern=False):
        files = {ACPI_LIB_HEADER: b'#define ARM_GAS32(x) {0,32,0,3,x}\r\n' if old else b'// moved\r\n'}
        if helper:
            files[ACPI_HELPER_HEADER] = b'#define ACPI_GAS32(x) {0,32,0,3,x}\r\n' if macro else b'// absent\r\n'
        path = ACPI_TABLE_DIRECTORY + 'Dbg2.aslc'
        data = b'#include <Library/AcpiLib.h>\r\nGAS gas = ARM_GAS32(0x123456789);\r\n'
        if modern:
            data = b'#include <AcpiHelperMacros.h>\r\n' + data.replace(b'ARM_GAS32', b'ACPI_GAS32')
        files[path.replace('custom/overlay/', 'src/')] = data
        if not mirror:
            files[path] = data
        for name, content in files.items():
            target = self.repo / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
        if mirror:
            target = self.repo / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.symlink_to(os.path.relpath(self.repo / path.replace('custom/overlay/', 'src/'), target.parent))
        git(self.repo, 'add', '.')
        git(self.repo, '-c', 'commit.gpgsign=false', 'commit', '-m', 'preimage')
        return git(self.repo, 'rev-parse', 'HEAD').stdout.strip(), path, data

    def test_old_header_and_modern_tables_are_exact_noops(self):
        for settings in ({'old': True}, {'modern': True}):
            with self.subTest(settings=settings):
                before, _, _ = self.fixture(**settings)
                self.assertEqual(adapt_source_libraries(self.repo, before), before)

    def test_mirror_materialises_only_custom_bytes_and_is_idempotent(self):
        before, path, data = self.fixture(mirror=True)
        after = adapt_source_libraries(self.repo, before)
        self.assertEqual(git(self.repo, 'diff', '--name-only', before, after).stdout.splitlines(), [path])
        old, new = GitTree(self.repo, before), GitTree(self.repo, after)
        self.assertEqual(new.entries[path].mode, '100644')
        imported = path.replace('custom/overlay/', 'src/')
        self.assertEqual(old.entries[imported], new.entries[imported])
        self.assertEqual(new.blob(path), b'#include <AcpiHelperMacros.h>\r\n' + data.replace(b'ARM_GAS32', b'ACPI_GAS32'))
        self.assertEqual(adapt_source_libraries(self.repo, after), after)
        self.assertEqual(git(self.repo, 'rev-parse', 'build').stdout.strip(), before)

    def test_missing_helper_or_definition_is_rejected(self):
        for settings, message in (({'helper': False}, 'missing selected ACPI'),
                                  ({'macro': False}, 'lacks ACPI_GAS32')):
            with self.subTest(settings=settings):
                before, _, _ = self.fixture(**settings)
                with self.assertRaisesRegex(ReconstructionError, message):
                    adapt_source_libraries(self.repo, before)

    def test_macro_names_require_token_boundaries(self):
        before, path, data = self.fixture()
        target = self.repo / path
        target.write_bytes(data.replace(b'ARM_GAS32(', b'SOME_ARM_GAS32('))
        git(self.repo, 'add', '.')
        git(self.repo, '-c', 'commit.gpgsign=false', 'commit', '-m', 'unrelated name')
        before = git(self.repo, 'rev-parse', 'HEAD').stdout.strip()
        self.assertEqual(acpi_helper_updates(GitTree(self.repo, before)), {})


class RealAcpiProducerTests(unittest.TestCase):
    def test_all_retained_checkpoints_are_unchanged(self):
        refs = git(ROOT, 'for-each-ref', '--format=%(refname)', 'refs/heads/source/unofficial/').stdout.splitlines()
        self.assertTrue(refs, 'retained source refs are required for qualification')
        for ref in refs:
            with self.subTest(ref=ref):
                self.assertEqual(acpi_helper_updates(GitTree(ROOT, ref)), {})

    def test_selected_header_transition_matches_all_retained_edk2_versions(self):
        refs = git(ROOT, 'for-each-ref', '--format=%(refname)', 'refs/heads/source/base/edk2/').stdout.splitlines()
        self.assertTrue(refs, 'retained source refs are required for qualification')
        for ref in refs:
            with self.subTest(ref=ref):
                header = git(ROOT, 'show', ref + ':EmbeddedPkg/Include/Library/AcpiLib.h').stdout
                old = bool(re.search(r'^#define ARM_GAS32\(', header, re.M))
                self.assertEqual(old, not ref.endswith('202608'))
                if not old:
                    header = git(ROOT, 'show', ref + ':MdeModulePkg/Include/AcpiHelperMacros.h').stdout
                    self.assertRegex(header, r'#define ACPI_GAS32\(')

    def test_real_tables_compile_and_preserve_emitted_bytes(self):
        source_repo = Path(os.environ.get('SOURCE_ACPI_TEST_REPO', ROOT))
        candidate = os.environ.get('SOURCE_ACPI_TEST_REF', 'source/unofficial/1.2.1/edk2-stable202605')
        source = GitTree(source_repo, candidate)
        # Use the actual imported EDK2 headers/types, not mocked GAS/DBG2 structs.
        with tempfile.TemporaryDirectory(prefix='radxa-acpi-compile-') as temporary:
            root = Path(temporary)
            headers = {}
            for version in ('202208', '202605', '202608'):
                destination = root / version
                destination.mkdir()
                ref = 'source/base/edk2/edk2-stable' + version
                archive = subprocess.check_output([
                    'git', '-C', str(ROOT), 'archive', ref,
                    'MdePkg/Include', 'EmbeddedPkg/Include', 'MdeModulePkg/Include',
                ])
                with tarfile.open(fileobj=io.BytesIO(archive)) as contents:
                    for member in contents:
                        if member.isfile():
                            target = destination / member.name
                            target.parent.mkdir(parents=True, exist_ok=True)
                            target.write_bytes(contents.extractfile(member).read())
                headers[version] = destination
            platform = root / 'platform'
            for name, prefix in (('AcpiPlatform.h', 'Platform'), ('AcpiCommon.h', 'Silicon'), ('MemoryMap.h', 'Silicon')):
                path = f'src/edk2-platforms/{prefix}/CIX/Sky1/Include/{name}'
                target = platform / 'Include' / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(source.blob(source.resolve(path)))
            # AutoGen provides these fixed PCD values in a normal EDK2 build.
            autogen = root / 'AutoGen.h'
            autogen.write_text('''#include <Uefi.h>
#define FixedPcdGet64(x) x
#define FixedPcdGet32(x) x
#define FixedPcdGet8(x) x
#define FixedPcdGetBool(x) x
#define PcdSerialRegisterBase 0x123456789ULL
#define PcdSerialDbgRegisterBase 0x23456789aULL
#define PcdSerialDbgInterrupt 88
#define PcdSerialDbgUartBaudRate 115200
#define PcdAcpiPrefPmProf 8
#define PcdAcpiUart3Enable UART3
''')

            def compile_table(data, table, version, uart, expect=True):
                unit = root / 'table.c'
                # Firmware headers may leave hidden visibility active. Keep the
                # host libc declarations public without changing table types.
                unit.write_bytes(data + b'\n#pragma GCC visibility push(default)\n#include <stdio.h>\n#pragma GCC visibility pop\nint main(void) { return fwrite(ReferenceAcpiTable, sizeof(' + table.encode() + b'), 1, stdout) != 1; }\n')
                include = headers[version]
                arch = 'AArch64' if machine().lower() in ('arm64', 'aarch64') else 'X64'
                command = [
                    'cc', '-std=c11', '-Wall', '-Werror', '-fshort-wchar', '-DUART3=' + str(uart),
                    '-include', str(autogen), '-I', str(platform), '-I', str(include / 'MdePkg/Include'),
                    '-I', str(include / 'MdePkg/Include' / arch), '-I', str(include / 'EmbeddedPkg/Include'),
                    '-I', str(include / 'MdeModulePkg/Include'), str(unit), '-o', str(root / 'table'),
                ]
                result = subprocess.run(command, text=True, capture_output=True)
                if not expect:
                    self.assertNotEqual(result.returncode, 0)
                    self.assertRegex(result.stderr, 'ARM_GAS32|NULL_GAS')
                    return None
                self.assertEqual(result.returncode, 0, result.stderr)
                return subprocess.check_output([str(root / 'table')])
            # Gate adaptation on the actual selected modern header definitions.
            modern_header = git(ROOT, 'show', 'source/base/edk2/edk2-stable202608:EmbeddedPkg/Include/Library/AcpiLib.h').stdout.encode()
            helper_header = git(ROOT, 'show', 'source/base/edk2/edk2-stable202608:MdeModulePkg/Include/AcpiHelperMacros.h').stdout.encode()

            class SelectedHeaders:
                entries = dict(source.entries, **{ACPI_HELPER_HEADER: None})
                resolve = source.resolve

                def blob(self, path):
                    return {ACPI_LIB_HEADER: modern_header, ACPI_HELPER_HEADER: helper_header}.get(path) or source.blob(path)
            selected = SelectedHeaders()
            selected.resolve = lambda path: path if path == ACPI_HELPER_HEADER else source.resolve(path)
            updates = acpi_helper_updates(selected)
            self.assertEqual(set(updates), {ACPI_TABLE_DIRECTORY + n + '.aslc' for n in ('Fadt', 'Dbg2', 'Spcr')})
            for table in ('Fadt', 'Dbg2', 'Spcr'):
                path = ACPI_TABLE_DIRECTORY + table + '.aslc'
                before = source.blob(source.resolve(path))
                after = updates[path]
                for uart in (0, 1):
                    with self.subTest(table=table, uart3=uart):
                        old = compile_table(before, table, '202208', uart)
                        self.assertEqual(compile_table(before, table, '202605', uart), old)
                        compile_table(before, table, '202608', uart, expect=False)
                        self.assertEqual(compile_table(after, table, '202608', uart), old)


if __name__ == '__main__':
    unittest.main()
