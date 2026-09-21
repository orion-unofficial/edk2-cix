#!/usr/bin/env python3
"""Keep new build safeguards and provenance out of upstream replay."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from mirror_build_outputs import mirror_raw_outputs
from test_firmware_build_recipe import source

ROOT = Path(__file__).resolve().parents[1]


class CustomBoundaryTests(unittest.TestCase):
    def test_public_chain_hook_is_absent_for_upstream(self):
        text = (ROOT/'Makefile').read_text()
        definition = re.search(r'define check_firmware_chain\n.*?\nendef', text, re.S).group()
        for mode in ('upstream', 'custom'):
            result = subprocess.run(['make', '-s', '-f', '-', 'probe'], input=definition +
                                    '\nprobe:\n\t@echo \'$(call check_firmware_chain,inputs,build,' + mode + ',)\'\n',
                                    text=True, capture_output=True, check=True)
            if mode == 'upstream':
                self.assertEqual(result.stdout.strip(), ':')
            else:
                self.assertIn('validate_firmware_chain.py', result.stdout)

    def test_source_chain_hooks_execute_only_for_custom(self):
        text = source('src/Makefile')
        lines = [line for line in text.splitlines() if 'python3 "$(FIRMWARE_CHAIN_VALIDATOR)"' in line
                 and '--print-scratch-layout' not in line]
        self.assertGreaterEqual(len(lines), 5)
        # Execute each real guard with a harmless stand-in for its validator;
        # this checks shell branching rather than just looking for a mode label.
        for line in lines:
            self.assertIn('if [[ "$(ARTEFACT_MODE)" == "custom" ]]', line)
            command = re.sub(r'python3 "\$\(FIRMWARE_CHAIN_VALIDATOR\)".*?; fi', 'printf ran; fi', line.strip().removeprefix('@'))
            command = command.removesuffix(' && \\')
            for mode in ('upstream', 'custom'):
                result = subprocess.run(['bash', '-c', command.replace('$(ARTEFACT_MODE)', mode)],
                                        capture_output=True, text=True, check=True)
                self.assertEqual(result.stdout, 'ran' if mode == 'custom' else '')

    def test_raw_mirroring_preserves_upstream_outputs(self):
        with tempfile.TemporaryDirectory(prefix='mirror-boundary-') as tmp:
            root = Path(tmp)
            build = root/'source/src/Build/O6/RELEASE_GCC'
            build.mkdir(parents=True)
            for name in ('cix_flash_all.bin', 'cix_flash_all.raw', 'cix_flash_ota.bin.tmp', 'firmware-rebuild.txt'):
                (build/name).write_text(name)
            for mode in ('upstream', 'custom', 'custom+fixes'):
                paths = mirror_raw_outputs(root/'source', root/'dist', 'release', mode, 'O6', 'RELEASE')
                names = {p.name for p in paths}
                self.assertIn('cix_flash_all.bin', names)
                if mode == 'upstream':
                    self.assertIn('cix_flash_all.raw', names)
                    self.assertIn('cix_flash_ota.bin.tmp', names)
                    self.assertNotIn('firmware-rebuild.txt', names)
                else:
                    self.assertNotIn('cix_flash_all.raw', names)
                    self.assertNotIn('cix_flash_ota.bin.tmp', names)
                    self.assertIn('firmware-rebuild.txt', names)


if __name__ == '__main__':
    unittest.main()
