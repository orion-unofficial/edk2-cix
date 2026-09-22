#!/usr/bin/env python3
"""Check every primary source, board, fixes and menu qualification dimension."""

import json
import unittest

from firmware_ci_matrix import ROOT, firmware_matrix
from reconstruction_common import matrix_release_branches, source_target_name


class FirmwareCIMatrixTests(unittest.TestCase):
    def test_primary_o6_matrix_has_sixteen_unique_builds(self):
        targets = [f"edk2-{edk2}/radxa-{radxa}/unofficial"
                   for edk2 in ("202208", "202605", "202608") for radxa in ("1.2.4", "1.3.1")]
        rows = firmware_matrix(targets + [targets[0]], ["1.2.4", "1.3.1"],
                               ["202208", "202608"], ["O6"])["include"]
        self.assertEqual(len(rows), 16)
        self.assertEqual(len({row['key'] for row in rows}), 16)
        self.assertFalse(any('202605' in row['release'] for row in rows))
        for target in {row['release'] for row in rows}:
            self.assertEqual({(row['firmware_fixes'], row['experimental'])
                              for row in rows if row['release'] == target},
                             {(False, False), (False, True), (True, False), (True, True)})

    def test_missing_tuple_cannot_be_replaced_by_another_edk2(self):
        with self.assertRaisesRegex(ValueError, 'edk2-202208/radxa-1.3.1'):
            firmware_matrix(['edk2-202608/radxa-1.3.1/unofficial'], ['1.3.1'],
                            ['202208', '202608'], ['O6'])

    def test_matrix_capacity_never_silently_drops_releases(self):
        releases = [str(n) for n in range(33)]
        targets = [f'edk2-{release}/radxa-1.3.1/unofficial' for release in releases]
        with self.assertRaisesRegex(ValueError, 'shard it without dropping source targets'):
            firmware_matrix(targets, ['1.3.1'], releases, ['O6', 'O6N'])

    def test_ci_keeps_o6n_and_both_menu_states(self):
        policy = json.loads((ROOT / 'config/policies.json').read_text())['firmware_qualification_policy']
        self.assertEqual(policy['edk2_releases'], ['202208', '202608'])
        self.assertEqual(policy['radxa_releases'], ['1.2.4', '1.3.1'])
        self.assertEqual(policy['hardware_qualification_boards'], ['O6'])
        branches, _ = matrix_release_branches(ROOT)
        rows = firmware_matrix([source_target_name(branch) for branch in branches],
                               policy['radxa_releases'], policy['edk2_releases'], policy['boards'])['include']
        self.assertEqual(len(rows), 32)
        self.assertEqual({row['board'] for row in rows}, {'O6', 'O6N'})
        self.assertEqual(sum(row['board'] == 'O6' for row in rows), 16)
        workflow = (ROOT / '.github/workflows/supported-firmware.yaml').read_text()
        self.assertIn('ENABLE_EXPERIMENTAL_UEFI_SETTINGS: ${{ matrix.experimental }}', workflow)
        self.assertIn('ENABLE_EXPERIMENTAL_UEFI_SETTINGS="${ENABLE_EXPERIMENTAL_UEFI_SETTINGS}"', workflow)


if __name__ == '__main__':
    unittest.main()
