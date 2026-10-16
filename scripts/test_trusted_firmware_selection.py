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
from reconstruction_common import resolve_ref, show_file, unofficial_component_steps, unofficial_firmware_refs, unofficial_checkpoints_by_edk2
from validate_build_variables import validate_cix_source


ROOT = Path(__file__).resolve().parents[1]
REF = os.environ.get('SOURCE_TEST_REF', 'source/unofficial/1.3/current')


class TrustedSelectionTests(unittest.TestCase):
    def test_hybrid_preflight_requires_the_rendered_component(self):
        entry = {'source_ref': REF, 'radxa_release': '1.3.1',
                 'render': {'steps': unofficial_component_steps(ROOT, REF)}}
        with patch.dict(os.environ, CIX_RELEASE='unofficial', STMM='unofficial'):
            problems = []
            validate_cix_source(ROOT, entry, problems)
            self.assertEqual(problems, [])
            entry['render']['steps'] = [step for step in entry['render']['steps']
                                        if step['component']['path'].endswith('/tee-unofficial')]
            validate_cix_source(ROOT, entry, problems)
            self.assertEqual(len(problems), 1)
            self.assertIn('hybrid StandaloneMM component is not selected', problems[0])

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
