#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from audit_final_image_manifest import compare_manifests, parse_ffs_manifest


GUID = "12345678-1234-1234-1234-1234567890AB"


class AuditFinalImageManifestTests(unittest.TestCase):
    def test_parse_ffs_manifest_supports_oi_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir_text:
            repo_root = Path(tempdir_text)
            module_dir = repo_root / "Ffs" / f"{GUID}ExampleDxe"
            module_dir.mkdir(parents=True)

            ffs_path = module_dir / f"{GUID}ExampleDxe.ffs"
            ffs_path.write_bytes(b"ffs")

            pe32_path = module_dir / f"{GUID}SEC1.1.pe32"
            pe32_path.write_bytes(b"pe32")
            pe32_sidecar = Path(str(pe32_path) + ".txt")
            source_path = repo_root / "src" / "edk2-platforms" / "Platform" / "Radxa" / "ExampleDxe.efi"
            source_path.parent.mkdir(parents=True)
            source_path.write_bytes(b"efi")
            pe32_sidecar.write_text(
                f'GenSec -s EFI_SECTION_PE32 -o "{module_dir / "out.pe32"}" "{source_path}"\n',
                encoding="utf-8",
            )

            ui_path = module_dir / f"{GUID}SEC2.ui"
            ui_path.write_bytes("ExampleDxe".encode("utf-16le"))

            manifest_path = module_dir / f"{GUID}ExampleDxe.ffs.txt"
            manifest_path.write_text(
                " ".join(
                    (
                        "GenFfs",
                        "-t",
                        "EFI_FV_FILETYPE_DRIVER",
                        "-g",
                        GUID,
                        "-oi",
                        str(pe32_path),
                        "-oi",
                        str(ui_path),
                    )
                )
                + "\n",
                encoding="utf-8",
            )

            manifest = parse_ffs_manifest(
                module_dir,
                repo_root,
                {GUID: "ExampleDxe"},
                {GUID: [{"fv": "FVMAIN", "offset": "0x00000000"}]},
            )

        self.assertEqual(manifest["guid"], GUID)
        self.assertEqual(len(manifest["sections"]), 2)
        self.assertEqual(manifest["sections"][0]["type"], "EFI_SECTION_PE32")
        self.assertEqual(
            manifest["sections"][0]["source"],
            "src/edk2-platforms/Platform/Radxa/ExampleDxe.efi",
        )
        self.assertEqual(manifest["sections"][1]["type"], "EFI_SECTION_USER_INTERFACE")
        self.assertEqual(manifest["sections"][1]["ui_name"], "ExampleDxe")

    def test_compare_manifests_ignores_unstable_path_metadata(self) -> None:
        expected = {
            "board": "O6",
            "target": "RELEASE_GCC5",
            "fd": {
                "path": "FV/SKY1_BL33_UEFI.fd",
                "size": 16,
                "sha256": "fd",
            },
            "fvs": {
                "FVMAIN": {
                    "name": "FVMAIN",
                    "total_size": "0x10",
                    "taken_size": "0x10",
                    "exists": True,
                    "size": 16,
                    "sha256": "fv",
                    "entries": [
                        {
                            "guid": GUID,
                            "offset": "0x00000000",
                            "module": "/workspaces/edk2-cix-offline-audits/src/Build/O6/RELEASE_GCC5/FV/FVMAIN.Fv",
                        }
                    ],
                }
            },
            "modules": [
                {
                    "guid": GUID,
                    "module": "/workspaces/edk2-cix-offline-audits/src/edk2-platforms/Platform/CIX/Sky1/Bin/ntroot.cer",
                    "directory": f"{GUID}ExampleDxe",
                    "ui_name": "ExampleDxe",
                    "file_type": "EFI_FV_FILETYPE_DRIVER",
                    "size": 16,
                    "sha256": "module",
                    "locations": [{"fv": "FVMAIN", "offset": "0x00000000"}],
                    "sections": [
                        {
                            "path": f"{GUID}SEC1.1.pe32",
                            "size": 4,
                            "sha256": "section",
                            "type": "EFI_SECTION_PE32",
                        }
                    ],
                }
            ],
        }
        actual = {
            "board": "O6",
            "target": "RELEASE_GCC5",
            "fd": {
                "path": "FV/SKY1_BL33_UEFI.fd",
                "size": 16,
                "sha256": "different-fd",
            },
            "fvs": {
                "FVMAIN": {
                    "name": "FVMAIN",
                    "total_size": "0x10",
                    "taken_size": "0x10",
                    "exists": True,
                    "size": 16,
                    "sha256": "different-fv",
                    "entries": [
                        {
                            "guid": GUID,
                            "offset": "0x00000000",
                            "module": "/workspaces/edk2-cix/src/Build/O6/RELEASE_GCC5/FV/FVMAIN.Fv",
                        }
                    ],
                }
            },
            "modules": [
                {
                    "guid": GUID,
                    "module": "/workspaces/edk2-cix/src/edk2-platforms/Platform/CIX/Sky1/Bin/ntroot.cer",
                    "directory": f"{GUID}ExampleDxe",
                    "ui_name": "ExampleDxe",
                    "file_type": "EFI_FV_FILETYPE_DRIVER",
                    "size": 16,
                    "sha256": "different-module",
                    "locations": [{"fv": "FVMAIN", "offset": "0x00000000"}],
                    "sections": [
                        {
                            "path": f"{GUID}SEC1.1.pe32",
                            "size": 4,
                            "sha256": "different-section",
                            "source": "src/edk2-platforms/Platform/CIX/Sky1/Bin/ntroot.cer",
                            "type": "EFI_SECTION_PE32",
                        }
                    ],
                }
            ],
        }

        self.assertEqual(compare_manifests(expected, actual), [])

    def test_compare_manifests_still_flags_real_offset_changes(self) -> None:
        expected = {
            "board": "O6",
            "target": "RELEASE_GCC5",
            "fd": {"path": "FV/SKY1_BL33_UEFI.fd", "size": 16, "sha256": "fd"},
            "fvs": {
                "FVMAIN": {
                    "name": "FVMAIN",
                    "total_size": "0x10",
                    "taken_size": "0x10",
                    "exists": True,
                    "size": 16,
                    "sha256": "fv",
                    "entries": [{"guid": GUID, "offset": "0x00000000", "module": "ExampleDxe"}],
                }
            },
            "modules": [
                {
                    "guid": GUID,
                    "module": "ExampleDxe",
                    "directory": f"{GUID}ExampleDxe",
                    "ui_name": "ExampleDxe",
                    "file_type": "EFI_FV_FILETYPE_DRIVER",
                    "size": 16,
                    "sha256": "module",
                    "locations": [{"fv": "FVMAIN", "offset": "0x00000000"}],
                    "sections": [],
                }
            ],
        }
        actual = {
            **expected,
            "fvs": {
                "FVMAIN": {
                    **expected["fvs"]["FVMAIN"],
                    "entries": [{"guid": GUID, "offset": "0x00000020", "module": "ExampleDxe"}],
                }
            },
            "modules": [
                {
                    **expected["modules"][0],
                    "locations": [{"fv": "FVMAIN", "offset": "0x00000020"}],
                }
            ],
        }

        mismatches = compare_manifests(expected, actual)
        self.assertIn("Changed firmware volumes: FVMAIN", mismatches)
        self.assertIn(f"Changed FFS modules: {GUID}", mismatches)


if __name__ == "__main__":
    unittest.main()
