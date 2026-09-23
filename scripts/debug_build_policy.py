#!/usr/bin/env python3
"""Shared, artifact-neutral debug preflight for public and raw source builds."""

from __future__ import annotations

import json
import re
from typing import Callable

HEADER = "src/edk2/MdePkg/Include/Library/DebugLib.h"
PACKAGE = "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/"
TRUE = {"1", "true", "on", "yes"}


def fdf_paths(board: str, experimental: bool = False) -> list[str]:
    """Mirror the custom build's package-path precedence for board FDFs."""
    prefixes = ["custom/overlay", "src"]
    if experimental:
        prefixes.insert(0, "custom/overlay-experimental-uefi-settings")
    return [f"{prefix}/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.fdf"
            for prefix in prefixes]


def debug_bits(header: str) -> dict[str, int]:
    section = header.split("Declare bits for PcdDebugPrintErrorLevel", 1)[-1]
    section = section.split("Aliases of debug message mask bits", 1)[0]
    bits = {name: int(value, 16) for name, value in re.findall(
        r"^#define\s+(DEBUG_\w+)\s+(0x[0-9a-fA-F]+)\b", section, re.M
    )}
    if not bits or "DEBUG_ERROR" not in bits:
        raise ValueError("cannot derive debug categories from the selected DebugLib.h")
    return bits


def fd_size(fdf: str, target: str) -> int:
    # Deliberately recognize the retained board layout rather than guess at
    # arbitrary DSC/FDF preprocessing. A new layout needs a reviewed parser.
    literal = re.search(r"^Size\s*=\s*(0x[0-9a-fA-F]+)\|gArmTokenSpaceGuid.PcdFdSize\s*$", fdf, re.M)
    if literal:
        return int(literal[1], 16)
    if "[Defines]" not in fdf:
        raise ValueError("missing board FD size definitions")
    defines = fdf.split("[Defines]", 1)[1].split("[FD.", 1)[0]
    block = re.search(r"!if\s+\$\(TARGET\)\s*==\s*DEBUG\s*\n(.*?)!else\s*\n(.*?)!endif", defines, re.S)
    if not block:
        raise ValueError("unrecognized board FD layout; cannot perform debug preflight")
    selected = block[1 if target == "DEBUG" else 2]
    match = re.search(r"^\s*DEFINE\s+SKY1_BL33_UEFI_FD_SIZE\s*=\s*(0x[0-9a-fA-F]+)\s*$", selected, re.M)
    if not match:
        raise ValueError("missing literal SKY1_BL33_UEFI_FD_SIZE in selected board layout")
    return int(match[1], 16)


def preflight(read: Callable[[str], str], *, board: str, target: str,
              verbose: str = "", mask: str = "", force: str = "",
              allow_large: str = "",
              fdf_override: str | None = None) -> dict:
    if force not in {"", "0", "1"}:
        raise ValueError("FORCE_DEBUG_BUILD must be 0 or 1")
    if allow_large not in {"", "0", "1"}:
        raise ValueError("DEBUG_ALLOW_LARGE_IMAGE must be 0 or 1")
    bits = debug_bits(read(HEADER))
    maximum = 0
    for bit in bits.values():
        maximum |= bit
    logging = verbose.lower() in TRUE
    effective = int(mask, 0) if mask else (maximum if logging else 0x80000040)
    if effective < 0 or effective > 0xFFFFFFFF or effective & ~maximum:
        raise ValueError(f"DEBUG_PRINT_ERROR_LEVEL contains invalid bits; accepted category mask is 0x{maximum:08X}")
    fdf = f"src/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.fdf"
    size = fd_size(fdf_override if fdf_override is not None else read(fdf), target)
    slots = []
    for name in ("all", "ota"):
        layout = json.loads(read(PACKAGE + f"spi_flash_config_{name}.json"))
        slots.extend(int(item["size"], 0) for item in layout["image_header_groups"]
                     if item["image_type"] == 7)
    if len(slots) != 2:
        raise ValueError("expected one bootloader3.img slot in each flash layout")
    # FD capacity is now measured and adjusted inside the custom build. Consent
    # is enforced against the actual signed FIP; masks are not size predictions.
    # Keep FORCE_DEBUG_BUILD as a compatibility input, never a guard bypass.
    return {"effective_mask": f"0x{effective:08X}", "accepted_mask": f"0x{maximum:08X}",
            "fd_size": size, "bootloader3_slot": min(slots), "experimental_reasons": [],
            "allow_large": allow_large == "1"}
