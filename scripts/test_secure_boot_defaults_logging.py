#!/usr/bin/env python3

from __future__ import annotations

import io
import pathlib
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from types import SimpleNamespace
from unittest import mock


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

import generate_microsoft_secure_boot_defaults


def read_repo_text(relative_path: str) -> str:
    return (REPO_ROOT / relative_path).read_text(encoding="utf-8")


class SecureBootDefaultsLoggingTests(unittest.TestCase):
    def test_custom_build_inputs_no_longer_refresh_payloads_twice(self) -> None:
        content = read_repo_text("src/Makefile")
        self.assertIn(
            '$(if $(filter custom,$(ARTEFACT_MODE)),check-microsoft-secure-boot-release)',
            content,
        )
        self.assertNotIn(
            '$(if $(filter custom,$(ARTEFACT_MODE)),refresh-microsoft-secure-boot-defaults check-microsoft-secure-boot-release)',
            content,
        )

    def test_no_change_message_lists_payload_names(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir_text:
            tempdir = pathlib.Path(tempdir_text)
            args = SimpleNamespace(
                arch="aarch64",
                input_dir=tempdir / "inputs",
                output_dir=tempdir / "outputs",
                manifest=tempdir / "manifest.lock.json",
                check=False,
                no_fetch=True,
            )
            manifest = {"target_arch": "aarch64"}
            payloads = {
                "PK.bin": b"pk",
                "KEK.bin": b"kek",
                "DB.bin": b"db",
                "DBX.bin": b"dbx",
            }
            stderr = io.StringIO()
            with mock.patch.object(
                generate_microsoft_secure_boot_defaults,
                "parse_args",
                return_value=args,
            ), mock.patch.object(
                generate_microsoft_secure_boot_defaults,
                "load_manifest",
                return_value=manifest,
            ), mock.patch.object(
                generate_microsoft_secure_boot_defaults,
                "ensure_inputs",
                return_value=None,
            ), mock.patch.object(
                generate_microsoft_secure_boot_defaults,
                "build_payloads",
                return_value=payloads,
            ), mock.patch.object(
                generate_microsoft_secure_boot_defaults,
                "check_generated_payload_hashes",
                return_value=[],
            ), mock.patch.object(
                generate_microsoft_secure_boot_defaults,
                "_write_payloads",
                return_value=[],
            ), redirect_stderr(stderr):
                self.assertEqual(generate_microsoft_secure_boot_defaults.main(), 0)

        self.assertIn(
            "Microsoft Secure Boot payloads already matched manifest.lock.json: DB.bin, DBX.bin, KEK.bin, PK.bin.",
            stderr.getvalue(),
        )


if __name__ == "__main__":
    unittest.main()
