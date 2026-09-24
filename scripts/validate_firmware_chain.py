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
from bl33_layout import FD_RAM_SIZE, select as select_bl33_layout, validation_layout


ROOT = Path(__file__).resolve().parents[1]
PACKAGE = "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool"
CIX_SIGNING_KEYS = "custom/signing-keys/cix-1.2"
CIX_BL1_SHA256 = "04be52c3a0df73fe4461cd5b41a5fe1be0e9d02a3b22cb0dd2514e305f102c74"
CIX_KEY_SHA256 = {
    "cix_privatekey.pem": "0f09cb62ae4f6e407cf2d389e5e37065968ef7f7ce144f794398123dbdb37f3e",
    "trusted_world_privatekey.pem": "01346dc66d6867058ccd5751bff9e70517b48a5a4f9b92d5e00e09189b77331d",
    "non_trusted_world_privatekey.pem": "ee67b84c5e156ae0f7bc21458a477eb093cf7c0c135e9d39e821627b8ec9338f",
    "bl31_privatekey.pem": "48e057e53bddb9aca995c01cb932e371d991ba9257c0e539b3b8f435314fa045",
    "bl32_privatekey.pem": "a3c9e8a6614e1f8810433575a8063f1eb1f1d24d301c8f50a224b02356780072",
    "bl33_privatekey.pem": "26dfdbba3f29b56e2fc51eb3ddd6c216b465dd15e189f7031cea28bbfc95256d",
    "oem_privatekey.pem": "675a9b4e5e02931bab05cb27f0c616c2ba4a03549dad424fd4cadfc1675e8d0d",
    "cix_publickey.pem": "e781e96d3f982e071c1ae1672a62e036d139c7ceeb7fffb92ac6eb497401998a",
    "oem_publickey.pem": "0ffdb8d7ef57fdda669aca686ae274dc3d693ba14914b048378932813c39cd72",
}
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


def preflight(package: Path, selected: dict, cix_release: str, mode: str,
              signing_keys: Path | None = None) -> None:
    if not cix_release.strip():
        return
    require(mode == "custom", "CIX_RELEASE is only supported with ARTEFACT_MODE=custom")
    require(cix_release.strip().lower().removeprefix("v") == "1.2", "unknown CIX trusted-firmware selection")
    package = package.resolve()
    signing_keys = signing_keys or package.parents[5] / CIX_SIGNING_KEYS
    for name, expected in CIX_KEY_SHA256.items():
        require(sha256(read(signing_keys / name)) == expected,
                f"CIX signing key differs from pinned upstream publication: {name}")
    try:
        result = subprocess.run(["openssl", "pkey", "-in", str(signing_keys / "cix_privatekey.pem"),
                                 "-pubout", "-outform", "DER", "-passin", "pass:"],
                                capture_output=True, timeout=30, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ChainError(f"cannot establish trusted-firmware signing authority: {exc}") from exc
    require(result.returncode == 0, "cannot read the pinned CIX trusted-root signing key")
    require(sha256(result.stdout) == selected["trusted_root_spki_sha256"],
            "CIX signing key does not match the selected vendor trusted-firmware root")
    cix_bl1 = read(package.parents[5] / "src/cix-v1.2/release-payloads/bootloader1-2026q1.img")
    require(sha256(cix_bl1) == CIX_BL1_SHA256,
            "CIX_RELEASE BL1 differs from the pinned CIX 2026Q1 vendor payload")
    selected["output_bl1_sha256"] = CIX_BL1_SHA256


def validate_uefi(data: bytes, selected: dict) -> dict:
    # Both presentations occur: a retained vendor certificate, or an OEM
    # certificate rooted at the key authorised by its signed .303 extension.
    # This list is derived from a pinned and verified vendor certificate, not
    # from an arbitrary key supplied in the image being checked.
    from firmware_chain import parse_fip
    entries = parse_fip(data)
    require(len(entries.get("nt-fw", b"")) <= FD_RAM_SIZE,
            "BL33 FD exceeds the reserved RAM before the GOP framebuffer")
    require("trusted-key-cert" in entries, "UEFI FIP lacks trusted-key certificate")
    root = sha256(Certificate(entries["trusted-key-cert"]).public_key)
    require(root in {selected["uefi_root_spki_sha256"], selected["uefi_oem_spki_sha256"]},
            "UEFI FIP root lacks the selected vendor's signing authority")
    result = validate_fip(data, "uefi", root, selected["uefi_counter"])
    cert = Certificate(entries["trusted-key-cert"])
    require(sha256(cert.extension("303")) == selected["uefi_oem_spki_sha256"], "UEFI FIP changes the vendor OEM delegation")
    return result


def check_payloads(directory: Path, selected: dict) -> dict:
    require(sha256(read(directory / "Firmwares/bootloader1.img")) == selected.get("output_bl1_sha256", selected["bl1_sha256"]),
            "BL1 does not match the qualified vendor boot-chain pairing")
    bl33 = read(directory / "Firmwares/bootloader3.img")
    allocation = select_bl33_layout(selected, len(bl33), selected.get("allow_large_bl33", False))
    return {
        "trusted": validate_fip(read(directory / "Firmwares/bootloader2.img"), "trusted",
                                selected["trusted_root_spki_sha256"], selected["trusted_counter"]),
        "uefi": validate_uefi(bl33, selected),
        "bl33_layout": allocation,
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
    allowed = selected.get("allow_large_bl33", False)
    entries = flash_entries(data, validation_layout(selected, allowed))
    allocation = select_bl33_layout(selected, len(entries[7]), allowed)
    if allocation["full_image_only"]:
        require(all(value == 0xFF for value in data[allocation["address"] + len(entries[7]):]),
                "unexpected data after enlarged BL33 payload")
    require(sha256(entries[1]) == selected.get("output_bl1_sha256", selected["bl1_sha256"]),
            "flash BL1 differs from qualified vendor pairing")
    return {"image_sha256": sha256(data), "bl33_layout": allocation,
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
    parser.add_argument("--signing-keys-dir", type=Path)
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
    parser.add_argument("--allow-large-bl33", choices=("0", "1"), default="0")
    args = parser.parse_args()
    if args.worktree and args.report is None and not args.print_scratch_layout and (ROOT / "config").is_dir():
        from reconstruction_common import firmware_chain_report_path
        args.report = firmware_chain_report_path(ROOT, args.worktree, args.board, args.firmware_target)
    report = {"check": "vendor-anchored-firmware-chain", "status": "failed", "board_fuse_acceptance": "not-tested", "board_rollback_state": "not-observed"}
    try:
        package = args.reference_dir or (args.worktree / PACKAGE if args.worktree else None)
        require(package is not None, "--reference-dir is required without --worktree")
        selected = reference(package, load_catalog(args.catalog))
        require(args.allow_large_bl33 == "0" or args.artefact_mode == "custom",
                "large BL33 layout is exclusive to custom images")
        selected["allow_large_bl33"] = args.allow_large_bl33 == "1"
        if args.print_scratch_layout:
            require(selected.get("reboot_scratch_layout") in (1, 2), "unknown vendor reboot scratch layout")
            print(selected["reboot_scratch_layout"])
            return 0
        preflight(package, selected, args.cix_release, args.artefact_mode, args.signing_keys_dir)
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
