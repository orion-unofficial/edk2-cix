#!/usr/bin/env python3
"""Exercise real vendor chains and reject corruption and the Stage 3 trust root."""

import hashlib
import json
import os
import shlex
import shutil
import sys
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest import mock
import zipfile

import firmware_chain as chain
import validate_firmware_chain as packaging
from reconstruction_common import show_file


ROOT = Path(__file__).resolve().parents[1]
PACKAGE = "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/"


def vendor(version, path):
    return show_file(ROOT, f"source/vendor/radxa/{version}/edk2-stable202208", PACKAGE + path)


def replace_entry(data, name, payload):
    entries = chain.parse_fip(data)
    entries[name] = payload
    result = bytearray(struct.pack("<IIQ", 0xAA640001, 0x12345678, 0))
    offset = 16 + 40 * (len(entries) + 1)
    body = bytearray()
    for key, value in entries.items():
        result += struct.pack("<16sQQQ", bytes.fromhex(chain.UUIDS[key]), offset, len(value), 0)
        body += value
        offset += len(value)
    result += struct.pack("<16sQQQ", bytes(16), offset, 0, 0)
    return bytes(result + body)


class FirmwareChainTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fip = vendor("1.3.1", "Firmwares/bootloader2.img")
        cls.entries = chain.parse_fip(cls.fip)
        cls.trust = chain.Certificate(cls.entries["trusted-key-cert"])
        cls.anchor = hashlib.sha256(cls.trust.public_key).hexdigest()

    def validate(self, data):
        return chain.validate_fip(data, "trusted", self.anchor, 31)

    def test_retained_vendor_trusted_images_validate_cryptographically(self):
        for version in ("0.2.0-1", "0.3.1-1", "1.0.0-1", "1.1.0-2", "1.2.4", "1.3.1"):
            with self.subTest(version=version):
                result = self.validate(vendor(version, "Firmwares/bootloader2.img"))
                self.assertEqual(result["root_spki_sha256"], self.anchor)
                self.assertEqual(result["status"], "verified")

    def test_each_signed_certificate_and_executable_is_checked(self):
        for name, value in self.entries.items():
            changed = bytearray(value)
            changed[-1] ^= 1
            with self.subTest(name=name), self.assertRaises(chain.ChainError):
                self.validate(replace_entry(self.fip, name, bytes(changed)))

    def test_missing_certificate_or_payload_fails(self):
        for name in self.entries:
            with self.subTest(name=name), self.assertRaises(chain.ChainError):
                self.validate(replace_entry(self.fip, name, b""))

    def test_counter_below_reference_fails(self):
        with self.assertRaisesRegex(chain.ChainError, "counter"):
            chain.validate_fip(self.fip, "trusted", self.anchor, 32)

    def test_correctly_signed_wrong_root_is_rejected(self):
        # A self-consistent certificate is insufficient: this reproduces the
        # Stage 3 helper selecting its own key as a new trusted-firmware root.
        with tempfile.TemporaryDirectory(prefix="wrong-fip-root-") as tmp:
            root = Path(tmp)
            key, cert = root / "test-key.pem", root / "test-cert.der"
            subprocess.run([
                "openssl", "req", "-new", "-x509", "-newkey", "rsa:3072",
                "-nodes", "-keyout", str(key), "-out", str(cert), "-outform", "DER",
                "-subj", "/CN=Test-only wrong firmware root", "-days", "1", "-sha256",
                "-sigopt", "rsa_padding_mode:pss", "-sigopt", "rsa_pss_saltlen:32",
            ], check=True, capture_output=True)
            wrong = chain.Certificate(cert.read_bytes())
            wrong.verify(wrong.public_key)
            with self.assertRaisesRegex(chain.ChainError, "trust anchor"):
                self.validate(replace_entry(self.fip, "trusted-key-cert", cert.read_bytes()))

    def test_uefi_delegation_does_not_authorise_trusted_firmware(self):
        cert = vendor("1.3.1", "certs/trusted_key_no.crt")
        with self.assertRaisesRegex(chain.ChainError, "trust anchor"):
            self.validate(replace_entry(self.fip, "trusted-key-cert", cert))

    def test_fip_rejects_duplicate_overlapping_and_truncated_entries(self):
        variants = [self.fip[:15], self.fip[:-1], b"not a FIP"]
        for offset, value in ((32, 16), (40, len(self.fip)), (48, 1)):
            data = bytearray(self.fip)
            struct.pack_into("<Q", data, offset, value)
            variants.append(bytes(data))
        data = bytearray(self.fip)
        data[56:72] = data[16:32]
        variants.append(bytes(data))
        for data in variants:
            with self.subTest(size=len(data)), self.assertRaises(chain.ChainError):
                self.validate(data)

    def test_invalid_der_is_not_accepted(self):
        for data in (b"", b"\x30\x80\x00\x00", b"\x30\x81\x01\x00", b"\x30\x05x"):
            with self.subTest(data=data), self.assertRaises(chain.ChainError):
                chain.Certificate(data)


class SignedFirmwareFixture:
    """Real vendor payloads and a test-only UEFI signed by its delegated key."""

    def __init__(self, package, *, payload=b"test-only UEFI executable bytes"):
        self.package = Path(package)
        for name in ("Firmwares/bootloader1.img", "Firmwares/bootloader2.img", "certs/trusted_key_no.crt", "Keys/oem_privatekey.pem", "spi_flash_config_all.json"):
            path = self.package / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(vendor("1.3.1", name))
        self.selected = packaging.reference(self.package, packaging.load_catalog())
        self.payload = payload
        pub = chain.Certificate(vendor("1.3.1", "certs/trusted_key_no.crt")).extension("303")

        def certificate(name, extensions):
            path = self.package / (name + ".der")
            args = ["openssl", "req", "-new", "-x509", "-key", str(self.package / "Keys/oem_privatekey.pem"),
                    "-out", str(path), "-outform", "DER", "-subj", "/CN=Test-only " + name, "-days", "1", "-sha256",
                    "-sigopt", "rsa_padding_mode:pss", "-sigopt", "rsa_pss_saltlen:32"]
            for suffix, value in extensions.items():
                args += ["-addext", chain.TBBR + suffix + "=critical,DER:" + value.hex()]
            subprocess.run(args, check=True, capture_output=True)
            return path.read_bytes()

        counter = bytes.fromhex("020200df")
        digest_prefix = bytes.fromhex("3031300d060960864801650304020105000420")
        self.uefi_entries = {
            "trusted-key-cert": vendor("1.3.1", "certs/trusted_key_no.crt"),
            "nt-fw-key-cert": certificate("key", {"2": counter, "1101": pub}),
            "nt-fw-cert": certificate("content", {"2": counter, "1201": digest_prefix + hashlib.sha256(self.payload).digest(),
                                                  "1202": digest_prefix + bytes(32)}),
            "nt-fw": self.payload,
        }
        self.oem_cert = certificate("OEM root", {"303": pub})
        # Reuse the same mechanical FIP encoder as the corruption tests.
        fip = bytearray(struct.pack("<IIQ", 0xaa640001, 0x12345678, 0))
        offset = 16 + 40 * (len(self.uefi_entries) + 1)
        body = bytearray()
        for name, value in self.uefi_entries.items():
            fip += struct.pack("<16sQQQ", bytes.fromhex(chain.UUIDS[name]), offset, len(value), 0)
            body += value
            offset += len(value)
        self.uefi = bytes(fip + struct.pack("<16sQQQ", bytes(16), offset, 0, 0) + body)

    def flash_fixture(self):
        payloads = {1: vendor("1.3.1", "Firmwares/bootloader1.img"),
                    2: vendor("1.3.1", "Firmwares/bootloader2.img"), 7: self.uefi}
        image = bytearray(8 * 1024 * 1024)
        slots = self.selected["flash_layout"]["entries"]
        struct.pack_into("<4I", image, 0x100000, 0x55aa55aa, 1, len(slots), 0)
        for index, (key, slot) in enumerate(slots.items()):
            kind = int(key)
            data = payloads.get(kind, b"fixture")
            address = slot["address"]
            struct.pack_into("<4I", image, 0x100010 + 16 * index, kind, address, len(data), 0)
            image[address:address + len(data)] = data
        return image, slots["7"]["address"] + len(self.uefi)

    def ota_fixture(self):
        return struct.pack("<8I", 0x55aa55aa, 1, 1, 0x80000000, 7,
                           self.selected["flash_layout"]["entries"]["7"]["address"], len(self.uefi), 0) + self.uefi


class PackagingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="firmware-chain-package-")
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.fixture = SignedFirmwareFixture(cls.tmp.name)
        for name, value in vars(cls.fixture).items():
            setattr(cls, name, value)

    def test_vendor_and_vendor_delegated_uefi_presentations(self):
        for data in (self.uefi, replace_entry(self.uefi, "trusted-key-cert", self.oem_cert)):
            self.assertEqual(packaging.validate_uefi(data, self.selected)["status"], "verified")

    def test_uefi_payload_and_signature_corruption_are_rejected(self):
        for name in ("nt-fw", "nt-fw-cert", "nt-fw-key-cert", "trusted-key-cert"):
            value = bytearray(self.uefi_entries[name])
            value[-1] ^= 1
            with self.subTest(name=name), self.assertRaises(chain.ChainError):
                packaging.validate_uefi(replace_entry(self.uefi, name, value), self.selected)

    def test_stage3_signing_key_is_not_vendor_trusted(self):
        for release in ("1.2", "v1.2", "V1.2"):
            with self.assertRaisesRegex(chain.ChainError, "UEFI OEM key"):
                packaging.preflight(self.package, self.selected, release, "custom")
        packaging.preflight(self.package, self.selected, "", "custom")

    def flash_fixture(self):
        return self.fixture.flash_fixture()

    def ota_fixture(self):
        return self.fixture.ota_fixture()

    def test_ota_payload_and_destination_are_checked(self):
        data = bytearray(self.ota_fixture())
        packaging.check_ota(data, self.selected)
        data[-1] ^= 1
        with self.assertRaisesRegex(chain.ChainError, "digest"):
            packaging.check_ota(data, self.selected)
        struct.pack_into("<I", data, 20, 0)
        with self.assertRaisesRegex(chain.ChainError, "address"):
            packaging.check_ota(data, self.selected)

    def test_ota_rejects_one_byte_beyond_the_reserved_slot(self):
        # The original vendor OTA packager accepts this boundary violation.
        data = bytearray(self.ota_fixture())
        size = self.selected["flash_layout"]["entries"]["7"]["size"] + 1
        data.extend(bytes(32 + size - len(data)))
        struct.pack_into("<I", data, 24, size)
        with self.assertRaisesRegex(chain.ChainError, "reserved size"):
            packaging.check_ota(data, self.selected)

    def test_validly_signed_four_mib_debug_uefi_is_not_flashable(self):
        # A valid certificate chain cannot make a 4 MiB DEBUG FD fit the
        # vendor's roughly 2 MiB BL33 slot. Check both package formats.
        with tempfile.TemporaryDirectory(prefix="oversized-signed-uefi-") as tmp:
            fixture = SignedFirmwareFixture(tmp, payload=bytes(0x400000))
            self.assertEqual(packaging.validate_uefi(fixture.uefi, fixture.selected)["status"], "verified")
            with self.assertRaisesRegex(chain.ChainError, "reserved size"):
                packaging.check_ota(fixture.ota_fixture(), fixture.selected)
            image, _ = fixture.flash_fixture()
            with self.assertRaisesRegex(chain.ChainError, "exactly 8 MiB|out-of-bounds|reserved size"):
                packaging.check_flash(image, fixture.selected)

    def test_archive_cannot_hide_a_corrupt_ota_beside_valid_full_flash(self):
        with tempfile.TemporaryDirectory(prefix="firmware-archive-") as tmp:
            path = Path(tmp) / "firmware.zip"
            for corrupt in (False, True):
                ota = bytearray(self.ota_fixture())
                if corrupt:
                    ota[-1] ^= 1
                with zipfile.ZipFile(path, "w") as archive:
                    archive.writestr("payload/cix_flash_all.bin", self.flash_fixture()[0])
                    archive.writestr("payload/cix_flash_ota.bin", ota)
                if corrupt:
                    with self.assertRaisesRegex(chain.ChainError, "digest"):
                        packaging.check_archive(path, self.selected)
                else:
                    self.assertEqual(len(packaging.check_archive(path, self.selected)), 2)

    def test_missing_crypto_tool_is_fatal(self):
        with mock.patch.object(chain.subprocess, "run", side_effect=FileNotFoundError("openssl")):
            with self.assertRaisesRegex(chain.ChainError, "mandatory certificate"):
                packaging.validate_uefi(self.uefi, self.selected)

    def test_full_flash_all_chains_and_layout_are_verified(self):
        image, address = self.flash_fixture()
        self.assertEqual(packaging.check_flash(bytes(image), self.selected)["uefi"]["status"], "verified")
        image[address - 1] ^= 1
        with self.assertRaisesRegex(chain.ChainError, "digest"):
            packaging.check_flash(bytes(image), self.selected)
        # Slot 100 follows BL1 in sorted catalogue order; overlap it with BL1.
        struct.pack_into("<I", image, 0x100024, 0x188000)
        with self.assertRaisesRegex(chain.ChainError, "overlapping"):
            packaging.check_flash(bytes(image), self.selected)

    def test_public_make_rejects_stage3_and_corrupt_outputs_without_validation_opt_out(self):
        # Actual top-level Make, input validation, certificate verification and
        # mirroring; substitute only source rendering, compilation and BL1 tool.
        with tempfile.TemporaryDirectory(prefix="chain-make-boundary-") as tmp:
            root = Path(tmp)
            wt = root / "rendered"
            shutil.copytree(self.package, wt / PACKAGE)
            for args in (("init", "-q"), ("add", "."),
                         ("-c", "user.name=Regression", "-c", "user.email=regression",
                          "-c", "commit.gpgsign=false", "commit", "-qm", "test inputs")):
                subprocess.run(["git", "-C", str(wt), *args], check=True, capture_output=True)
            proxy = root / "python-proxy.py"
            proxy.write_text("import os,sys\n"
                             "if sys.argv[1].endswith('render_release_branch.py'):\n"
                             f" print({str(wt)!r})\n"
                             "elif sys.argv[1].endswith('validate_bootloader1.py'):\n pass\n"
                             "else:\n os.execv(sys.executable,[sys.executable,*sys.argv[1:]])\n")
            (wt / "Makefile").write_text(
                ".PHONY: buildbox-firmware-build\n"
                "buildbox-firmware-build:\n\t@touch compilation-reached\n"
                "\t@mkdir -p src/Build/O6/RELEASE_GCC\n"
                "\t@cp fixture-output.bin src/Build/O6/RELEASE_GCC/cix_flash_all.bin\n")
            command = ["make", "--no-print-directory", "build", "RELEASE=edk2-202605/radxa-1.3.1/unofficial",
                       "ARTEFACT_MODE=custom", "FIRMWARE_BOARD=O6", "FIRMWARE_TARGET=RELEASE",
                       "FIRMWARE_DISTRO=trixie", "ENABLE_FIRMWARE_FIXES=true", "ENABLE_CORE_ORDER=cix",
                       "ENABLE_EXPERIMENTAL_UEFI_SETTINGS=false", "DEBUG_VERBOSE=false",
                       "FIRMWARE_VALIDATE_ON_BUILD=false",
                       f"PYTHON={shlex.quote(sys.executable)} {shlex.quote(str(proxy))}",
                       f"BUILD_DIST_ROOT={root / 'dist'}", f"FIRMWARE_CACHE_ROOT={root / 'cache'}"]
            from reconstruction_common import firmware_chain_report_path
            report = firmware_chain_report_path(ROOT, wt, "O6", "RELEASE")
            self.addCleanup(shutil.rmtree, report.parent, True)
            env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
            result = subprocess.run(command + ["CIX_RELEASE=1.2"], cwd=ROOT, env=env,
                                    capture_output=True, text=True, timeout=120)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("CIX_RELEASE must be empty", result.stderr)
            self.assertFalse((wt / "compilation-reached").exists())
            self.assertFalse(report.exists())
            image, end = self.flash_fixture()
            (wt / "fixture-output.bin").write_bytes(image)
            result = subprocess.run(command + ["CIX_RELEASE="], cwd=ROOT, env=env,
                                    capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            published = next((root / "dist").rglob("cix_flash_all.bin"))
            self.assertEqual(published.read_bytes(), image)
            previous_report = report.read_bytes()
            # A valid previous Stage 2 output must not allow a later Stage 3
            # request to report success, even when all old files still exist.
            (wt / "compilation-reached").unlink()
            result = subprocess.run(command + ["CIX_RELEASE=1.2", "DEBUG_VERBOSE=true"],
                                    cwd=ROOT, env=env, capture_output=True, text=True, timeout=120)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("CIX_RELEASE must be empty", result.stderr)
            self.assertFalse((wt / "compilation-reached").exists())
            self.assertEqual(published.read_bytes(), image)
            self.assertEqual(report.read_bytes(), previous_report)
            image[end - 1] ^= 1
            (wt / "fixture-output.bin").write_bytes(image)
            result = subprocess.run(command + ["CIX_RELEASE="], cwd=ROOT, env=env,
                                    capture_output=True, text=True, timeout=120)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("digest", result.stderr)
            self.assertNotEqual(published.read_bytes(), image)
            self.assertEqual(json.loads(report.read_text())["status"], "failed")

    def test_catalogue_pins_are_backed_by_immutable_vendor_commits(self):
        for row in packaging.load_catalog():
            for provenance in row["provenance"]:
                for path, field in (("Firmwares/bootloader1.img", "bl1_sha256"),
                                    ("Firmwares/bootloader2.img", "trusted_fip_sha256"),
                                    ("certs/trusted_key_no.crt", "uefi_certificate_sha256")):
                    data = subprocess.check_output(["git", "-C", str(ROOT), "show",
                                                    provenance["commit"] + ":" + PACKAGE + path])
                    self.assertEqual(hashlib.sha256(data).hexdigest(), row[field])
                config = subprocess.check_output(["git", "-C", str(ROOT), "show",
                                                  provenance["commit"] + ":" + PACKAGE + "spi_flash_config_all.json"])
                self.assertIn(hashlib.sha256(config).hexdigest(), {item["config_sha256"] for item in row["flash_layouts"]})


if __name__ == "__main__":
    unittest.main()
