#!/usr/bin/env python3
"""Construct a qualification control with native core/CPU and vendor storage.

Called by qualify_stmm_releases.py; never used for production source selection.
The complete vendor FD must match the selected component's existing source pin.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct
import uuid


MODULES = {
    '3aade4ec-63cc-4a48-a928-5a374dd463eb': (
        'FaultTolerantWriteStandaloneMm',
        'MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteStandaloneMm.inf'),
    '7ee2c0c1-c21a-4113-a53a-66824a95696f': (
        'VariableStandaloneMm',
        'MdeModulePkg/Universal/Variable/RuntimeDxe/VariableStandaloneMm.inf'),
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def extract(data: bytes, expected: str) -> dict[str, bytes]:
    require(hashlib.sha256(data).hexdigest() == expected, 'vendor control FD differs from source pin')
    require(len(data) == 2 * 1024 * 1024 and data[40:44] == b'_FVH', 'invalid vendor control FV')
    require(struct.unpack_from('<Q', data, 32)[0] == len(data), 'invalid FV length')
    size = struct.unpack_from('<H', data, 48)[0]
    require(56 <= size <= len(data) and size % 2 == 0, 'invalid FV header length')
    require(sum(struct.unpack_from('<' + str(size // 2) + 'H', data)) & 65535 == 0,
            'invalid FV checksum')
    result = {}
    position = (size + 7) & ~7
    seen = set()
    while position + 24 <= len(data):
        header = data[position:position + 24]
        if header == b'\xff' * 24:
            require(data[position:] == b'\xff' * (len(data) - position), 'non-erased FV tail')
            break
        size = int.from_bytes(header[20:23], 'little')
        require(24 <= size < 0xffffff and position + size <= len(data), 'invalid FFS length')
        require((sum(header) - header[17] - header[23]) & 255 == 0, 'invalid FFS checksum')
        body = data[position + 24:position + size]
        require((sum(body) + header[17]) & 255 == 0 if header[19] & 0x40 else header[17] == 0xaa,
                'invalid FFS data checksum')
        guid = str(uuid.UUID(bytes_le=header[:16]))
        if guid in MODULES:
            require(guid not in seen and header[18] == 0x0e, 'duplicate or wrong storage FFS type')
            seen.add(guid)
            name = MODULES[guid][0]
            sections = {}
            offset = 0
            while offset + 4 <= len(body):
                length = int.from_bytes(body[offset:offset + 3], 'little')
                kind = body[offset + 3]
                require(4 <= length < 0xffffff and offset + length <= len(body), 'invalid section length')
                require(kind not in sections, 'duplicate storage section')
                sections[kind] = body[offset + 4:offset + length]
                offset = (offset + length + 3) & ~3
            require(0x10 in sections and 0x1c in sections and 0x15 in sections,
                    'missing storage PE, DEPEX or name')
            require(sections[0x15].decode('utf-16le').rstrip('\0') == name, 'storage name mismatch')
            pe = sections[0x10]
            require(len(pe) >= 64 and pe[:2] == b'MZ', 'invalid storage PE')
            pe_offset = struct.unpack_from('<I', pe, 60)[0]
            valid_header = pe_offset + 6 <= len(pe) and pe[pe_offset:pe_offset + 4] == b'PE\0\0'
            require(valid_header, 'invalid storage PE header bounds')
            require(struct.unpack_from('<H', pe, pe_offset + 4)[0] == 0xaa64, 'storage PE is not AArch64')
            result[name + '.efi'] = pe
            result[name + '.depex'] = sections[0x1c]
        position = (position + size + 7) & ~7
    require(seen == set(MODULES), 'missing vendor storage modules')
    return result


def configure(source: Path, data: bytes, manifest: dict) -> dict:
    files = extract(data, manifest['source_fd_sha256'])
    target = source / 'CixStandaloneMmPkg/QualificationVendorStorage'
    target.mkdir(exist_ok=False)
    for name, content in files.items():
        (target / name).write_bytes(content)
    fdf = source / 'CixStandaloneMmPkg/CixStandaloneMm.fdf'
    text = fdf.read_text()
    for guid, (name, inf) in MODULES.items():
        old = '  INF ' + inf
        require(text.count(old) == 1, 'storage INF is missing or ambiguous')
        base = 'CixStandaloneMmPkg/QualificationVendorStorage/' + name
        text = text.replace(old, f'  FILE MM_STANDALONE = {guid} {{\n'
                            f'    SECTION SMM_DEPEX = {base}.depex\n'
                            f'    SECTION PE32 = {base}.efi\n'
                            f'    SECTION UI = "{name}"\n  }}')
    fdf.write_text(text)
    return {name: hashlib.sha256(content).hexdigest() for name, content in files.items()}
