#!/usr/bin/env python3
"""Verify selected source resolution against the complete repository matrix."""

from pathlib import Path
import unittest
from unittest.mock import patch

import reconstruction_common as common

ROOT = Path(__file__).resolve().parents[1]


class SelectedReleaseEntryTests(unittest.TestCase):
    def test_all_supported_names_match_complete_entries(self):
        common.clear_metadata_caches()
        entries = common.release_entries(ROOT)
        matrix = common.matrix_release_branches(ROOT)
        # This read-only snapshot keeps the exhaustive lookup test focused on
        # resolution and projection rather than repeatedly enumerating refs.
        with patch.object(common, 'matrix_release_branches', return_value=matrix):
            for branch, expected in entries.items():
                names = {branch, common.short_release(branch), common.source_target_name(branch),
                         common.branch_to_ref(branch)}
                for name in names:
                    with self.subTest(name=name):
                        self.assertEqual(common.release_entry(ROOT, name), (branch, expected))

    def test_explicit_lookup_projects_only_selected_entry(self):
        branch = 'source/cache/release/upstream/edk2-202608/radxa-1.3.1'
        with patch.object(common, 'matrix_release_branches', return_value=({branch}, {})), \
             patch.object(common, 'synthesise_release_entry', return_value={'source_ref': 'selected'}) as project, \
             patch.object(common, 'release_entries', side_effect=AssertionError('full projection')):
            self.assertEqual(common.release_entry(ROOT, common.source_target_name(branch)),
                             (branch, {'source_ref': 'selected'}))
            project.assert_called_once_with(ROOT, branch)

    def test_ambiguous_and_unknown_names_preserve_errors(self):
        upstream = 'source/cache/release/upstream/edk2-202608/radxa-1.3.1'
        vendor = 'source/cache/release/vendor/edk2-202608/radxa-1.3.1'
        with patch.object(common, 'matrix_release_branches', return_value=({upstream, vendor}, {})), \
             patch.object(common, 'synthesise_release_entry', side_effect=AssertionError('projection')):
            with self.assertRaises(common.ReconstructionError) as ambiguous:
                common.release_entry(ROOT, 'edk2-202608/radxa-1.3.1')
            self.assertEqual(str(ambiguous.exception),
                             'ambiguous firmware source target: edk2-202608/radxa-1.3.1\n'
                             f'  - {upstream}\n  - {vendor}')
            with self.assertRaises(common.ReconstructionError) as unknown:
                common.release_entry(ROOT, 'edk2-unknown/radxa-1.3.1')
            self.assertEqual(str(unknown.exception),
                             'unknown firmware source target: edk2-unknown/radxa-1.3.1\n'
                             "Use 'make help-source-targets' to list configured source targets.")

    def test_default_retains_complete_resolution(self):
        branch = 'source/cache/release/upstream/edk2-202608/radxa-1.3.1'
        expected = {'source_ref': 'original'}
        with patch.object(common, 'default_release', return_value=common.source_target_name(branch)), \
             patch.object(common, 'release_entries', return_value={branch: expected}) as complete:
            self.assertEqual(common.release_entry(ROOT, None), (branch, expected))
            complete.assert_called_once_with(ROOT)

    def test_missing_default_keeps_required_and_optional_errors(self):
        with patch.object(common, 'default_release', return_value=''):
            for required, message in ((False, 'no release selected'),
                                      (True, 'RELEASE is required and no default release is configured')):
                with self.subTest(required=required):
                    with self.assertRaises(common.ReconstructionError) as error:
                        common.release_entry(ROOT, None, require=required)
                    self.assertEqual(str(error.exception), message)

    def test_unrelated_correction_integrity_gate_remains(self):
        upstream = 'source/cache/release/upstream/edk2-202608/radxa-1.3.1'
        unofficial = 'source/cache/release/custom/edk2-202608/radxa-1.3.1/unofficial'
        with patch.object(common, 'matrix_release_branches', return_value=({upstream, unofficial}, {})), \
             patch.object(common, 'unofficial_correction_refs', side_effect=common.ReconstructionError('bad correction')):
            with self.assertRaisesRegex(common.ReconstructionError, 'bad correction'):
                common.release_entry(ROOT, upstream)


if __name__ == '__main__':
    unittest.main()
