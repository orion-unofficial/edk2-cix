#!/usr/bin/env python3
"""Check the experimental HII recipe and repeated-build provenance changes."""
import json
import os
from pathlib import Path
import shlex
import tempfile
import types
import unittest

from test_firmware_reconfiguration import FirmwareReconfigurationTests
from reconstruction_common import show_file

ROOT = Path(__file__).resolve().parents[1]


def source(relative):
    if os.environ.get('SOURCE_TEST_ROOT'):
        return (Path(os.environ['SOURCE_TEST_ROOT']) / relative).read_text()
    return show_file(ROOT, os.environ.get('SOURCE_TEST_REF', 'source/unofficial/1.3/current'), relative).decode()


class RecipeTests(unittest.TestCase):
    def setUp(self):
        self.module = types.ModuleType('generate_build_recipe')
        exec(compile(source('src/scripts/generate_build_recipe.py'), 'generate_build_recipe.py', 'exec'), self.module.__dict__)
        self.config = {'ARTEFACT_MODE': 'custom', 'ENABLE_EXPERIMENTAL_UEFI_SETTINGS': 'TRUE',
                       'ENABLE_FIRMWARE_FIXES': 'TRUE', 'CIX_RELEASE': '', 'UEFI_TARGET': 'RELEASE',
                       'DEBUG_VERBOSE': 'FALSE', 'DEBUG_PRINT_ERROR_LEVEL': '0x80000040',
                       'BUILD_DATE': '2026-09-21T00:00:00Z'}

    def test_every_build_option_including_empty_cix_is_explicit(self):
        lines = self.module.recipe(self.config, 'edk2-202605/radxa-1.3.1/unofficial', 'O6', 'orion-o6', 'trixie')
        values = dict(word.split('=', 1) for word in shlex.split(' '.join(lines))[2:])
        self.assertEqual(set(values), {
            'RELEASE', 'ARTEFACT_MODE', 'FIRMWARE_BOARD',
            'FIRMWARE_TARGET', 'FIRMWARE_DISTRO', 'ENABLE_FIRMWARE_FIXES', 'ENABLE_CORE_ORDER',
            'CIX_RELEASE', 'ENABLE_TF_A_FIXES', 'ENABLE_EXPERIMENTAL_UEFI_SETTINGS',
            'DEBUG_ON_UART3', 'UART3_ENABLE', 'DEBUG_VERBOSE', 'DEBUG_PRINT_ERROR_LEVEL', 'BUILD_DATE',
        })
        self.assertEqual(values['CIX_RELEASE'], '')
        self.assertEqual(values['DEBUG_VERBOSE'], 'false')
        self.assertEqual(values['DEBUG_PRINT_ERROR_LEVEL'], '0x80000040')

    def test_generation_replaces_stale_strings_and_preserves_source_links(self):
        with tempfile.TemporaryDirectory(prefix='recipe-') as tmp:
            root = Path(tmp)
            overlay = root / 'original'
            module = overlay / self.module.MODULE
            module.mkdir(parents=True)
            (root/'upstream.c').write_text('upstream')
            (module/'input.c').symlink_to(root/'upstream.c')
            (module/'BuildRecipe.uni').write_text('template')
            output = root/'generated'
            for verbose in ('TRUE', 'FALSE'):
                self.config['DEBUG_VERBOSE'] = verbose
                self.module.generate(overlay, output, self.config, 'release', 'O6N', 'orion-o6n', 'trixie', 'a'*40)
                text = (output/self.module.MODULE/'BuildRecipe.uni').read_text()
                receipt = json.loads((output/'firmware-rebuild.json').read_text())
                self.assertIn('DEBUG_VERBOSE=' + verbose.lower(), text)
                self.assertEqual(receipt['config']['DEBUG_VERBOSE'], verbose)
                if verbose == 'TRUE':
                    self.assertIn('FORCE_DEBUG_BUILD=1', text)
                self.assertIn('Build checkout: ' + 'a'*40, text)
                self.assertNotIn('Platforms: ', text)
                self.assertEqual((output/self.module.MODULE/'input.c').read_text(), 'upstream')
                self.assertFalse((output/self.module.MODULE/'input.c').is_symlink())
            self.assertNotIn('DEBUG_VERBOSE=true', text)
            self.assertEqual((module/'BuildRecipe.uni').read_text(), 'template')
            self.assertTrue((module/'input.c').is_symlink())

    def test_recipe_is_inline_in_existing_component_version_form(self):
        module = str(self.module.MODULE)
        self.assertTrue(module.endswith('/SystemInfoDxe'))
        overlay = 'custom/overlay-experimental-uefi-settings/'
        vfr = source(overlay + module + '/SystemInfoHii.vfr')
        self.assertEqual(vfr.count('form formid'), 1)
        self.assertLess(vfr.index('STR_FIRMWARE_INFO'), vfr.index('#include "BuildRecipe.hfr"'))
        self.assertLess(vfr.index('#include "BuildRecipe.hfr"'), vfr.index('endform;'))
        # These component rows already exist in the same form; the recipe
        # retains the command and build checkout without duplicating them.
        for token in ('EDK2_CIX', 'EDK2', 'EDK2_NON_OSI', 'EDK2_PLATFORMS'):
            self.assertIn('STR_SOURCE_CODE_' + token + '_VALUE', vfr)
        old = module.replace('/SystemInfoDxe', '/PlatformConfigDxe')
        self.assertNotIn('BuildRecipe', source(overlay + old + '/PlatformConfigHii.vfr'))
        self.assertNotIn('BuildRecipe', source(overlay + old + '/PlatformConfigDxe.inf'))
        with tempfile.TemporaryDirectory(prefix='inline-recipe-') as tmp:
            root = Path(tmp)
            (root/'overlay'/self.module.MODULE).mkdir(parents=True)
            self.module.generate(root/'overlay', root/'out', self.config,
                                 'release', 'O6', 'orion-o6', 'trixie', 'a'*40)
            hfr = (root/'out'/self.module.MODULE/'BuildRecipe.hfr').read_text()
            self.assertIn('subtitle text', hfr)
            self.assertNotIn('form formid', hfr)
            self.assertNotIn('endform', hfr)
            self.assertNotIn('goto', hfr)
            # Generation itself enforces both gates; undefined vendor macros
            # must not hide rows in SystemInfo's different preprocessor context.
            self.assertNotIn('#if', hfr)

    def test_upstream_or_nonexperimental_build_cannot_generate_menu(self):
        for changes in ({'ARTEFACT_MODE': 'upstream'}, {'ENABLE_EXPERIMENTAL_UEFI_SETTINGS': 'FALSE'}):
            with self.assertRaises(ValueError):
                self.module.recipe({**self.config, **changes}, 'release', 'O6', 'orion-o6', 'trixie')

    def test_uefi_and_shell_quoting_preserve_values(self):
        value = 'a "quoted" \\ tag'
        config = {**self.config, 'O6_SMBIOS_BASEBOARD_ASSET_TAG': value}
        lines = self.module.recipe(config, 'release', 'O6', 'orion-o6', 'trixie')
        self.assertIn('O6_SMBIOS_BASEBOARD_ASSET_TAG=' + value, shlex.split(' '.join(lines)))
        self.assertIn('\\"', self.module.uni_string(value))
        with self.assertRaises(ValueError):
            self.module.uni_string('bad\nline')


class RecipeReconfigurationTests(FirmwareReconfigurationTests):
    def test_recipe_identity_changes_invalidate_cached_build_outputs(self):
        initial = {'ENABLE_EXPERIMENTAL_UEFI_SETTINGS': 'true', 'FIRMWARE_REBUILD_RELEASE': 'release',
                   'FIRMWARE_REBUILD_BUILD_COMMIT': 'a'*40, 'FIRMWARE_DISTRO': 'trixie', 'FIRMWARE_PRODUCT': 'orion-o6'}
        for key, value in [('FIRMWARE_REBUILD_RELEASE', 'other-release'), ('FIRMWARE_REBUILD_BUILD_COMMIT', 'b'*40),
                           ('FIRMWARE_DISTRO', 'bookworm'), ('FIRMWARE_PRODUCT', 'orion-o6n')]:
            output, _ = self.configure(initial)
            stale = output/'stale.bin'
            stale.write_bytes(b'old')
            _, config = self.configure({**initial, key: value})
            self.assertFalse(stale.exists(), key)
            self.assertEqual(config[key], value)


if __name__ == '__main__':
    unittest.main()
