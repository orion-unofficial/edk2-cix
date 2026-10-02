#!/usr/bin/env python3
"""Verify O6 normalization runs only for new Radxa 1.3 candidates."""

from contextlib import ExitStack
from pathlib import Path
import unittest
from unittest.mock import patch

import release_expansion_source as source


class FdfPreparationTests(unittest.TestCase):
    repo = Path("/unused-private-batch")

    def prepare_until_validation(self, radxa, existing=False):
        calls = []

        def record(name, result=None):
            def called(*args, **kwargs):
                calls.append((name, args, kwargs))
                if name == "validate":
                    raise RuntimeError("stop before checkpoint registration")
                return result
            return called

        with ExitStack() as stack:
            for name, value in {
                "finish_registration": None, "check_immutable_refs": None,
                "valid_source": "retained" if existing else None,
                "ref_exists": existing, "load_ref_records": [],
                "matrix_release_values": ["202605", "202608"],
                "radxa_source_ref": "port", "enforce_source_tree_policy": None,
                "unofficial_seed": ("202605", radxa, "seed"),
                "validated_dsdt_resolution": None,
                "apply_source_delta_to_base": "replayed",
                "align_release_metadata": "aligned",
            }.items():
                stack.enter_context(patch.object(source, name, return_value=value))
            stack.enter_context(patch.object(source, "adapt_source_libraries",
                                             side_effect=record("adapt", "adapted")))
            normalize = stack.enter_context(
                patch.object(source, "normalize_source_fdf", side_effect=record("normalize", "normalized")))
            register = stack.enter_context(patch.object(source, "register_new"))
            stack.enter_context(patch.object(source, "validate_source_libraries",
                                             side_effect=record("validate")))
            with self.assertRaisesRegex(RuntimeError, "stop before checkpoint registration"):
                source.prepare(self.repo, "202608", radxa, self.repo / "journal.json", {})
            register.assert_not_called()
            return calls, normalize.call_args_list

    def test_new_13_candidate_is_normalized_after_library_adaptation(self):
        for radxa in ("1.3.0", "1.3.1"):
            with self.subTest(radxa=radxa):
                calls, _ = self.prepare_until_validation(radxa)
                self.assertEqual([name for name, _, _ in calls], ["adapt", "normalize", "validate"])
                self.assertEqual(calls[1][1], (self.repo, "adapted", "202608", radxa))
                self.assertEqual(calls[2][1], (self.repo, "normalized"))

    def test_retained_checkpoint_is_validated_without_normalization(self):
        calls, normalize_calls = self.prepare_until_validation("1.3.1", existing=True)
        self.assertEqual(normalize_calls, [])
        self.assertEqual(calls, [("validate", (self.repo, "retained"), {})])

    def test_new_12_candidate_keeps_its_existing_fdf_policy(self):
        calls, normalize_calls = self.prepare_until_validation("1.2.4")
        self.assertEqual(normalize_calls, [])
        self.assertEqual([name for name, _, _ in calls], ["adapt", "validate"])
        self.assertEqual(calls[-1][1], (self.repo, "adapted"))


if __name__ == "__main__":
    unittest.main()
