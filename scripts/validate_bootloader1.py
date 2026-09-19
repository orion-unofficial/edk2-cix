#!/usr/bin/env python3
"""Require unchanged, qualified vendor BL1 bytes in build inputs and outputs.

This is a whole-file integrity check, not a Boot ROM/eFuse acceptance test.
Vendor signature qualification of the pinned payloads is recorded separately.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import subprocess
import tarfile
import zipfile
from pathlib import Path

from bootloader1_vendor import verify_or_warn
from reconstruction_common import ReconstructionError, bootloader1_report_path, for_each_ref, main_wrapper, resolve_ref


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "config/bootloader1-payloads.json"
STOCK = "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/Firmwares/bootloader1.img"
CIX = "src/cix-v1.2/release-payloads/bootloader1-2026q1.img"
MAX_IMAGE_SIZE = 64 * 1024 * 1024
FLASH_HEADERS = (0x100000, 0x200000)
DISTRIBUTION_TARGETS = {"build-all", "buildbox-zip", "buildbox-targz", "buildbox-firmware-stage"}


def load_catalog(path: Path = CATALOG) -> dict[str, dict]:
    document = json.loads(path.read_text())
    if document.get("schema_version") != 1:
        raise ReconstructionError("unsupported BL1 catalogue schema")
    result = {}
    for entry in document["payloads"]:
        digest = entry["sha256"]
        if (not re.fullmatch(r"[0-9a-f]{64}", digest) or digest in result
                or not 0 < entry["size"] <= MAX_IMAGE_SIZE
                or entry.get("vendor_signature_qualified") is not True):
            raise ReconstructionError("invalid or unqualified BL1 catalogue entry")
        result[digest] = entry
    if not result:
        raise ReconstructionError("empty BL1 catalogue")
    return result


def read_image(path: Path) -> bytes:
    if not path.is_file() or not 0 < path.stat().st_size <= MAX_IMAGE_SIZE:
        raise ReconstructionError(f"missing, empty, or oversized BL1/flash image: {path}")
    return path.read_bytes()


def check_payload(data: bytes, label: str, catalog: dict[str, dict],
                  expected: set[str] | None = None) -> dict:
    digest = hashlib.sha256(data).hexdigest()
    entry = catalog.get(digest)
    if entry is None or len(data) != entry["size"]:
        raise ReconstructionError(
            f"{label}: BL1 is not an unchanged qualified vendor payload "
            f"(size={len(data)}, sha256={digest})"
        )
    if expected is not None and digest not in expected:
        raise ReconstructionError(f"{label}: BL1 differs from the selected source payload")
    return {"image": label, "size": len(data), "sha256": digest}


def extract_bl1(data: bytes, label: str) -> bytes:
    headers = [offset for offset in FLASH_HEADERS
               if data[offset:offset + 4] == b"\xaa\x55\xaa\x55"]
    if len(headers) != 1:
        raise ReconstructionError(f"{label}: expected one full-flash firmware header")
    offset = headers[0]
    if offset + 16 > len(data):
        raise ReconstructionError(f"{label}: truncated firmware header")
    _, version, count, flags = struct.unpack_from("<4I", data, offset)
    end = offset + 16 * (count + 1)
    if version != 1 or flags != 0 or not 1 <= count <= 128 or end > len(data):
        raise ReconstructionError(f"{label}: invalid full-flash firmware table")
    entries = [struct.unpack_from("<4I", data, offset + 16 * (i + 1)) for i in range(count)]
    bl1 = [entry for entry in entries if entry[0] == 1]
    if len(bl1) != 1:
        raise ReconstructionError(f"{label}: expected exactly one BL1 entry")
    _, start, size, _ = bl1[0]
    if not size or start + size > len(data) or (start < end and offset < start + size):
        raise ReconstructionError(f"{label}: invalid BL1 bounds")
    for kind, address, length, _ in entries:
        if address + length > len(data):
            raise ReconstructionError(f"{label}: firmware entry exceeds image bounds")
        if kind != 1 and length and address < start + size and start < address + length:
            raise ReconstructionError(f"{label}: firmware entry overlaps BL1")
    # Preserve the complete declared payload, including its own alignment bytes.
    return data[start:start + size]


def git_bytes(repo: Path, ref: str, path: str) -> bytes:
    resolved = resolve_ref(repo, ref)
    result = subprocess.run(["git", "-C", str(repo), "show", f"{resolved}:{path}"],
                            capture_output=True, check=False)
    if result.returncode:
        raise ReconstructionError(f"cannot read committed BL1 reference {ref}:{path}")
    return result.stdout


def source_payloads(worktree: Path, catalog: dict[str, dict], mode: str,
                    cix_release: str, target: str) -> set[str]:
    cix_release = cix_release.strip().lower().removeprefix("v")
    if cix_release not in {"", "1.2"}:
        raise ReconstructionError(f"unsupported BL1 CIX release: {cix_release}")
    if mode not in {"upstream", "custom", "custom+fixes"}:
        raise ReconstructionError(f"unsupported BL1 artefact mode: {mode}")
    paths = [CIX] if mode != "upstream" and cix_release else [STOCK]
    if target == "build-all":
        # build-all owns its variant matrix and overrides the caller's options.
        paths = [STOCK] + ([CIX] if (worktree / CIX).is_file() else [])
    expected = set()
    for path in paths:
        committed = git_bytes(worktree, "HEAD", path)
        record = check_payload(committed, f"committed {path}", catalog)
        current = read_image(worktree / path)
        if current != committed:
            raise ReconstructionError(f"{path}: BL1 input differs from the committed vendor payload")
        expected.add(record["sha256"])
    return expected


def check_archive(path: Path, catalog: dict[str, dict], expected: set[str],
                  payloads: dict[str, bytes] | None = None) -> list[dict]:
    records = []

    def check_member(name, size, stream):
        if not 0 < size <= MAX_IMAGE_SIZE:
            raise ReconstructionError(f"{path}:{name}: invalid flash-image size")
        data = stream.read(MAX_IMAGE_SIZE + 1)
        if len(data) != size:
            raise ReconstructionError(f"{path}:{name}: truncated flash-image member")
        label = f"{path}:{name}"
        payload = extract_bl1(data, label)
        record = check_payload(payload, label, catalog, expected)
        records.append(record)
        if payloads is not None:
            payloads[record["sha256"]] = payload

    if path.suffix == ".zip":
        with zipfile.ZipFile(path) as archive:
            for member in archive.infolist():
                if Path(member.filename).name == "cix_flash_all.bin":
                    with archive.open(member) as stream:
                        check_member(member.filename, member.file_size, stream)
    else:
        with tarfile.open(path, "r:gz") as archive:
            for member in archive:
                if Path(member.name).name == "cix_flash_all.bin":
                    if not member.isfile():
                        raise ReconstructionError(f"{path}:{member.name}: flash image is not a regular file")
                    with archive.extractfile(member) as stream:
                        check_member(member.name, member.size, stream)
    if not records:
        raise ReconstructionError(f"{path}: archive contains no full flash image to validate")
    return records


def check_outputs(worktree: Path, board: str, firmware_target: str, target: str,
                  catalog: dict[str, dict], expected: set[str], report_path: Path | None = None) -> list[dict]:
    report_path = report_path or bootloader1_report_path(ROOT, worktree, board, firmware_target)
    report_path.unlink(missing_ok=True)
    records = []
    payloads = {}
    build = worktree / "src/Build" / board
    prefix = firmware_target.upper()
    roots = [p for p in sorted(build.glob("*")) if p.is_dir()
             and re.fullmatch(r"(?:RELEASE|DEBUG)_[A-Z0-9]+", p.name)
             and (target == "build-all" or p.name == prefix or p.name.startswith(prefix + "_"))]
    if target != "build-all" and len(roots) > 1:
        raise ReconstructionError(f"multiple {prefix} firmware output trees exist for {board}")
    for root in roots:
        path = root / "cix_flash_all.bin"
        payload = extract_bl1(read_image(path), str(path))
        record = check_payload(payload, str(path), catalog, expected)
        records.append(record)
        payloads[record["sha256"]] = payload
        staged = root / "Firmwares/bootloader1.img"
        if staged.exists():
            check_payload(read_image(staged), str(staged), catalog, expected)
    if target in DISTRIBUTION_TARGETS:
        # dist/ can contain several legitimate variants from this source tree.
        # Every copied variant must retain one of this tree's vendor payloads.
        distribution_expected = source_payloads(worktree, catalog, "custom", "", "build-all")
        for path in sorted((worktree / "dist").rglob("*")):
            if not path.is_file():
                continue
            if path.name == "cix_flash_all.bin":
                payload = extract_bl1(read_image(path), str(path))
                record = check_payload(payload, str(path), catalog, distribution_expected)
                records.append(record)
                payloads[record["sha256"]] = payload
            elif target != "buildbox-firmware-stage" and path.name.endswith((".tar.gz", ".tgz", ".zip")):
                records.extend(check_archive(path, catalog, distribution_expected, payloads))
    if not records:
        raise ReconstructionError("no packaged full flash image was found for BL1 validation")
    vendor_result = verify_or_warn(payloads)
    report = {"check": "vendor-bl1-whole-file-integrity", "board_fuse_acceptance": "not-tested",
              "vendor_signatures": vendor_result,
              "acceptance_basis": ("approved-vendor-hash-and-signature" if vendor_result["status"] == "verified"
                                   else "approved-vendor-hash-fallback"),
              "approved_payloads": [{"sha256": digest, "size": catalog[digest]["size"],
                                     "provenance": catalog[digest]["provenance"]}
                                    for digest in sorted(payloads)],
              "expected_sha256": sorted(expected), "images": records}
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    return records


def check_source_refs(repo: Path, catalog: dict[str, dict]) -> int:
    checked = 0
    for digest, entry in catalog.items():
        for origin in entry["provenance"]:
            data = git_bytes(repo, origin["commit"], origin["path"])
            check_payload(data, origin["ref"], catalog, {digest})
            checked += 1
    refs = for_each_ref(repo, "source/unofficial/")
    if not refs:
        raise ReconstructionError("no Unofficial source refs available for BL1 audit")
    for ref in refs:
        for path in (STOCK, CIX):
            check_payload(git_bytes(repo, ref, path), f"{ref}:{path}", catalog)
            checked += 1
    return checked


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    choice = parser.add_mutually_exclusive_group(required=True)
    choice.add_argument("--image", type=Path, help="validate a standalone bootloader1.img")
    choice.add_argument("--flash-image", type=Path, help="validate BL1 inside a full flash image")
    choice.add_argument("--worktree", type=Path)
    choice.add_argument("--check-source-refs", action="store_true")
    parser.add_argument("--phase", choices=("inputs", "outputs"), default="inputs")
    parser.add_argument("--board", choices=("O6", "O6N"), default="O6")
    parser.add_argument("--firmware-target", default="RELEASE")
    parser.add_argument("--build-target", default="buildbox-firmware-build")
    parser.add_argument("--artefact-mode", default="custom")
    parser.add_argument("--cix-release", default="")
    parser.add_argument("--report", type=Path, help="host-writable JSON report path; defaults to the build-branch cache")
    args = parser.parse_args()
    catalog = load_catalog()
    if args.check_source_refs:
        count = check_source_refs(ROOT, catalog)
        print(f"[bl1] Qualified vendor bytes confirmed in {count} source inputs")
    elif args.worktree:
        report_path = args.report or bootloader1_report_path(ROOT, args.worktree, args.board, args.firmware_target)
        report_path.unlink(missing_ok=True)
        expected = source_payloads(args.worktree, catalog, args.artefact_mode, args.cix_release, args.build_target)
        if args.phase == "outputs":
            records = check_outputs(args.worktree, args.board, args.firmware_target, args.build_target,
                                    catalog, expected, report_path=report_path)
            print(f"[bl1] Unchanged vendor BL1 confirmed in {len(records)} packaged image(s)")
            print(f"[bl1] Validation report: {report_path}")
        else:
            print("[bl1] Selected BL1 input matches qualified vendor bytes")
    else:
        path = args.image or args.flash_image
        data = read_image(path)
        payload = extract_bl1(data, str(path)) if args.flash_image else data
        record = check_payload(payload, str(path), catalog)
        verify_or_warn({record["sha256"]: payload})
        print(f"[bl1] Unchanged vendor payload: {record['sha256']}")


if __name__ == "__main__":
    main_wrapper(main)
