#!/usr/bin/env python3
"""Validate the optional CIX MPAM table in an EDK2 build output."""

from __future__ import annotations

import argparse
from pathlib import Path
import struct


MPAM_GUID = "427A981E-C9DA-4E89-A183-C73F16EEB2F3"
MPAM_MODULE = f"{MPAM_GUID}SocMpamAcpiTable"
MPAM_OUTPUT = Path(
    "AARCH64/Platform/CIX/Sky1/Drivers/AcpiSocTables/"
    "AcpiMpamTables/OUTPUT/Mpam.acpi"
)
MPAM_TABLE_LENGTH = 132


class ValidationError(ValueError):
    """Raised when a generated MPAM artifact violates the expected contract."""


def require_equal(name: str, actual: object, expected: object) -> None:
    if actual != expected:
        raise ValidationError(f"{name}: expected {expected!r}, got {actual!r}")


def unpack_from(data: bytes, offset: int, fmt: str) -> tuple[object, ...]:
    size = struct.calcsize(fmt)
    if offset + size > len(data):
        raise ValidationError(
            f"table is truncated at offset {offset}: need {size} bytes"
        )
    return struct.unpack_from(fmt, data, offset)


def validate_mpam_table(path: Path) -> dict[str, int]:
    data = path.read_bytes()
    if len(data) < MPAM_TABLE_LENGTH:
        raise ValidationError(
            f"{path}: expected at least {MPAM_TABLE_LENGTH} bytes, got {len(data)}"
        )

    signature, length, revision, checksum = unpack_from(data, 0, "<4sIBB")
    require_equal("signature", signature, b"MPAM")
    require_equal("table length", length, MPAM_TABLE_LENGTH)
    require_equal("revision", revision, 2)
    require_equal("build-time checksum", checksum, 0)
    require_equal("OEM ID", data[10:16], b"CIXTEK")
    require_equal("OEM table ID", data[16:24], b"SKY1EDK2")

    node_length, interface_type, reserved, identifier = unpack_from(
        data, 36, "<HBBI"
    )
    base_address = unpack_from(data, 44, "<Q")[0]
    (
        mmio_size,
        overflow_interrupt,
        overflow_flags,
        reserved1,
        overflow_affinity,
        error_interrupt,
        error_flags,
        reserved2,
        error_affinity,
        max_nrdy_usec,
    ) = unpack_from(data, 52, "<10I")
    linked_hardware_id = unpack_from(data, 92, "<Q")[0]
    linked_instance_id, resource_count = unpack_from(data, 100, "<II")

    require_equal("MSC node length", node_length, 96)
    require_equal("MSC interface type", interface_type, 0)
    require_equal("MSC reserved byte", reserved, 0)
    require_equal("MSC identifier", identifier, 1)
    require_equal("MSC base address", base_address, 0x0F010000)
    require_equal("MSC MMIO size", mmio_size, 0x10000)
    for name, value in (
        ("overflow interrupt", overflow_interrupt),
        ("overflow interrupt flags", overflow_flags),
        ("MSC reserved field 1", reserved1),
        ("overflow interrupt affinity", overflow_affinity),
        ("error interrupt", error_interrupt),
        ("error interrupt flags", error_flags),
        ("MSC reserved field 2", reserved2),
        ("error interrupt affinity", error_affinity),
        ("maximum not-ready time", max_nrdy_usec),
        ("linked hardware ID", linked_hardware_id),
        ("linked instance ID", linked_instance_id),
    ):
        require_equal(name, value, 0)
    require_equal("resource count", resource_count, 1)

    resource_id, ris_index, resource_reserved, locator_type = unpack_from(
        data, 108, "<IBHB"
    )
    cache_reference, locator_reserved, dependency_count = unpack_from(
        data, 116, "<QII"
    )
    require_equal("resource identifier", resource_id, 1)
    require_equal("RIS index", ris_index, 0)
    require_equal("resource reserved field", resource_reserved, 0)
    require_equal("resource locator type", locator_type, 0)
    require_equal("PPTT cache reference", cache_reference, 1)
    require_equal("cache locator reserved field", locator_reserved, 0)
    require_equal("functional dependency count", dependency_count, 0)

    return {
        "container_size": len(data),
        "table_length": length,
        "msc_base": base_address,
        "msc_size": mmio_size,
        "cache_reference": cache_reference,
    }


def validate_mpam_build(build_dir: Path, expect: str) -> dict[str, int] | None:
    table_path = build_dir / MPAM_OUTPUT
    ffs_path = build_dir / "FV" / "Ffs" / MPAM_MODULE / f"{MPAM_GUID}.ffs"
    fv_inf_path = build_dir / "FV" / "FVMAIN.inf"
    fv_inf = fv_inf_path.read_text(encoding="utf-8") if fv_inf_path.is_file() else ""

    if expect == "absent":
        stale = [
            str(path)
            for path in (table_path, ffs_path)
            if path.exists()
        ]
        if MPAM_GUID in fv_inf:
            stale.append(f"{fv_inf_path} contains {MPAM_GUID}")
        if stale:
            raise ValidationError(
                "MPAM must be absent when firmware fixes are disabled: "
                + ", ".join(stale)
            )
        return None

    if not table_path.is_file():
        raise ValidationError(f"MPAM table output is missing: {table_path}")
    if not ffs_path.is_file() or ffs_path.stat().st_size == 0:
        raise ValidationError(f"MPAM FFS output is missing or empty: {ffs_path}")
    if MPAM_GUID not in fv_inf:
        raise ValidationError(f"{fv_inf_path} does not contain {MPAM_GUID}")
    return validate_mpam_table(table_path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--expect", choices=("present", "absent"), required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        result = validate_mpam_build(args.build_dir.resolve(), args.expect)
    except (OSError, UnicodeError, ValidationError) as exc:
        print(f"[validate-mpam] ERROR: {exc}")
        return 1

    if result is None:
        print("[validate-mpam] MPAM correctly absent")
    else:
        print(
            "[validate-mpam] MPAM valid: "
            f"table={result['table_length']} bytes, "
            f"MSC=0x{result['msc_base']:x}+0x{result['msc_size']:x}, "
            f"PPTT cache ID={result['cache_reference']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
