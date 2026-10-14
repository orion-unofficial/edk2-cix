#!/usr/bin/env python3
"""Check vendor StandaloneMM selection, corruption rejection and embedding."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zlib

from firmware_chain import parse_fip
from reconstruction_common import show_file
from stage_vendor_stmm import StmmError, load_payload, sha256, verify_embedded, write_changed


ROOT = Path(__file__).resolve().parents[1]
PAYLOADS = "src/cix-v1.2/release-payloads/standalone-mm/"
PACKAGE = "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/"


def source_bytes(path):
    selected = os.environ.get("SOURCE_TEST_ROOT")
    if selected:
        return (Path(selected) / path).read_bytes()
    return show_file(ROOT, os.environ.get("SOURCE_TEST_REF", "source/unofficial/1.3/current"), path)


class VendorStmmTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="vendor-stmm-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.manifest = self.root / "manifest.json"
        self.manifest.write_bytes(source_bytes(PAYLOADS + "manifest.json"))
        self.entries = json.loads(self.manifest.read_text())["images"]
        for entry in self.entries:
            (self.root / entry["compressed_file"]).write_bytes(source_bytes(PAYLOADS + entry["compressed_file"]))
        self.vendor = self.root / "vendor.img"

    def select(self, version):
        self.vendor.write_bytes(show_file(ROOT, f"source/vendor/radxa/{version}/edk2-stable202208",
                                          PACKAGE + "Firmwares/bootloader2.img"))
        return load_payload(self.manifest, self.vendor)

    def test_both_checkpoint_payloads_and_stock_embedding(self):
        for version in ("1.2.4", "1.3.1"):
            with self.subTest(version=version):
                entry, image = self.select(version)
                self.assertEqual(entry["radxa_release"], version)
                self.assertEqual(sha256(image), entry["fd_sha256"])
                tee = parse_fip(self.vendor.read_bytes())["tos-fw"]
                self.assertEqual(verify_embedded(tee, image), entry["vendor_tee_compressed_offset"])

    def test_unknown_fip_and_corrupt_or_missing_blob_fail_closed(self):
        entry, _ = self.select("1.3.1")
        self.vendor.write_bytes(b"not the pinned stock firmware")
        with self.assertRaisesRegex(StmmError, "exactly one pinned"):
            load_payload(self.manifest, self.vendor)
        self.select("1.3.1")
        blob = self.root / entry["compressed_file"]
        data = bytearray(blob.read_bytes())
        data[-1] ^= 1
        blob.write_bytes(data)
        with self.assertRaisesRegex(StmmError, "compressed hash"):
            load_payload(self.manifest, self.vendor)
        blob.unlink()
        with self.assertRaisesRegex(StmmError, "missing"):
            load_payload(self.manifest, self.vendor)

    def test_trailing_compressed_data_is_rejected_even_with_updated_hash(self):
        entry, _ = self.select("1.3.1")
        blob = self.root / entry["compressed_file"]
        blob.write_bytes(blob.read_bytes() + b"trailing data")
        document = json.loads(self.manifest.read_text())
        for row in document["images"]:
            if row["radxa_release"] == "1.3.1":
                row["compressed_sha256"] = sha256(blob.read_bytes())
        self.manifest.write_text(json.dumps(document))
        with self.assertRaisesRegex(StmmError, "trailing"):
            load_payload(self.manifest, self.vendor)

    def test_stage_preserves_unchanged_mtime_and_repairs_wrong_bytes(self):
        _, image = self.select("1.3.1")
        output = self.root / "BL32_AP_EFI_STMM.fd"
        write_changed(output, image)
        timestamp = output.stat().st_mtime_ns
        write_changed(output, image)
        self.assertEqual(output.stat().st_mtime_ns, timestamp)
        output.write_bytes(b"unreviewed image")
        os.utime(output, ns=(timestamp, timestamp))
        write_changed(output, image)
        self.assertEqual(output.read_bytes(), image)
        self.assertFalse(list(self.root.glob("*.tmp")))

    def test_missing_or_duplicate_embedded_payload_is_rejected(self):
        _, image = self.select("1.3.1")
        tee = parse_fip(self.vendor.read_bytes())["tos-fw"]
        with self.assertRaisesRegex(StmmError, "exactly one copy"):
            verify_embedded(b"cached OP-TEE without StandaloneMM", image)
        with self.assertRaisesRegex(StmmError, "exactly one copy"):
            verify_embedded(tee + tee, image)

    def test_unapproved_standalone_image_fails_before_signing(self):
        _, image = self.select("1.3.1")
        # Preserve a valid FV header and a complete compressed stream. An
        # unapproved replacement must fail despite passing these format checks:
        # the legacy CIX loader cannot accept an arbitrary newer MM payload.
        replacement = bytearray(image)
        replacement[0x1000] ^= 1
        tee = self.root / "tee.bin"
        tee.write_bytes(zlib.compress(replacement))
        report = self.root / "verification.json"
        result = subprocess.run(
            [sys.executable, str(ROOT / "scripts/stage_vendor_stmm.py"),
             "--manifest", str(self.manifest), "--vendor-fip", str(self.vendor),
             "--verify-tee", str(tee), "--report", str(report)],
            capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exactly one copy of the pinned", result.stderr)
        self.assertFalse(report.exists())

    def test_cli_stages_and_records_verified_component(self):
        entry, image = self.select("1.3.1")
        output = self.root / "stmm.fd"
        report = self.root / "report.json"
        command = [sys.executable, str(ROOT / "scripts/stage_vendor_stmm.py"),
                   "--manifest", str(self.manifest), "--vendor-fip", str(self.vendor)]
        result = subprocess.run(command + ["--output", str(output)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output.read_bytes(), image)
        tee = self.root / "tee.bin"
        tee.write_bytes(parse_fip(self.vendor.read_bytes())["tos-fw"])
        result = subprocess.run(command + ["--verify-tee", str(tee), "--report", str(report)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        recorded = json.loads(report.read_text())
        self.assertEqual(recorded["vendor_stmm"]["fd_sha256"], entry["fd_sha256"])
        self.assertEqual(recorded["source_built_tee_sha256"], sha256(tee.read_bytes()))


if __name__ == "__main__":
    unittest.main()
