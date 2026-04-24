#!/usr/bin/env python3

from __future__ import annotations

import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

import export_firmware_payload


class ExportFirmwarePayloadTests(unittest.TestCase):
    def write_bytes(self, path: Path, data: bytes) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def write_text(self, path: Path, data: str) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(data, encoding="utf-8")

    def create_repo(self, root: Path, board: str) -> None:
        target = "RELEASE_GCC5"
        build_dir = root / "src" / "Build" / board / target
        active_platform = f"src/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.dsc"
        flash_definition = f"src/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.fdf"

        self.write_bytes(build_dir / "cix_flash_all.bin", b"all")
        self.write_bytes(build_dir / "cix_flash_ota.bin", b"ota")
        self.write_text(
            build_dir / "BuildOptions",
            "\n".join(
                (
                    f"Active Platform: {active_platform}",
                    f"Flash Image Definition: {flash_definition}",
                )
            ),
        )
        self.write_bytes(build_dir / "AARCH64" / "EnrollFromDefaultKeysApp.efi", b"enroll")
        self.write_bytes(build_dir / "AARCH64" / "VariableInfo.efi", b"varinfo")
        self.write_bytes(build_dir / "AARCH64" / "Shell.efi", b"shell")
        self.write_bytes(build_dir / "Firmwares" / "bootloader3.img", b"bootloader3")

        flash_tool_dir = (
            root / "src" / "edk2-non-osi" / "Platform" / "CIX" / "Sky1" / "FlashTool"
        )
        self.write_bytes(flash_tool_dir / "BurnImage.efi", b"burn")
        self.write_bytes(flash_tool_dir / "FlashUpdate.efi", b"flash")

        self.write_bytes(
            root
            / "src"
            / "edk2-non-osi"
            / "Emulator"
            / "X86EmulatorDxe"
            / "AArch64"
            / "LoadOpRom.efi",
            b"loadoprom",
        )
        self.write_text(root / "src" / "scripts" / "startup.nsh", "echo update\n")

        self.write_text(
            root / "src" / "edk2-platforms" / "Platform" / "Radxa" / "Orion" / board / f"{board}.dsc",
            "# dsc\n",
        )
        self.write_text(
            root / "src" / "edk2-platforms" / "Platform" / "Radxa" / "Orion" / board / f"{board}.fdf",
            "# fdf\n",
        )

    def test_payload_mapping_flattens_custom_o6_exports(self) -> None:
        mapping = export_firmware_payload.payload_mapping(
            Path("/repo"),
            "O6",
            "RELEASE_GCC5",
            "custom",
        )
        destinations = [destination.as_posix() for _, destination in mapping]

        self.assertIn("BuildOptions", destinations)
        self.assertIn("startup.nsh", destinations)
        self.assertIn("tools/LoadOpRom.efi", destinations)
        self.assertFalse(any(destination.startswith("orion-o6/") for destination in destinations))

    def test_payload_mapping_omits_load_op_rom_when_not_custom_o6(self) -> None:
        for board, artefact_mode in (("O6N", "custom"), ("O6", "upstream")):
            with self.subTest(board=board, artefact_mode=artefact_mode):
                mapping = export_firmware_payload.payload_mapping(
                    Path("/repo"),
                    board,
                    "RELEASE_GCC5",
                    artefact_mode,
                )
                destinations = [destination.as_posix() for _, destination in mapping]
                self.assertNotIn("tools/LoadOpRom.efi", destinations)

    def test_stage_payload_zeroes_helper_efi_files_for_custom_o6(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir_text:
            repo_root = Path(tempdir_text)
            self.create_repo(repo_root, "O6")
            output_dir = repo_root / "out"
            zeroed_sources: list[str] = []

            def fake_zero_debug_metadata(_: Path, source: Path, destination: Path) -> None:
                zeroed_sources.append(source.name)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)

            with mock.patch.object(
                export_firmware_payload,
                "resolve_genfw",
                return_value=repo_root / "GenFw",
            ), mock.patch.object(
                export_firmware_payload,
                "zero_debug_metadata",
                side_effect=fake_zero_debug_metadata,
            ):
                export_firmware_payload.stage_payload(
                    repo_root,
                    "O6",
                    "orion-o6",
                    "RELEASE_GCC5",
                    "custom",
                    output_dir,
                )

            self.assertTrue((output_dir / "BuildOptions").is_file())
            self.assertTrue((output_dir / "startup.nsh").is_file())
            self.assertTrue((output_dir / "tools" / "LoadOpRom.efi").is_file())
            self.assertFalse((output_dir / "orion-o6").exists())
            self.assertEqual(
                sorted(zeroed_sources),
                ["BurnImage.efi", "FlashUpdate.efi", "LoadOpRom.efi"],
            )

    def test_audit_custom_payload_includes_nested_tool_paths(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir_text:
            tempdir = Path(tempdir_text)
            stage_root = tempdir / "stage"
            build_dir = tempdir / "build"
            self.write_bytes(stage_root / "BuildOptions", b"options")
            self.write_bytes(stage_root / "tools" / "LoadOpRom.efi", b"tool")
            self.write_bytes(build_dir / "Firmwares" / "bootloader3.img", b"bootloader")
            captured: dict[str, list[tuple[str, Path]]] = {}

            def fake_audit_targets(targets: list[tuple[str, Path]]) -> list[object]:
                captured["targets"] = targets
                return []

            with mock.patch.object(
                export_firmware_payload,
                "audit_targets",
                side_effect=fake_audit_targets,
            ):
                export_firmware_payload.audit_custom_payload(
                    stage_root,
                    build_dir,
                    "O6",
                    "RELEASE_GCC5",
                )

            target_names = [name for name, _ in captured["targets"]]
            self.assertIn("tools/LoadOpRom.efi", target_names)
            self.assertIn("Build/O6/RELEASE_GCC5/Firmwares/bootloader3.img", target_names)


if __name__ == "__main__":
    unittest.main()
