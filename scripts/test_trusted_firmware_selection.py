#!/usr/bin/env python3
"""Check selector boundaries independently of the normal-world EDK2 release."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import types
import unittest
from unittest.mock import patch

from build_profiles import resolve_profile
from reconstruction_common import NATIVE_STMM_CHECKPOINT_TARGET, release_entry, resolve_ref, show_file, unofficial_component_steps, unofficial_firmware_refs, unofficial_checkpoints_by_edk2
from validate_build_variables import validate_cix_source


ROOT = Path(__file__).resolve().parents[1]
REF = os.environ.get('SOURCE_TEST_REF', 'source/unofficial/1.3/current')


class TrustedSelectionTests(unittest.TestCase):
    def test_shared_adapter_resolves_remote_tracking_source_refs(self):
        from stmm_integration import feature_files, FEATURE_REF
        source_ref = 'source/unofficial/1.3.2/edk2-stable202208'
        objects = subprocess.check_output([
            'git', '-C', str(ROOT), 'rev-parse', '--path-format=absolute', '--git-path', 'objects'], text=True).strip()
        with tempfile.TemporaryDirectory(prefix='stmm-remote-refs-') as directory:
            repo = Path(directory)
            subprocess.run(['git', '-C', directory, 'init', '-q'], check=True)
            (repo / '.git/objects/info/alternates').write_text(objects + '\n')
            for ref in (FEATURE_REF, source_ref):
                oid = subprocess.check_output([
                    'git', '-C', str(ROOT), 'rev-parse', resolve_ref(ROOT, ref)], text=True).strip()
                subprocess.run(['git', '-C', directory, 'update-ref',
                                'refs/remotes/origin/' + ref, oid], check=True)
            self.assertNotEqual(subprocess.run([
                'git', '-C', directory, 'show-ref', '--verify', '--quiet',
                'refs/heads/' + FEATURE_REF]).returncode, 0)
            files = feature_files(repo, source_ref)
            self.assertIn('src/stmm-integration-v1/PlatformStandaloneMmRpmb.dsc', files)
            self.assertIn(b'--stmm "$(STMM)"', files['src/Makefile'][1])

    def test_hybrid_preflight_requires_the_rendered_component(self):
        _, entry = release_entry(ROOT, NATIVE_STMM_CHECKPOINT_TARGET, require=True)
        with patch.dict(os.environ, CIX_RELEASE='unofficial', STMM='unofficial',
                        FIRMWARE_BOARD='O6', FIRMWARE_TARGET='RELEASE'):
            problems = []
            validate_cix_source(ROOT, entry, problems)
            self.assertEqual(problems, [])
            entry['render']['steps'] = [step for step in entry['render']['steps']
                                        if step.get('component', {}).get('path') != 'src/cix-v1.2/stmm-unofficial']
            validate_cix_source(ROOT, entry, problems)
            self.assertEqual(len(problems), 1)
            self.assertIn('hybrid StandaloneMM component is not selected', problems[0])

            _, current = release_entry(ROOT, 'edk2-202608/radxa-1.3.2/unofficial', require=True)
            problems = []
            validate_cix_source(ROOT, current, problems)
            self.assertEqual(problems, [])

    def test_shared_integration_accepts_old_and_new_normal_world_bases(self):
        with patch.dict(os.environ, CIX_RELEASE='unofficial', STMM='unofficial'):
            for release in ('1.2.4', '1.3.2'):
                _, entry = release_entry(ROOT, f'edk2-202608/radxa-{release}/unofficial', require=True)
                problems = []
                validate_cix_source(ROOT, entry, problems)
                self.assertEqual(problems, [], release)
                _, old = release_entry(ROOT, f'edk2-202208/radxa-{release}/unofficial', require=True)
                validate_cix_source(ROOT, old, problems)
                self.assertEqual(problems, [], release)

    def test_adapter_preserves_the_release_target_and_shell_recipe_contract(self):
        from stmm_integration import adapt_makefile
        for release, radxa in (('202208', '1.3.2'), ('202502', '1.3.1'), ('202608', '1.3.2')):
            ref = f'source/unofficial/{radxa}/edk2-stable{release}'
            original = show_file(ROOT, ref, 'src/Makefile')
            adapted = adapt_makefile(original, 'src/Makefile')
            preamble = original[original.index(b'FIRMWARE_TARGET ?='):original.index(b'UNAME :=')]
            self.assertIn(preamble, adapted)
            self.assertIn(b'@python3 "$(CIX_RELEASE_STMM_HELPER)" $(CIX_RELEASE_STMM_ARGS)', adapted)
            signer = adapted[adapted.index(b'bash "$(abspath $(CIX_RELEASE_BOOTLOADER2_HELPER))"'):]
            self.assertIn(b'--stmm "$(STMM)"', signer.split(b'--jobs', 1)[0])

    def test_profile_selects_maintained_component_without_changing_release(self):
        stock = resolve_profile(ROOT, requested_profile='latest')
        for selector, expected in (('1.2', '1.2'), ('v1.2', '1.2'), ('unofficial', 'unofficial')):
            profile = resolve_profile(ROOT, requested_profile='latest', cix_release_override=selector)
            self.assertEqual(profile['release'], stock['release'])
            self.assertEqual(profile['cix_early_boot_release'], expected)

    def test_hybrid_requires_maintained_optee(self):
        make = shutil.which('gmake') or shutil.which('make')
        for selector in ('', 'v1.2', '1.2'):
            result = subprocess.run([make, '-f', str(ROOT / 'Makefile'), '-n', 'help',
                                     f'CIX_RELEASE={selector}', 'STMM=unofficial'],
                                    text=True, capture_output=True, cwd=ROOT)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('requires CIX_RELEASE=unofficial', result.stderr)

    def test_pre_2025_hybrid_rejected_at_every_firmware_entry(self):
        make = shutil.which('gmake') or shutil.which('make')
        with tempfile.TemporaryDirectory(prefix='stmm-selector-') as directory:
            for relative in ('Makefile', '.github/local/Makefile.local', 'src/Makefile'):
                path = Path(directory) / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(show_file(ROOT, REF, relative))
                result = subprocess.run([make, '-f', str(path), '-n', 'CIX_RELEASE=unofficial',
                                         'STMM=unofficial'], text=True, capture_output=True, cwd=directory)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('pre-2025 bases are not qualified', result.stderr)

    def test_distinct_output_paths_and_version_aliases(self):
        module = types.ModuleType('firmware_layout')
        import sys
        sys.modules[module.__name__] = module
        self.addCleanup(sys.modules.pop, module.__name__, None)
        exec(compile(show_file(ROOT, REF, 'scripts/firmware_layout.py'), 'firmware_layout.py', 'exec'), module.__dict__)
        legacy = module.FirmwareLayout(cix_release='1.2')
        alias = module.FirmwareLayout(cix_release='v1.2')
        maintained = module.FirmwareLayout(cix_release='unofficial')
        self.assertEqual(legacy.leaf_parts(), alias.leaf_parts())
        self.assertNotEqual(legacy.leaf_parts(), maintained.leaf_parts())
        hybrid = module.FirmwareLayout(cix_release='unofficial', stmm='unofficial')
        self.assertNotEqual(maintained.leaf_parts(), hybrid.leaf_parts())
        self.assertIn('stmm_unofficial', hybrid.archive_suffix_tokens())

    def test_shared_layout_keeps_vendor_and_unofficial_payloads_separate(self):
        from stmm_integration import feature_files
        import sys
        for release in ('202208', '202608'):
            ref = f'source/unofficial/1.3.2/edk2-stable{release}'
            module = types.ModuleType('shared_firmware_layout_' + release)
            sys.modules[module.__name__] = module
            self.addCleanup(sys.modules.pop, module.__name__, None)
            content = feature_files(ROOT, ref)['scripts/firmware_layout.py'][1]
            exec(compile(content, module.__name__, 'exec'), module.__dict__)
            vendor = module.FirmwareLayout(cix_release='unofficial')
            modern = module.FirmwareLayout(cix_release='unofficial', stmm='unofficial')
            self.assertNotEqual(vendor.leaf_parts(), modern.leaf_parts())
            self.assertNotEqual(vendor.archive_suffix_tokens(), modern.archive_suffix_tokens())
            self.assertIn('stmm_unofficial', modern.leaf_parts())

    def test_hybrid_component_has_no_unresolved_submodules(self):
        entries = subprocess.check_output([
            'git', '-C', str(ROOT), 'ls-tree', '-r',
            resolve_ref(ROOT, 'source/unofficial/stmm/edk2-stable202608')], text=True)
        self.assertFalse(any(line.startswith('160000 ') for line in entries.splitlines()))

    def test_component_refs_are_not_firmware_checkpoints(self):
        firmware = ['source/unofficial/edk2-stable202608',
                    'source/unofficial/1.3/current',
                    'source/unofficial/1.3.1/edk2-stable202608']
        components = ['source/unofficial/stmm/edk2-stable202608',
                      'source/unofficial/cix/1.2/op-tee/3.17.0']
        with patch('reconstruction_common.for_each_ref', return_value=firmware + components):
            self.assertEqual(unofficial_firmware_refs(ROOT), firmware)
            self.assertEqual(unofficial_checkpoints_by_edk2(ROOT), {
                'edk2-stable202608': {'1.3.1': firmware[2]}})

    def test_renderer_uses_one_component_ref(self):
        self.assertEqual(unofficial_component_steps(ROOT), [{'component': {
            'path': 'src/cix-v1.2/tee-unofficial',
            'ref': 'source/unofficial/cix/1.2/op-tee/3.17.0',
        }}, {'component': {
            'path': 'src/cix-v1.2/stmm-unofficial',
            'ref': 'source/unofficial/stmm/edk2-stable202608',
        }}])


if __name__ == '__main__':
    unittest.main()
