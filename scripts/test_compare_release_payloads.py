#!/usr/bin/env python3

from __future__ import annotations

import argparse
import importlib.util
import json
import pathlib
import tempfile
import unittest
from unittest import mock

import compare_release_payloads


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]


class CompareReleasePayloadsTests(unittest.TestCase):
    def test_replay_creates_report_root_before_first_buildbox(self) -> None:
        makefile = (REPO_ROOT / ".github/local/Makefile.local").read_text(
            encoding="utf-8"
        )
        replay_start = makefile.index("__deterministic-replay:")
        replay = makefile[replay_start:]

        self.assertLess(
            replay.index('mkdir -p "$(REPO_ROOT)/build-validation"'),
            replay.index('"$(REPO_ROOT)/scripts/run_in_buildbox.sh"'),
        )

    def test_collects_complete_published_board_payload_at_package_paths(self) -> None:
        replay_path = (
            pathlib.Path(__file__).resolve().parents[1]
            / "src"
            / "scripts"
            / "replay_o6_release.py"
        )
        spec = importlib.util.spec_from_file_location("replay_o6_release", replay_path)
        assert spec is not None and spec.loader is not None
        replay = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(replay)

        with tempfile.TemporaryDirectory() as tempdir:
            release_dir = pathlib.Path(tempdir)
            expected = {
                "cix_flash_all.bin",
                "cix_flash_ota.bin",
                "BuildOptions",
                "BurnImage.efi",
                "EnrollFromDefaultKeysApp.efi",
                "FlashUpdate.efi",
                "Shell.efi",
                "VariableInfo.efi",
                "startup.nsh",
            }
            for relative_name in expected:
                path = release_dir / relative_name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(relative_name.encode("utf-8"))

            references = replay.collect_release_payload_files(release_dir)

            self.assertEqual(set(references), expected)
            self.assertFalse(any(name.startswith("AARCH64/") for name in references))

    def test_uses_staged_payload_then_falls_back_to_internal_build_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            root = pathlib.Path(tempdir)
            reference = root / "reference"
            staged = root / "staged"
            build = root / "build"
            report = root / "report.json"
            for relative_name, destination in (
                ("BurnImage.efi", staged),
                ("startup.nsh", staged),
                ("FV/SKY1_BL33_UEFI.fd", build),
            ):
                payload = relative_name.encode("utf-8")
                for tree in (reference, destination):
                    path = tree / relative_name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(payload)

            args = argparse.Namespace(
                build_dir=staged,
                fallback_build_dir=build,
                reference_dir=reference,
                report_json=report,
                strict=True,
            )
            with mock.patch.object(compare_release_payloads, "parse_args", return_value=args):
                self.assertEqual(compare_release_payloads.main(), 0)

            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(
                payload["summary"],
                {"matched": 3, "mismatched": 0, "missing": 0, "total": 3},
            )
            self.assertEqual(
                pathlib.Path(payload["files"]["FV/SKY1_BL33_UEFI.fd"]["build_path"]),
                (build / "FV/SKY1_BL33_UEFI.fd").resolve(),
            )


if __name__ == "__main__":
    unittest.main()
