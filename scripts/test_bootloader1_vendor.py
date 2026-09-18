#!/usr/bin/env python3
"""Launch failures warn; a running verifier's rejection must fail the build."""

from contextlib import nullcontext, redirect_stderr
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import urllib.error
from unittest.mock import patch

import bootloader1_vendor as vendor
import qualify_bootloader1_signatures as qualification
from reconstruction_common import ReconstructionError


class VendorPolicyTests(unittest.TestCase):
    def test_linux_x86_uses_native_and_arm_uses_container(self):
        with patch.object(vendor.platform, "system", return_value="Linux"), \
                patch.object(vendor.platform, "machine", return_value="x86_64"):
            self.assertEqual(vendor.select_runner(), "native")
        for system, machine in (("Linux", "aarch64"), ("Darwin", "arm64"), ("Darwin", "x86_64")):
            with patch.object(vendor.platform, "system", return_value=system), \
                    patch.object(vendor.platform, "machine", return_value=machine), \
                    patch.object(vendor.shutil, "which", return_value="/usr/bin/docker"):
                self.assertEqual(vendor.select_runner(), "docker")

    def test_missing_runtime_and_download_failure_warn(self):
        for error in (vendor.VendorUnavailable("no runtime"), vendor.VendorUnavailable("download failed")):
            with patch.object(vendor, "verify_payloads", side_effect=error), io.StringIO() as output, redirect_stderr(output):
                self.assertEqual(vendor.verify_or_warn({})["status"], "unavailable")
                self.assertIn("WARNING", output.getvalue())

    def test_untrusted_tool_is_not_executed(self):
        with tempfile.TemporaryDirectory(prefix="bl1-tool-") as temporary:
            tool = Path(temporary) / "tool"
            tool.write_bytes(b"not the pinned tool")
            with self.assertRaises(vendor.VendorUnavailable), patch.object(vendor.subprocess, "run") as run:
                vendor.prepare_tool(tool, download=False)
            run.assert_not_called()

    def test_download_is_checked_cached_and_published_without_partial_files(self):
        data = b"fixture executable"
        metadata = {"vendor_tool": {"sha256": hashlib.sha256(data).hexdigest(), "url": "https://example.invalid/tool"}}
        with tempfile.TemporaryDirectory(prefix="bl1-download-") as temporary, \
                patch.object(vendor.json, "loads", return_value=metadata), \
                patch.object(vendor.urllib.request, "urlopen", return_value=io.BytesIO(data)) as fetch:
            path = Path(temporary) / "tool"
            self.assertEqual(vendor.prepare_tool(path), path.resolve())
            self.assertEqual(path.read_bytes(), data)
            vendor.prepare_tool(path)
            self.assertEqual(fetch.call_count, 1)
            self.assertEqual(list(Path(temporary).iterdir()), [path])
        with tempfile.TemporaryDirectory(prefix="bl1-download-failure-") as temporary, \
                patch.object(vendor.urllib.request, "urlopen", side_effect=urllib.error.URLError("offline")):
            with self.assertRaises(vendor.VendorUnavailable):
                vendor.prepare_tool(Path(temporary) / "tool")

    def test_probe_failures_are_unavailable(self):
        for returncode in (1, 125, 126, 127, -4):
            with patch.object(vendor.subprocess, "run", return_value=subprocess.CompletedProcess([], returncode, "", "cannot start")):
                with self.assertRaises(vendor.VendorUnavailable):
                    vendor.execute(["tool", "--help"], probe=True, container=True)
        with patch.object(vendor.subprocess, "run", side_effect=FileNotFoundError("no loader")):
            with self.assertRaises(vendor.VendorUnavailable):
                vendor.execute(["tool"], probe=True, container=False)

    def test_timeout_after_successful_launch_is_a_failure(self):
        with patch.object(vendor.subprocess, "run", side_effect=subprocess.TimeoutExpired([], 60)):
            with self.assertRaises(vendor.VendorUnavailable):
                vendor.execute(["tool"], probe=True, container=False)
            with self.assertRaises(ReconstructionError):
                vendor.execute(["tool"], probe=False, container=False)

    def test_container_launch_failure_after_probe_is_unavailable(self):
        for code in (125, 126, 127):
            with patch.object(vendor.subprocess, "run", return_value=subprocess.CompletedProcess([], code, "", "cannot launch")):
                with self.assertRaises(vendor.VendorUnavailable):
                    vendor.execute(["docker"], probe=False, container=True)

    def test_rejection_and_missing_success_evidence_are_fatal(self):
        data = b"fixture"
        payloads = {hashlib.sha256(data).hexdigest(): data}
        for result in (subprocess.CompletedProcess([], 255, "signature failure", ""),
                       subprocess.CompletedProcess([], 0, "", ""),
                       subprocess.CompletedProcess([], -11, "", "crash")):
            with tempfile.TemporaryDirectory(prefix="bl1-rejection-") as temporary, \
                    patch.object(vendor, "select_runner", return_value="native"), \
                    patch.object(vendor, "prepare_tool", return_value=Path("/tool")), \
                    patch.object(vendor, "temp_dir", return_value=nullcontext(temporary)), \
                    patch.object(vendor.subprocess, "run", side_effect=[
                        subprocess.CompletedProcess([], 0, "--verify", ""), result]):
                with self.assertRaisesRegex(ReconstructionError, "FAILED"):
                    vendor.verify_or_warn(payloads)

    def test_success_records_verified_status(self):
        data = b"fixture"
        payloads = {hashlib.sha256(data).hexdigest(): data}
        with tempfile.TemporaryDirectory(prefix="bl1-success-") as temporary, \
                patch.object(vendor, "select_runner", return_value="native"), \
                patch.object(vendor, "prepare_tool", return_value=Path("/tool")), \
                patch.object(vendor, "temp_dir", return_value=nullcontext(temporary)), \
                patch.object(vendor.subprocess, "run", side_effect=[
                    subprocess.CompletedProcess([], 0, "--verify", ""),
                    subprocess.CompletedProcess([], 0, "Verify signature OK\nCompare hash data of image 1 successful", "")]):
            result = vendor.verify_or_warn(payloads)
        self.assertEqual(result["status"], "verified")
        self.assertEqual(result["runner"], "native")

    def test_batch_uses_distinct_immutable_input_files(self):
        payloads = {hashlib.sha256(data).hexdigest(): data for data in (b"first payload", b"second payload")}
        observed = []

        def execute(command, **kwargs):
            if "--help" in command:
                return subprocess.CompletedProcess(command, 0, "--verify", "")
            path = Path(command[-1])
            observed.append(path)
            for previous in observed:
                self.assertEqual(hashlib.sha256(previous.read_bytes()).hexdigest(), previous.stem)
            return subprocess.CompletedProcess(command, 0, "Verify signature OK\nCompare hash data of image 1 successful", "")

        with tempfile.TemporaryDirectory(prefix="bl1-batch-") as temporary, \
                patch.object(vendor, "select_runner", return_value="native"), \
                patch.object(vendor, "prepare_tool", return_value=Path("/tool")), \
                patch.object(vendor, "temp_dir", return_value=nullcontext(temporary)), \
                patch.object(vendor.subprocess, "run", side_effect=execute):
            vendor.verify_payloads(payloads)
        self.assertEqual(len(set(observed)), 2)

    def test_qualification_warns_only_for_unavailability_and_removes_stale_report(self):
        data = b"fixture"
        digest = hashlib.sha256(data).hexdigest()
        catalog = {digest: {"size": len(data), "provenance": [{"commit": "fixture", "path": "image", "ref": "vendor"}]}}
        with tempfile.TemporaryDirectory(prefix="bl1-qualification-") as temporary:
            report = Path(temporary) / "report.json"
            for error in (vendor.VendorUnavailable("offline"), ReconstructionError("signature rejected")):
                report.write_text('{"status": "verified"}')
                with patch.object(qualification.sys, "argv", ["qualify", "--download", "--allow-unavailable", "--report", str(report)]), \
                        patch.object(qualification, "load_catalog", return_value=catalog), \
                        patch.object(qualification, "git_bytes", return_value=data), \
                        patch.object(qualification, "verify_payloads", side_effect=error):
                    if isinstance(error, vendor.VendorUnavailable):
                        with io.StringIO() as output, redirect_stderr(output):
                            qualification.main()
                            self.assertIn("WARNING", output.getvalue())
                        self.assertEqual(json.loads(report.read_text())["status"], "unavailable")
                    else:
                        with self.assertRaisesRegex(ReconstructionError, "signature rejected"):
                            qualification.main()
                        self.assertFalse(report.exists())


if __name__ == "__main__":
    unittest.main()
