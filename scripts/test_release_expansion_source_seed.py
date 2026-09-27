#!/usr/bin/env python3
"""Focused source seed selection checks for Radxa 1.3.x expansion."""

from pathlib import Path
import unittest
from unittest.mock import patch

import release_expansion_source as source
from reconstruction_common import ReconstructionError


class UnofficialSeedTests(unittest.TestCase):
    repo = Path("/unused-private-batch")
    vendor_seed = ("202408", "1.2.4", "source/unofficial/1.2.4/edk2-stable202408")

    def test_structural_gate_rejects_13_regressions_and_unsupported_profiles(self):
        with patch.object(source, "validate_radxa13_source",
                          return_value=["unsupported structural profile"]) as validate:
            with self.assertRaisesRegex(ReconstructionError, "unsupported structural profile"):
                source.validate_structural_source(self.repo, "candidate", "202211", "1.3.0")
            validate.assert_called_once_with(self.repo, "candidate", "202211", "1.3.0")
            validate.reset_mock()
            source.validate_structural_source(self.repo, "candidate", "202211", "1.2.4")
            validate.assert_not_called()

    def test_nearest_same_radxa_checkpoint_precedes_vendor_seed(self):
        checkpoints = {
            ("202308", "1.3.0"): "source/unofficial/1.3.0/edk2-stable202308",
            ("202408", "1.3.0"): "source/unofficial/1.3.0/edk2-stable202408",
        }
        with patch.object(source, "matrix_release_values",
                          return_value=["202208", "202308", "202408", "202508"]), patch.object(
                              source, "valid_source", side_effect=lambda _, e, r: checkpoints.get((e, r))) as valid:
            seed = source.unofficial_seed(self.repo, "202508", "1.3.0", self.vendor_seed)
        self.assertEqual(seed, ("202408", "1.3.0", checkpoints[("202408", "1.3.0")]))
        valid.assert_called_once_with(self.repo, "202408", "1.3.0")

    def test_invalid_nearest_source_is_skipped(self):
        def entry(_, branch):
            return {"source_ref": branch}

        with patch.object(source, "matrix_release_values",
                          return_value=["202208", "202308", "202408"]), patch.object(
                              source, "synthesise_release_entry", side_effect=entry), patch.object(
                                  source, "input_problems",
                                  side_effect=lambda _, row: ["invalid"] if "202408" in row["source_ref"] else []):
            seed = source.unofficial_seed(self.repo, "202508", "1.3.1", self.vendor_seed)
        self.assertEqual(seed, ("202308", "1.3.1",
                                source.release_to_branch(source.target("202308", "1.3.1"))))

    def test_fallback_and_12x_behavior(self):
        with patch.object(source, "matrix_release_values",
                          return_value=["202208", "202308"]), patch.object(
                              source, "valid_source", return_value=None) as valid:
            self.assertEqual(source.unofficial_seed(self.repo, "202408", "1.3.0", self.vendor_seed),
                             self.vendor_seed)
            self.assertIsNone(source.unofficial_seed(self.repo, "202408", "1.3.1", None))
            valid.assert_called()
            valid.reset_mock()
            self.assertEqual(source.unofficial_seed(self.repo, "202408", "1.2.4", self.vendor_seed),
                             self.vendor_seed)
            valid.assert_not_called()

    def test_invalid_existing_checkpoint_still_requires_review(self):
        with patch.object(source, "finish_registration"), patch.object(
                source, "check_immutable_refs"), patch.object(
                    source, "valid_source", return_value=None), patch.object(
                        source, "ref_exists", return_value=True), patch.object(
                            source, "register_new") as register:
            with self.assertRaisesRegex(ReconstructionError, "existing checkpoint fails provenance"):
                source.prepare(self.repo, "202408", "1.3.0", self.repo / "journal.json", {})
            register.assert_not_called()

    def test_prepare_keeps_vendor_port_seed_separate_from_custom_seed(self):
        def radxa_ref(_, radxa, base):
            if (radxa, base) == ("1.3.0", "edk2-stable202508"):
                raise ReconstructionError("vendor port needed")
            return f"source/vendor/radxa/{radxa}/{base}"

        candidates = {
            ("202508", "1.2.4"): self.vendor_seed[2],
            ("202408", "1.3.0"): "source/unofficial/1.3.0/edk2-stable202408",
        }
        records = [{"type": "vendor-source", "vendor": "radxa", "radxa_release": r}
                   for r in ("1.2.4", "1.3.0")]
        with patch.object(source, "finish_registration"), patch.object(
                source, "check_immutable_refs"), patch.object(
                    source, "ref_exists", return_value=False), patch.object(
                        source, "load_ref_records", return_value=records), patch.object(
                            source, "matrix_release_values", return_value=["202408", "202508"]), patch.object(
                                source, "valid_source", side_effect=lambda _, e, r: candidates.get((e, r))), patch.object(
                                    source, "radxa_source_ref", side_effect=radxa_ref), patch.object(
                                        source, "port_candidate", return_value="vendor-port-oid") as port, patch.object(
                                            source, "enforce_source_tree_policy"), patch.object(
                                                source, "register_new"), patch.object(
                                                    source, "apply_source_delta_to_base",
                                                    side_effect=RuntimeError("stop after seed selection")) as custom:
            with self.assertRaisesRegex(RuntimeError, "stop after seed selection"):
                source.prepare(self.repo, "202508", "1.3.0", self.repo / "journal.json", {})
        self.assertEqual(port.call_args.kwargs["from_release"], "1.2.4")
        self.assertEqual(port.call_args.kwargs["to_release"], "1.3.0")
        self.assertEqual(custom.call_args.kwargs["old_base_ref"],
                         "source/vendor/radxa/1.3.0/edk2-stable202408")
        self.assertEqual(custom.call_args.kwargs["source_ref"], candidates[("202408", "1.3.0")])
        self.assertEqual(custom.call_args.kwargs["new_base_ref"],
                         "source/port/radxa/1.3.0/edk2-stable202508")


if __name__ == "__main__":
    unittest.main()
