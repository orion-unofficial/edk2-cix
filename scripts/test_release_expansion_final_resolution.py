#!/usr/bin/env python3
"""Identity checks for explicitly final unofficial source resolutions."""

import json
import os
import tempfile
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import release_expansion_source as source
from reconstruction_common import ReconstructionError, clear_metadata_caches, tree_id
from test_support import commit_all, git, write_file


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


class CorrectionFinalResolutionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="final-correction-", dir=os.environ.get("EDK2_CIX_TMP_ROOT"))
        self.addCleanup(self.temp.cleanup)
        self.addCleanup(clear_metadata_caches)
        self.repo = Path(self.temp.name)
        git(self.repo, "init", "-b", "build")
        git(self.repo, "config", "user.name", "Final Correction Test")
        git(self.repo, "config", "user.email", "final-correction-test")
        write_file(self.repo, "firmware.c", "original\n")
        self.original = commit_all(self.repo, "original checkpoint")
        self.checkpoint = "source/unofficial/1.3.1/edk2-stable202608"
        self.correction = "source/unofficial/corrections/1.3.1/edk2-stable202608/cppc-v1"
        self.port = "source/port/radxa/1.3.1/edk2-stable202608"
        self.destination = "source/port/radxa/1.3.0/edk2-stable202608"
        self.target = "source/unofficial/1.3.0/edk2-stable202608"
        for ref in (self.checkpoint, self.port, self.destination):
            git(self.repo, "branch", ref, self.original)
        write_file(self.repo, "firmware.c", "corrected\n")
        self.corrected = commit_all(self.repo, "CPPC correction")
        git(self.repo, "branch", self.correction, self.corrected)
        write_file(self.repo, "radxa", "1.3.0\n")
        self.final = commit_all(self.repo, "reviewed backport")
        self.original_record = {
            "ref": self.checkpoint, "type": "unofficial-release-checkpoint",
            "object_id": self.original, "tree_id": tree_id(self.repo, self.original),
            "radxa_release": "1.3.1", "edk2_base": "edk2-stable202608",
            "radxa_source_ref": self.port,
        }
        self.correction_record = {
            "ref": self.correction, "type": "unofficial-source-correction", "immutable": True,
            "object_id": self.corrected, "tree_id": tree_id(self.repo, self.corrected),
            "radxa_release": "1.3.1", "edk2_base": "edk2-stable202608",
            "corrects_ref": self.checkpoint, "corrects_object_id": self.original,
            "corrects_tree_id": tree_id(self.repo, self.original),
        }
        self.write_metadata()
        self.bindings = {
            "unofficial_final_commit": self.final, "unofficial_final_tree": tree_id(self.repo, self.final),
            "unofficial_final_parent_ref": self.correction, "unofficial_final_parent_commit": self.corrected,
            "unofficial_final_parent_port_ref": self.port, "unofficial_final_parent_port_commit": self.original,
            "unofficial_final_destination_port_ref": self.destination,
            "unofficial_final_destination_port_commit": self.original,
            "unofficial_final_destination_source_ref": self.target,
        }

    def write_metadata(self, selected=True):
        write_file(self.repo, "config/refs-unofficial.json", json.dumps({"refs": [self.original_record]}))
        write_file(self.repo, "config/refs-unofficial-corrections.json", json.dumps({
            "refs": [self.correction_record], "selected_refs": [self.correction] if selected else [],
        }))
        clear_metadata_caches()

    def validate(self):
        return source.validated_final_resolution(self.repo, self.bindings, self.final, self.destination, self.target)

    def test_selected_correction_can_bind_lifecycle_ownership_baseline(self):
        from source_lifecycle import validated_source_base
        write_file(self.repo, "config/refs-radxa.json", json.dumps({"refs": [{
            "ref": self.port, "type": "ported-vendor-source",
            "object_id": self.original, "tree_id": tree_id(self.repo, self.original),
        }]}))
        clear_metadata_caches()
        self.assertEqual(validated_source_base(self.repo, self.correction, self.port), self.port)
        self.write_metadata(selected=False)
        with self.assertRaisesRegex(ReconstructionError, "not selected"):
            validated_source_base(self.repo, self.correction, self.port)

    def test_selected_correction_inherits_exact_original_checkpoint_port(self):
        self.assertEqual(self.validate(), (self.correction, self.port))

    def test_unselected_correction_cannot_supply_final_parent(self):
        self.write_metadata(selected=False)
        with self.assertRaisesRegex(ReconstructionError, "not selected"):
            self.validate()

    def test_original_checkpoint_identity_tuple_and_port_are_required(self):
        original = dict(self.original_record)
        for field, value, error in (
            ("object_id", "0" * 40, "original provenance"),
            ("tree_id", "0" * 40, "original provenance"),
            ("radxa_release", "1.3.0", "original provenance"),
            ("edk2_base", "edk2-stable202605", "original provenance"),
            ("radxa_source_ref", self.destination, "parent port differs"),
            ("type", "unofficial-line-tip", "parent port differs"),
        ):
            with self.subTest(field=field):
                self.original_record = {**original, field: value}
                self.write_metadata()
                with self.assertRaisesRegex(ReconstructionError, error):
                    self.validate()

    def test_moved_correction_and_missing_original_ref_fail_closed(self):
        git(self.repo, "branch", "-f", self.correction, self.original)
        clear_metadata_caches()
        with self.assertRaisesRegex(ReconstructionError, "parent differs"):
            self.validate()
        git(self.repo, "branch", "-f", self.correction, self.corrected)
        git(self.repo, "branch", "-D", self.checkpoint)
        clear_metadata_caches()
        with self.assertRaisesRegex(ReconstructionError, "unavailable"):
            self.validate()


if __name__ == "__main__":
    unittest.main()
