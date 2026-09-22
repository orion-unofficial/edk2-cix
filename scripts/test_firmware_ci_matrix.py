#!/usr/bin/env python3
"""Check every primary source, board, fixes and menu qualification dimension."""

import copy
import hashlib
import json
import unittest

from firmware_ci_matrix import (
    ROOT, firmware_matrix, qualification_releases, stock_matrix, validate_stock_entry,
)
from reconstruction_common import (
    ReconstructionError, matrix_release_branches, release_entries, show_file, source_target_name,
)


class FirmwareCIMatrixTests(unittest.TestCase):
    def policy(self):
        return json.loads((ROOT / 'config/policies.json').read_text())

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
        policy = self.policy()['firmware_qualification_policy']
        edk2, radxa = qualification_releases(self.policy())
        selected = self.policy()['unofficial_source_policy']
        current = selected['lines'][selected['default_line']]
        self.assertEqual(edk2, ['202208', current['current_edk2_release']])
        self.assertEqual(radxa, ['1.2.4', current['current_radxa_release']])
        self.assertEqual(policy['hardware_qualification_boards'], ['O6'])
        branches, _ = matrix_release_branches(ROOT)
        rows = firmware_matrix([source_target_name(branch) for branch in branches],
                               radxa, edk2, policy['boards'])['include']
        self.assertEqual(len(rows), 32)
        self.assertEqual({row['board'] for row in rows}, {'O6', 'O6N'})
        self.assertEqual(sum(row['board'] == 'O6' for row in rows), 16)
        workflow = (ROOT / '.github/workflows/supported-firmware.yaml').read_text()
        self.assertIn('ENABLE_EXPERIMENTAL_UEFI_SETTINGS: ${{ matrix.experimental }}', workflow)
        self.assertIn('ENABLE_EXPERIMENTAL_UEFI_SETTINGS="${ENABLE_EXPERIMENTAL_UEFI_SETTINGS}"', workflow)

    def test_custom_scope_follows_intentional_line_promotion(self):
        policy = self.policy()
        selected = policy['unofficial_source_policy']['default_line']
        policy['unofficial_source_policy']['lines'][selected].update(
            current_edk2_release='202611', current_radxa_release='1.3.2')
        # An unrelated imported line must not become the advertised/qualified one.
        policy['unofficial_source_policy']['lines']['9.9'] = {
            'current_edk2_release': '209911', 'current_radxa_release': '9.9.9'}
        edk2, radxa = qualification_releases(policy)
        self.assertEqual(edk2, ['202208', '202611'])
        self.assertEqual(radxa, ['1.2.4', '1.3.2'])
        old_targets = [f'edk2-{e}/radxa-{r}/unofficial'
                       for e in ('202208', '202608') for r in ('1.2.4', '1.3.1')]
        with self.assertRaisesRegex(ValueError, 'missing primary source targets'):
            firmware_matrix(old_targets, radxa, edk2, ['O6'])
        with self.assertRaisesRegex(ValueError, 'stock replay coverage'):
            stock_matrix([], policy)

    def test_stock_scope_retains_all_six_releases_and_vendor_inputs(self):
        entries = {source_target_name(branch): entry for branch, entry in release_entries(ROOT).items()}
        rows = stock_matrix(list(entries), self.policy())['include']
        self.assertEqual([row['version'] for row in rows][:6],
                         ['1.2.1', '1.2.2', '1.2.3', '1.2.4', '1.3.0', '1.3.1'])
        for row in rows:
            with self.subTest(version=row['version']):
                entry = entries[row['release']]
                validate_stock_entry(entry, '202208', row['version'])
                harness = entry['render']['steps'][0]['overlay_paths']['ref']
                for board in self.policy()['firmware_qualification_policy']['boards']:
                    prefix = f"validation/replay-inputs/{row['version']}/{board}"
                    manifest = json.loads(show_file(ROOT, harness, f'{prefix}/manifest.json'))
                    self.assertEqual(manifest['board'], board)
                    self.assertIn('BUILD_DATE', manifest['replay_environment'])
                    self.assertEqual(manifest['reference_artefacts']['cix_flash_all.bin']['size'], 8 * 1024**2)
                    for name, cert in manifest['certificates'].items():
                        data = show_file(ROOT, harness, f'{prefix}/certs/{name}')
                        self.assertEqual(len(data), cert['size'])
                        self.assertEqual(hashlib.sha256(data).hexdigest(), cert['sha256'])

    def test_missing_stock_release_cannot_silently_shrink_matrix(self):
        with self.assertRaisesRegex(ValueError, 'edk2-202208/radxa-1.2.1'):
            stock_matrix(['edk2-202208/radxa-1.3.1'], self.policy())

    def test_stock_source_cannot_use_a_relabelled_or_uplifted_checkpoint(self):
        entry = {'source_ref': 'source/vendor/radxa/1.3.1/edk2-stable202208',
                 'render': {'base': {'ref': 'source/vendor/radxa/1.3.1/edk2-stable202208'}},
                 'radxa_release': '1.3.1', 'edk2_release': 'edk2-stable202208',
                 'unofficial_delta': False}
        for ref in ('source/vendor/radxa/1.2.1/edk2-stable202208',
                    'source/unofficial/1.3.1/edk2-stable202208',
                    'source/port/radxa/1.3.1/edk2-stable202608'):
            for field in ('source_ref', 'render_base'):
                with self.subTest(ref=ref, field=field):
                    changed = copy.deepcopy(entry)
                    if field == 'source_ref':
                        changed['source_ref'] = ref
                    else:
                        changed['render']['base']['ref'] = ref
                    with self.assertRaisesRegex(ReconstructionError, 'exact vendor source'):
                        validate_stock_entry(changed, '202208', '1.3.1')

    def test_help_distinguishes_maintained_stock_custom_and_legacy(self):
        from list_source_targets import render_help
        help_text = render_help(ROOT)
        stock = help_text.split('Maintained stock Radxa targets', 1)[1].split('Primary custom qualification targets:')[0]
        custom, legacy = help_text.split('Primary custom qualification targets:', 1)[1].split(
            'Other provenance-compatible source targets', 1)
        for version in self.policy()['firmware_qualification_policy']['stock_radxa_releases']:
            self.assertIn(f'  edk2-202208/radxa-{version}\n', stock)
            self.assertNotIn(f'  edk2-202208/radxa-{version}\n', legacy)
        self.assertEqual(custom.count('/unofficial\n'), 4)
        self.assertNotIn('edk2-202211/radxa-1.3.1/unofficial', help_text)

    def test_ci_calls_exact_replay_for_each_maintained_version_and_requires_enumeration(self):
        workflow = (ROOT / '.github/workflows/build-branch-ci.yaml').read_text()
        self.assertIn('python3 scripts/firmware_ci_matrix.py --stock', workflow)
        self.assertIn('matrix: ${{ fromJSON(needs.replay-matrix.outputs.releases) }}', workflow)
        self.assertIn('replay_source_target: ${{ matrix.release }}', workflow)
        self.assertIn('replay_version: ${{ matrix.version }}', workflow)
        self.assertIn('[[ "${REPLAY_MATRIX_RESULT}" == success ]]', workflow)
        self.assertIn('[[ "${UPSTREAM_REPLAY_RESULT}" == success ]]', workflow)
        replay = (ROOT / '.github/workflows/deterministic-replay.yaml').read_text()
        self.assertIn('board:\n          - O6\n          - O6N', replay)


if __name__ == '__main__':
    unittest.main()
