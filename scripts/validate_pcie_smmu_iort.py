#!/usr/bin/env python3
"""Validate the PCIe SMMUv3 topology in a generated CIX IORT."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import struct


IORT_OUTPUT = Path(
    "AARCH64/Platform/CIX/Sky1/Drivers/AcpiSocTables/"
    "AcpiSocTables/OUTPUT/Iort.acpi"
)
IORT_HEADER_SIZE = 48
IORT_NODE_HEADER_SIZE = 16
IORT_ID_MAPPING_SIZE = 20
IORT_TYPE_ITS_GROUP = 0
IORT_TYPE_ROOT_COMPLEX = 2
IORT_TYPE_SMMUV3 = 4
PCIE_SMMUV3_BASE = 0x0B010000


class ValidationError(ValueError):
    """Raised when a generated IORT violates the expected PCIe topology."""


@dataclass(frozen=True)
class IortNode:
    offset: int
    node_type: int
    length: int
    mapping_count: int
    mapping_offset: int


def unpack_from(data: bytes, offset: int, fmt: str) -> tuple[object, ...]:
    size = struct.calcsize(fmt)
    if offset + size > len(data):
        raise ValidationError(
            f"IORT is truncated at offset {offset}: need {size} bytes"
        )
    return struct.unpack_from(fmt, data, offset)


def parse_nodes(data: bytes) -> tuple[bytes, list[IortNode]]:
    if len(data) < IORT_HEADER_SIZE:
        raise ValidationError(
            f"IORT container is too small: expected at least {IORT_HEADER_SIZE} bytes"
        )

    signature, table_length = unpack_from(data, 0, "<4sI")
    if signature != b"IORT":
        raise ValidationError(f"expected IORT signature, got {signature!r}")
    if table_length > len(data):
        raise ValidationError(
            f"IORT table length {table_length} exceeds container size {len(data)}"
        )
    if table_length < IORT_HEADER_SIZE:
        raise ValidationError(f"invalid IORT table length: {table_length}")

    node_count, node_offset = unpack_from(data, 36, "<II")
    if node_offset < IORT_HEADER_SIZE or node_offset >= table_length:
        raise ValidationError(f"invalid first node offset: {node_offset}")

    table = data[:table_length]
    nodes: list[IortNode] = []
    offset = node_offset
    for index in range(node_count):
        node_type, length, _revision, _identifier, mapping_count, mapping_offset = (
            unpack_from(table, offset, "<BHBI2I")
        )
        if length < IORT_NODE_HEADER_SIZE:
            raise ValidationError(f"node {index} has invalid length {length}")
        if offset + length > table_length:
            raise ValidationError(
                f"node {index} at {offset} overruns table length {table_length}"
            )
        if mapping_count:
            mapping_end = (
                offset + mapping_offset + mapping_count * IORT_ID_MAPPING_SIZE
            )
            if mapping_offset < IORT_NODE_HEADER_SIZE or mapping_end > offset + length:
                raise ValidationError(f"node {index} has invalid ID mappings")
        nodes.append(
            IortNode(
                offset=offset,
                node_type=node_type,
                length=length,
                mapping_count=mapping_count,
                mapping_offset=mapping_offset,
            )
        )
        offset += length

    if offset != table_length:
        raise ValidationError(
            f"{node_count} nodes end at {offset}, not table length {table_length}"
        )
    return table, nodes


def validate_pcie_smmu_iort(path: Path, expect: str) -> dict[str, int | str]:
    table, nodes = parse_nodes(path.read_bytes())
    nodes_by_offset = {node.offset: node for node in nodes}
    root_complexes = [
        node for node in nodes if node.node_type == IORT_TYPE_ROOT_COMPLEX
    ]
    if len(root_complexes) != 1:
        raise ValidationError(
            f"expected exactly one PCI root-complex node, got {len(root_complexes)}"
        )
    root = root_complexes[0]
    if root.mapping_count != 1:
        raise ValidationError(
            f"PCI root complex must have one ID mapping, got {root.mapping_count}"
        )

    mapping = root.offset + root.mapping_offset
    output_reference = unpack_from(table, mapping + 12, "<I")[0]
    target = nodes_by_offset.get(output_reference)
    if target is None:
        raise ValidationError(
            f"PCI root mapping refers to missing node at {output_reference:#x}"
        )

    pcie_smmus = []
    for node in nodes:
        if node.node_type != IORT_TYPE_SMMUV3:
            continue
        base_address = unpack_from(table, node.offset + 16, "<Q")[0]
        if base_address == PCIE_SMMUV3_BASE:
            pcie_smmus.append(node)

    if expect == "enabled":
        if len(pcie_smmus) != 1:
            raise ValidationError(
                f"expected one PCIe SMMUv3 node at {PCIE_SMMUV3_BASE:#x}, "
                f"got {len(pcie_smmus)}"
            )
        if target != pcie_smmus[0]:
            raise ValidationError(
                "PCI root complex does not map through the PCIe SMMUv3 node"
            )
    else:
        if pcie_smmus:
            raise ValidationError(
                f"PCIe SMMUv3 node at {PCIE_SMMUV3_BASE:#x} must be absent"
            )
        if target.node_type != IORT_TYPE_ITS_GROUP:
            raise ValidationError(
                "PCI root complex must map directly to the ITS when SMMU is disabled"
            )

    return {
        "node_count": len(nodes),
        "root_offset": root.offset,
        "target_offset": target.offset,
        "target": "SMMUv3" if target.node_type == IORT_TYPE_SMMUV3 else "ITS",
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--expect", choices=("enabled", "disabled"), required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    path = args.build_dir.resolve() / IORT_OUTPUT
    try:
        result = validate_pcie_smmu_iort(path, args.expect)
    except (OSError, ValidationError) as exc:
        print(f"[validate-pcie-smmu] ERROR: {exc}")
        return 1

    print(
        "[validate-pcie-smmu] PCI root topology valid: "
        f"nodes={result['node_count']}, target={result['target']} "
        f"at {result['target_offset']:#x}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
