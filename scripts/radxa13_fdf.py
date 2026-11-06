#!/usr/bin/env python3
"""Bounded Orion FDF normalization for newly prepared Radxa 1.3 checkpoints.

The project starting capacity is independent of the selected EDK2 version.
This does not select a physical flash allocation: build_bl33 and bl33_layout
still enforce the selected firmware catalog and final signed-FIP bounds.
"""

from __future__ import annotations

import os
from pathlib import Path
import posixpath
import re
import subprocess
from typing import TYPE_CHECKING

from reconstruction_common import ReconstructionError, git, load_json, temp_dir

if TYPE_CHECKING:
    from validate_radxa13_source import GitTree


BOARD_FDF = "edk2-platforms/Platform/Radxa/Orion/O6/O6.fdf"
O6N_FDF = "edk2-platforms/Platform/Radxa/Orion/O6N/O6N.fdf"
SOURCE_FDF = "src/" + BOARD_FDF
ORDINARY_FDF = "custom/overlay/" + BOARD_FDF
EXPERIMENTAL_FDF = "custom/overlay-experimental-uefi-settings/" + BOARD_FDF
RELEASE_SIZE = 0x1F4000
RADXA_132_RELEASE_SIZE = 0x220000
DEBUG_SIZE = 0x400000
MIRROR = posixpath.relpath(ORDINARY_FDF, posixpath.dirname(EXPERIMENTAL_FDF)).encode()
EXPERIMENTAL_COMMENT = (
    b"# Custom experimental RELEASE volume: use 8 KiB of the existing BL3 slot\n"
    b"# headroom. Full-flash and OTA validators still enforce vendor slot limits.\n"
)


def supported_family(repo: Path, edk2: str, radxa: str) -> bool:
    """Use the build catalog, never candidate metadata, to bound this family."""
    if radxa == "1.3.2" and edk2 != "202608":
        return False
    if radxa not in ("1.3.0", "1.3.1", "1.3.2"):
        return False
    configured = {row["edk2_ref"] for row in load_json(repo, "config/refs-edk2.json")["releases"]}
    return "edk2-stable" + edk2 in configured


def canonical_fdf(data: bytes, *, radxa: str = "1.3.1") -> bytes:
    """Change only reviewed capacity fields and the optional ACPITABLE rule.

    Reject extra FD regions, altered geometry, and unfamiliar preprocessing.
    Preserve the complete module/include/rule body from this source era.
    """
    if radxa == "1.3.2":
        release_size = RADXA_132_RELEASE_SIZE
        literal_sizes = conditional_sizes = (release_size,)
    elif radxa in ("1.3.0", "1.3.1"):
        release_size = RELEASE_SIZE
        literal_sizes = (0x1F0000, 0x1F2000)
        conditional_sizes = (*literal_sizes, release_size)
    else:
        raise ReconstructionError(f"unsupported Radxa FDF release: {radxa}")
    data = data.replace(b"\r\n", b"\n")
    if b"\r" in data:
        raise ReconstructionError("unreviewed Orion FDF line endings")
    data = data.replace(EXPERIMENTAL_COMMENT, b"")
    sections = re.findall(rb"(?m)^\[FD\.([^\]]+)\]$", data)
    if sections != [b"SKY1_BL33_UEFI"]:
        raise ReconstructionError("unreviewed Orion FDF FD sections")
    fd = data.split(b"[FD.SKY1_BL33_UEFI]\n", 1)[1].split(b"[FV.", 1)[0]
    if re.search(rb"(?m)^[ \t]*\[[^\]]+\]", fd):
        raise ReconstructionError("unreviewed Orion FDF section within FD layout")
    if re.findall(rb"(?m)^\[FV\.([^\]]+)\]$", data) != [b"FvMain", b"FVMAIN_COMPACT"]:
        raise ReconstructionError("unreviewed Orion FDF FV sections")
    for field in (b"BaseAddress", b"Size", b"ErasePolarity", b"BlockSize", b"NumBlocks"):
        if len(re.findall(rb"(?m)^[ \t]*" + field + rb"[ \t]*=", fd)) != 1:
            raise ReconstructionError("unreviewed Orion FDF field count")
    if not re.search(rb"(?m)^BaseAddress[ \t]*=[ \t]*0x84400000\|gArmTokenSpaceGuid.PcdFdBaseAddress$", fd):
        raise ReconstructionError("unreviewed Orion FDF base address")
    if not re.search(rb"(?m)^ErasePolarity[ \t]*=[ \t]*1$", fd):
        raise ReconstructionError("unreviewed Orion FDF erase polarity")
    if len(re.findall(rb"(?m)^BlockSize[ \t]*=[ \t]*0x00001000$", fd)) != 1:
        raise ReconstructionError("unreviewed Orion FDF block geometry")
    regions = re.findall(rb"(?m)^[ \t]*0x[0-9a-fA-F]+[ \t]*\|([^\n]+)$", fd)
    if len(regions) != 1:
        raise ReconstructionError("unreviewed Orion FDF region count")
    conditional = re.compile(
        rb"\[Defines\]\n!if \$\(TARGET\) == DEBUG\n"
        rb"  DEFINE SKY1_BL33_UEFI_FD_SIZE   = (0x[0-9a-fA-F]+)\n"
        rb"  DEFINE SKY1_BL33_UEFI_FD_BLOCKS = (0x[0-9a-fA-F]+)\n!else\n"
        rb"  DEFINE SKY1_BL33_UEFI_FD_SIZE   = (0x[0-9a-fA-F]+)\n"
        rb"  DEFINE SKY1_BL33_UEFI_FD_BLOCKS = (0x[0-9a-fA-F]+)\n!endif\n"
    )
    matches = list(conditional.finditer(data))
    definitions = re.findall(rb"(?m)^\s*DEFINE SKY1_BL33_UEFI_FD_(?:SIZE|BLOCKS)\b", data)
    if len(matches) == 1 and len(definitions) == 4:
        values = [int(value, 16) for value in matches[0].groups()]
        if (values[:2] != [DEBUG_SIZE, DEBUG_SIZE // 0x1000] or
                values[2] not in conditional_sizes or
                values[3] != values[2] // 0x1000):
            raise ReconstructionError("unreviewed Orion FDF DEBUG/RELEASE capacity")
        expected = (
            rb"(?m)^Size[ \t]*=[ \t]*\$\(SKY1_BL33_UEFI_FD_SIZE\)\|gArmTokenSpaceGuid.PcdFdSize$",
            rb"(?m)^NumBlocks[ \t]*=[ \t]*\$\(SKY1_BL33_UEFI_FD_BLOCKS\)$",
            rb"(?m)^0x00000000\|\$\(SKY1_BL33_UEFI_FD_SIZE\)\n"
            rb"gArmTokenSpaceGuid.PcdFvBaseAddress\|gArmTokenSpaceGuid.PcdFvSize\nFV = FVMAIN_COMPACT$",
        )
        if any(len(re.findall(pattern, fd)) != 1 for pattern in expected):
            raise ReconstructionError("unreviewed Orion FDF conditional region layout")
        match = matches[0]
        # Keep spacing and every body byte; replace only production literals.
        for group, value in ((4, f"0x{release_size // 0x1000:x}".encode()),
                             (3, f"0x{release_size:08x}".encode())):
            start, end = match.span(group)
            data = data[:start] + value + data[end:]
    elif not definitions and b"[Defines]" not in data:
        sizes = re.findall(rb"(?m)^Size[ \t]*=[ \t]*(0x[0-9a-fA-F]+)\|gArmTokenSpaceGuid.PcdFdSize$", fd)
        blocks = re.findall(rb"(?m)^NumBlocks[ \t]*=[ \t]*(0x[0-9a-fA-F]+)$", fd)
        if (len(sizes) != 1 or len(blocks) != 1 or
                int(sizes[0], 16) not in literal_sizes or
                int(blocks[0], 16) != int(sizes[0], 16) // 0x1000 or
                regions[0] != sizes[0] or
                not re.search(rb"(?m)^0x00000000\|0x[0-9a-fA-F]+\n"
                              rb"gArmTokenSpaceGuid.PcdFvBaseAddress\|gArmTokenSpaceGuid.PcdFvSize\nFV = FVMAIN_COMPACT$", fd)):
            raise ReconstructionError("unreviewed Orion FDF literal region layout")
        defines = ("[Defines]\n!if $(TARGET) == DEBUG\n"
                   "  DEFINE SKY1_BL33_UEFI_FD_SIZE   = 0x00400000\n"
                   "  DEFINE SKY1_BL33_UEFI_FD_BLOCKS = 0x400\n!else\n"
                   f"  DEFINE SKY1_BL33_UEFI_FD_SIZE   = 0x{release_size:08x}\n"
                   f"  DEFINE SKY1_BL33_UEFI_FD_BLOCKS = 0x{release_size // 0x1000:x}\n!endif\n\n").encode()
        data = data.replace(b"[FD.SKY1_BL33_UEFI]\n", defines + b"[FD.SKY1_BL33_UEFI]\n", 1)
        for pattern, replacement in (
            (rb"(?m)^(Size[ \t]*=[ \t]*)0x[0-9a-fA-F]+(?=\|gArmTokenSpaceGuid.PcdFdSize$)",
             rb"\1$(SKY1_BL33_UEFI_FD_SIZE)"),
            (rb"(?m)^(NumBlocks[ \t]*=[ \t]*)0x[0-9a-fA-F]+$", rb"\1$(SKY1_BL33_UEFI_FD_BLOCKS)"),
            (rb"(?m)^0x00000000\|0x[0-9a-fA-F]+$", b"0x00000000|$(SKY1_BL33_UEFI_FD_SIZE)"),
            (rb"(?m)^# UEFI image size 0x[0-9a-fA-F]+$", b"# UEFI image size $(SKY1_BL33_UEFI_FD_SIZE)"),
        ):
            data, count = re.subn(pattern, replacement, data)
            if count != 1:
                raise ReconstructionError("unreviewed Orion FDF literal field count")
    else:
        raise ReconstructionError("unreviewed Orion FDF size definitions")
    rule = re.compile(rb"(\[Rule.Common.USER_DEFINED.ACPITABLE\]\n[^\[]*?\n[ \t]*RAW ASL)(?: Optional)?([ \t]+\|\.aml\n)")
    data, count = rule.subn(rb"\1 Optional       |.aml\n", data)
    if count != 1:
        raise ReconstructionError("unreviewed Orion FDF ACPITABLE rule")
    return data


def expected_fdf(tree: GitTree, *, source_path: str = SOURCE_FDF, radxa: str = "1.3.1") -> bytes:
    """Derive expected content from the candidate's own imported source FDF."""
    if source_path not in tree.entries or tree.entries[source_path].mode != "100644":
        raise ReconstructionError(f"missing regular source FDF: {source_path}")
    data = tree.blob(source_path)
    # Every literal module must exist in this source era. In particular, the
    # ArmGic vs ArmGicDxe path is preserved rather than copied from 202608.
    modules = re.findall(rb"(?m)^[ \t]*INF[ \t]+(?:RuleOverride[ \t]*=[ \t]*\w+[ \t]+)?([^\s]+\.inf)[ \t]*\r?$", data)
    declarations = re.findall(rb"(?m)^[ \t]*INF[ \t]+[^\r\n]+", data)
    if not modules or len(modules) != len(declarations):
        raise ReconstructionError("unreviewed Orion FDF module declarations")
    for module in modules:
        path = module.decode("ascii")
        if path.startswith("/") or ".." in path.split("/"):
            raise ReconstructionError(f"unsafe O6 FDF module path: {path}")
        if not any(f"src/{package}/{path}" in tree.entries
                   for package in ("edk2", "edk2-platforms", "edk2-non-osi")):
            raise ReconstructionError(f"missing O6 FDF source module: {path}")
    return canonical_fdf(data, radxa=radxa)


def fdf_updates(tree: GitTree, *, radxa: str = "1.3.1") -> dict[str, tuple[str, bytes]]:
    """Normalize recognized custom FDFs, rejecting any unreviewed body delta."""
    updates = _board_fdf_updates(tree, BOARD_FDF, radxa)
    if radxa == "1.3.2":
        updates.update(_board_fdf_updates(tree, O6N_FDF, radxa))
    return updates


def _board_fdf_updates(tree: GitTree, board_fdf: str, radxa: str) -> dict[str, tuple[str, bytes]]:
    expected = expected_fdf(tree, source_path="src/" + board_fdf, radxa=radxa)
    ordinary_path = "custom/overlay/" + board_fdf
    experimental_path = "custom/overlay-experimental-uefi-settings/" + board_fdf
    mirror = posixpath.relpath(ordinary_path, posixpath.dirname(experimental_path)).encode()
    ordinary, experimental = tree.entries.get(ordinary_path), tree.entries.get(experimental_path)
    if ordinary is not None:
        if ordinary.mode != "100644" or experimental is None or experimental.mode != "120000":
            raise ReconstructionError(f"ambiguous custom FDF topology: {ordinary_path}")
        if tree.resolve(experimental_path) != ordinary_path:
            raise ReconstructionError(f"experimental FDF does not mirror ordinary FDF: {experimental_path}")
        active = ordinary_path
    elif experimental is not None and experimental.mode == "100644":
        active = experimental_path
    else:
        raise ReconstructionError(f"missing recognized custom FDF: {ordinary_path}")
    if canonical_fdf(tree.blob(active), radxa=radxa) != expected:
        raise ReconstructionError(f"custom FDF has unreviewed changes from its own source body: {active}")
    updates = {}
    for path, mode, data in ((ordinary_path, "100644", expected),
                             (experimental_path, "120000", mirror)):
        if path not in tree.entries or tree.entries[path].mode != mode or tree.blob(path) != data:
            updates[path] = mode, data
    return updates


def normalize_source_fdf(repo: Path, candidate: str, edk2: str, radxa: str) -> str:
    """Return a child source commit without moving refs or editing source files."""
    from validate_radxa13_source import GitTree

    if not supported_family(repo, edk2, radxa):
        raise ReconstructionError(f"unsupported Orion FDF family: edk2-{edk2}/radxa-{radxa}")
    tree = GitTree(repo, candidate)
    updates = fdf_updates(tree, radxa=radxa)
    if not updates:
        return candidate
    with temp_dir(repo, "radxa13-fdf-index-") as scratch:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(scratch) / "index"))

        def indexed(*args: str) -> str:
            return subprocess.run(["git", "-C", str(repo), *args], env=env,
                                  check=True, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True).stdout.strip()

        indexed("read-tree", candidate)
        for path, (mode, data) in updates.items():
            oid = subprocess.run(["git", "-C", str(repo), "hash-object", "-w", "--stdin"],
                                 input=data, check=True, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE).stdout.decode().strip()
            indexed("update-index", "--add", "--cacheinfo", mode, oid, path)
        result = indexed("write-tree")
    message = ("source: normalize O6 Radxa 1.3 custom FDF layout\n\n"
               f"Source-Fdf-Input: {candidate}\n"
               f"Source-Fdf-Source-Blob: {tree.entries[SOURCE_FDF].oid}\n"
               "Source-Fdf-Policy: O6 ordinary overlay, RELEASE 0x1f4000, DEBUG 0x400000\n")
    if radxa == "1.3.2":
        message = ("source: normalize Orion Radxa 1.3.2 custom FDF layouts\n\n"
                   f"Source-Fdf-Input: {candidate}\n"
                   f"Source-Fdf-Source-Blob: {tree.entries[SOURCE_FDF].oid}\n"
                   f"Source-Fdf-O6N-Source-Blob: {tree.entries['src/' + O6N_FDF].oid}\n"
                   "Source-Fdf-Policy: O6/O6N ordinary overlays, RELEASE 0x220000, DEBUG 0x400000\n")
    return git(repo, "commit-tree", result, "-p", candidate, "-m", message).stdout.strip()
