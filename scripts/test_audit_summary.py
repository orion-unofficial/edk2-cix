#!/usr/bin/env python3
"""Audit summaries remain deterministic and do not change either audit gate."""
import contextlib
import io
import unittest

from audit_summary import changes, print_summary
from audit_acpi_regression import compare_audits
from audit_final_image_manifest import compare_manifests


class AuditSummaryTests(unittest.TestCase):
    def test_guid_keyed_modules_do_not_turn_insertions_into_replacements(self):
        old = {'modules': [{'guid': 'a', 'size': 12}, {'guid': 'b', 'size': 20}]}
        new = {'modules': [{'guid': 'c', 'size': 15}, {'guid': 'b', 'size': 24}, {'guid': 'a', 'size': 12}]}
        self.assertEqual(list(changes(old, new)), ['modules/b/size: 20 -> 24', 'modules/c: added'])
        self.assertTrue(compare_manifests(old, new))

    def test_table_hash_change_is_visible_even_with_identical_size(self):
        old = {'tables': {'Pptt.acpi': {'size': 100, 'sha256': 'old'}}}
        new = {'tables': {'Pptt.acpi': {'size': 100, 'sha256': 'new'}}}
        self.assertEqual(list(changes(old, new)), ["tables/Pptt.acpi/sha256: 'old' -> 'new'"])
        self.assertTrue(compare_audits(old, new))

    def test_ignored_manifest_hashes_remain_ignored(self):
        old = {'fd': {'size': 100, 'sha256': 'old'}}
        new = {'fd': {'size': 100, 'sha256': 'new'}}
        self.assertEqual(list(changes(old, new, ignore=frozenset({'sha256'}))), [])
        self.assertEqual(compare_manifests(old, new), [])

    def test_output_is_bounded_but_complete_details_remain_available(self):
        details = list(changes({str(i): i for i in range(30)}, {}))
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            print_summary(details)
        text = output.getvalue()
        self.assertEqual(text.count('  - '), 12)
        self.assertIn('18 more', text)
        self.assertIn('--report-json', text)
        self.assertEqual(len(details), 30)

    def test_duplicates_are_not_lost_by_guid_mapping(self):
        self.assertEqual(list(changes([{'guid': 'a'}, {'guid': 'a'}], [{'guid': 'a'}])), ['/count: 2 -> 1'])


if __name__ == '__main__':
    unittest.main()
