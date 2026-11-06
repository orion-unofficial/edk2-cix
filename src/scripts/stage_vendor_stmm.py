#!/usr/bin/env python3
"""Stage a checkpoint-pinned vendor StandaloneMM image for CIX OP-TEE."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import zlib


class StmmError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise StmmError(message)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_bounded(path: Path, maximum: int) -> bytes:
    require(path.is_file() and 0 < path.stat().st_size <= maximum,
            f"missing, empty or oversized StandaloneMM input: {path}")
    return path.read_bytes()


def load_payload(manifest: Path, vendor_fip: Path) -> tuple[dict, bytes]:
    document = json.loads(read_bounded(manifest, 65536))
    require(document.get("schema_version") == 1, "unsupported StandaloneMM manifest")
    fingerprint = sha256(read_bounded(vendor_fip, 8 * 1024 * 1024))
    entries = [entry for entry in document["images"] if entry["vendor_fip_sha256"] == fingerprint]
    require(len(entries) == 1, "vendor FIP does not select exactly one pinned StandaloneMM image")
    entry = entries[0]
    filename = entry["compressed_file"]
    require(Path(filename).name == filename, "StandaloneMM blob must be beside its manifest")
    compressed = read_bounded(manifest.parent / filename, 4 * 1024 * 1024)
    require(sha256(compressed) == entry["compressed_sha256"], "vendor StandaloneMM compressed hash mismatch")
    require(entry["fd_size"] == 2 * 1024 * 1024, "unexpected pinned StandaloneMM size")
    decoder = zlib.decompressobj()
    image = decoder.decompress(compressed, entry["fd_size"] + 1)
    require(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail,
            "vendor StandaloneMM compression is truncated, oversized or has trailing data")
    require(len(image) == entry["fd_size"] and sha256(image) == entry["fd_sha256"],
            "vendor StandaloneMM image hash or size mismatch")
    require(image[40:44] == b"_FVH" and struct.unpack_from("<Q", image, 32)[0] == len(image),
            "vendor StandaloneMM firmware-volume header mismatch")
    header_size = struct.unpack_from("<H", image, 48)[0]
    require(56 <= header_size <= len(image) and header_size % 2 == 0,
            "invalid vendor StandaloneMM firmware-volume header size")
    require(sum(struct.unpack_from("<" + str(header_size // 2) + "H", image)) & 65535 == 0,
            "vendor StandaloneMM firmware-volume checksum mismatch")
    return entry, image


def write_changed(path: Path, data: bytes) -> None:
    if path.is_file() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
        temporary.chmod(0o644)
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def verify_embedded(tee: bytes, image: bytes) -> int:
    matches = []
    start = 0
    while True:
        offset = tee.find(b"\x78", start)
        if offset < 0 or offset + 1 >= len(tee):
            break
        start = offset + 1
        if (tee[offset] * 256 + tee[offset + 1]) % 31:
            continue
        try:
            decoder = zlib.decompressobj()
            decoded = decoder.decompress(tee[offset:], len(image) + 1)
        except zlib.error:
            continue
        if decoder.eof and decoded == image:
            matches.append(offset)
    require(len(matches) == 1, "OP-TEE must contain exactly one copy of the pinned vendor StandaloneMM image")
    return matches[0]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--vendor-fip", type=Path, required=True,
                        help="Original selected vendor FIP, never a source-built staging output")
    operation = parser.add_mutually_exclusive_group(required=True)
    operation.add_argument("--output", type=Path)
    operation.add_argument("--verify-tee", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    try:
        entry, image = load_payload(args.manifest, args.vendor_fip)
        report = {"schema_version": 1, "status": "verified", "vendor_stmm": entry}
        if args.output:
            write_changed(args.output, image)
        else:
            tee = read_bounded(args.verify_tee, 8 * 1024 * 1024)
            report["embedded_offset"] = verify_embedded(tee, image)
            report["source_built_tee_sha256"] = sha256(tee)
        if args.report:
            write_changed(args.report, (json.dumps(report, indent=2, sort_keys=True) + "\n").encode())
        print(f"[cix-release] Verified vendor StandaloneMM {entry['radxa_release']} {entry['fd_sha256']}")
    except (StmmError, OSError, ValueError, KeyError, TypeError, zlib.error) as error:
        print(f"[cix-release] {error}", file=sys.stderr)
        raise SystemExit(1) from error


if __name__ == "__main__":
    main()
