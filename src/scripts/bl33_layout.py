#!/usr/bin/env python3
"""Select a bounded custom BL33 allocation without changing vendor inputs."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path

from firmware_chain import require

# Exact BL2 audited in docs/src/bl33-capacity-review-20260922.md. Other loaders
# may use their vendor-declared slot, but cannot silently inherit this extension.
AUDITED_BL1 = "fcddd093649e243b16e0b9f92ec3bec7e68e03fe78fed2c594952f0eefd72f15"
FLASH_SIZE = 0x800000


def limits(selected: dict) -> dict:
    layout = selected["flash_layout"]
    slot = layout["entries"]["7"]
    start, original = slot["address"], slot["size"]
    require(layout["size"] == FLASH_SIZE and 0 < start < start + original <= FLASH_SIZE,
            "invalid vendor BL33 allocation")
    maximum = original
    if (selected["bl1_sha256"] == AUDITED_BL1 and start == 0x406000
            and original == 0x1F9000):
        require(all(row["address"] + row["size"] <= start
                    for kind, row in layout["entries"].items() if kind != "7"),
                "BL33 extension would overlap a later vendor allocation")
        maximum = FLASH_SIZE - start
    return {"address": start, "original_slot_size": original,
            "maximum_slot_size": maximum, "flash_size": FLASH_SIZE}


def select(selected: dict, payload_size: int, allow_large: bool = False) -> dict:
    result = limits(selected)
    require(0 < payload_size <= result["maximum_slot_size"],
            f"BL33 FIP size 0x{payload_size:x} exceeds maximum supported allocation "
            f"0x{result['maximum_slot_size']:x} (including all certificates and headers)")
    enlarged = payload_size > result["original_slot_size"]
    require(not enlarged or allow_large,
            f"BL33 FIP size 0x{payload_size:x} exceeds original slot "
            f"0x{result['original_slot_size']:x}; use DEBUG_ALLOW_LARGE_IMAGE=1 "
            "to permit the audited full-image layout")
    result.update(payload_size=payload_size, mode="full-image" if enlarged else "original",
                  slot_size=result["maximum_slot_size"] if enlarged else result["original_slot_size"],
                  full_image_only=enlarged)
    return result


def validation_layout(selected: dict, allow_large: bool) -> dict:
    result = copy.deepcopy(selected["flash_layout"])
    if allow_large:
        result["entries"]["7"]["size"] = limits(selected)["maximum_slot_size"]
    return result


def prepare(stage: Path, package: Path, allow_large: bool) -> dict:
    # Import here so the verifier can also reuse the pure allocation functions.
    from validate_firmware_chain import load_catalog, reference, validate_uefi
    selected = reference(package, load_catalog())
    payload = (stage / "Firmwares/bootloader3.img").read_bytes()
    validate_uefi(payload, selected)
    result = select(selected, len(payload), allow_large)
    result["payload_sha256"] = hashlib.sha256(payload).hexdigest()
    result["vendor_bl1_sha256"] = selected["bl1_sha256"]
    for kind in ("all", "ota"):
        name = f"spi_flash_config_{kind}.json"
        original_bytes = (package / name).read_bytes()
        original = json.loads(original_bytes)
        path = stage / name
        if path.exists():
            # Repeated packaging can see our previous selection, but no other
            # change is authorised, even when large-image consent is present.
            current = json.loads(path.read_text())
            for row in current["image_header_groups"]:
                if row["image_type"] == 7:
                    row["size"] = next(r["size"] for r in original["image_header_groups"]
                                       if r["image_type"] == 7)
            require(current == original, "staged flash configuration changes more than the BL33 allocation")
        if result["mode"] == "original":
            path.write_bytes(original_bytes)
        else:
            rows = [row for row in original["image_header_groups"] if row["image_type"] == 7]
            require(len(rows) == 1, "expected exactly one BL33 packaging entry")
            rows[0]["size"] = f"0x{result['slot_size']:x}"
            path.write_text(json.dumps(original, indent=2) + "\n")
    (stage / "bl33-layout.json").write_text(json.dumps(result, indent=2) + "\n")
    marker = stage / "bl33-full-image-only"
    if result["full_image_only"]:
        marker.write_text("Install cix_flash_all.bin through the full-image updater; no OTA image is emitted.\n")
    else:
        marker.unlink(missing_ok=True)
    return result


def main() -> None:
    from firmware_chain import ChainError
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--reference-dir", type=Path, required=True)
    parser.add_argument("--allow-large", choices=("0", "1"), default="0")
    args = parser.parse_args()
    try:
        result = prepare(args.stage, args.reference_dir, args.allow_large == "1")
    except (ChainError, OSError, ValueError) as exc:
        parser.exit(2, f"[bl33-layout] REJECTED: {exc}\n")
    print(f"[bl33-layout] {result['mode']}: FIP=0x{result['payload_size']:x}, "
          f"slot=0x{result['address']:x}..0x{result['address'] + result['slot_size']:x}")


if __name__ == "__main__":
    main()
