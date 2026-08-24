#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import importlib.util
import json
import pathlib
import tempfile
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "src" / "scripts" / "replay_o6_release.py"
SPEC = importlib.util.spec_from_file_location("replay_o6_release", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
REPLAY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPLAY)


class ReplayO6ReleaseTests(unittest.TestCase):
    def test_replay_uses_a_separate_structural_acpi_profile(self) -> None:
        makefile = (REPO_ROOT / ".github" / "local" / "Makefile.local").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "DETERMINISTIC_REPLAY_ACPI_PROFILE ?= upstream-$(word 1,",
            makefile,
        )
        self.assertIn('--profile "$(DETERMINISTIC_REPLAY_ACPI_PROFILE)"', makefile)
        self.assertIn(
            '--baseline-file "$(DETERMINISTIC_REPLAY_ACPI_BASELINE)"', makefile
        )

        structures = json.loads(
            (REPO_ROOT / "validation" / "replay-acpi-structures.json").read_text(
                encoding="utf-8"
            )
        )["profiles"]
        self.assertEqual(
            set(structures), {"upstream-1.2-bookworm", "upstream-1.3-bookworm"}
        )
        for profile in structures.values():
            self.assertEqual(set(profile["boards"]), {"O6", "O6N"})

        for board in ("O6", "O6N"):
            tables_12 = set(
                structures["upstream-1.2-bookworm"]["boards"][board]["acpi"][
                    "tables"
                ]
            )
            tables_13 = set(
                structures["upstream-1.3-bookworm"]["boards"][board]["acpi"][
                    "tables"
                ]
            )
            self.assertEqual(
                tables_13 - tables_12,
                {
                    "AARCH64/Platform/CIX/Sky1/Drivers/AcpiSocTables/"
                    "AcpiSocTables/OUTPUT/Iort-NoSmmu.acpi"
                },
            )

    def test_recorded_replay_inputs_match_the_validation_profiles(self) -> None:
        replay_root = REPO_ROOT / "validation" / "replay-inputs"
        index = json.loads((replay_root / "index.json").read_text(encoding="utf-8"))
        profiles = json.loads(
            (REPO_ROOT / "validation" / "expected-hashes.json").read_text(
                encoding="utf-8"
            )
        )["profiles"]
        self.assertEqual(
            set(index["releases"]),
            {"1.2.1", "1.2.2", "1.2.3", "1.2.4", "1.3.0", "1.3.1"},
        )

        for version, release in index["releases"].items():
            profile = profiles[release["profile"]]
            for board, board_index in release["boards"].items():
                with self.subTest(version=version, board=board):
                    board_root = replay_root / version / board
                    manifest = json.loads(
                        (replay_root / board_index["manifest"]).read_text(
                            encoding="utf-8"
                        )
                    )
                    self.assertEqual(
                        manifest["reference_artefacts"],
                        profile["boards"][board]["artefacts"],
                    )
                    for name, expected in manifest["certificates"].items():
                        payload = (board_root / "certs" / name).read_bytes()
                        self.assertEqual(len(payload), expected["size"])
                        self.assertEqual(hashlib.sha256(payload).hexdigest(), expected["sha256"])
                    env = (board_root / "replay.env").read_text(encoding="utf-8")
                    self.assertNotIn("SIGNING_CERT_SOURCE_DIR=", env)
                    self.assertNotIn("EXACT_REPLAY_NT_FW_SOURCE=", env)
                    for name in (
                        "SOURCE_COMMIT",
                        "SOURCE_COMMIT_HASH_LENGTH",
                        "EDK2_SOURCE_COMMIT",
                        "EDK2_COMMIT_HASH_LENGTH",
                        "EDK2_NON_OSI_SOURCE_COMMIT",
                        "EDK2_NON_OSI_COMMIT_HASH_LENGTH",
                        "EDK2_PLATFORMS_SOURCE_COMMIT",
                        "EDK2_PLATFORMS_COMMIT_HASH_LENGTH",
                    ):
                        self.assertIn(f"{name}=", env)

    def test_recorded_flash_layouts_cover_every_replay_release(self) -> None:
        expected_types = {
            **{version: {1, 2, 3, 4, 6, 7, 8, 100} for version in (
                "1.2.1", "1.2.2", "1.2.3", "1.2.4"
            )},
            "1.3.0": {1, 2, 3, 4, 6, 7, 8, 9, 100},
            "1.3.1": {1, 2, 3, 4, 6, 7, 8, 9, 100},
        }
        relative = pathlib.Path(
            "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/"
            "spi_flash_config_all.json"
        )
        for version, image_types in expected_types.items():
            with self.subTest(version=version):
                path = REPO_ROOT / "validation" / "replay-source" / version / relative
                config = json.loads(path.read_text(encoding="utf-8"))
                recorded_types = {
                    entry["image_type"] for entry in config["image_header_groups"]
                }
                self.assertEqual(recorded_types, image_types)
                self.assertEqual(config["image_count"], len(recorded_types))

    def test_copies_published_nt_fw_as_reference_only(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            root = pathlib.Path(tempdir)
            flash_dir = root / "flash"
            output_dir = root / "output"
            flash_dir.mkdir()
            (flash_dir / "nt-fw.bin").write_bytes(b"published-bl33")

            reference = REPLAY.copy_reference_files(
                {"FV/SKY1_BL33_UEFI.fd": flash_dir / "nt-fw.bin"}, output_dir
            )

            assert reference is not None
            copied = reference / "FV" / "SKY1_BL33_UEFI.fd"
            self.assertEqual(copied.read_bytes(), b"published-bl33")

    def test_rebuild_wrapper_rebuilds_nt_fw_from_source(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            root = pathlib.Path(tempdir)
            wrapper = root / "rebuild.sh"
            env_values = {
                "FIRMWARE_BOARD": "O6",
                "BUILD_DATE": "2026-03-19T00:00:00+00:00",
                "SOURCE_DATE_EPOCH": "1773878400",
                "PM_CONFIG_SOURCE_DATE_EPOCH": "1773878400",
                "SIGNING_CERT_SOURCE_DIR": str(root / "certs"),
            }

            REPLAY.write_rebuild_wrapper(wrapper, env_values, ("firmware",))

            text = wrapper.read_text(encoding="utf-8")
            self.assertIn("ARTEFACT_MODE=upstream", text)
            self.assertNotIn("EXACT_REPLAY_NT_FW_SOURCE=", text)


if __name__ == "__main__":
    unittest.main()
