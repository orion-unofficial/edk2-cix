#!/usr/bin/env python3
"""Check selector boundaries independently of the normal-world EDK2 release."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import types
import unittest

from build_profiles import resolve_profile
from reconstruction_common import show_file, unofficial_component_steps


ROOT = Path(__file__).resolve().parents[1]
REF = os.environ.get('SOURCE_TEST_REF', 'source/unofficial/1.3/current')


class TrustedSelectionTests(unittest.TestCase):
    def test_profile_selects_maintained_component_without_changing_release(self):
        stock = resolve_profile(ROOT, requested_profile='latest')
        for selector, expected in (('1.2', '1.2'), ('v1.2', '1.2'), ('unofficial', 'unofficial')):
            profile = resolve_profile(ROOT, requested_profile='latest', cix_release_override=selector)
            self.assertEqual(profile['release'], stock['release'])
            self.assertEqual(profile['cix_early_boot_release'], expected)

    def test_unavailable_stmm_rejected_at_every_make_entry(self):
        make = shutil.which('gmake') or shutil.which('make')
        # The rejection happens before any compiler, download or source rendering.
        with tempfile.TemporaryDirectory(prefix='stmm-selector-') as directory:
            path = Path(directory) / 'Makefile'
            for relative in ('Makefile', '.github/local/Makefile.local', 'src/Makefile'):
                path.write_bytes(show_file(ROOT, REF, relative))
                for release in ('202208', '202411', '202502', '202608'):
                    result = subprocess.run([make, '-f', str(path), '-n', 'CIX_RELEASE=unofficial',
                                             'STMM=unofficial', f'EDK2_BASE={release}'],
                                            text=True, capture_output=True, cwd=directory)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('not yet qualified for any EDK2 base', result.stderr)
            result = subprocess.run([make, '-f', str(ROOT / 'Makefile'), '-n',
                                     'CIX_RELEASE=unofficial', 'STMM=unofficial'],
                                    text=True, capture_output=True, cwd=ROOT)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('pre-2025', result.stderr)

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

    def test_renderer_uses_one_component_ref(self):
        self.assertEqual(unofficial_component_steps(ROOT), [{'component': {
            'path': 'src/cix-v1.2/tee-unofficial',
            'ref': 'source/unofficial/cix/1.2/op-tee/3.17.0',
        }}])


if __name__ == '__main__':
    unittest.main()
