#!/usr/bin/env python3
"""Reject unsigned trusted-component selections before any build work begins."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from build_profiles import resolve_profile
from reconstruction_common import ReconstructionError, for_each_ref, show_file

ROOT = Path(__file__).resolve().parents[1]
ERROR = 'CIX_RELEASE must be empty'


class CixReleasePolicyTests(unittest.TestCase):
    def test_public_make_rejects_before_python_or_shell_and_preserves_old_outputs(self):
        with tempfile.TemporaryDirectory(prefix='cix-policy-') as tmp:
            root = Path(tmp)
            old = root / 'cix_flash_all.bin'
            old.write_bytes(b'previous valid firmware')
            for value in ('1.2', 'v1.2', 'v', 'unknown', '0'):
                for goal in ('build', 'firmware', 'help-vars'):
                    result = subprocess.run(
                        ['make', '--no-print-directory', '-f', str(ROOT / 'Makefile'), goal,
                         'CIX_RELEASE=' + value, 'PYTHON=/must-not-run', 'SHELL=/must-not-run'],
                        cwd=root, capture_output=True, text=True, timeout=5)
                    self.assertNotEqual(result.returncode, 0, (value, goal))
                    self.assertIn(ERROR, result.stderr)
                    self.assertNotIn('must-not-run', result.stderr)
                    self.assertEqual(list(root.iterdir()), [old])
                    self.assertEqual(old.read_bytes(), b'previous valid firmware')
            result = subprocess.run(['make', '-f', str(ROOT/'Makefile'), '-n', 'build'],
                                    cwd=root, env=dict(os.environ, CIX_RELEASE='1.2'),
                                    capture_output=True, text=True, timeout=5)
            self.assertIn(ERROR, result.stderr)
            self.assertNotEqual(result.returncode, 0)

    def test_help_accepts_blank_or_unset_and_does_not_advertise_unusable_option(self):
        env = dict(os.environ)
        env.pop('CIX_RELEASE', None)
        for args in ([], ['CIX_RELEASE=']):
            result = subprocess.run(['make', '--no-print-directory', 'help-vars', *args],
                                    cwd=ROOT, env=env, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn('CIX_RELEASE=', result.stdout)

    def test_retained_source_make_entry_points_fail_before_includes_or_tools(self):
        refs = for_each_ref(ROOT, 'source/unofficial/')
        self.assertTrue(refs)
        from test_firmware_build_recipe import RecipeTests
        with tempfile.TemporaryDirectory(prefix='cix-source-policy-') as tmp:
            root = Path(tmp)
            makefile = root/'Makefile'
            for ref in refs:
                with patch.dict(os.environ, SOURCE_TEST_REF=ref):
                    recipe = RecipeTests('test_recipe_is_inline_in_existing_component_version_form')
                    recipe.setUp()
                    recipe.test_recipe_is_inline_in_existing_component_version_form()
                for name in ('Makefile', 'src/Makefile', '.github/local/Makefile.local'):
                    makefile.write_bytes(show_file(ROOT, ref, name))
                    result = subprocess.run([shutil.which('gmake') or 'make', '-f', str(makefile),
                                             'help', 'CIX_RELEASE=1.2', 'SHELL=/must-not-run'],
                                            cwd=root, capture_output=True, text=True, timeout=5)
                    self.assertNotEqual(result.returncode, 0, (ref, name))
                    self.assertIn(ERROR, result.stderr, (ref, name, result.stderr))
                    self.assertNotIn('must-not-run', result.stderr)

    def test_profile_and_direct_validator_reject_before_resolving_sources(self):
        with patch('build_profiles.load_json', side_effect=AssertionError('must not load policy')):
            with self.assertRaisesRegex(ReconstructionError, ERROR):
                resolve_profile(ROOT, cix_release_override='v')
        result = subprocess.run([sys.executable, str(ROOT/'scripts/validate_build_variables.py'),
                                 '--repo-root', '/does-not-exist'],
                                env=dict(os.environ, CIX_RELEASE='1.2'),
                                capture_output=True, text=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(ERROR, result.stderr)
        self.assertNotIn('not a git repository', result.stderr)


if __name__ == '__main__':
    unittest.main()
