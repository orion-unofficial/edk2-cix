#!/usr/bin/env python3
"""Verify CIX FIP signatures and signed bindings against an external trust anchor.

OpenSSL performs the cryptography. The bounded DER reader only extracts the
signed bytes, keys and TBBR extensions; it does not implement cryptography.
This proves a package's chain against a selected vendor reference, not eFuse
contents, the board's current rollback counters, or runtime compatibility.
"""

from __future__ import annotations

import base64
from dataclasses import dataclass
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile


UUIDS = {
    "soc-fw": "47d4086d4cfe98469b952950cbbd5a00",
    "tos-fw": "05d0e18953dc13478d2b500a4b7a3e38",
    "tos-fw-extra1": "0b70c29b2a5a78409f650a5682738288",
    "tos-fw-extra2": "8ea87bb1cfa23f4d85fde7bba50220d9",
    "nt-fw": "d6d0eea7fcead54b97829934f234b6e4",
    "soc-fw-config": "9979814b0376fb468c8e8d267f7859e0",
    "tos-fw-config": "26257c1adbc67f478d96c4c4b0248021",
    "nt-fw-config": "28da981593e87e44ac661aaf801550f9",
    "trusted-key-cert": "827ee890f860e411a1b4777a21b4f94c",
    "soc-fw-key-cert": "8ab8beccf960e4119ad0eb4822d8dcf8",
    "tos-fw-key-cert": "9477d603fb60e41185ddb7105b8cee04",
    "nt-fw-key-cert": "8ad5832afb60e4118aafdf30bbc49859",
    "soc-fw-cert": "e2b20c205e63e4119ce8abccf92bb666",
    "tos-fw-cert": "a49f44115e63e41187283f05722af33d",
    "nt-fw-cert": "8ec4c1f35d63e411a7a987ee40b23fa7",
}
UUID_NAMES = {bytes.fromhex(value): name for name, value in UUIDS.items()}
TBBR = "1.3.6.1.4.1.4128.2100."
SHA256_OID = "2.16.840.1.101.3.4.2.1"
MAX_IMAGE_SIZE = 64 * 1024 * 1024


class ChainError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ChainError(message)


@dataclass(frozen=True)
class Der:
    tag: int
    value: bytes
    encoded: bytes

    def children(self) -> list[Der]:
        return der_items(self.value)


def der_items(data: bytes) -> list[Der]:
    result = []
    pos = 0
    while pos < len(data):
        start = pos
        require(pos + 2 <= len(data), "truncated DER header")
        tag, length = data[pos:pos + 2]
        require(tag & 31 != 31, "unsupported DER high tag")
        pos += 2
        if length & 128:
            count = length & 127
            require(0 < count <= 4 and pos + count <= len(data), "invalid DER length")
            require(data[pos] != 0, "nonminimal DER length")
            length = int.from_bytes(data[pos:pos + count], "big")
            require(length >= 128, "nonminimal DER length")
            pos += count
        require(pos + length <= len(data), "truncated DER value")
        result.append(Der(tag, data[pos:pos + length], data[start:pos + length]))
        pos += length
    return result


def one(data: bytes, tag: int) -> Der:
    items = der_items(data)
    require(len(items) == 1 and items[0].tag == tag, f"expected one DER tag {tag:#x}")
    return items[0]


def oid(item: Der) -> str:
    require(item.tag == 6 and bool(item.value), "invalid DER OID")
    values = []
    value = 0
    first = True
    for byte in item.value:
        require(not (first and byte == 128), "nonminimal DER OID")
        value = (value << 7) | (byte & 127)
        first = not (byte & 128)
        if first:
            values.append(value)
            value = 0
    require(first, "truncated DER OID")
    lead = min(values[0] // 40, 2)
    return ".".join(map(str, [lead, values[0] - 40 * lead, *values[1:]]))


def integer(data: bytes) -> int:
    value = one(data, 2).value
    require(bool(value) and not value[0] & 128, "invalid negative/empty DER integer")
    require(len(value) == 1 or value[0] != 0 or value[1] & 128, "nonminimal DER integer")
    return int.from_bytes(value, "big")


def sha256_algorithm(item: Der) -> None:
    require(item.tag == 0x30, "invalid hash algorithm")
    parts = item.children()
    require(1 <= len(parts) <= 2 and oid(parts[0]) == SHA256_OID, "unsupported hash algorithm")
    require(len(parts) == 1 or parts[1].encoded == b"\x05\x00", "invalid SHA256 parameters")


def pss_algorithm(item: Der) -> None:
    require(item.tag == 0x30, "invalid signature algorithm")
    parts = item.children()
    require(len(parts) == 2 and oid(parts[0]) == "1.2.840.113549.1.1.10", "unsupported signature algorithm")
    require(parts[1].tag == 0x30, "missing RSA-PSS parameters")
    params = {p.tag: p for p in parts[1].children()}
    require(len(params) == len(parts[1].children()), "duplicate RSA-PSS parameters")
    require(set(params) in ({0xa0, 0xa1, 0xa2}, {0xa0, 0xa1, 0xa2, 0xa3}), "unsupported RSA-PSS defaults")
    sha256_algorithm(one(params[0xa0].value, 0x30))
    mgf = one(params[0xa1].value, 0x30).children()
    require(len(mgf) == 2 and oid(mgf[0]) == "1.2.840.113549.1.1.8", "unsupported RSA-PSS mask")
    sha256_algorithm(mgf[1])
    require(integer(params[0xa2].value) == 32, "unsupported RSA-PSS salt length")
    require(0xa3 not in params or integer(params[0xa3].value) == 1, "unsupported RSA-PSS trailer")


class Certificate:
    def __init__(self, data: bytes):
        require(len(data) <= 65536, "certificate too large")
        parts = one(data, 0x30).children()
        require(len(parts) == 3 and parts[0].tag == 0x30, "invalid certificate structure")
        self.tbs = parts[0].encoded
        pss_algorithm(parts[1])
        require(parts[2].tag == 3 and parts[2].value[:1] == b"\0", "invalid certificate signature")
        self.signature = parts[2].value[1:]
        fields = parts[0].children()
        require(len(fields) >= 8 and fields[0].tag == 0xa0, "expected X509 v3 certificate")
        require(integer(fields[0].value) == 2, "expected X509 v3 certificate")
        require(fields[2].encoded == parts[1].encoded, "certificate signature algorithms differ")
        require(fields[6].tag == 0x30, "missing certificate public key")
        self.public_key = fields[6].encoded
        extensions = [p for p in fields[7:] if p.tag == 0xa3]
        require(len(extensions) == 1, "missing/duplicate certificate extensions")
        self.extensions = {}
        for extension in one(extensions[0].value, 0x30).children():
            require(extension.tag == 0x30, "invalid certificate extension")
            parts = extension.children()
            require(len(parts) in (2, 3) and parts[-1].tag == 4, "invalid extension value")
            if len(parts) == 3:
                require(parts[1].tag == 1 and parts[1].value in (b"\0", b"\xff"), "invalid extension critical flag")
            name = oid(parts[0])
            require(name not in self.extensions, "duplicate certificate extension")
            self.extensions[name] = parts[-1].value

    def extension(self, suffix: str) -> bytes:
        name = TBBR + suffix
        require(name in self.extensions, f"missing signed certificate extension {name}")
        return self.extensions[name]

    def verify(self, signer: bytes) -> None:
        require(self.public_key == signer, "certificate key differs from authenticated parent delegation")
        with tempfile.TemporaryDirectory(prefix="firmware-signature-") as tmp:
            root = Path(tmp)
            encoded = base64.encodebytes(signer)
            (root / "public.pem").write_bytes(b"-----BEGIN PUBLIC KEY-----\n" + encoded + b"-----END PUBLIC KEY-----\n")
            (root / "signature").write_bytes(self.signature)
            (root / "tbs").write_bytes(self.tbs)
            try:
                result = subprocess.run([
                    "openssl", "dgst", "-sha256", "-verify", str(root / "public.pem"),
                    "-signature", str(root / "signature"), "-sigopt", "rsa_padding_mode:pss",
                    "-sigopt", "rsa_pss_saltlen:32", "-sigopt", "rsa_mgf1_md:sha256", str(root / "tbs"),
                ], capture_output=True, timeout=30, check=False)
            except (OSError, subprocess.TimeoutExpired) as exc:
                raise ChainError(f"cannot execute mandatory certificate signature verification: {exc}") from exc
            require(result.returncode == 0, "certificate cryptographic signature verification failed")


def parse_fip(data: bytes) -> dict[str, bytes]:
    require(16 <= len(data) <= MAX_IMAGE_SIZE, "invalid FIP size")
    magic, _, flags = struct.unpack_from("<IIQ", data)
    require(magic == 0xaa640001 and flags == 0, "invalid/unsupported FIP header")
    entries = []
    names = set()
    pos = 16
    while True:
        require(pos + 40 <= len(data) and len(entries) <= len(UUIDS), "truncated/excessive FIP table")
        key, offset, length, flags = struct.unpack_from("<16sQQQ", data, pos)
        pos += 40
        if key == bytes(16):
            require(length == 0 and flags == 0 and offset == len(data), "invalid FIP terminator")
            break
        require(key in UUID_NAMES, "unsupported FIP entry")
        name = UUID_NAMES[key]
        require(name not in names, "duplicate FIP entry")
        names.add(name)
        require(flags == 0 and length > 0 and offset + length <= len(data), "invalid FIP entry bounds/flags")
        entries.append((offset, length, name))
    previous_end = pos
    for offset, length, _ in sorted(entries):
        require(offset >= previous_end, "FIP entries overlap each other or table")
        require(not any(data[previous_end:offset]), "nonzero unauthenticated FIP padding")
        previous_end = offset + length
    require(not any(data[previous_end:]), "nonzero unauthenticated FIP trailing bytes")
    return {name: data[offset:offset + length] for offset, length, name in entries}


def digest_binding(cert: Certificate, suffix: str, payload: bytes | None) -> None:
    digest_info = one(cert.extension(suffix), 0x30).children()
    require(len(digest_info) == 2 and digest_info[1].tag == 4, "invalid signed digest")
    sha256_algorithm(digest_info[0])
    # cert_create represents an absent optional image with an all-zero digest.
    expected = hashlib.sha256(payload).digest() if payload is not None else bytes(32)
    require(digest_info[1].value == expected, f"payload digest does not match signed extension {suffix}")


def validate_fip(data: bytes, kind: str, root_spki_sha256: str, minimum_counter: int) -> dict:
    require(kind in {"trusted", "uefi"}, "unknown FIP chain kind")
    entries = parse_fip(data)
    trusted = kind == "trusted"
    required = ({"trusted-key-cert", "soc-fw-key-cert", "tos-fw-key-cert", "soc-fw-cert", "tos-fw-cert", "soc-fw", "tos-fw"}
                if trusted else {"trusted-key-cert", "nt-fw-key-cert", "nt-fw-cert", "nt-fw"})
    optional = ({"soc-fw-config", "tos-fw-config", "tos-fw-extra1", "tos-fw-extra2"}
                if trusted else {"nt-fw-config"})
    require(required <= entries.keys() and entries.keys() <= required | optional, "missing/unexpected FIP chain members")
    certs = {name: Certificate(value) for name, value in entries.items() if name.endswith("cert")}
    root = certs["trusted-key-cert"]
    root_hash = hashlib.sha256(root.public_key).hexdigest()
    require(root_hash == root_spki_sha256, "FIP root does not match the selected vendor trust anchor")
    root.verify(root.public_key)
    delegated = root.extension("302" if trusted else "303")
    counter_oid = "1" if trusted else "2"
    counters = []

    def check_counter(cert):
        count = integer(cert.extension(counter_oid))
        require(count >= minimum_counter, "firmware counter is below the selected vendor reference")
        counters.append(count)

    if trusted:
        check_counter(root)
    for prefix, key_oid, bindings in (
        [("soc", "501", [("603", "soc-fw"), ("604", "soc-fw-config")]),
         ("tos", "901", [("1001", "tos-fw"), ("1002", "tos-fw-extra1"), ("1003", "tos-fw-extra2"), ("1004", "tos-fw-config")])]
        if trusted else [("nt", "1101", [("1201", "nt-fw"), ("1202", "nt-fw-config")])]
    ):
        key = certs[prefix + "-fw-key-cert"]
        content = certs[prefix + "-fw-cert"]
        key.verify(delegated)
        content.verify(key.extension(key_oid))
        check_counter(key)
        check_counter(content)
        for suffix, name in bindings:
            digest_binding(content, suffix, entries.get(name))
    require(len(set(counters)) == 1, "firmware certificates disagree on rollback counter")
    return {"status": "verified", "kind": kind, "root_spki_sha256": root_hash,
            "counter": counters[0], "sha256": hashlib.sha256(data).hexdigest(),
            "payloads": {name: hashlib.sha256(value).hexdigest() for name, value in entries.items()}}
