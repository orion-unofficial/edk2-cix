#!/usr/bin/env python3
"""Exercise custom update-script packaging without executing a flash utility."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from reconstruction_common import for_each_ref, show_file

ROOT = Path(__file__).resolve().parents[1]


def selected_refs():
    candidates = os.environ.get("SOURCE_STARTUP_CANDIDATES")
    if candidates:
        return [entry["new"] for entry in json.loads(Path(candidates).read_text())]
    return for_each_ref(ROOT, "source/unofficial/")


class CustomStartupTests(unittest.TestCase):
    def test_every_source_has_the_same_guarded_custom_wrapper(self):
        scripts = set()
        for ref in selected_refs():
            with self.subTest(ref=ref):
                custom = show_file(ROOT, ref, "custom/scripts/startup.nsh").decode()
                vendor = show_file(ROOT, ref, "src/scripts/startup.nsh").decode()
                scripts.add(custom)
                self.assertNotIn("edk2_cix_flash_status", vendor)
                self.assertIn('"%0\\..\\FlashUpdate.efi" -f "%0\\..\\cix_flash_all.bin" -n\n'
                              'set -v edk2_cix_flash_status %lasterror%\n'
                              'if not %edk2_cix_flash_status% == 0 then', custom)
                failure = custom.split('if not %edk2_cix_flash_status% == 0 then', 1)[1].split('endif', 1)[0]
                self.assertIn('exit /b %edk2_cix_flash_status%', failure)
                self.assertNotIn('reset ', failure)
                self.assertNotIn('BIOS Update completed!', custom)
                for name in ('FlashUpdate.efi', 'cix_flash_all.bin'):
                    check = custom.split(f'if not exist "%0\\..\\{name}" then', 1)[1].split('endif', 1)[0]
                    self.assertIn('exit /b 14', check)
        self.assertEqual(len(scripts), 1)

    def test_real_exporter_regressions_for_each_retained_variant(self):
        checked = set()
        for ref in selected_refs():
            files = {path: show_file(ROOT, ref, path) for path in (
                'scripts/export_firmware_payload.py', 'scripts/test_export_firmware_payload.py',
                'scripts/firmware_layout.py', 'scripts/firmware_metadata_audit.py')}
            identity = tuple(files.values())
            if identity in checked:
                continue
            checked.add(identity)
            with self.subTest(ref=ref), tempfile.TemporaryDirectory(prefix='startup-export-') as tmp:
                root = Path(tmp)
                for path, data in files.items():
                    output = root / path
                    output.parent.mkdir(parents=True, exist_ok=True)
                    output.write_bytes(data)
                self.assertIn(b'def test_startup_selection_is_mode_specific',
                              files['scripts/test_export_firmware_payload.py'])
                result = subprocess.run([sys.executable, str(root / 'scripts/test_export_firmware_payload.py')],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_make_preserves_custom_mode_through_debuild_environment_filtering(self):
        checked = set()
        for ref in selected_refs():
            makefile = show_file(ROOT, ref, '.github/local/Makefile.local').decode()
            snippet = makefile[makefile.index('ARTEFACT_MODE ?= custom'):makefile.index('FIRMWARE_DISTRO ?=')]
            if snippet in checked:
                continue
            checked.add(snippet)
            with self.subTest(ref=ref), tempfile.TemporaryDirectory(prefix='startup-debuild-') as tmp:
                root = Path(tmp)
                (root / 'Makefile').write_text(snippet + '\nall:\n\t@printf "%s\\n" "$(CUSTOM_DEBUILD_ARG)"\n')
                for args, custom in (([], True), (['ARTEFACT_MODE=custom'], True),
                                     (['ARTEFACT_MODE=upstream'], False)):
                    result = subprocess.run(['make', '--no-print-directory', 'all',
                                             'CUSTOM_DEBUILD_ARG=-aarm64', *args],
                                            cwd=root, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(result.stdout.strip(), '-aarm64 --set-envvar=ARTEFACT_MODE=custom'
                                     if custom else '-aarm64')

    def test_debian_hook_changes_only_custom_package_copies(self):
        checked = set()
        for ref in selected_refs():
            rule = show_file(ROOT, ref, '.github/local/rules.local')
            if rule in checked:
                continue
            checked.add(rule)
            with self.subTest(ref=ref), tempfile.TemporaryDirectory(prefix='startup-debian-') as tmp:
                root = Path(tmp)
                (root / 'rules.local').write_bytes(rule)
                # Provide the no-op debhelper extension point for upstream mode.
                (root / 'Makefile').write_text('execute_after_dh_install:\ninclude rules.local\n')
                source = root / 'custom/scripts/startup.nsh'
                source.parent.mkdir(parents=True)
                source.write_bytes(b'custom script\n')
                destinations = [root / f'debian/edk2-cix/usr/share/edk2/radxa/{board}/startup.nsh'
                                for board in ('orion-o6', 'orion-o6n')]
                for mode in ('custom', 'upstream', '', 'custom'):
                    for path in destinations:
                        path.parent.mkdir(parents=True, exist_ok=True)
                        path.write_bytes(b'vendor script\r\n')
                    result = subprocess.run(['make', '--no-print-directory', 'execute_after_dh_install',
                                             f'ARTEFACT_MODE={mode}'], cwd=root, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    for path in destinations:
                        self.assertEqual(path.read_bytes(), b'custom script\n' if mode == 'custom' else b'vendor script\r\n')
                destinations[0].unlink()
                destinations[0].parent.rmdir()
                destinations[1].write_bytes(b'vendor script\r\n')
                result = subprocess.run(['make', '--no-print-directory', 'execute_after_dh_install',
                                         'ARTEFACT_MODE=custom'], cwd=root, capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(destinations[1].read_bytes(), b'vendor script\r\n')


if __name__ == '__main__':
    unittest.main()
