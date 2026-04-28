#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import struct
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import uuid


SCRIPT_PATH = pathlib.Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent
DEFAULT_INPUT_DIR = (
    REPO_ROOT
    / "custom"
    / "overlay"
    / "edk2-platforms"
    / "Platform"
    / "Radxa"
    / "Platforms"
    / "CIX"
    / "Sky1"
    / "SecureBootDefaults"
    / "Microsoft"
    / "Inputs"
)
DEFAULT_OUTPUT_DIR = DEFAULT_INPUT_DIR.parent
DEFAULT_MANIFEST_PATH = DEFAULT_OUTPUT_DIR / "manifest.lock.json"
DEFAULT_ARCH = "aarch64"
DEFAULT_RAW_SOURCE_BASE_URL = "https://raw.githubusercontent.com/microsoft/secureboot_objects"
EFI_CERT_X509_GUID = uuid.UUID("a5c059a1-94e4-4aa7-87b5-ab155c2bf072")
EFI_CERT_SHA256_GUID = uuid.UUID("c1c41626-504c-4092-aca9-41f936934328")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate the custom-path Microsoft Secure Boot payloads embedded by "
            "the Orion O6/O6N firmware builds."
        )
    )
    parser.add_argument("--input-dir", type=pathlib.Path, default=DEFAULT_INPUT_DIR)
    parser.add_argument("--output-dir", type=pathlib.Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--manifest", type=pathlib.Path, default=DEFAULT_MANIFEST_PATH)
    parser.add_argument("--arch", default=DEFAULT_ARCH)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Fail if the manifest-pinned inputs or payloads do not match the checked-in files.",
    )
    parser.add_argument(
        "--no-fetch",
        action="store_true",
        help="Do not download missing Microsoft source inputs from the pinned upstream release tag.",
    )
    return parser.parse_args()


def _guid_bytes(value: uuid.UUID) -> bytes:
    return value.bytes_le


def _encode_signature_list_header(signature_type: uuid.UUID, list_size: int, signature_size: int) -> bytes:
    return _guid_bytes(signature_type) + struct.pack("<III", list_size, 0, signature_size)


def _encode_x509_signature_list(certificate: bytes, signature_owner: uuid.UUID) -> bytes:
    signature = _guid_bytes(signature_owner) + certificate
    return _encode_signature_list_header(
        EFI_CERT_X509_GUID,
        16 + 12 + len(signature),
        len(signature),
    ) + signature


def _encode_sha256_signature_list(hashes: list[bytes], signature_owner: uuid.UUID) -> bytes:
    signature_size = 16 + 32
    payload = bytearray(
        _encode_signature_list_header(
            EFI_CERT_SHA256_GUID,
            16 + 12 + (len(hashes) * signature_size),
            signature_size,
        )
    )
    for digest in hashes:
        payload.extend(_guid_bytes(signature_owner))
        payload.extend(digest)
    return bytes(payload)


def _read_hashes(json_path: pathlib.Path, arch: str) -> list[bytes]:
    data = json.loads(json_path.read_text(encoding="utf-8"))
    arch_hashes = data["images"][arch]
    digests: list[bytes] = []
    for entry in arch_hashes:
        if entry["hashType"] != "SHA256":
            raise ValueError(f"Unsupported dbx hash type: {entry['hashType']}")
        digest = bytes.fromhex(entry["authenticodeHash"])
        if len(digest) != 32:
            raise ValueError(f"Unexpected dbx digest size for {entry['filename']}")
        digests.append(digest)
    return digests


def load_manifest(manifest_path: pathlib.Path) -> dict[str, object]:
    return json.loads(manifest_path.read_text(encoding="utf-8"))


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def check_named_files(
    base_dir: pathlib.Path,
    entries: dict[str, dict[str, str]],
    *,
    label: str,
) -> list[str]:
    failures: list[str] = []
    for name, metadata in entries.items():
        path = base_dir / name
        if not path.is_file():
            failures.append(f"missing {label}: {path}")
            continue
        expected_sha256 = metadata["sha256"]
        actual_sha256 = sha256_file(path)
        if actual_sha256 != expected_sha256:
            failures.append(
                f"{label} checksum mismatch for {path}: expected {expected_sha256}, got {actual_sha256}"
            )
    return failures


def _raw_source_url(manifest: dict[str, object], repo_path: str) -> str:
    source_ref = manifest.get("source_tag", manifest["source_commit"])
    quoted_path = urllib.parse.quote(repo_path, safe="/")
    return f"{DEFAULT_RAW_SOURCE_BASE_URL}/{source_ref}/{quoted_path}"


def _write_bytes_if_changed(path: pathlib.Path, data: bytes) -> bool:
    if path.is_file() and path.read_bytes() == data:
        return False

    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as tmp_file:
        tmp_file.write(data)
        tmp_path = pathlib.Path(tmp_file.name)
    tmp_path.replace(path)
    return True


def ensure_inputs(
    input_dir: pathlib.Path,
    manifest: dict[str, object],
    *,
    allow_fetch: bool,
) -> None:
    input_dir.mkdir(parents=True, exist_ok=True)
    input_entries: dict[str, dict[str, str]] = manifest["inputs"]  # type: ignore[assignment]

    for name, metadata in input_entries.items():
        path = input_dir / name
        if path.is_file():
            expected_sha256 = metadata["sha256"]
            actual_sha256 = sha256_file(path)
            if actual_sha256 != expected_sha256:
                raise RuntimeError(
                    f"Input checksum mismatch for {path}: expected {expected_sha256}, got {actual_sha256}"
                )
            continue

        if not allow_fetch:
            raise RuntimeError(
                f"Missing input {path}. Re-run without --no-fetch to download it from the pinned Microsoft source."
            )

        repo_path = metadata["repo_path"]
        url = _raw_source_url(manifest, repo_path)
        try:
            with urllib.request.urlopen(url) as response:
                data = response.read()
        except urllib.error.URLError as error:
            raise RuntimeError(f"Failed to download {url}: {error}") from error

        actual_sha256 = sha256_bytes(data)
        expected_sha256 = metadata["sha256"]
        if actual_sha256 != expected_sha256:
            raise RuntimeError(
                f"Downloaded input checksum mismatch for {url}: expected {expected_sha256}, got {actual_sha256}"
            )

        _write_bytes_if_changed(path, data)


def build_payloads(input_dir: pathlib.Path, arch: str, manifest: dict[str, object]) -> dict[str, bytes]:
    input_dir = input_dir.resolve()
    payloads: dict[str, bytes] = {}
    signature_owner = uuid.UUID(str(manifest["signature_owner"]))
    pk_input = str(manifest["pk_input"])
    kek_input = str(manifest["kek_input"])
    db_inputs = tuple(str(name) for name in manifest["db_inputs"])
    dbx_input = str(manifest["dbx_input"])

    pk_certificate = (input_dir / pk_input).read_bytes()
    payloads["PK.bin"] = _encode_x509_signature_list(pk_certificate, signature_owner)

    kek_certificate = (input_dir / kek_input).read_bytes()
    payloads["KEK.bin"] = _encode_x509_signature_list(kek_certificate, signature_owner)

    db_payload = bytearray()
    for certificate_name in db_inputs:
        db_payload.extend(_encode_x509_signature_list((input_dir / certificate_name).read_bytes(), signature_owner))
    payloads["DB.bin"] = bytes(db_payload)

    dbx_hashes = _read_hashes(input_dir / dbx_input, arch)
    payloads["DBX.bin"] = _encode_sha256_signature_list(dbx_hashes, signature_owner)

    return payloads


def check_generated_payload_hashes(
    payloads: dict[str, bytes],
    manifest: dict[str, object],
) -> list[str]:
    failures: list[str] = []
    output_entries: dict[str, dict[str, str]] = manifest["outputs"]  # type: ignore[assignment]
    for name, payload in payloads.items():
        expected_sha256 = output_entries[name]["sha256"]
        actual_sha256 = sha256_bytes(payload)
        if actual_sha256 != expected_sha256:
            failures.append(
                f"generated payload checksum mismatch for {name}: expected {expected_sha256}, got {actual_sha256}"
            )
    return failures


def _write_payloads(output_dir: pathlib.Path, payloads: dict[str, bytes]) -> list[pathlib.Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    changed_paths: list[pathlib.Path] = []
    for name, payload in payloads.items():
        path = output_dir / name
        if _write_bytes_if_changed(path, payload):
            changed_paths.append(path)
    return changed_paths


def _check_payload_bytes(output_dir: pathlib.Path, payloads: dict[str, bytes]) -> list[str]:
    mismatches: list[str] = []
    for name, expected in payloads.items():
        path = output_dir / name
        if not path.is_file():
            mismatches.append(f"missing payload: {path}")
            continue
        actual = path.read_bytes()
        if actual != expected:
            mismatches.append(f"payload mismatch: {path}")
    return mismatches


def main() -> int:
    args = parse_args()
    manifest = load_manifest(args.manifest.resolve())

    if args.arch != str(manifest["target_arch"]):
        print(
            f"manifest {args.manifest} is pinned for {manifest['target_arch']}, got --arch {args.arch}",
            file=sys.stderr,
        )
        return 1

    try:
        ensure_inputs(args.input_dir.resolve(), manifest, allow_fetch=not args.no_fetch)
        payloads = build_payloads(args.input_dir, args.arch, manifest)
    except RuntimeError as error:
        print(error, file=sys.stderr)
        return 1

    failures = check_generated_payload_hashes(payloads, manifest)

    if args.check:
        failures.extend(
            check_named_files(
                args.input_dir.resolve(),
                manifest["inputs"],  # type: ignore[arg-type]
                label="input",
            )
        )
        failures.extend(
            check_named_files(
                args.output_dir.resolve(),
                manifest["outputs"],  # type: ignore[arg-type]
                label="payload",
            )
        )
        failures.extend(_check_payload_bytes(args.output_dir.resolve(), payloads))
        if failures:
            for failure in failures:
                print(failure, file=sys.stderr)
            return 1
        print("Microsoft Secure Boot inputs and payloads match manifest.lock.json.")
        return 0

    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    changed_paths = _write_payloads(args.output_dir.resolve(), payloads)
    for name in sorted(payloads):
        print(args.output_dir.resolve() / name)
    if not changed_paths:
        payload_names = ", ".join(sorted(payloads))
        print(
            f"Microsoft Secure Boot payloads already matched manifest.lock.json: {payload_names}.",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
