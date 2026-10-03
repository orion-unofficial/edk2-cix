#!/usr/bin/env python3
"""Qualify the selected CIX AML instance against real old and public APIs."""
from pathlib import Path
import io
import os
import platform
from collections import Counter
import re
import shlex
import subprocess
import tarfile
import tempfile
import unittest
import time

from radxa_source_compatibility import (
    AML_MODULE, AML_OVERLAY, AML_CODEGEN, AML_PUBLIC_HEADER, DESCRIPTORS,
    aml_library_updates, adapt_source_libraries, compatibility_update_mode,
)
from reconstruction_common import ReconstructionError, for_each_ref, git, rev_parse, show_file
from source_lifecycle import lifecycle_errors, mirror_symlink_target, project_overlay_tree
from source_policy import enforce_source_tree_policy
from validate_radxa13_source import GitTree

ROOT = Path(__file__).resolve().parents[1]
OLD = 'source/unofficial/1.2.1/edk2-stable202605'
MODERN = 'source/base/edk2/edk2-stable202608'


def archive_files(ref, paths, destination):
    archive = subprocess.check_output(['git', '-C', str(ROOT), 'archive', rev_parse(ROOT, ref), *paths])
    with tarfile.open(fileobj=io.BytesIO(archive)) as contents:
        for member in contents:
            if member.isfile():
                target = destination / member.name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(contents.extractfile(member).read())


def selected_tree(source):
    """Substitute the exact selected upstream header in an otherwise real tree."""
    header = show_file(ROOT, MODERN, 'DynamicTablesPkg/Include/Library/AmlLib/AmlLib.h')

    class Selected:
        entries = source.entries
        resolve = source.resolve

        def blob(self, path):
            return header if path == AML_PUBLIC_HEADER else source.blob(path)
    return Selected()


class AmlAdaptationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='radxa-aml-compatibility-')
        self.addCleanup(self.tmp.cleanup)
        self.repo = Path(self.tmp.name)
        git(self.repo, 'init', '-b', 'build')
        git(self.repo, 'config', 'user.name', 'Compatibility Test')
        git(self.repo, 'config', 'user.email', 'compatibility@example.invalid')
        self.source = selected_tree(GitTree(ROOT, rev_parse(ROOT, OLD)))

    def fixture(self, mutate=None, old_header=False, binding=True):
        imported = 'src/' + AML_MODULE
        files = {path: self.source.blob(self.source.resolve(path))
                 for path in self.source.entries if path.startswith(imported)}
        files['src/Makefile'] = b'CUSTOM_EDK2_OVERLAY_SOURCES := \\\n\tunchanged\n'
        files[AML_PUBLIC_HEADER] = (b'// private API\r\n' if old_header
                                    else self.source.blob(AML_PUBLIC_HEADER))
        files[DESCRIPTORS[1]] = (b'[LibraryClasses]\r\n  AmlLib|Platform/CIX/Sky1/Library/Acpi/CIX/AmlLib/AmlLib.inf\r\n'
                                 if binding else b'[LibraryClasses]\r\n')
        if mutate:
            mutate(files)
        for name, data in files.items():
            target = self.repo / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        git(self.repo, 'add', '.')
        git(self.repo, '-c', 'commit.gpgsign=false', 'commit', '-m', 'real CIX preimage')
        return git(self.repo, 'rev-parse', 'HEAD').stdout.strip()

    def test_complete_module_preserves_imported_bytes_and_refs(self):
        before = self.fixture()
        after = adapt_source_libraries(self.repo, before)
        self.assertEqual(git(self.repo, 'rev-parse', after + '^').stdout.strip(), before)
        self.assertEqual(git(self.repo, 'rev-parse', 'build').stdout.strip(), before)
        self.assertEqual(git(self.repo, 'diff', '--name-only', before, after, '--', 'src').stdout, 'src/Makefile\n')
        self.assertEqual(git(self.repo, 'diff', '--name-only', before, after, '--', 'src/edk2-platforms').stdout, '')
        tree = GitTree(self.repo, after)
        message = git(self.repo, 'show', '-s', '--format=%B', after).stdout
        self.assertIn('Source-Compatibility-AML-Header: ' + AML_PUBLIC_HEADER + '=' + tree.entries[AML_PUBLIC_HEADER].oid, message)
        self.assertIn('Source-Compatibility-AML-Vendor: src/' + AML_MODULE + 'CodeGen/AmlCodeGen.c=', message)
        imported = 'src/' + AML_MODULE
        paths = [path for path in tree.entries if path.startswith(AML_OVERLAY)]
        self.assertEqual(len(paths), len([path for path in tree.entries if path.startswith(imported)]))
        old = GitTree(self.repo, before).blob(imported + 'CodeGen/AmlCodeGen.c')
        expected = old.replace(b'STATIC\r\nEFI_STATUS\r\nEFIAPI\r\nAmlCodeGenMethod',
                               b'EFI_STATUS\r\nEFIAPI\r\nAmlCodeGenMethod')
        self.assertEqual(tree.blob(AML_CODEGEN), expected)
        for path in paths:
            if path == AML_CODEGEN:
                self.assertEqual(tree.entries[path].mode, '100644')
            else:
                self.assertEqual(tree.entries[path].mode, '120000')
                self.assertEqual(tree.blob(path).decode(), mirror_symlink_target(path))
                self.assertEqual(tree.blob(tree.resolve(path)), tree.blob(path.replace('custom/overlay/', 'src/')))
        enforce_source_tree_policy(self.repo, ref=after)
        projections = project_overlay_tree(self.repo, after, before, paths)
        self.assertEqual(lifecycle_errors(projections), [])
        self.assertTrue(all(item.action == 'keep' for item in projections))
        self.assertEqual(adapt_source_libraries(self.repo, after), after)

    def test_old_headers_and_unselected_provider_are_noops(self):
        for settings in ({'old_header': True}, {'binding': False}):
            with self.subTest(settings=settings):
                before = self.fixture(**settings)
                self.assertEqual(adapt_source_libraries(self.repo, before), before)

    def test_unknown_method_signature_body_and_header_are_rejected(self):
        code = 'src/' + AML_MODULE + 'CodeGen/AmlCodeGen.c'
        mutations = (
            (code, b'UINT8            NumArgs,', b'UINT16           NumArgs,'),
            (code, b'(NumArgs > 6)', b'(NumArgs > 7)'),
            (code, b'STATIC\r\nEFI_STATUS\r\nEFIAPI\r\nAmlCodeGenMethod', b'STATIC EFI_STATUS\r\nEFIAPI\r\nAmlCodeGenMethod'),
            (code, b'STATIC\r\nEFI_STATUS\r\nEFIAPI\r\nAmlCodeGenMethod', b'STATIC /* private */\r\nEFI_STATUS\r\nEFIAPI\r\nAmlCodeGenMethod'),
            (AML_PUBLIC_HEADER, b'UINT8                   NumArgs', b'UINT16                  NumArgs'),
        )
        for path, old, new in mutations:
            with self.subTest(path=path, old=old):
                def change(files):
                    self.assertTrue(old in files[path], 'expected mutation anchor missing')
                    files[path] = files[path].replace(old, new)
                before = self.fixture(mutate=change)
                with self.assertRaisesRegex(ReconstructionError, 'unreviewed'):
                    adapt_source_libraries(self.repo, before)


class RealAmlCompileTests(unittest.TestCase):
    def test_retained_sources_and_header_selection(self):
        refs = for_each_ref(ROOT, 'source/unofficial/')
        self.assertTrue(refs)
        for ref in refs:
            with self.subTest(ref=ref):
                self.assertEqual(aml_library_updates(GitTree(ROOT, rev_parse(ROOT, ref))), {})
        bases = for_each_ref(ROOT, 'source/base/edk2/')
        self.assertTrue(bases)
        for ref in bases:
            header = show_file(ROOT, ref, 'DynamicTablesPkg/Include/Library/AmlLib/AmlLib.h', check=False)
            public = bool(re.search(rb'^AmlCodeGenMethod\s*\(', header, re.M))
            self.assertEqual(public, ref.endswith('202608'), ref)

    def test_affected_releases_share_the_exact_reviewed_module(self):
        source = GitTree(ROOT, rev_parse(ROOT, OLD))
        imported = 'src/' + AML_MODULE
        entries = {path: entry for path, entry in source.entries.items() if path.startswith(imported)}
        for release in ('1.2.2', '1.2.3'):
            other = GitTree(ROOT, rev_parse(ROOT, 'source/unofficial/' + release + '/edk2-stable202605'))
            self.assertEqual({path: entry for path, entry in other.entries.items() if path.startswith(imported)}, entries)
            updates = aml_library_updates(selected_tree(other))
            self.assertIn(AML_CODEGEN, updates)
        descriptor = source.blob(source.resolve(DESCRIPTORS[1]))
        providers = re.findall(rb'(?m)^[ \t]*AmlLib[ \t]*\|([^\r\n]+)', descriptor)
        self.assertEqual(providers, [b'Platform/CIX/Sky1/Library/Acpi/CIX/AmlLib/AmlLib.inf'])
        inf = source.blob(imported + 'AmlLib.inf')
        dependencies = re.search(rb'\[LibraryClasses\](.*?)\[BuildOptions\]', inf, re.S)[1]
        self.assertNotRegex(dependencies, rb'(?m)^[ \t]*AmlLib[ \t]*\r?$')
        for path in entries:
            self.assertLessEqual(len(Path(path.removeprefix(imported)).parts), 2,
                                 'dependency wildcard must cover every module input')

    def test_make_dependency_and_basetools_module_selection(self):
        source = selected_tree(GitTree(ROOT, rev_parse(ROOT, OLD)))
        updates = aml_library_updates(source)
        makefile = updates['src/Makefile'].decode()
        self.assertRegex(makefile, r'(?m)^Build/%/.*SKY1_BL33_UEFI.fd: .*\$\(CUSTOM_EDK2_BUILD_INPUTS\)')
        with tempfile.TemporaryDirectory(prefix='radxa-aml-resolution-') as temporary:
            root = Path(temporary)
            archive_files(OLD, ['src/' + AML_MODULE.rstrip('/')], root)
            archive_files(MODERN, ['BaseTools/Source/Python/Common'], root)
            for path, data in updates.items():
                target = root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                if compatibility_update_mode(path, data) == '120000':
                    target.symlink_to(data.decode())
                else:
                    target.write_bytes(data)
            src = root / 'src'
            package_block = re.search(r'^CUSTOM_OVERLAY_PACKAGE_ROOTS := .*?(?=^EDK2_PACKAGES_PATH)',
                                      makefile, re.M | re.S)
            self.assertIsNotNone(package_block)
            dependencies = re.search(r'^CUSTOM_EDK2_OVERLAY_SOURCES := .*?(?=^EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_SOURCES)',
                                     makefile, re.M | re.S)[0]
            build_inputs = re.search(r'^CUSTOM_EDK2_BUILD_INPUTS := .*?(?=^CUSTOM_EDK2_ORDER_ONLY_PREREQS)',
                                     makefile, re.M | re.S)[0]
            probe = src / 'probe.mk'
            probe.write_text('CUSTOM_OVERLAY_ROOT := ' + str(root / 'custom/overlay') + '\n' +
                             package_block[0] + dependencies + build_inputs +
                             '\nFV: $(filter $(CUSTOM_OVERLAY_ROOT)/' + AML_MODULE +
                             '%,$(CUSTOM_EDK2_BUILD_INPUTS))\n\t@echo REBUILD\n' +
                             '.PHONY: packages\npackages:\n\t@echo $(EDK2_PACKAGE_ROOTS)\n')
            result = subprocess.check_output(['make', '-s', '-f', str(probe), 'packages',
                                              'ARTEFACT_MODE=custom'], cwd=src, text=True)
            packages = result.strip().split()
            self.assertEqual(packages[0], str(root / 'custom/overlay/edk2-platforms'))
            lookup = ("import sys; sys.path.insert(0, sys.argv[1]); "
                      "from Common.MultipleWorkspace import MultipleWorkspace as W; "
                      "W.setWs(sys.argv[2], sys.argv[3]); print(W.join(sys.argv[2], sys.argv[4]))")
            inf = subprocess.check_output(['python3', '-c', lookup,
                                          str(root / 'BaseTools/Source/Python'), str(src / 'edk2'),
                                          os.pathsep.join(packages), AML_MODULE.removeprefix('edk2-platforms/') + 'AmlLib.inf'],
                                         text=True).strip()
            self.assertEqual(Path(inf), root / AML_OVERLAY / 'AmlLib.inf')
            self.assertEqual((Path(inf).parent / 'CodeGen/AmlCodeGen.c').read_bytes(), updates[AML_CODEGEN])
            fv = src / 'FV'
            fv.touch()
            stamp = time.time() - 10
            for path in (root / AML_OVERLAY).rglob('*'):
                os.utime(path, (stamp - 2, stamp - 2))
            os.utime(fv, (stamp, stamp))
            command = ['make', '-n', '-f', str(probe), 'FV']
            self.assertNotIn('REBUILD', subprocess.check_output(command + ['ARTEFACT_MODE=custom'], cwd=src, text=True))
            changed = root / AML_CODEGEN
            os.utime(changed, (stamp + 2, stamp + 2))
            self.assertIn('REBUILD', subprocess.check_output(command + ['ARTEFACT_MODE=custom'], cwd=src, text=True))
            self.assertNotIn('REBUILD', subprocess.check_output(command + ['ARTEFACT_MODE=upstream'], cwd=src, text=True))

    def test_actual_inf_sources_compile_across_header_transition(self):
        compiler = shlex.split(os.environ.get('AML_TEST_CC', 'cc'))
        compiler_version = subprocess.check_output([*compiler, '--version'], text=True)
        if 'clang' in compiler_version.lower():
            self.skipTest('complete vendor module qualification requires GCC; set AML_TEST_CC')
        source = GitTree(ROOT, rev_parse(ROOT, OLD))
        selected = selected_tree(source)
        updates = aml_library_updates(selected)
        self.assertIn(AML_CODEGEN, updates)
        with tempfile.TemporaryDirectory(prefix='radxa-aml-compile-') as temporary:
            root = Path(temporary)
            archive_files(OLD, ['src/' + AML_MODULE.rstrip('/'),
                               'src/edk2-platforms/Platform/CIX/Sky1/Include',
                               'src/edk2-platforms/Silicon/CIX/Sky1/Include'], root)
            module = root / ('src/' + AML_MODULE)
            inf = (module / 'AmlLib.inf').read_text()
            sources = re.search(r'\[Sources\](.*?)\n\[Packages\]', inf, re.S)[1]
            sources = [line.strip() for line in sources.splitlines() if line.strip().endswith('.c')]
            self.assertGreater(len(sources), 20)
            fixed = root / AML_OVERLAY
            for path, data in updates.items():
                target = root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                if compatibility_update_mode(path, data) == '120000':
                    target.symlink_to(data.decode())
                else:
                    target.write_bytes(data)
            self.assertEqual((fixed / 'AmlLib.inf').read_bytes(), (module / 'AmlLib.inf').read_bytes())
            for path in module.rglob('*'):
                if path.is_file() and path.relative_to(module).as_posix() != 'CodeGen/AmlCodeGen.c':
                    self.assertEqual((fixed / path.relative_to(module)).read_bytes(), path.read_bytes())
            public = set(re.findall(rb'(?m)^([A-Za-z_]\w*)[ \t]*\(', selected.blob(AML_PUBLIC_HEADER)))
            private = set()
            for path in module.rglob('*.c'):
                private.update(re.findall(rb'(?m)^STATIC\r?\n[^;{}]+?\r?\n([A-Za-z_]\w*)[ \t]*\(', path.read_bytes()))
            self.assertEqual(private & public, {b'AmlCodeGenMethod'})
            symbols = shlex.split(os.environ.get('AML_TEST_NM', 'nm'))
            arch = os.environ.get('AML_TEST_ARCH', 'AArch64' if platform.machine().lower() in ('arm64', 'aarch64') else 'X64')
            for version in ('202208', '202605', '202608'):
                headers = root / version
                archive_files('source/base/edk2/edk2-stable' + version,
                              ['MdePkg/Include', 'DynamicTablesPkg/Include'], headers)
                if version == '202208':
                    archive_files('source/unofficial/1.2.1/edk2-stable202208',
                                  ['src/edk2/MdePkg/Include', 'src/edk2/DynamicTablesPkg/Include'], headers)
                    headers = headers / 'src/edk2'
                # This is the existing firmware warning policy, not a new exemption.
                tools = show_file(ROOT, 'source/base/edk2/edk2-stable' + version, 'BaseTools/Conf/tools_def.template').decode()
                self.assertTrue(re.search(r'DEFINE GCC(?:5)?_AARCH64_CC_(?:COMMON|FLAGS)[^\n]+-Wno-address', tools), version)
                includes = [headers / 'MdePkg/Include', headers / ('MdePkg/Include/' + arch),
                            headers / 'DynamicTablesPkg', headers / 'DynamicTablesPkg/Include',
                            root / 'src/edk2-platforms/Platform/CIX/Sky1/Include',
                            root / 'src/edk2-platforms/Silicon/CIX/Sky1/Include']
                includes += [path / 'Library' for path in includes if path.name == 'Include']
                includes += [path.parent for path in includes if path.name == 'Include']
                objects = root / (version + '-objects')
                objects.mkdir()
                def compile_unit(base, name):
                    command = [*compiler, '-std=c11', '-Wall', '-Werror', '-Wno-address', '-fshort-wchar', '-DAML_HANDLE',
                               '-include', 'Uefi.h', '-c', str(base / name), '-o', str(objects / (name.replace('/', '_') + '.o')), '-I', str(base)]
                    for include in includes:
                        command.extend(['-I', str(include)])
                    return subprocess.run(command, text=True, capture_output=True)
                if version == '202608':
                    bad = compile_unit(module, 'CodeGen/AmlCodeGen.c')
                    self.assertNotEqual(bad.returncode, 0)
                    self.assertRegex(bad.stderr, 'static declaration.*AmlCodeGenMethod|AmlCodeGenMethod.*static declaration')
                base = fixed if version == '202608' else module
                if version == '202208':
                    matching = root / 'vendor202208'
                    archive_files('source/unofficial/1.2.1/edk2-stable202208',
                                  ['src/' + AML_MODULE.rstrip('/')], matching)
                    base = matching / ('src/' + AML_MODULE)
                for name in sources:
                    with self.subTest(version=version, source=name):
                        result = compile_unit(base, name)
                        self.assertEqual(result.returncode, 0, result.stderr)
                names = subprocess.check_output([*symbols, '-g', '--defined-only', *map(str, objects.glob('*.o'))], text=True)
                names = re.findall(r'(?m)^.* [A-Za-z] ([A-Za-z_]\w*)$', names)
                counts = Counter(names)
                self.assertEqual(counts.get('AmlCodeGenMethod', 0), int(version == '202608'))
                self.assertEqual([name for name, count in counts.items() if count > 1], [])


if __name__ == '__main__':
    unittest.main()
