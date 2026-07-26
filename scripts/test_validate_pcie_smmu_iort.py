#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import struct
import tempfile
import unittest

from validate_pcie_smmu_iort import (
    IORT_TYPE_ITS_GROUP,
    IORT_TYPE_ROOT_COMPLEX,
    IORT_TYPE_SMMUV3,
    PCIE_SMMUV3_BASE,
    ValidationError,
    validate_pcie_smmu_iort,
)


def node_header(
    node_type: int,
    length: int,
    mapping_count: int = 0,
    mapping_offset: int = 0,
) -> bytes:
    return struct.pack(
        "<BHBI2I",
        node_type,
        length,
        0,
        0,
        mapping_count,
        mapping_offset,
    )


def its_node() -> bytes:
    return node_header(IORT_TYPE_ITS_GROUP, 24) + struct.pack("<2I", 1, 0)


def smmuv3_node(base_address: int, its_offset: int) -> bytes:
    length = 88
    data = bytearray(node_header(IORT_TYPE_SMMUV3, length, 1, 68))
    data.extend(struct.pack("<Q", base_address))
    data.extend(bytes(44))
    data.extend(struct.pack("<5I", 0, 0xFFFF, 0, its_offset, 0))
    return bytes(data)


def root_complex_node(output_reference: int) -> bytes:
    length = 60
    data = bytearray(node_header(IORT_TYPE_ROOT_COMPLEX, length, 1, 40))
    data.extend(bytes(24))
    data.extend(struct.pack("<5I", 0, 0xFFFF, 0, output_reference, 0))
    return bytes(data)


def iort_table(enabled: bool) -> bytes:
    first_node = 48
    nodes = [its_node()]
    root_target = first_node
    if enabled:
        root_target = first_node + len(nodes[0])
        nodes.append(smmuv3_node(PCIE_SMMUV3_BASE, first_node))
    nodes.append(root_complex_node(root_target))
    table_length = first_node + sum(len(node) for node in nodes)
    header = bytearray(table_length)
    struct.pack_into("<4sI", header, 0, b"IORT", table_length)
    struct.pack_into("<II", header, 36, len(nodes), first_node)
    offset = first_node
    for node in nodes:
        header[offset : offset + len(node)] = node
        offset += len(node)
    return bytes(header)


class ValidatePcieSmmuIortTests(unittest.TestCase):
    def validate(self, data: bytes, expect: str) -> dict[str, int | str]:
        with tempfile.TemporaryDirectory() as tempdir:
            path = Path(tempdir) / "Iort.acpi"
            path.write_bytes(data)
            return validate_pcie_smmu_iort(path, expect)

    def test_accepts_enabled_pcie_smmu_topology(self) -> None:
        result = self.validate(iort_table(enabled=True), "enabled")
        self.assertEqual(result["target"], "SMMUv3")
        self.assertEqual(result["node_count"], 3)

    def test_accepts_disabled_direct_its_topology(self) -> None:
        result = self.validate(iort_table(enabled=False), "disabled")
        self.assertEqual(result["target"], "ITS")
        self.assertEqual(result["node_count"], 2)

    def test_rejects_enabled_topology_when_disabled_is_expected(self) -> None:
        with self.assertRaisesRegex(ValidationError, "must be absent"):
            self.validate(iort_table(enabled=True), "disabled")

    def test_rejects_disabled_topology_when_enabled_is_expected(self) -> None:
        with self.assertRaisesRegex(ValidationError, "expected one PCIe SMMUv3"):
            self.validate(iort_table(enabled=False), "enabled")


if __name__ == "__main__":
    unittest.main()
