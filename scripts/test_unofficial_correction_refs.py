#!/usr/bin/env python3
"""Additive corrections select exact source trees without moving checkpoints."""

from __future__ import annotations

import json
import os
from pathlib import Path
import tempfile
import unittest

from reconstruction_common import (
    ReconstructionError,
    active_unofficial_source_ref,
    clear_metadata_caches,
    source_target_ref_records,
    synthesise_release_entry,
    tree_id,
    unofficial_correction_ref,
    unofficial_rendered_tree,
    unofficial_source_ref,
)
from render_release_branch import render_from_plan
from test_support import commit_all, git, write_file


class UnofficialCorrectionRefsTests(unittest.TestCase):
    def setUp(self) -> None:
        root = os.environ.get("EDK2_CIX_TMP_ROOT")
        self.temp = tempfile.TemporaryDirectory(prefix="correction-refs-", dir=root)
        self.addCleanup(self.temp.cleanup)
        self.addCleanup(clear_metadata_caches)
        self.repo = Path(self.temp.name)
        git(self.repo, "init", "-b", "build")
        git(self.repo, "config", "user.name", "Correction Test")
        git(self.repo, "config", "user.email", "correction-test")
        write_file(self.repo, "VERSION", "1.2.4\n")
        write_file(self.repo, "debian/changelog", "edk2-cix (1.2.4) old\n")
        write_file(self.repo, "src/firmware.c", "int firmware = 1;\n")
        self.old = commit_all(self.repo, "original checkpoint")
        self.radxa = "1.2.4"
        self.edk2 = "edk2-stable202608"
        self.checkpoint = f"source/unofficial/{self.radxa}/{self.edk2}"
        self.current = "source/unofficial/1.2/current"
        self.correction = f"source/unofficial/corrections/{self.radxa}/{self.edk2}/cppc-v1"
        self.current_correction = self.correction + "-current"
        git(self.repo, "branch", self.checkpoint, self.old)
        git(self.repo, "branch", self.current, self.old)
        write_file(self.repo, "src/firmware.c", "int firmware = 2;\n")
        self.new = commit_all(self.repo, "CPPC source correction")
        git(self.repo, "branch", self.correction, self.new)
        git(self.repo, "branch", self.current_correction, self.new)
        write_file(self.repo, "debian/changelog", "edk2-cix (1.2.4) Radxa metadata\n")
        vendor = commit_all(self.repo, "vendor metadata")
        git(self.repo, "branch", f"source/port/radxa/{self.radxa}/{self.edk2}", vendor)
        self.record = {
            "ref": self.correction,
            "type": "unofficial-source-correction",
            "immutable": True,
            "radxa_release": self.radxa,
            "edk2_base": self.edk2,
            "corrects_ref": self.checkpoint,
            "corrects_object_id": self.old,
            "corrects_tree_id": tree_id(self.repo, self.old),
            "object_id": self.new,
            "tree_id": tree_id(self.repo, self.new),
        }
        self.current_record = {
            **self.record,
            "ref": self.current_correction,
            "corrects_ref": self.current,
        }
        self.write_manifest([self.record, self.current_record])
        write_file(self.repo, "config/policies.json", json.dumps({
            "unofficial_source_policy": {
                "default_line": "1.2",
                "lines": {"1.2": {
                    "current_ref": self.current,
                    "current_radxa_release": self.radxa,
                    "current_edk2_release": "202608",
                }},
            },
        }))
        self.target = "source/cache/release/custom/edk2-202608/radxa-1.2.4/unofficial"
        write_file(self.repo, "config/refs-source-target-cache.json", json.dumps({"refs": [{
            "ref": self.target,
            "tree_id": tree_id(self.repo, self.checkpoint),
            "type": "custom-firmware-source",
            "immutable": True,
        }]}))
        clear_metadata_caches()

    def write_manifest(self, records: list[dict], selected: list[str] | None = None) -> None:
        if selected is None:
            selected = [record["ref"] for record in records]
        write_file(self.repo, "config/refs-unofficial-corrections.json", json.dumps({
            "refs": records, "selected_refs": selected,
        }))
        clear_metadata_caches()

    def test_selects_exact_correction_for_checkpoint_and_active_line(self) -> None:
        self.assertEqual(unofficial_source_ref(self.repo, self.radxa, self.edk2), self.correction)
        self.assertEqual(active_unofficial_source_ref(self.repo, self.radxa, self.edk2), self.current_correction)
        self.assertEqual(unofficial_correction_ref(self.repo, self.radxa, self.edk2), self.correction)
        self.assertIsNone(unofficial_correction_ref(self.repo, "1.3.1", self.edk2))
        self.assertEqual(git(self.repo, "rev-parse", self.checkpoint).stdout.strip(), self.old)
        self.assertEqual(git(self.repo, "rev-parse", self.current).stdout.strip(), self.old)

    def test_render_plan_and_record_use_exact_corrected_tree(self) -> None:
        entry = synthesise_release_entry(self.repo, self.target)
        expected = unofficial_rendered_tree(
            self.repo, self.current_correction,
            f"source/port/radxa/{self.radxa}/{self.edk2}", self.radxa,
        )
        self.assertEqual(entry["render"]["base"]["ref"], self.current_correction)
        self.assertEqual(entry["tree_id"], expected)
        self.assertNotEqual(expected, tree_id(self.repo, self.current_correction))
        record = source_target_ref_records(self.repo)[self.target]
        self.assertEqual(record["tree_id"], expected)
        self.assertEqual(record["derived_from"], self.current_correction)
        rendered = render_from_plan(self.repo, self.target, entry, verbose=False)
        self.assertEqual(tree_id(self.repo, rendered), expected)

    def test_missing_or_unrecorded_ref_fails_closed(self) -> None:
        git(self.repo, "branch", "-D", self.correction)
        clear_metadata_caches()
        with self.assertRaisesRegex(ReconstructionError, "unavailable"):
            unofficial_source_ref(self.repo, self.radxa, self.edk2)
        self.write_manifest([self.current_record])
        git(self.repo, "branch", self.correction, self.new)
        clear_metadata_caches()
        with self.assertRaisesRegex(ReconstructionError, "unrecorded correction"):
            unofficial_source_ref(self.repo, self.radxa, self.edk2)

    def test_wrong_tuple_or_manifest_identity_fails_closed(self) -> None:
        cases = (
            ({"radxa_release": "1.3.1"}, "tuple"),
            ({"edk2_base": "edk2-stable202605"}, "tuple"),
            ({"corrects_ref": "source/unofficial/1.3.1/edk2-stable202608"}, "corrects_ref"),
            ({"corrects_object_id": "0" * 40}, "corrects_object_id"),
            ({"corrects_tree_id": "0" * 40}, "corrects_tree_id"),
            ({"object_id": "0" * 40}, "object or tree"),
            ({"tree_id": "0" * 40}, "object or tree"),
        )
        for changed, error in cases:
            with self.subTest(changed=changed):
                self.write_manifest([{**self.record, **changed}, self.current_record])
                with self.assertRaisesRegex(ReconstructionError, error):
                    unofficial_source_ref(self.repo, self.radxa, self.edk2)

    def test_ambiguous_correction_fails_closed(self) -> None:
        other = self.correction.removesuffix("cppc-v1") + "cppc-v2"
        git(self.repo, "branch", other, self.new)
        self.write_manifest([self.record, {**self.record, "ref": other}, self.current_record])
        with self.assertRaisesRegex(ReconstructionError, "ambiguous correction"):
            unofficial_source_ref(self.repo, self.radxa, self.edk2)

    def test_non_descendant_correction_fails_closed(self) -> None:
        unrelated = git(self.repo, "commit-tree", tree_id(self.repo, self.new), "-m", "unrelated").stdout.strip()
        git(self.repo, "update-ref", f"refs/heads/{self.correction}", unrelated)
        self.write_manifest([{**self.record, "object_id": unrelated}, self.current_record])
        with self.assertRaisesRegex(ReconstructionError, "not a descendant"):
            unofficial_source_ref(self.repo, self.radxa, self.edk2)

    def test_local_correction_identity_takes_precedence_over_stale_origin(self) -> None:
        git(self.repo, "update-ref", f"refs/remotes/origin/{self.correction}", self.old)
        clear_metadata_caches()
        self.assertEqual(unofficial_source_ref(self.repo, self.radxa, self.edk2), self.correction)
        # A correct remote-tracking copy cannot conceal a wrong local head.
        git(self.repo, "update-ref", f"refs/remotes/origin/{self.correction}", self.new)
        git(self.repo, "branch", "-f", self.correction, self.old)
        clear_metadata_caches()
        with self.assertRaisesRegex(ReconstructionError, "object or tree"):
            unofficial_source_ref(self.repo, self.radxa, self.edk2)

    def test_pending_local_current_advance_with_stale_origin(self) -> None:
        git(self.repo, "update-ref", f"refs/remotes/origin/{self.current}", self.old)
        advanced = self.advance_current()
        write_file(self.repo, "src/firmware.c", "int firmware = 2;\n")
        replacement = commit_all(self.repo, "correct pending current advance")
        replacement_ref = self.current_correction + "-pending"
        git(self.repo, "branch", replacement_ref, replacement)
        record = {
            **self.current_record,
            "ref": replacement_ref,
            "corrects_object_id": advanced,
            "corrects_tree_id": tree_id(self.repo, advanced),
            "object_id": replacement,
            "tree_id": tree_id(self.repo, replacement),
        }
        self.write_manifest([self.record, self.current_record, record], [self.correction, replacement_ref])
        self.assertEqual(active_unofficial_source_ref(self.repo, self.radxa, self.edk2), replacement_ref)
        self.write_manifest([self.record, self.current_record, {
            **record, "corrects_object_id": self.old,
            "corrects_tree_id": tree_id(self.repo, self.old),
        }], [self.correction, replacement_ref])
        with self.assertRaisesRegex(ReconstructionError, "corrects_object_id differs"):
            active_unofficial_source_ref(self.repo, self.radxa, self.edk2)

    def test_checkpoint_correction_cannot_replace_unmatched_active_source(self) -> None:
        git(self.repo, "branch", "-D", self.current_correction)
        self.write_manifest([self.record])
        self.assertEqual(unofficial_source_ref(self.repo, self.radxa, self.edk2), self.correction)
        with self.assertRaisesRegex(ReconstructionError, "none corrects active source"):
            active_unofficial_source_ref(self.repo, self.radxa, self.edk2)

    def advance_current(self) -> str:
        git(self.repo, "switch", "-c", "advance-test", self.old)
        write_file(self.repo, "src/active-feature.c", "int active_feature = 1;\n")
        advanced = commit_all(self.repo, "advance active line")
        git(self.repo, "branch", "-f", self.current, advanced)
        clear_metadata_caches()
        return advanced

    def test_retained_unselected_correction_survives_current_advancement(self) -> None:
        advanced = self.advance_current()
        self.write_manifest([self.record, self.current_record], [self.correction])
        self.assertEqual(unofficial_source_ref(self.repo, self.radxa, self.edk2), self.correction)
        # Selecting a checkpoint alone still cannot override the active source.
        with self.assertRaisesRegex(ReconstructionError, "none corrects active source"):
            active_unofficial_source_ref(self.repo, self.radxa, self.edk2)
        self.write_manifest([self.record, self.current_record], [])
        self.assertEqual(active_unofficial_source_ref(self.repo, self.radxa, self.edk2), self.current)
        self.assertEqual(git(self.repo, "rev-parse", self.current).stdout.strip(), advanced)

    def test_selected_stale_current_correction_is_rejected(self) -> None:
        self.advance_current()
        with self.assertRaisesRegex(ReconstructionError, "corrects_object_id differs"):
            active_unofficial_source_ref(self.repo, self.radxa, self.edk2)

    def test_replacement_selection_retains_old_correction(self) -> None:
        advanced = self.advance_current()
        write_file(self.repo, "src/firmware.c", "int firmware = 2;\n")
        replacement = commit_all(self.repo, "correct advanced source")
        replacement_ref = self.current_correction + "-v2"
        git(self.repo, "branch", replacement_ref, replacement)
        self.write_manifest([self.record, self.current_record, {
            **self.current_record,
            "ref": replacement_ref,
            "corrects_object_id": advanced,
            "corrects_tree_id": tree_id(self.repo, advanced),
            "object_id": replacement,
            "tree_id": tree_id(self.repo, replacement),
        }], [self.correction, replacement_ref])
        self.assertEqual(active_unofficial_source_ref(self.repo, self.radxa, self.edk2), replacement_ref)
        self.assertEqual(git(self.repo, "rev-parse", self.current_correction).stdout.strip(), self.new)

    def test_unselected_correction_still_checks_immutable_identity_and_ancestry(self) -> None:
        self.advance_current()
        for update, error in (
            ({"tree_id": "0" * 40}, "object or tree"),
            ({"corrects_tree_id": "0" * 40}, "corrects_tree_id"),
            ({"corrects_object_id": "0" * 40}, "available commit"),
            ({"corrects_object_id": self.new,
              "corrects_tree_id": tree_id(self.repo, self.new)}, "not a descendant"),
        ):
            with self.subTest(update=update):
                record = {**self.current_record, **update}
                if "corrects_object_id" in update and update["corrects_object_id"] == self.new:
                    record["object_id"] = self.old
                    record["tree_id"] = tree_id(self.repo, self.old)
                    git(self.repo, "branch", "-f", self.current_correction, self.old)
                self.write_manifest([self.record, record], [])
                with self.assertRaisesRegex(ReconstructionError, error):
                    unofficial_correction_ref(self.repo, self.radxa, self.edk2)

    def test_selected_current_tuple_must_match_policy(self) -> None:
        policy_path = self.repo / "config/policies.json"
        policy = json.loads(policy_path.read_text())
        policy["unofficial_source_policy"]["lines"]["1.2"]["current_radxa_release"] = "1.2.3"
        policy_path.write_text(json.dumps(policy))
        clear_metadata_caches()
        with self.assertRaisesRegex(ReconstructionError, "active tuple"):
            unofficial_correction_ref(self.repo, self.radxa, self.edk2)

    def test_unknown_duplicate_and_malformed_selections_fail_closed(self) -> None:
        for selected, error in (
            ([self.correction + "-missing"], "not recorded"),
            ([self.correction, self.correction], "duplicate selected"),
            ("not-a-list", "list of refs"),
        ):
            with self.subTest(selected=selected):
                self.write_manifest([self.record, self.current_record], selected)
                with self.assertRaisesRegex(ReconstructionError, error):
                    unofficial_correction_ref(self.repo, self.radxa, self.edk2)

    def test_remote_only_correction_selection_and_export_publication_discovery(self) -> None:
        from create_minimised_clone import required_refspecs
        from check_remote_source_coherence import expected_remote_refs
        from publish_source_update import verify_metadata
        from reconstruction_common import load_ref_records
        for ref in (self.checkpoint, self.current, self.correction, self.current_correction):
            oid = git(self.repo, "rev-parse", ref).stdout.strip()
            git(self.repo, "update-ref", f"refs/remotes/origin/{ref}", oid)
            git(self.repo, "branch", "-D", ref)
        clear_metadata_caches()
        self.assertEqual(active_unofficial_source_ref(self.repo, self.radxa, self.edk2), self.current_correction)
        exported = dict((target, source) for source, target in required_refspecs(self.repo))
        for ref in (self.correction, self.current_correction):
            self.assertEqual(exported[f"refs/heads/{ref}"], f"refs/remotes/origin/{ref}")
            self.assertEqual(expected_remote_refs(self.repo)[f"refs/heads/{ref}"], self.new)
        # Publication validates the same manifest records once exported as heads.
        git(self.repo, "branch", self.current_correction, self.new)
        self.assertEqual(verify_metadata(self.repo, f"refs/heads/{self.current_correction}", load_ref_records(self.repo)),
                         "config/refs-unofficial-corrections.json")
        commit_all(self.repo, "record correction selection")
        bare = self.repo / "export.git"
        clone = self.repo / "fresh-clone"
        git(self.repo, "init", "--bare", str(bare))
        specs = [f"{source}:{target}" for source, target in required_refspecs(self.repo)]
        git(self.repo, "push", str(bare), *specs)
        git(self.repo, "clone", "--branch", "build", str(bare), str(clone))
        self.assertEqual(active_unofficial_source_ref(clone, self.radxa, self.edk2), self.current_correction)
        self.assertEqual(unofficial_source_ref(clone, self.radxa, self.edk2), self.correction)
        self.assertEqual(git(clone, "show-ref", "--verify", "--quiet", f"refs/heads/{self.current_correction}", check=False).returncode, 1)

    def test_active_source_correction_preserves_its_other_changes(self) -> None:
        git(self.repo, "switch", "-c", "active-test", self.old)
        write_file(self.repo, "src/active-feature.c", "int active_feature = 1;\n")
        active_base = commit_all(self.repo, "unrelated active source change")
        git(self.repo, "branch", "-f", self.current, active_base)
        write_file(self.repo, "src/firmware.c", "int firmware = 2;\n")
        corrected_active = commit_all(self.repo, "CPPC correction on active source")
        git(self.repo, "branch", "-f", self.current_correction, corrected_active)
        self.write_manifest([self.record, {
            **self.current_record,
            "corrects_object_id": active_base,
            "corrects_tree_id": tree_id(self.repo, active_base),
            "object_id": corrected_active,
            "tree_id": tree_id(self.repo, corrected_active),
        }])
        self.assertEqual(
            active_unofficial_source_ref(self.repo, self.radxa, self.edk2),
            self.current_correction,
        )
        self.assertEqual(
            git(self.repo, "show", f"{self.current_correction}:src/active-feature.c").stdout,
            "int active_feature = 1;\n",
        )


if __name__ == "__main__":
    unittest.main()
