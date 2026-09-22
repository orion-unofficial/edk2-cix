#!/usr/bin/env python3

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import pathlib
import shutil
import subprocess
import sys
import tempfile


TRUSTED_KEY_OID = "1.3.6.1.4.1.4128.2100.303"
NON_TRUSTED_FW_NVCOUNTER_OID = "1.3.6.1.4.1.4128.2100.2"
NON_TRUSTED_FW_CONTENT_CERT_PK_OID = "1.3.6.1.4.1.4128.2100.1101"
NON_TRUSTED_WORLD_BOOTLOADER_HASH_OID = "1.3.6.1.4.1.4128.2100.1201"
NON_TRUSTED_FW_CONFIG_HASH_OID = "1.3.6.1.4.1.4128.2100.1202"
TRUSTED_KEY_CERT_COMMON_NAME = "Trusted Key Certificate"
NON_TRUSTED_FW_KEY_CERT_COMMON_NAME = "Non-Trusted Firmware Key Certificate"
NON_TRUSTED_FW_CONTENT_CERT_COMMON_NAME = "Non-Trusted Firmware Content Certificate"
CERT_VALIDITY_DAYS = 365 * 20
SHA256_DIGEST_INFO_PREFIX = bytes.fromhex("3031300d060960864801650304020105000420")


class ValidationError(RuntimeError):
    pass


def run(
    cmd: list[str],
    *,
    input_bytes: bytes | None = None,
    text: bool = False,
    cwd: pathlib.Path | None = None,
) -> subprocess.CompletedProcess[str] | subprocess.CompletedProcess[bytes]:
    result = subprocess.run(
        cmd,
        input=input_bytes,
        capture_output=True,
        check=False,
        cwd=cwd,
        text=text,
    )
    if result.returncode != 0:
        raise ValidationError(
            f"command failed ({result.returncode}): {' '.join(str(part) for part in cmd)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return result


def require_file(path: pathlib.Path) -> pathlib.Path:
    if not path.is_file():
        raise ValidationError(f"missing required file: {path}")
    return path


def sha256(path: pathlib.Path) -> bytes:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.digest()


def pem_public_key_to_der(path: pathlib.Path) -> bytes:
    return run(
        ["openssl", "pkey", "-pubin", "-in", str(path), "-pubout", "-outform", "DER"]
    ).stdout


def pem_private_key_to_public_der(path: pathlib.Path) -> bytes:
    return run(
        ["openssl", "pkey", "-in", str(path), "-pubout", "-outform", "DER"]
    ).stdout


def cert_public_key_der(path: pathlib.Path) -> bytes:
    cert_pub_pem = run(
        ["openssl", "x509", "-inform", "DER", "-in", str(path), "-pubkey", "-noout"]
    ).stdout
    return run(
        ["openssl", "pkey", "-pubin", "-pubout", "-outform", "DER"],
        input_bytes=cert_pub_pem,
    ).stdout


def cert_text(path: pathlib.Path) -> str:
    return run(
        ["openssl", "x509", "-inform", "DER", "-in", str(path), "-text", "-noout"],
        text=True,
    ).stdout


def cert_dates(path: pathlib.Path) -> tuple[dt.datetime, dt.datetime]:
    output = run(
        ["openssl", "x509", "-inform", "DER", "-in", str(path), "-dates", "-noout"],
        text=True,
    ).stdout.splitlines()
    values: dict[str, dt.datetime] = {}
    for line in output:
        key, value = line.split("=", 1)
        values[key] = dt.datetime.strptime(value, "%b %d %H:%M:%S %Y %Z")
    return values["notBefore"], values["notAfter"]


def extract_extension_der(path: pathlib.Path, oid: str) -> bytes | None:
    output = run(
        ["openssl", "asn1parse", "-inform", "DER", "-in", str(path)],
        text=True,
    ).stdout.splitlines()
    saw_oid = False
    for line in output:
        if oid in line:
            saw_oid = True
            continue
        if saw_oid and "[HEX DUMP]:" in line:
            return bytes.fromhex(line.split("[HEX DUMP]:", 1)[1].strip())
    return None


def encode_uint64_asn1_integer(value: int) -> bytes:
    if value < 0:
        raise ValidationError(f"nv counter must be non-negative, got {value}")
    body = value.to_bytes(max(1, (value.bit_length() + 7) // 8), "big")
    if body[0] & 0x80:
        body = b"\x00" + body
    return bytes([0x02, len(body)]) + body


def encode_sha256_digest_info(path: pathlib.Path) -> bytes:
    return SHA256_DIGEST_INFO_PREFIX + sha256(path)


def require_text_fragment(cert_info: str, fragment: str, cert_label: str) -> None:
    if fragment not in cert_info:
        raise ValidationError(f"{cert_label}: missing expected certificate text: {fragment}")


def require_any_text_fragment(cert_info: str, fragments: tuple[str, ...], cert_label: str) -> None:
    for fragment in fragments:
        if fragment in cert_info:
            return
    raise ValidationError(
        f"{cert_label}: missing expected certificate text variants: {' | '.join(fragments)}"
    )


def assert_common_cert_properties(cert_path: pathlib.Path, common_name: str, cert_label: str) -> str:
    cert_info = cert_text(cert_path)
    required_fragments = (
        "Signature Algorithm: rsassaPss",
        "Hash Algorithm: sha256",
        "Mask Algorithm: mgf1 with sha256",
        "Salt Length: 0x20",
        "X509v3 Subject Key Identifier:",
        "X509v3 Authority Key Identifier:",
        "X509v3 Basic Constraints:",
        "CA:FALSE",
    )
    for fragment in required_fragments:
        require_text_fragment(cert_info, fragment, cert_label)
    require_any_text_fragment(
        cert_info,
        (f"Issuer: CN={common_name}", f"Issuer: CN = {common_name}"),
        cert_label,
    )
    require_any_text_fragment(
        cert_info,
        (f"Subject: CN={common_name}", f"Subject: CN = {common_name}"),
        cert_label,
    )

    start, end = cert_dates(cert_path)
    if end - start != dt.timedelta(days=CERT_VALIDITY_DAYS):
        raise ValidationError(
            f"{cert_label}: expected {CERT_VALIDITY_DAYS}-day validity window, got {end - start}"
        )

    return cert_info


def require_equal(actual: bytes, expected: bytes, description: str) -> None:
    if actual != expected:
        raise ValidationError(f"{description} did not match the expected value")


def validate_trusted_key_cert(
    cert_path: pathlib.Path,
    oem_public_key: pathlib.Path,
    sign_key: pathlib.Path,
    *,
    allow_reused_subject_mismatch: bool = False,
) -> None:
    assert_common_cert_properties(cert_path, TRUSTED_KEY_CERT_COMMON_NAME, "trusted-key cert")
    if not allow_reused_subject_mismatch:
        require_equal(
            cert_public_key_der(cert_path),
            pem_private_key_to_public_der(sign_key),
            "trusted-key cert subject public key",
        )
    extension = extract_extension_der(cert_path, TRUSTED_KEY_OID)
    if extension is None:
        raise ValidationError(f"trusted-key cert: missing extension {TRUSTED_KEY_OID}")
    require_equal(
        extension,
        pem_public_key_to_der(oem_public_key),
        "trusted-key cert embedded OEM public key",
    )


def validate_nt_fw_key_cert(
    cert_path: pathlib.Path,
    nt_fw_key: pathlib.Path,
    non_trusted_world_key: pathlib.Path,
    ntfw_nvctr: int,
) -> None:
    assert_common_cert_properties(
        cert_path,
        NON_TRUSTED_FW_KEY_CERT_COMMON_NAME,
        "non-trusted FW key cert",
    )
    require_equal(
        cert_public_key_der(cert_path),
        pem_private_key_to_public_der(non_trusted_world_key),
        "non-trusted FW key cert subject public key",
    )
    nvcounter = extract_extension_der(cert_path, NON_TRUSTED_FW_NVCOUNTER_OID)
    if nvcounter is None:
        raise ValidationError(
            f"non-trusted FW key cert: missing extension {NON_TRUSTED_FW_NVCOUNTER_OID}"
        )
    require_equal(
        nvcounter,
        encode_uint64_asn1_integer(ntfw_nvctr),
        "non-trusted FW key cert NV counter extension",
    )
    embedded_key = extract_extension_der(cert_path, NON_TRUSTED_FW_CONTENT_CERT_PK_OID)
    if embedded_key is None:
        raise ValidationError(
            f"non-trusted FW key cert: missing extension {NON_TRUSTED_FW_CONTENT_CERT_PK_OID}"
        )
    require_equal(
        embedded_key,
        pem_private_key_to_public_der(nt_fw_key),
        "non-trusted FW key cert embedded content-cert public key",
    )


def validate_nt_fw_content_cert(
    cert_path: pathlib.Path,
    nt_fw_key: pathlib.Path,
    nt_fw: pathlib.Path,
    nt_fw_config: pathlib.Path | None,
    ntfw_nvctr: int,
) -> None:
    assert_common_cert_properties(
        cert_path,
        NON_TRUSTED_FW_CONTENT_CERT_COMMON_NAME,
        "non-trusted FW content cert",
    )
    require_equal(
        cert_public_key_der(cert_path),
        pem_private_key_to_public_der(nt_fw_key),
        "non-trusted FW content cert subject public key",
    )
    nvcounter = extract_extension_der(cert_path, NON_TRUSTED_FW_NVCOUNTER_OID)
    if nvcounter is None:
        raise ValidationError(
            f"non-trusted FW content cert: missing extension {NON_TRUSTED_FW_NVCOUNTER_OID}"
        )
    require_equal(
        nvcounter,
        encode_uint64_asn1_integer(ntfw_nvctr),
        "non-trusted FW content cert NV counter extension",
    )
    bootloader_hash = extract_extension_der(cert_path, NON_TRUSTED_WORLD_BOOTLOADER_HASH_OID)
    if bootloader_hash is None:
        raise ValidationError(
            f"non-trusted FW content cert: missing extension {NON_TRUSTED_WORLD_BOOTLOADER_HASH_OID}"
        )
    require_equal(
        bootloader_hash,
        encode_sha256_digest_info(nt_fw),
        "non-trusted FW content cert BL33 digest extension",
    )
    config_hash = extract_extension_der(cert_path, NON_TRUSTED_FW_CONFIG_HASH_OID)
    if config_hash is None:
        raise ValidationError(
            f"non-trusted FW content cert: missing extension {NON_TRUSTED_FW_CONFIG_HASH_OID}"
        )
    if nt_fw_config is None:
        expected_config_hash = SHA256_DIGEST_INFO_PREFIX + (b"\x00" * hashlib.sha256().digest_size)
    else:
        expected_config_hash = encode_sha256_digest_info(nt_fw_config)
    require_equal(
        config_hash,
        expected_config_hash,
        "non-trusted FW content cert firmware-config digest extension",
    )


def validate_fip_acceptance(
    fiptool: pathlib.Path,
    trusted_key_cert: pathlib.Path,
    nt_fw_key_cert: pathlib.Path,
    nt_fw_cert: pathlib.Path,
    nt_fw: pathlib.Path,
) -> None:
    fiptool = fiptool.resolve()
    with tempfile.TemporaryDirectory(prefix="cix-fip-cert-validate-") as td:
        temp_dir = pathlib.Path(td)
        image = temp_dir / "bootloader3.img"
        unpack_dir = temp_dir / "unpack"
        unpack_dir.mkdir()

        run(
            [
                str(fiptool),
                "create",
                "--trusted-key-cert",
                str(trusted_key_cert),
                "--nt-fw-key-cert",
                str(nt_fw_key_cert),
                "--nt-fw-cert",
                str(nt_fw_cert),
                "--nt-fw",
                str(nt_fw),
                str(image),
            ]
        )
        run([str(fiptool), "info", str(image)])

        local_image = unpack_dir / image.name
        shutil.copy2(image, local_image)
        run([str(fiptool), "unpack", local_image.name], cwd=unpack_dir)

        expected_files = {
            "trusted-key-cert.bin": trusted_key_cert,
            "nt-fw-key-cert.bin": nt_fw_key_cert,
            "nt-fw-cert.bin": nt_fw_cert,
            "nt-fw.bin": nt_fw,
        }
        for unpacked_name, source_path in expected_files.items():
            unpacked_path = unpack_dir / unpacked_name
            if not unpacked_path.is_file():
                raise ValidationError(f"fiptool unpack did not produce {unpacked_name}")
            if unpacked_path.read_bytes() != source_path.read_bytes():
                raise ValidationError(
                    f"fiptool round-trip mismatch for {unpacked_name}"
                )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Validate generated CIX FIP certificates structurally and semantically, "
            "and optionally prove that fiptool accepts them."
        )
    )
    parser.add_argument("--trusted-key-cert", type=pathlib.Path, required=True)
    parser.add_argument("--nt-fw-key-cert", type=pathlib.Path, required=True)
    parser.add_argument("--nt-fw-cert", type=pathlib.Path, required=True)
    parser.add_argument("--oem-public-key", type=pathlib.Path, required=True)
    parser.add_argument("--trusted-sign-key", type=pathlib.Path, required=True)
    parser.add_argument("--nt-fw-key", type=pathlib.Path, required=True)
    parser.add_argument("--non-trusted-world-key", type=pathlib.Path, required=True)
    parser.add_argument("--nt-fw", type=pathlib.Path, required=True)
    parser.add_argument("--nt-fw-config", type=pathlib.Path)
    parser.add_argument("--ntfw-nvctr", type=int, required=True)
    parser.add_argument("--fiptool", type=pathlib.Path)
    parser.add_argument(
        "--allow-reused-trusted-key-cert",
        action="store_true",
        help=(
            "Allow the trusted-key cert subject public key to differ from the local "
            "signing key while still validating the embedded OEM key, structure, and "
            "downstream fiptool acceptance."
        ),
    )
    parser.add_argument("--verbose", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    require_file(args.trusted_key_cert)
    require_file(args.nt_fw_key_cert)
    require_file(args.nt_fw_cert)
    require_file(args.oem_public_key)
    require_file(args.trusted_sign_key)
    require_file(args.nt_fw_key)
    require_file(args.non_trusted_world_key)
    require_file(args.nt_fw)
    if args.nt_fw_config is not None:
        require_file(args.nt_fw_config)
    if args.fiptool is not None:
        require_file(args.fiptool)

    validate_trusted_key_cert(
        args.trusted_key_cert,
        args.oem_public_key,
        args.trusted_sign_key,
        allow_reused_subject_mismatch=args.allow_reused_trusted_key_cert,
    )
    validate_nt_fw_key_cert(
        args.nt_fw_key_cert,
        args.nt_fw_key,
        args.non_trusted_world_key,
        args.ntfw_nvctr,
    )
    validate_nt_fw_content_cert(
        args.nt_fw_cert,
        args.nt_fw_key,
        args.nt_fw,
        args.nt_fw_config,
        args.ntfw_nvctr,
    )

    if args.fiptool is not None:
        validate_fip_acceptance(
            args.fiptool,
            args.trusted_key_cert,
            args.nt_fw_key_cert,
            args.nt_fw_cert,
            args.nt_fw,
        )

    if args.verbose:
        print("CIX FIP certificate validation passed")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except ValidationError as exc:
        print(exc, file=sys.stderr)
        sys.exit(1)
