#!/usr/bin/env python3
"""Labels alone must not certify a mismatched vendor source/payload."""
from pathlib import Path
import os
import subprocess
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from validate_release_inputs import FIRMWARES, LOCKED_PAYLOADS, input_problems
from reconstruction_common import release_entry


class ReleaseInputProvenanceTests(unittest.TestCase):
    def test_public_build_rejects_historical_relabel_before_rendering(self):
        result = subprocess.run([
            'make', 'build', 'RELEASE=edk2-202211/radxa-1.3.1/unofficial',
            'ARTEFACT_MODE=custom', 'FORCE_DEBUG_BUILD=1',
        ], cwd=Path(__file__).resolve().parents[1], capture_output=True, text=True,
            env={**os.environ, 'PYTHONDONTWRITEBYTECODE': '1'})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('release input provenance failed', result.stderr)
        self.assertNotIn('[render]', result.stdout + result.stderr)

    def test_primary_targets_bind_the_requested_vendor_release(self):
        root = Path(__file__).resolve().parents[1]
        for edk2 in ('202208', '202608'):
            for radxa in ('1.2.4', '1.3.1'):
                target = f'edk2-{edk2}/radxa-{radxa}/unofficial'
                with self.subTest(target=target):
                    entry = release_entry(root, target)[1]
                    self.assertEqual(input_problems(root, entry), [])
                    self.assertEqual(entry['edk2_release'], 'edk2-stable' + edk2)

    def check(self, baseline='1.3.1', changed=None, absent=None):
        vendor = {FIRMWARES + name: SimpleNamespace(object_id=name) for name in LOCKED_PAYLOADS}
        if absent:
            vendor.pop(FIRMWARES + absent)
        actual = dict(vendor)
        if changed:
            actual[FIRMWARES + changed] = SimpleNamespace(object_id='modified')
        entry = dict(unofficial_delta=True, source_ref='selected', radxa_release='1.3.1',
                     edk2_release='edk2-stable202608')
        with patch('validate_release_inputs.baseline_release', return_value=baseline), \
             patch('validate_release_inputs.release_metadata_ref', return_value='vendor'), \
             patch('validate_release_inputs.tree_entries', side_effect=[actual, vendor]):
            return input_problems(Path('.'), entry)

    def test_matching_version_and_vendor_boot_payloads(self):
        self.assertEqual(self.check(), [])

    def test_metadata_only_relabel_is_rejected_even_with_matching_payloads(self):
        self.assertIn('release_metadata', '\n'.join(self.check(baseline='1.2.1')))

    def test_matching_label_cannot_hide_a_wrong_vendor_payload(self):
        for name in LOCKED_PAYLOADS:
            with self.subTest(name=name):
                self.assertIn(name, '\n'.join(self.check(changed=name)))

    def test_only_optional_trustzone_payload_may_be_absent_from_both(self):
        self.assertEqual(self.check(absent='trustzone_config.bin'), [])
        for name in set(LOCKED_PAYLOADS) - {'trustzone_config.bin'}:
            self.assertIn(name, '\n'.join(self.check(absent=name)))


if __name__ == '__main__':
    unittest.main()
