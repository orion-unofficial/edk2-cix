#!/usr/bin/env python3
"""Identity checks for explicitly final unofficial source resolutions."""

from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import release_expansion_source as source
from reconstruction_common import ReconstructionError


class FinalResolutionTests(unittest.TestCase):
    repo = Path("/unused-private-batch")
    resolved = "a" * 40
    tree = "b" * 40
    parent = "c" * 40
    parent_port_oid = "d" * 40
    destination_port_oid = "e" * 40
    parent_ref = "source/unofficial/1.3.1/edk2-stable202208"
    parent_port = "source/vendor/radxa/1.3.1/edk2-stable202208"
    destination_port = "source/vendor/radxa/1.3.0/edk2-stable202208"
    destination_source = "source/unofficial/1.3.0/edk2-stable202208"

    def bindings(self):
        return {
            "unofficial_final_commit": self.resolved,
            "unofficial_final_tree": self.tree,
            "unofficial_final_parent_ref": self.parent_ref,
            "unofficial_final_parent_commit": self.parent,
            "unofficial_final_parent_port_ref": self.parent_port,
            "unofficial_final_parent_port_commit": self.parent_port_oid,
            "unofficial_final_destination_port_ref": self.destination_port,
            "unofficial_final_destination_port_commit": self.destination_port_oid,
            "unofficial_final_destination_source_ref": self.destination_source,
        }

    def validate(self, bindings, *, parent_record_port=None):
        refs = {self.parent_ref: self.parent, self.parent_port: self.parent_port_oid,
                self.destination_port: self.destination_port_oid}
        record = {"ref": self.parent_ref, "type": "unofficial-release-checkpoint",
                  "radxa_source_ref": parent_record_port or self.parent_port}
        with patch.object(source, "tree_id", return_value=self.tree), patch.object(
                source, "git", return_value=SimpleNamespace(stdout=f"{self.resolved} {self.parent}\n")), patch.object(
                    source, "rev_parse", side_effect=lambda _, ref: refs.get(ref, "f" * 40)), patch.object(
                        source, "load_ref_records", return_value=[record]):
            return source.validated_final_resolution(
                self.repo, bindings, self.resolved, self.destination_port, self.destination_source)

    def test_valid_bindings_return_actual_parent_source_and_port(self):
        self.assertEqual(self.validate(self.bindings()), (self.parent_ref, self.parent_port))

    def test_every_missing_or_changed_binding_fails_closed(self):
        original = self.bindings()
        for key in original:
            with self.subTest(key=key, change="missing"):
                row = dict(original)
                del row[key]
                with self.assertRaises(ReconstructionError):
                    self.validate(row)
            with self.subTest(key=key, change="changed"):
                row = dict(original)
                row[key] = "f" * 40 if key.endswith(("commit", "tree")) else "source/other"
                with self.assertRaises(ReconstructionError):
                    self.validate(row)

    def test_parent_manifest_port_must_match_reviewed_port(self):
        with self.assertRaisesRegex(ReconstructionError, "parent port differs"):
            self.validate(self.bindings(), parent_record_port="source/vendor/radxa/other")

    def test_prepare_final_provenance_uses_reviewed_parent(self):
        seed = "source/unofficial/1.2.4/edk2-stable202208"
        records = [{"type": "vendor-source", "vendor": "radxa", "radxa_release": "1.2.4"}]
        with patch.object(source, "finish_registration"), patch.object(
                source, "check_immutable_refs"), patch.object(
                    source, "ref_exists", return_value=False), patch.object(
                        source, "valid_source", side_effect=lambda _, e, r: seed if r == "1.2.4" else None), patch.object(
                            source, "load_ref_records", return_value=records), patch.object(
                                source, "matrix_release_values", return_value=["202208"]), patch.object(
                                    source, "radxa_source_ref", side_effect=lambda _, r, b: f"source/vendor/radxa/{r}/{b}"), patch.object(
                                        source, "rev_parse", return_value=self.resolved), patch.object(
                                            source, "resolved_source_port_stage", return_value="final"), patch.object(
                                                source, "validated_final_resolution",
                                                return_value=(self.parent_ref, self.parent_port)), patch.object(
                                                    source, "resume_source_delta_tree", return_value=self.tree), patch.object(
                                                        source, "git", return_value=SimpleNamespace(stdout=self.resolved)) as git, patch.object(
                                                            source, "align_release_metadata",
                                                            side_effect=RuntimeError("stop after provenance")):
            with self.assertRaisesRegex(RuntimeError, "stop after provenance"):
                source.prepare(self.repo, "202208", "1.3.0", self.repo / "journal.json",
                               {"unofficial_ref": self.resolved, "unofficial_stage": "final"})
        message = git.call_args.args[-1]
        self.assertIn(f"Source-Unofficial-From: {self.parent_ref}\n", message)
        self.assertIn(f"Source-Port-From: {self.parent_port}\n", message)
        self.assertNotIn(f"Source-Unofficial-From: {seed}\n", message)


if __name__ == "__main__":
    unittest.main()
