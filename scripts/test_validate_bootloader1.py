#!/usr/bin/env python3
"""BL1 integrity regression tests, including the public Makefile boundary."""

from __future__ import annotations

import hashlib
import io
from contextlib import redirect_stderr, redirect_stdout
import json
import os
import shlex
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

import validate_bootloader1 as bl1
from reconstruction_common import ReconstructionError


def catalogue(*images):
    return {hashlib.sha256(data).hexdigest(): {"size": len(data)} for data in images}


def flash(data: bytes, header: int = 0x100000) -> bytes:
    start = header + 0x88000
    result = bytearray(start + len(data))
    struct.pack_into("<4I", result, header, 0x55AA55AA, 1, 1, 0)
    struct.pack_into("<4I", result, header + 16, 1, start, len(data), 0)
    result[start:] = data
    return bytes(result)


def git(repo, *args):
    return subprocess.run(["git", "-C", str(repo), *args], check=True, capture_output=True)


class PayloadTests(unittest.TestCase):
    def test_every_byte_including_padding_is_covered(self):
        original = b"CIXBTFF!" + bytes(range(64)) + bytes(32)
        catalog = catalogue(original)
        bl1.check_payload(original, "fixture", catalog)
        for offset in range(len(original)):
            changed = bytearray(original)
            changed[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ReconstructionError):
                bl1.check_payload(bytes(changed), "fixture", catalog)
        for changed in (b"", original[:-1], original + b"\0", b"\0" + original):
            with self.assertRaises(ReconstructionError):
                bl1.check_payload(changed, "fixture", catalog)

    def test_another_approved_payload_does_not_satisfy_selected_source(self):
        catalog = catalogue(b"stock", b"cix")
        with self.assertRaisesRegex(ReconstructionError, "selected source"):
            bl1.check_payload(b"cix", "fixture", catalog, {hashlib.sha256(b"stock").hexdigest()})

    def test_full_flash_extracts_exact_declared_bytes_at_both_layouts(self):
        original = b"CIXBTFF!payload\0\0\0"
        for header in bl1.FLASH_HEADERS:
            self.assertEqual(bl1.extract_bl1(flash(original, header), "fixture"), original)

    def test_bad_tables_fail_closed(self):
        original = flash(b"payload")
        variants = [original[:0x100003], b"not firmware"]
        for entry in ((1, 0x188000, 1000, 0), (1, 0x100000, 7, 0), (2, 0x188000, 7, 0)):
            changed = bytearray(original)
            struct.pack_into("<4I", changed, 0x100010, *entry)
            variants.append(changed)
        for count in (0, 129):
            changed = bytearray(original)
            struct.pack_into("<I", changed, 0x100008, count)
            variants.append(changed)
        for second in ((1, 0x188000, 7, 0), (2, 0x188001, 2, 0)):
            changed = bytearray(original)
            struct.pack_into("<I", changed, 0x100008, 2)
            struct.pack_into("<4I", changed, 0x100020, *second)
            variants.append(changed)
        for changed in variants:
            with self.subTest(data=bytes(changed[:8])), self.assertRaises(ReconstructionError):
                bl1.extract_bl1(changed, "fixture")

    def test_zip_and_tar_validate_all_full_images_without_extracting(self):
        good = b"vendor payload"
        catalog = catalogue(good)
        expected = set(catalog)
        with tempfile.TemporaryDirectory(prefix="bl1-archives-") as tmp:
            root = Path(tmp)
            for suffix in (".zip", ".tar.gz"):
                for last in (good, b"modified payload"):
                    path = root / ("firmware" + suffix)
                    members = {"a/cix_flash_all.bin": flash(good), "b/cix_flash_all.bin": flash(last)}
                    if suffix == ".zip":
                        with zipfile.ZipFile(path, "w") as archive:
                            for name, data in members.items():
                                archive.writestr(name, data)
                    else:
                        with tarfile.open(path, "w:gz") as archive:
                            for name, data in members.items():
                                member = tarfile.TarInfo(name)
                                member.size = len(data)
                                archive.addfile(member, io.BytesIO(data))
                    if last == good:
                        self.assertEqual(len(bl1.check_archive(path, catalog, expected)), 2)
                    else:
                        with self.assertRaises(ReconstructionError):
                            bl1.check_archive(path, catalog, expected)
            self.assertFalse((root / "a").exists())


class SourceRefTests(unittest.TestCase):
    def test_vendor_reads_and_source_audit_accept_remote_tracking_refs(self):
        # A regular clone has origin/source/**, without local source branches.
        with tempfile.TemporaryDirectory(prefix="bl1-remote-refs-") as tmp:
            repo = Path(tmp)
            git(repo, "init", "-q", "-b", "build")
            payload = b"unchanged fixture vendor BL1"
            for path in (bl1.STOCK, bl1.CIX):
                destination = repo / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(payload)
            git(repo, "add", "src")
            git(repo, "-c", "user.name=BL1 regression", "-c", "user.email=bl1-regression",
                "-c", "commit.gpgsign=false", "commit", "-qm", "fixture vendor payloads")
            commit = git(repo, "rev-parse", "HEAD").stdout.decode().strip()
            vendor = "source/vendor/cix/fixture"
            for ref in (vendor, "source/unofficial/fixture"):
                git(repo, "update-ref", f"refs/remotes/origin/{ref}", commit)
            self.assertEqual(git(repo, "for-each-ref", "refs/heads/source/").stdout, b"")
            catalog = catalogue(payload)
            catalog[hashlib.sha256(payload).hexdigest()]["provenance"] = [
                {"commit": commit, "ref": vendor, "path": bl1.STOCK},
            ]
            self.assertEqual(bl1.git_bytes(repo, vendor, bl1.STOCK), payload)
            # One pinned vendor reference and both inputs on the Unofficial ref.
            self.assertEqual(bl1.check_source_refs(repo, catalog), 3)


class BuildBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="bl1-build-boundary-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.report = self.root / "reports/bootloader1-validation.json"
        self.worktree = self.root / "rendered"
        self.worktree.mkdir()
        self.stock = bl1.git_bytes(bl1.ROOT, "source/vendor/radxa/1.3.1/edk2-stable202208", bl1.STOCK)
        cix = bl1.git_bytes(bl1.ROOT, "source/vendor/cix/1.2/bootloader1", bl1.STOCK.removeprefix("src/edk2-non-osi/"))
        for path, data in ((bl1.STOCK, self.stock), (bl1.CIX, cix)):
            destination = self.worktree / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
        git(self.worktree, "init", "-q")
        git(self.worktree, "add", "src")
        git(self.worktree, "-c", "user.name=BL1 regression", "-c", "user.email=bl1-regression",
            "-c", "commit.gpgsign=false", "commit", "-qm", "fixture vendor payloads")

    def test_selection_and_dirty_input(self):
        catalog = bl1.load_catalog()
        stock = bl1.source_payloads(self.worktree, catalog, "custom", "", "buildbox-firmware-build")
        cix = bl1.source_payloads(self.worktree, catalog, "custom", "v1.2", "buildbox-firmware-build")
        self.assertNotEqual(stock, cix)
        self.assertEqual(bl1.source_payloads(self.worktree, catalog, "upstream", "1.2", "buildbox-firmware-build"), stock)
        self.assertEqual(bl1.source_payloads(self.worktree, catalog, "upstream", "", "build-all"), stock | cix)
        # Even replacing the source with another approved image is rejected.
        (self.worktree / bl1.STOCK).write_bytes((self.worktree / bl1.CIX).read_bytes())
        with self.assertRaisesRegex(ReconstructionError, "committed vendor"):
            bl1.source_payloads(self.worktree, catalog, "custom", "", "buildbox-firmware-build")

    def test_public_make_build_gates_inputs_and_outputs_before_mirroring(self):
        # Substitute only rendering and compilation. The public Makefile,
        # argument validation, BL1 checks, and mirroring all execute for real.
        cached_report = bl1.bootloader1_report_path(bl1.ROOT, self.worktree, "O6", "RELEASE")
        self.addCleanup(shutil.rmtree, cached_report.parent, True)
        proxy = self.root / "python-proxy.py"
        proxy.write_text(
            "import os,sys\n"
            "if sys.argv[1].endswith('render_release_branch.py'):\n"
            f" print({str(self.worktree)!r})\n"
            "else:\n os.execv(sys.executable,[sys.executable,*sys.argv[1:]])\n"
        )
        (self.worktree / "Makefile").write_text(
            ".PHONY: buildbox-firmware-build\n"
            "buildbox-firmware-build:\n"
            "\t@touch compilation-reached\n"
            "\t@mkdir -p src/Build/O6/RELEASE_GCC\n"
            "\t@cp fixture-output.bin src/Build/O6/RELEASE_GCC/cix_flash_all.bin\n"
        )
        dist = self.root / "dist"
        command = ["make", "--no-print-directory", "build", "RELEASE=edk2-202605/radxa-1.3.1/unofficial",
                   "ARTEFACT_MODE=custom", "FIRMWARE_BOARD=O6", "FIRMWARE_TARGET=RELEASE",
                   "FIRMWARE_DISTRO=trixie", "ENABLE_FIRMWARE_FIXES=false", "ENABLE_CORE_ORDER=cix",
                   "ENABLE_EXPERIMENTAL_UEFI_SETTINGS=false", "DEBUG_VERBOSE=false", "CIX_RELEASE=",
                   f"PYTHON={shlex.quote(sys.executable)} {shlex.quote(str(proxy))}",
                   f"BUILD_DIST_ROOT={dist}", f"FIRMWARE_CACHE_ROOT={self.root / 'cache'}"]
        (self.worktree / "fixture-output.bin").write_bytes(flash(self.stock))
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", EDK2_CIX_BL1_TOOL=str(self.root / "unavailable-tool"))
        result = subprocess.run(command, cwd=bl1.ROOT, env=env, capture_output=True, text=True, timeout=120)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        reports = list(dist.rglob("bootloader1-validation.json"))
        self.assertEqual(len(reports), 1)
        self.assertEqual(json.loads(reports[0].read_text())["board_fuse_acceptance"], "not-tested")
        self.assertEqual(json.loads(reports[0].read_text())["vendor_signatures"]["status"], "unavailable")
        self.assertEqual(json.loads(reports[0].read_text())["acceptance_basis"], "approved-vendor-hash-fallback")
        self.assertIn("WARNING", result.stderr)
        published = next(dist.rglob("cix_flash_all.bin"))
        published_bytes = published.read_bytes()
        marker = self.worktree / "compilation-reached"
        marker.unlink()
        (self.worktree / bl1.STOCK).write_bytes(self.stock + b"\0")
        result = subprocess.run(command, cwd=bl1.ROOT, env=env, capture_output=True, text=True, timeout=120)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(marker.exists(), result.stdout + result.stderr)
        (self.worktree / bl1.STOCK).write_bytes(self.stock)
        (self.worktree / "fixture-output.bin").write_bytes(flash(self.stock + b"\0"))
        result = subprocess.run(command, cwd=bl1.ROOT, env=env, capture_output=True, text=True, timeout=120)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(marker.exists())
        self.assertEqual(published.read_bytes(), published_bytes)
        self.assertFalse(cached_report.exists())

    def test_distribution_checks_each_variant_against_this_source_tree(self):
        catalog = bl1.load_catalog()
        expected = bl1.source_payloads(self.worktree, catalog, "custom", "", "buildbox-zip")
        output = self.worktree / "src/Build/O6/RELEASE_GCC"
        output.mkdir(parents=True)
        (output / "cix_flash_all.bin").write_bytes(flash(self.stock))
        distribution = self.worktree / "dist/cix-variant/cix_flash_all.bin"
        distribution.parent.mkdir(parents=True)
        cix = (self.worktree / bl1.CIX).read_bytes()
        distribution.write_bytes(flash(cix))
        with patch.object(bl1, "verify_or_warn", return_value={"status": "verified"}) as verify:
            records = bl1.check_outputs(self.worktree, "O6", "RELEASE", "buildbox-zip", catalog, expected,
                                        report_path=self.report)
            self.assertEqual(len(records), 2)
            self.assertEqual(set(verify.call_args.args[0]), {hashlib.sha256(data).hexdigest() for data in (self.stock, cix)})
            distribution.write_bytes(flash(cix + b"\0"))
            verify.reset_mock()
            with self.assertRaises(ReconstructionError):
                bl1.check_outputs(self.worktree, "O6", "RELEASE", "buildbox-zip", catalog, expected,
                                  report_path=self.report)
            verify.assert_not_called()
            self.assertFalse(self.report.exists())

    def test_explicit_cli_report_is_written_and_invalidated_on_input_failure(self):
        output = self.worktree / "src/Build/O6/RELEASE_GCC"
        output.mkdir(parents=True)
        (output / "cix_flash_all.bin").write_bytes(flash(self.stock))
        command = ["validate_bootloader1.py", "--worktree", str(self.worktree),
                   "--phase", "outputs", "--report", str(self.report)]
        with patch.object(sys, "argv", command), redirect_stdout(io.StringIO()), \
                patch.object(bl1, "verify_or_warn", return_value={"status": "verified"}):
            bl1.main()
        self.assertEqual(json.loads(self.report.read_text())["acceptance_basis"],
                         "approved-vendor-hash-and-signature")
        (self.worktree / bl1.STOCK).write_bytes(self.stock + b"\0")
        command[command.index("outputs")] = "inputs"
        with patch.object(sys, "argv", command):
            with self.assertRaisesRegex(ReconstructionError, "committed vendor"):
                bl1.main()
        self.assertFalse(self.report.exists())

    def test_unavailable_verifier_accepts_only_unchanged_vendor_bl1_including_cix_2026q1(self):
        catalog = bl1.load_catalog()
        output = self.worktree / "src/Build/O6/RELEASE_GCC"
        output.mkdir(parents=True)
        image = output / "cix_flash_all.bin"
        report = self.report
        for cix_release, data in (("", self.stock), ("1.2", (self.worktree / bl1.CIX).read_bytes())):
            expected = bl1.source_payloads(self.worktree, catalog, "custom", cix_release, "buildbox-firmware-build")
            image.write_bytes(flash(data))
            with patch.dict(os.environ, {"EDK2_CIX_BL1_TOOL": str(self.root / "missing-tool")}), \
                    io.StringIO() as warning, redirect_stderr(warning):
                bl1.check_outputs(self.worktree, "O6", "RELEASE", "buildbox-firmware-build", catalog, expected,
                                  report_path=report)
                self.assertIn("WARNING", warning.getvalue())
            result = json.loads(report.read_text())
            self.assertEqual(result["acceptance_basis"], "approved-vendor-hash-fallback")
            self.assertTrue(result["approved_payloads"][0]["provenance"])
            for changed in (bytes([data[0] ^ 1]) + data[1:], data[:-1] + bytes([data[-1] ^ 1]),
                            data + b"\0", b"unknown locally rebuilt bootloader"):
                image.write_bytes(flash(changed))
                with patch.object(bl1, "verify_or_warn") as verifier:
                    with self.assertRaisesRegex(ReconstructionError, "not an unchanged qualified vendor"):
                        bl1.check_outputs(self.worktree, "O6", "RELEASE", "buildbox-firmware-build", catalog, expected,
                                          report_path=report)
                    verifier.assert_not_called()
                self.assertFalse(report.exists())

    def test_committing_modified_bl1_does_not_approve_it(self):
        path = self.worktree / bl1.CIX
        path.write_bytes(path.read_bytes() + b"\0")
        git(self.worktree, "add", bl1.CIX)
        git(self.worktree, "-c", "user.name=BL1 regression", "-c", "user.email=bl1-regression",
            "-c", "commit.gpgsign=false", "commit", "-qm", "changed BL1 remains unapproved")
        with self.assertRaisesRegex(ReconstructionError, "not an unchanged qualified vendor"):
            bl1.source_payloads(self.worktree, bl1.load_catalog(), "custom", "1.2", "buildbox-firmware-build")


if __name__ == "__main__":
    unittest.main()
