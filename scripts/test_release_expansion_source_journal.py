#!/usr/bin/env python3
"""Explicit source-journal caller checks, including registration provenance."""
from contextlib import ExitStack
from pathlib import Path
import unittest
from unittest.mock import patch

import release_expansion_source as source
from reconstruction_common import ReconstructionError
from radxa13_source_conflicts import RadxaSourceConflictError, INPUTS


class SourceJournalCallerTests(unittest.TestCase):
    repo = Path("/unused-private-batch")
    candidate = "a" * 40

    def prepare(self, resolutions, *, validator_error=None):
        def radxa_ref(_, radxa, base):
            if radxa == "1.3.1":
                raise ReconstructionError("vendor port missing")
            return f"source/port/radxa/{radxa}/{base}"

        records = [{"type": "vendor-source", "vendor": "radxa", "radxa_release": "1.2.4"}]
        with ExitStack() as stack:
            patches = {
                "finish_registration": {}, "check_immutable_refs": {},
                "ref_exists": {"return_value": False},
                "load_ref_records": {"return_value": records},
                "valid_source": {"side_effect": lambda _, e, r: "nearest-1.2.4" if r == "1.2.4" else None},
                "radxa_source_ref": {"side_effect": radxa_ref},
                "rev_parse": {"side_effect": lambda _, ref: ref},
                "enforce_source_tree_policy": {},
                "port_candidate": {"return_value": self.candidate},
                "validated_source_resolution": {"return_value": self.candidate, "side_effect": validator_error},
                "register_new": {"side_effect": RuntimeError("stop after registration")},
            }
            mocks = {name: stack.enter_context(patch.object(source, name, **kwargs))
                     for name, kwargs in patches.items()}
            try:
                source.prepare(self.repo, "202408.01", "1.3.1", self.repo / "registration.json", resolutions)
            except (RuntimeError, ReconstructionError) as exc:
                return mocks, exc
        self.fail("expected caller to reach registration or reject input")

    def test_journal_derives_oid_and_records_actual_merge_inputs(self):
        mocks, error = self.prepare({"port_source_journal": "/reviewed/source-resolution.json"})
        self.assertEqual(str(error), "stop after registration")
        mocks["validated_source_resolution"].assert_called_once_with(
            self.repo, Path("/reviewed/source-resolution.json"), "202408.01", "1.3.1")
        mocks["port_candidate"].assert_not_called()
        args = mocks["register_new"].call_args.args
        self.assertEqual(args[3], self.candidate)
        self.assertEqual(args[4]["ported_from"], INPUTS["new"][0])
        self.assertEqual(args[4]["vendor_delta_from"], INPUTS["old"][0])
        self.assertEqual(args[4]["vendor_delta_to"], INPUTS["source"][0])

    def test_optional_matching_port_ref_accepted(self):
        mocks, error = self.prepare({"port_source_journal": "/reviewed/journal", "port_ref": self.candidate})
        self.assertEqual(str(error), "stop after registration")
        mocks["register_new"].assert_called_once()

    def test_mismatched_port_ref_rejected_before_registration(self):
        mocks, error = self.prepare({"port_source_journal": "/reviewed/journal", "port_ref": "b" * 40})
        self.assertIsInstance(error, ReconstructionError)
        self.assertIn("differs", str(error))
        mocks["enforce_source_tree_policy"].assert_not_called()
        mocks["register_new"].assert_not_called()

    def test_invalid_journal_rejected_before_registration(self):
        for error in (RadxaSourceConflictError("wrong pair"), OSError("missing receipt"), ValueError("malformed receipt")):
            with self.subTest(error=type(error).__name__):
                mocks, raised = self.prepare({"port_source_journal": "/reviewed/journal"}, validator_error=error)
                self.assertIsInstance(raised, ReconstructionError)
                self.assertIn("source journal rejected", str(raised))
                mocks["register_new"].assert_not_called()

    def test_manual_port_behavior_remains_available(self):
        mocks, error = self.prepare({"port_ref": self.candidate})
        self.assertEqual(str(error), "stop after registration")
        mocks["validated_source_resolution"].assert_not_called()
        metadata = mocks["register_new"].call_args.args[4]
        self.assertEqual(metadata["ported_from"], "source/port/radxa/1.2.4/edk2-stable202408.01")
        self.assertNotIn("vendor_delta_from", metadata)


if __name__ == "__main__":
    unittest.main()
