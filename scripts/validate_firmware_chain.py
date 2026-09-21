#!/usr/bin/env python3
"""Mandatory vendor-anchored FIP checks before packaging and publishing firmware."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tarfile
import zipfile

from firmware_chain import Certificate, ChainError, MAX_IMAGE_SIZE, require, validate_fip


ROOT = Path(__file__).resolve().parents[1]
PACKAGE = "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool"
CATALOG = (ROOT / "config/firmware-trust.json" if (ROOT / "config").is_dir()
           else Path(__file__).with_name("firmware-trust.json"))


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read(path: Path) -> bytes:
    require(path.is_file() and 0 < path.stat().st_size <= MAX_IMAGE_SIZE,
            f"missing, empty or oversized firmware input: {path}")
    return path.read_bytes()


def load_catalog(path: Path = CATALOG) -> list[dict]:
    document = json.loads(path.read_text())
    require(document.get("schema_version") == 1 and bool(document.get("references")), "invalid firmware trust catalogue")
    for row in document["references"]:
        for field in ("bl1_sha256", "trusted_fip_sha256", "trusted_root_spki_sha256", "uefi_certificate_sha256", "uefi_root_spki_sha256", "uefi_oem_spki_sha256"):
            require(bool(re.fullmatch("[0-9a-f]{64}", row.get(field, ""))), "invalid pinned firmware fingerprint")
        require(all(isinstance(row.get(k), int) and row[k] >= 0 for k in ("trusted_counter", "uefi_counter")), "invalid pinned firmware counter")
    return document["references"]


def reference(package: Path, catalog: list[dict]) -> dict:
    bl1 = read(package / "Firmwares/bootloader1.img")
    fip = read(package / "Firmwares/bootloader2.img")
    cert_data = read(package / "certs/trusted_key_no.crt")
    matches = [r for r in catalog if (r["bl1_sha256"], r["trusted_fip_sha256"], r["uefi_certificate_sha256"])
               == (sha256(bl1), sha256(fip), sha256(cert_data))]
    require(len(matches) == 1, "firmware source inputs do not match a pinned vendor BL1/FIP/certificate reference")
    selected = dict(matches[0])
    layout_hash = sha256(read(package / "spi_flash_config_all.json"))
    layouts = [item for item in selected["flash_layouts"] if item["config_sha256"] == layout_hash]
    require(len(layouts) == 1, "flash layout does not match the selected vendor reference")
    selected["flash_layout"] = layouts[0]
    validate_fip(fip, "trusted", selected["trusted_root_spki_sha256"], selected["trusted_counter"])
    cert = Certificate(cert_data)
    require(sha256(cert.public_key) == selected["uefi_root_spki_sha256"], "vendor UEFI trust anchor differs")
    cert.verify(cert.public_key)
    require(sha256(cert.extension("303")) == selected["uefi_oem_spki_sha256"], "vendor UEFI delegation differs")
    return selected


def preflight(package: Path, selected: dict, cix_release: str, mode: str) -> None:
    if mode == "upstream" or not cix_release.strip():
        return
    require(cix_release.strip().lower().removeprefix("v") == "1.2", "unknown CIX trusted-firmware selection")
    # The current source-build helper supplies this key for --rot-key and
    # --trusted-world-key. A vendor-signed BL1 cannot make that key trusted.
    try:
        result = subprocess.run(["openssl", "pkey", "-in", str(package / "Keys/oem_privatekey.pem"),
                                 "-pubout", "-outform", "DER", "-passin", "pass:"],
                                capture_output=True, timeout=30, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ChainError(f"cannot establish trusted-firmware signing authority: {exc}") from exc
    require(result.returncode == 0, "cannot read the source-build signing key")
    require(sha256(result.stdout) == selected["trusted_root_spki_sha256"],
            "CIX_RELEASE=1.2 cannot produce qualified flash firmware: the helper uses the UEFI OEM key "
            "as the trusted-firmware root, which does not match the vendor trust anchor. "
            "Use CIX_RELEASE= to retain vendor BL31/OP-TEE. Source compilation does not confer signing authority.")
    raise ChainError("CIX_RELEASE=1.2 has no qualified vendor BL1/trusted-FIP pairing; do not publish a flash image")


def validate_uefi(data: bytes, selected: dict) -> dict:
    # Both presentations occur: a retained vendor certificate, or an OEM
    # certificate rooted at the key authorised by its signed .303 extension.
    # This list is derived from a pinned and verified vendor certificate, not
    # from an arbitrary key supplied in the image being checked.
    from firmware_chain import parse_fip
    entries = parse_fip(data)
    require("trusted-key-cert" in entries, "UEFI FIP lacks trusted-key certificate")
    root = sha256(Certificate(entries["trusted-key-cert"]).public_key)
    require(root in {selected["uefi_root_spki_sha256"], selected["uefi_oem_spki_sha256"]},
            "UEFI FIP root lacks the selected vendor's signing authority")
    result = validate_fip(data, "uefi", root, selected["uefi_counter"])
    cert = Certificate(entries["trusted-key-cert"])
    require(sha256(cert.extension("303")) == selected["uefi_oem_spki_sha256"], "UEFI FIP changes the vendor OEM delegation")
    return result


def check_payloads(directory: Path, selected: dict) -> dict:
    require(sha256(read(directory / "Firmwares/bootloader1.img")) == selected["bl1_sha256"],
            "BL1 does not match the qualified vendor boot-chain pairing")
    return {
        "trusted": validate_fip(read(directory / "Firmwares/bootloader2.img"), "trusted",
                                selected["trusted_root_spki_sha256"], selected["trusted_counter"]),
        "uefi": validate_uefi(read(directory / "Firmwares/bootloader3.img"), selected),
    }


def flash_entries(data: bytes, layout: dict) -> dict[int, bytes]:
    headers = [offset for offset in (0x100000, 0x200000) if data[offset:offset + 4] == b"\xaa\x55\xaa\x55"]
    require(len(data) == 8 * 1024 * 1024, "full flash image must be exactly 8 MiB")
    require(len(headers) == 1, "expected exactly one full-flash header")
    offset = headers[0]
    require(offset == layout["header"] and len(data) == layout["size"], "flash header or size differs from vendor layout")
    require(offset + 16 <= len(data), "truncated flash header")
    _, version, count, flags = struct.unpack_from("<4I", data, offset)
    end = offset + 16 * (count + 1)
    require(version == 1 and flags == 0 and 1 <= count <= 128 and end <= len(data), "invalid flash table")
    entries = {}
    intervals = [(offset, end)]
    for index in range(count):
        kind, address, length, entry_flags = struct.unpack_from("<4I", data, offset + 16 * (index + 1))
        require(kind not in entries and address + length <= len(data), "duplicate/out-of-bounds flash entry")
        if length:
            require(all(address >= stop or address + length <= start for start, stop in intervals), "overlapping flash entries")
            intervals.append((address, address + length))
        slot = layout["entries"].get(str(kind))
        require(slot is not None and address == slot["address"] and length <= slot["size"] and entry_flags == 0,
                "flash entry differs from vendor address or reserved size")
        entries[kind] = data[address:address + length]
    require(set(map(str, entries)) == set(layout["entries"]), "flash image lacks required vendor entries")
    return entries


def check_flash(data: bytes, selected: dict) -> dict:
    entries = flash_entries(data, selected["flash_layout"])
    require(sha256(entries[1]) == selected["bl1_sha256"], "flash BL1 differs from qualified vendor pairing")
    return {"image_sha256": sha256(data),
            "trusted": validate_fip(entries[2], "trusted", selected["trusted_root_spki_sha256"], selected["trusted_counter"]),
            "uefi": validate_uefi(entries[7], selected)}


def check_ota(data: bytes, selected: dict) -> dict:
    require(32 <= len(data) <= MAX_IMAGE_SIZE, "invalid OTA image size")
    magic, version, count, flags, kind, address, length, entry_flags = struct.unpack_from("<8I", data)
    require((magic, version, count, flags, kind, entry_flags) == (0x55aa55aa, 1, 1, 0x80000000, 7, 0),
            "unsupported OTA header or payload selection")
    slot = selected["flash_layout"]["entries"]["7"]
    require(address == slot["address"] and 0 < length <= slot["size"] and 32 + length <= len(data),
            "OTA payload differs from vendor address or reserved size")
    require(not any(data[32 + length:]), "unexpected OTA trailing bytes")
    return {"image_sha256": sha256(data), "uefi": validate_uefi(data[32:32 + length], selected)}


def check_archive(path: Path, selected: dict) -> list[dict]:
    records = []

    def member(name, size, stream):
        require(0 < size <= MAX_IMAGE_SIZE, "oversized firmware archive member")
        data = stream.read(MAX_IMAGE_SIZE + 1)
        require(len(data) == size, "truncated firmware archive member")
        check = check_ota if Path(name).name == "cix_flash_ota.bin" else check_flash
        records.append({"path": f"{path}:{name}", **check(data, selected)})

    if path.suffix == ".zip":
        with zipfile.ZipFile(path) as archive:
            for info in archive.infolist():
                if Path(info.filename).name in {"cix_flash_all.bin", "cix_flash_ota.bin"}:
                    with archive.open(info) as stream:
                        member(info.filename, info.file_size, stream)
    else:
        with tarfile.open(path, "r:gz") as archive:
            for info in archive:
                if Path(info.name).name in {"cix_flash_all.bin", "cix_flash_ota.bin"}:
                    require(info.isfile(), "firmware archive image is not a regular file")
                    with archive.extractfile(info) as stream:
                        member(info.name, info.size, stream)
    require(any(r["path"].endswith("cix_flash_all.bin") for r in records), "firmware archive contains no full flash image")
    return records


def check_outputs(worktree: Path, selected: dict, board: str, target: str, build_target: str) -> list[dict]:
    records = []
    builds = worktree / "src/Build" / board
    roots = [p for p in sorted(builds.glob("*")) if p.is_dir() and re.fullmatch(r"(?:RELEASE|DEBUG)_[A-Z0-9]+", p.name)
             and (build_target == "build-all" or p.name.startswith(target.upper() + "_"))]
    require(build_target == "build-all" or len(roots) <= 1, "ambiguous firmware output trees")
    for root in roots:
        path = root / "cix_flash_all.bin"
        records.append({"path": str(path), **check_flash(read(path), selected)})
        ota = root / "cix_flash_ota.bin"
        if ota.exists():
            records.append({"path": str(ota), **check_ota(read(ota), selected)})
    if build_target in {"build-all", "buildbox-zip", "buildbox-targz", "buildbox-firmware-stage"}:
        for path in sorted((worktree / "dist").rglob("*")):
            if path.is_file() and path.name == "cix_flash_all.bin":
                records.append({"path": str(path), **check_flash(read(path), selected)})
            elif path.is_file() and path.name == "cix_flash_ota.bin":
                records.append({"path": str(path), **check_ota(read(path), selected)})
            elif path.is_file() and path.name.endswith((".zip", ".tgz", ".tar.gz")):
                records.extend(check_archive(path, selected))
    require(bool(records), "no full firmware output found for certificate-chain verification")
    return records


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, default=CATALOG)
    parser.add_argument("--reference-dir", type=Path)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--worktree", type=Path)
    selection.add_argument("--payload-dir", type=Path)
    selection.add_argument("--flash-image", type=Path)
    selection.add_argument("--ota-image", type=Path)
    parser.add_argument("--phase", choices=("inputs", "outputs"), default="inputs")
    parser.add_argument("--cix-release", default="")
    parser.add_argument("--artefact-mode", default="custom")
    parser.add_argument("--board", default="O6")
    parser.add_argument("--firmware-target", default="RELEASE")
    parser.add_argument("--build-target", default="buildbox-firmware-build")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--print-scratch-layout", action="store_true")
    args = parser.parse_args()
    if args.worktree and args.report is None and not args.print_scratch_layout and (ROOT / "config").is_dir():
        from reconstruction_common import firmware_chain_report_path
        args.report = firmware_chain_report_path(ROOT, args.worktree, args.board, args.firmware_target)
    report = {"check": "vendor-anchored-firmware-chain", "status": "failed", "board_fuse_acceptance": "not-tested", "board_rollback_state": "not-observed"}
    try:
        package = args.reference_dir or (args.worktree / PACKAGE if args.worktree else None)
        require(package is not None, "--reference-dir is required without --worktree")
        selected = reference(package, load_catalog(args.catalog))
        if args.print_scratch_layout:
            require(selected.get("reboot_scratch_layout") in (1, 2), "unknown vendor reboot scratch layout")
            print(selected["reboot_scratch_layout"])
            return 0
        preflight(package, selected, args.cix_release, args.artefact_mode)
        report["reference"] = selected
        if args.payload_dir:
            report["images"] = [check_payloads(args.payload_dir, selected)]
        elif args.flash_image:
            report["images"] = [check_flash(read(args.flash_image), selected)]
        elif args.ota_image:
            report["images"] = [check_ota(read(args.ota_image), selected)]
        elif args.phase == "outputs":
            report["images"] = check_outputs(args.worktree, selected, args.board, args.firmware_target, args.build_target)
        report["status"] = "verified" if "images" in report else "inputs-verified"
        print(f"[firmware-chain] {report['status']}")
        return 0
    except (ChainError, OSError, ValueError, tarfile.TarError, zipfile.BadZipFile) as exc:
        report["error"] = str(exc)
        print(f"[firmware-chain] REJECTED: {exc}", file=sys.stderr)
        return 2
    finally:
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
