#!/usr/bin/env python3

from pathlib import Path
import struct
import tempfile
import unittest

from validate_mpam_build import (
    MPAM_GUID,
    MPAM_MODULE,
    MPAM_OUTPUT,
    ValidationError,
    validate_mpam_build,
)


def make_mpam_table() -> bytes:
    data = bytearray(132)
    struct.pack_into("<4sIBB", data, 0, b"MPAM", 132, 2, 0)
    data[10:16] = b"CIXTEK"
    data[16:24] = b"SKY1EDK2"
    struct.pack_into("<HBBI", data, 36, 96, 0, 0, 1)
    struct.pack_into("<Q", data, 44, 0x0F010000)
    struct.pack_into("<I", data, 52, 0x10000)
    struct.pack_into("<I", data, 104, 1)
    struct.pack_into("<IBHB", data, 108, 1, 0, 0, 0)
    struct.pack_into("<QII", data, 116, 1, 0, 0)
    return bytes(data)


def write_present_build(build_dir: Path) -> Path:
    table_path = build_dir / MPAM_OUTPUT
    table_path.parent.mkdir(parents=True)
    table_path.write_bytes(make_mpam_table() + bytes(412))

    ffs_path = build_dir / "FV" / "Ffs" / MPAM_MODULE / f"{MPAM_GUID}.ffs"
    ffs_path.parent.mkdir(parents=True)
    ffs_path.write_bytes(b"ffs")
    (build_dir / "FV" / "FVMAIN.inf").write_text(
        f"EFI_FILE_NAME = {MPAM_GUID}.ffs\n",
        encoding="utf-8",
    )
    return table_path


class ValidateMpamBuildTest(unittest.TestCase):
    def test_accepts_expected_present_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build_dir = Path(directory)
            write_present_build(build_dir)
            result = validate_mpam_build(build_dir, "present")

        self.assertIsNotNone(result)
        self.assertEqual(result["table_length"], 132)
        self.assertEqual(result["cache_reference"], 1)

    def test_rejects_wrong_msc_base(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build_dir = Path(directory)
            table_path = write_present_build(build_dir)
            data = bytearray(table_path.read_bytes())
            struct.pack_into("<Q", data, 44, 0x0F020000)
            table_path.write_bytes(data)

            with self.assertRaisesRegex(ValidationError, "MSC base address"):
                validate_mpam_build(build_dir, "present")

    def test_accepts_clean_absent_build(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build_dir = Path(directory)
            (build_dir / "FV").mkdir(parents=True)
            (build_dir / "FV" / "FVMAIN.inf").write_text("", encoding="utf-8")
            self.assertIsNone(validate_mpam_build(build_dir, "absent"))

    def test_rejects_stale_artifacts_when_absent(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build_dir = Path(directory)
            write_present_build(build_dir)
            with self.assertRaisesRegex(ValidationError, "must be absent"):
                validate_mpam_build(build_dir, "absent")


if __name__ == "__main__":
    unittest.main()
