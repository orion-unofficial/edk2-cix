#!/usr/bin/env python3
"""Historical trust catalogue checks against independent repository snapshots."""

from copy import deepcopy
import hashlib
import json
from pathlib import Path
import unittest

from check_source_build_inputs import chain_validator_problems, source_catalogue_provenance
from firmware_trust_audit import PACKAGE_INPUTS, compatible_reference, parse_catalogue, require_provenance
from reconstruction_common import ReconstructionError, show_file
from validate_firmware_chain import CIX_KEY_SHA256, CIX_SIGNING_KEYS, PACKAGE


ROOT = Path(__file__).resolve().parents[1]
HISTORICAL = "source/unofficial/1.3.1/edk2-stable202608"
UPLIFTED = "source/unofficial/1.3.2/edk2-stable202608"
EMBEDDED = "src/scripts/firmware-trust.json"


def package_fingerprints(ref):
    return tuple(hashlib.sha256(show_file(ROOT, ref, f"{PACKAGE}/{path}")).hexdigest()
                 for path in PACKAGE_INPUTS)


def catalogue(document):
    return parse_catalogue(json.dumps(document).encode())


class FirmwareTrustAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.historical = parse_catalogue(show_file(ROOT, HISTORICAL, EMBEDDED))
        cls.current_bytes = (ROOT / "config/firmware-trust.json").read_bytes()
        cls.current = parse_catalogue(cls.current_bytes)
        cls.old_inputs = package_fingerprints(HISTORICAL)
        cls.new_inputs = package_fingerprints(UPLIFTED)

    def setUp(self):
        self.document = json.loads(self.current_bytes)
        self.reference = next(
            row for row in self.document["references"]
            if row["bl1_sha256"] == self.old_inputs[0]
        )
        self.layout = next(
            layout for layout in self.reference["flash_layouts"]
            if layout["config_sha256"] == self.old_inputs[-1]
        )

    def test_verified_extension_preserves_historical_snapshot(self):
        self.assertNotEqual(self.historical, self.current)
        provenance = compatible_reference(self.historical, self.current, self.old_inputs)
        require_provenance(provenance, *source_catalogue_provenance(ROOT, HISTORICAL, {}))

    def test_additional_reference_is_allowed(self):
        older = json.loads(show_file(ROOT, HISTORICAL, EMBEDDED))
        older["references"].pop(0)
        self.assertTrue(compatible_reference(catalogue(older), self.current, self.old_inputs))

    def test_uplifted_checkpoint_requires_its_layout_and_provenance(self):
        data = show_file(ROOT, UPLIFTED, EMBEDDED)
        self.assertEqual(self.new_inputs[-1], "475272613851746e400b69580200d44456b2e05684fb29e6b5305eddbfc688d0")
        provenance = compatible_reference(parse_catalogue(data), self.current, self.new_inputs)
        source = source_catalogue_provenance(ROOT, UPLIFTED, {})
        self.assertEqual(source, ("source/vendor/radxa/1.3.2/edk2-stable202208",
                                  "638c33eb98c358aab0c025bb1b06a359027e5ff2"))
        require_provenance(provenance, *source)
        with self.assertRaisesRegex(ReconstructionError, "lacks source flash layout"):
            compatible_reference(self.historical, self.current, self.new_inputs)
        omitted = json.loads(data)
        row = next(row for row in omitted["references"] if row["bl1_sha256"] == self.new_inputs[0])
        row["provenance"] = [record for record in row["provenance"] if record["ref"] != source[0]]
        incomplete = compatible_reference(catalogue(omitted), self.current, self.new_inputs)
        with self.assertRaisesRegex(ReconstructionError, "lacks pinned source provenance"):
            require_provenance(incomplete, *source)

    def test_uplifted_verifier_has_mandatory_executable_identity(self):
        current = (ROOT / "scripts/validate_firmware_chain.py").read_bytes()
        vendor = show_file(ROOT, UPLIFTED, "src/scripts/validate_firmware_chain.py")
        helper = show_file(ROOT, UPLIFTED, "src/scripts/build_cix_release_bootloader2.sh")
        keys = {name: show_file(ROOT, UPLIFTED, f"{CIX_SIGNING_KEYS}/{name}") for name in CIX_KEY_SHA256}
        self.assertEqual(vendor, current)
        self.assertEqual(chain_validator_problems(vendor, keys, helper, current), [])
        self.assertTrue(chain_validator_problems(vendor + b"\n", keys, helper, current))

    def test_historical_security_pin_mutation_and_removal_are_rejected(self):
        for field, changed in (
            ("bl1_sha256", "a" * 64),
            ("trusted_fip_sha256", "a" * 64),
            ("uefi_certificate_sha256", "a" * 64),
            ("trusted_root_spki_sha256", "a" * 64),
            ("uefi_root_spki_sha256", "a" * 64),
            ("uefi_oem_spki_sha256", "a" * 64),
            ("trusted_counter", 30),
            ("uefi_counter", 222),
            ("reboot_scratch_layout", 1),
        ):
            with self.subTest(field=field):
                old_value = self.reference[field]
                self.reference[field] = changed
                with self.assertRaisesRegex(ReconstructionError, "historical trust"):
                    compatible_reference(self.historical, catalogue(self.document), self.old_inputs)
                del self.reference[field]
                with self.assertRaisesRegex(ReconstructionError, "invalid reference fields"):
                    catalogue(self.document)
                self.reference[field] = old_value

    def test_historical_layout_and_provenance_mutation_and_removal_are_rejected(self):
        for node, key, replacement in (
            (self.layout, "config_sha256", "a" * 64),
            (self.layout["entries"]["7"], "size", self.layout["entries"]["7"]["size"] - 1),
            (self.layout["entries"]["7"], "address", self.layout["entries"]["7"]["address"] + 1),
            (self.reference["provenance"][0], "commit", "a" * 40),
        ):
            with self.subTest(key=key):
                previous = node[key]
                node[key] = replacement
                with self.assertRaisesRegex(ReconstructionError, "missing or changed historical"):
                    compatible_reference(self.historical, catalogue(self.document), self.old_inputs)
                node[key] = previous

        for collection, expected in (("flash_layouts", "flash layout"), ("provenance", "provenance")):
            old_record = self.layout if collection == "flash_layouts" else self.reference[collection][0]
            self.reference[collection].remove(old_record)
            with self.subTest(collection=collection), self.assertRaisesRegex(ReconstructionError, expected):
                compatible_reference(self.historical, catalogue(self.document), self.old_inputs)
            self.reference[collection].append(old_record)

    def test_duplicate_records_and_contradictory_layouts_are_rejected(self):
        for collection, pattern in (
            (self.document["references"], "duplicate trust reference"),
            (self.reference["flash_layouts"], "duplicate flash layout"),
            (self.reference["provenance"], "duplicate vendor provenance"),
        ):
            collection.append(deepcopy(collection[0]))
            with self.subTest(pattern=pattern), self.assertRaisesRegex(ReconstructionError, pattern):
                catalogue(self.document)
            collection.pop()
        # The first two real references share the same layout digest and geometry.
        row = self.document["references"][0]
        row["flash_layouts"][0]["entries"]["7"]["size"] -= 1
        with self.assertRaisesRegex(ReconstructionError, "contradictory flash layout"):
            catalogue(self.document)

    def test_malformed_catalogues_and_unknown_schema_are_rejected(self):
        for data, expected in (
            (b"{", "malformed catalogue JSON"),
            (b"[]", "invalid catalogue fields"),
            (b'{"nested":{"size":1,"size":2}}', "duplicate JSON key"),
        ):
            with self.subTest(data=data), self.assertRaisesRegex(ReconstructionError, expected):
                parse_catalogue(data)

        for node, key, value, expected in (
            (self.document, "schema_version", 2, "unsupported catalogue schema"),
            (self.document, "schema_version", True, "unsupported catalogue schema"),
            (self.document, "future_field", {}, "invalid catalogue fields"),
            (self.reference, "new_signing_identity", "a" * 64, "invalid reference fields"),
            (self.reference, "trusted_counter", True, "invalid rollback counter"),
            (self.layout, "entries", [], "missing required flash entries"),
            (self.layout["entries"]["7"], "size", -1, "invalid flash entry allocation"),
        ):
            previous = node.get(key)
            node[key] = value
            with self.subTest(key=key), self.assertRaisesRegex(ReconstructionError, expected):
                catalogue(self.document)
            if previous is None:
                del node[key]
            else:
                node[key] = previous


if __name__ == "__main__":
    unittest.main()
