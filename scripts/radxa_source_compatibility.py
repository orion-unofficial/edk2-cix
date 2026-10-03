#!/usr/bin/env python3
"""Narrow Sky1 source adaptations for selected EDK2 interfaces."""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import re
import subprocess

from reconstruction_common import ReconstructionError, git, temp_dir
from validate_radxa13_source import GitTree
from source_lifecycle import mirror_symlink_target

LEGACY_EXCEPTION = "ArmPkg/Library/ArmExceptionLib/ArmExceptionLib.inf"
DXE_EXCEPTION = "UefiCpuPkg/Library/CpuExceptionHandlerLib/DxeCpuExceptionHandlerLib.inf"
SKY1_DESCRIPTOR = "edk2-platforms/Platform/CIX/Sky1/Sky1Common.dsc.inc"
DESCRIPTORS = ("src/" + SKY1_DESCRIPTOR, "custom/overlay/" + SKY1_DESCRIPTOR)
DEPENDENCY_BINDINGS = (
    ("GptLib", "MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf",
     "MdeModulePkg/Library/GptLib/GptLib.inf", "NetLib"),
    ("ArmSmcccSocIdLib", "ArmPkg/Universal/Smbios/ProcessorSubClassDxe/ProcessorSubClassDxe.inf",
     "ArmPkg/Library/ArmSmcccSocIdLib/ArmSmcccSocIdLib.inf", "ArmSmcLib"),
)


def inf_library_classes(data: bytes) -> set[str]:
    """Read common/AArch64 INF dependencies, excluding other architectures."""
    result = set()
    selected = False
    for raw in data.splitlines():
        line = raw.split(b"#", 1)[0].strip()
        if line.startswith(b"["):
            selected = line.lower() in (b"[libraryclasses]", b"[libraryclasses.common]",
                                        b"[libraryclasses.aarch64]")
        elif selected and re.fullmatch(rb"\w+", line):
            result.add(line.decode("ascii"))
    return result


def add_library_binding(data: bytes, name: str, provider: str, anchor: str) -> bytes:
    """Insert a reviewed binding in its common scope and matching runtime scope."""
    scopes = {b"[libraryclasses]", b"[libraryclasses.common]"}
    if name == "ArmSmcccSocIdLib":
        scopes.add(b"[libraryclasses.common.dxe_runtime_driver]")
    lines = data.splitlines(keepends=True)
    section = b""
    sections: dict[bytes, list[int]] = {}
    for index, raw in enumerate(lines):
        line = raw.split(b"#", 1)[0].strip()
        if line.startswith(b"["):
            section = line.lower()
        if section in scopes:
            sections.setdefault(section, []).append(index)
    if not (set(sections) & {b"[libraryclasses]", b"[libraryclasses.common]"}):
        raise ReconstructionError("missing common Sky1 library scope")
    insertions = {}
    for scope, indexes in sections.items():
        existing = [index for index in indexes
                    if re.match(rb"\s*" + name.encode() + rb"\s*\|", lines[index])]
        if existing:
            continue
        anchors = [index for index in indexes
                   if re.match(rb"\s*" + anchor.encode() + rb"\s*\|", lines[index])]
        if len(anchors) != 1:
            raise ReconstructionError(f"unexpected {anchor} anchor for {name} in {scope.decode()}")
        index = anchors[0]
        newline = b"\r\n" if lines[index].endswith(b"\r\n") else b"\n"
        indent = re.match(rb"[ \t]*", lines[index])[0]
        insertions[index] = indent + name.encode() + b"|" + provider.encode() + newline
    return b"".join(line + insertions.get(index, b"") for index, line in enumerate(lines))


def source_library_updates(tree: GitTree) -> dict[str, bytes]:
    """Adapt known dependencies together, gated on the selected consumer INFs."""
    updates = exception_library_updates(tree)
    updates.update(acpi_helper_updates(tree))
    updates.update(aml_library_updates(tree))
    for path in DESCRIPTORS:
        if path not in tree.entries:
            continue
        resolved = tree.resolve(path)
        data = updates.get(resolved, tree.blob(resolved))
        for name, consumer, provider, anchor in DEPENDENCY_BINDINGS:
            if not re.search(rb"(?m)^[ \t]*" + re.escape(consumer.encode()) + rb"(?:[ \t{\r]|$)", data):
                continue
            consumer_path = "src/edk2/" + consumer
            if consumer_path not in tree.entries:
                raise ReconstructionError(f"missing selected library consumer: {consumer}")
            if name not in inf_library_classes(tree.blob(tree.resolve(consumer_path))):
                continue
            provider_path = "src/edk2/" + provider
            if provider_path not in tree.entries:
                raise ReconstructionError(f"missing selected dependency provider: {provider}")
            declaration = rb"(?m)^[ \t]*LIBRARY_CLASS[ \t]*=[ \t]*" + name.encode() + rb"(?:[ \t|\r]|$)"
            if not re.search(declaration, tree.blob(tree.resolve(provider_path))):
                raise ReconstructionError(f"selected provider does not declare {name}: {provider}")
            data = add_library_binding(data, name, provider, anchor)
        if data != tree.blob(resolved):
            if tree.entries[resolved].mode != "100644":
                raise ReconstructionError(f"unexpected Sky1 descriptor mode: {resolved}")
            updates[resolved] = data
    return updates


ACPI_TABLE_DIRECTORY = "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/"
ACPI_LIB_HEADER = "src/edk2/EmbeddedPkg/Include/Library/AcpiLib.h"
ACPI_HELPER_HEADER = "src/edk2/MdeModulePkg/Include/AcpiHelperMacros.h"
ACPI_MACRO_RENAMES = {"NULL_GAS": "ACPI_NULL_GAS", "ARM_GAS32": "ACPI_GAS32"}


def acpi_helper_updates(tree: GitTree) -> dict[str, bytes]:
    """Adapt custom table producers when the selected EDK2 removed old GAS macros.

    Imported table bytes stay intact. A mirror needing adaptation becomes a
    regular custom overlay; unaffected mirrors retain their exact symlink blob.
    """
    if ACPI_LIB_HEADER not in tree.entries:
        return {}
    library = tree.blob(tree.resolve(ACPI_LIB_HEADER))
    updates = {}
    for name in ("Fadt", "Dbg2", "Spcr"):
        path = ACPI_TABLE_DIRECTORY + name + ".aslc"
        if path not in tree.entries:
            continue
        data = tree.blob(tree.resolve(path))
        needed = [old for old in ACPI_MACRO_RENAMES
                  if re.search(rb"\b" + old.encode() + rb"\b", data)
                  and not re.search(rb"(?m)^#define[ \t]+" + old.encode() + rb"\b", library)]
        if not needed:
            continue
        if ACPI_HELPER_HEADER not in tree.entries:
            raise ReconstructionError("missing selected ACPI helper macro header")
        helper = tree.blob(tree.resolve(ACPI_HELPER_HEADER))
        for old in needed:
            new = ACPI_MACRO_RENAMES[old]
            if not re.search(rb"(?m)^#define[ \t]+" + new.encode() + rb"\b", helper):
                raise ReconstructionError(f"selected ACPI helper header lacks {new}")
            data = re.sub(rb"\b" + old.encode() + rb"\b", new.encode(), data)
        include = b"#include <AcpiHelperMacros.h>"
        if include not in data:
            anchor = rb"(?m)^#include <Library/AcpiLib.h>(\r?\n)"
            data, count = re.subn(anchor, lambda m: include + m[1] + m[0], data)
            if count != 1:
                raise ReconstructionError(f"unexpected ACPI library include in {path}")
        if tree.entries[path].mode not in ("100644", "120000"):
            raise ReconstructionError(f"unexpected ACPI table mode: {path}")
        updates[path] = data
    return updates


AML_MODULE = "edk2-platforms/Platform/CIX/Sky1/Library/Acpi/CIX/AmlLib/"
AML_OVERLAY = "custom/overlay/" + AML_MODULE
AML_CODEGEN = AML_OVERLAY + "CodeGen/AmlCodeGen.c"
AML_PUBLIC_HEADER = "src/edk2/DynamicTablesPkg/Include/Library/AmlLib/AmlLib.h"
# Reviewed source evidence: the vendor's 202208/202605 private method and the
# public declaration added by upstream 95a7323e86126560e0a37b3801bc6c12a8428cda.
AML_PRIVATE_METHOD_SHA256 = "8ca9649636e27e94d53f6d2d87c7c51187f3858f59902d6ca0ae6462de3933f6"
AML_PUBLIC_SIGNATURE_SHA256 = "6958de4323d7d97369eb5f257ab17e7948642b64d796380757b649445d5336c6"


def aml_library_updates(tree: GitTree) -> dict[str, bytes]:
    """Export the vendor method through a custom module when EDK2 exposes it.

    The selected CIX instance supplies AmlLib in place of the upstream instance.
    Its method already has the public API's ABI through AML_HANDLE. Preserve
    its implementation and callers; remove only the obsolete static linkage.
    """
    if AML_PUBLIC_HEADER not in tree.entries:
        return {}
    header = tree.blob(tree.resolve(AML_PUBLIC_HEADER))
    if not re.search(rb"(?m)^[ \t]*AmlCodeGenMethod[ \t]*\(", header):
        return {}
    binding = rb"(?m)^[ \t]*AmlLib[ \t]*\|[ \t]*" + re.escape(
        AML_MODULE.removeprefix("edk2-platforms/").encode() + b"AmlLib.inf") + rb"[ \t]*\r?$"
    if not any(path in tree.entries and re.search(binding, tree.blob(tree.resolve(path)))
               for path in DESCRIPTORS if path.startswith("custom/")):
        return {}
    prototype = re.search(rb"(?m)^EFI_STATUS\r?\nEFIAPI\r?\nAmlCodeGenMethod\s*\([^;]+;", header)
    if (prototype is None or hashlib.sha256(re.sub(rb"\s+", b"", prototype[0])).hexdigest()
            != AML_PUBLIC_SIGNATURE_SHA256):
        raise ReconstructionError("unreviewed public AML method signature")
    imported = "src/" + AML_MODULE
    source = AML_CODEGEN if AML_CODEGEN in tree.entries else imported + "CodeGen/AmlCodeGen.c"
    if source not in tree.entries:
        raise ReconstructionError("missing selected CIX AML method implementation")
    data = tree.blob(tree.resolve(source))
    pattern = rb"(?m)^STATIC(\r?\n)(EFI_STATUS\r?\nEFIAPI\r?\nAmlCodeGenMethod[ \t]*\()"
    private = re.search(pattern, data)
    if private:
        normalized = data.replace(b"\r\n", b"\n")
        declaration = re.search(rb"(?m)^STATIC\nEFI_STATUS\nEFIAPI\nAmlCodeGenMethod\s*\(", normalized)
        try:
            end = normalized.index(b"\n}", declaration.end()) + 2
        except (ValueError, AttributeError) as exc:
            raise ReconstructionError("invalid private CIX AML method body") from exc
        method = normalized[declaration.start():end]
        if hashlib.sha256(method).hexdigest() != AML_PRIVATE_METHOD_SHA256:
            raise ReconstructionError("unreviewed private CIX AML method implementation")
    rewritten, count = re.subn(pattern, lambda match: match[2], data)
    if count == 0:
        # Ignore comments only to detect unfamiliar private declaration layouts.
        # Never use this comparison view to rewrite imported source bytes.
        tokens = re.sub(rb"/\*.*?\*/|//[^\r\n]*", b" ", data, flags=re.S)
        if re.search(rb"\bSTATIC\s+EFI_STATUS\s+EFIAPI\s+AmlCodeGenMethod\s*\(", tokens):
            raise ReconstructionError("unreviewed private CIX AML method declaration")
        if AML_CODEGEN not in tree.entries:
            return {}
        normalized = data.replace(b"\r\n", b"\n")
        declaration = re.search(rb"(?m)^EFI_STATUS\nEFIAPI\nAmlCodeGenMethod\s*\(", normalized)
        try:
            end = normalized.index(b"\n}", declaration.end()) + 2
        except (ValueError, AttributeError) as exc:
            raise ReconstructionError("invalid custom CIX AML method body") from exc
        method = b"STATIC\n" + normalized[declaration.start():end]
        if hashlib.sha256(method).hexdigest() != AML_PRIVATE_METHOD_SHA256:
            raise ReconstructionError("unreviewed custom CIX AML method implementation")
    elif count != 1:
        raise ReconstructionError("unexpected private CIX AML method declarations")
    inf = imported + "AmlLib.inf"
    if inf not in tree.entries or not re.search(
            rb"(?m)^[ \t]*LIBRARY_CLASS[ \t]*=[ \t]*AmlLib[ \t]*\r?$",
            tree.blob(tree.resolve(inf))):
        raise ReconstructionError("selected CIX AML provider does not declare AmlLib")
    if not re.search(rb"(?m)^[ \t]*\*_\*_\*_CC_FLAGS[ \t]*=[ \t]*-DAML_HANDLE[ \t]*\r?$",
                     tree.blob(tree.resolve(inf))):
        raise ReconstructionError("selected CIX AML provider lacks reviewed AML_HANDLE ABI")
    updates = {AML_CODEGEN: rewritten} if rewritten != data or source != AML_CODEGEN else {}
    # EDK2 module resolution selects a directory as a unit. Complete that unit
    # with canonical imported-source mirrors rather than a partial overlay.
    for path, entry in tree.entries.items():
        if not path.startswith(imported):
            continue
        overlay = "custom/overlay/" + path.removeprefix("src/")
        if overlay in tree.entries or overlay in updates:
            continue
        if entry.mode != "100644":
            raise ReconstructionError(f"unexpected imported CIX AML module mode: {path}")
        updates[overlay] = mirror_symlink_target(overlay).encode()
    # The delegated firmware Makefile owns FV freshness even when invoked
    # directly in a rendered tree. The build-branch caller cannot observe those
    # local edits. Carry this custom-only dependency with its source overlay.
    makefile = "src/Makefile"
    if makefile not in tree.entries:
        raise ReconstructionError("missing delegated AML build dependency owner")
    data = tree.blob(makefile)
    dependency = (b"\t$(wildcard $(CUSTOM_OVERLAY_ROOT)/" + AML_MODULE.encode() +
                  b"* $(CUSTOM_OVERLAY_ROOT)/" + AML_MODULE.encode() + b"*/*) \\\n")
    if dependency not in data:
        anchor = b"CUSTOM_EDK2_OVERLAY_SOURCES := \\\n"
        if data.count(anchor) != 1:
            raise ReconstructionError("unexpected custom AML build dependency anchor")
        updates[makefile] = data.replace(anchor, anchor + dependency, 1)
    return updates


def compatibility_update_mode(path: str, data: bytes) -> str:
    """Give generated module mirrors symlink mode; source edits remain regular."""
    if path.startswith(AML_OVERLAY) and data == mirror_symlink_target(path).encode():
        return "120000"
    return "100644"


def exception_library_updates(tree: GitTree) -> dict[str, bytes]:
    """Change only an obsolete Sky1 binding when the replacement supports AArch64."""
    if "src/edk2/" + LEGACY_EXCEPTION in tree.entries:
        return {}
    updates = {}
    for path in DESCRIPTORS:
        if path not in tree.entries:
            continue
        resolved = tree.resolve(path)
        data = tree.blob(resolved)
        if LEGACY_EXCEPTION.encode() not in data:
            continue
        replacement = "src/edk2/" + DXE_EXCEPTION
        if replacement not in tree.entries:
            raise ReconstructionError("missing replacement AArch64 exception library")
        inf = tree.blob(tree.resolve(replacement))
        if not re.search(rb"(?m)^\[Sources\.AARCH64\]\r?$", inf):
            raise ReconstructionError("replacement exception library lacks AArch64 sources")
        pattern = (rb"(?m)^(\s*CpuExceptionHandlerLib\s*\|\s*)" +
                   re.escape(LEGACY_EXCEPTION.encode()) + rb"([ \t]*(?:#[^\r\n]*)?\r?$)")
        rewritten, count = re.subn(pattern, lambda m: m[1] + DXE_EXCEPTION.encode() + m[2], data)
        if count != 1 or LEGACY_EXCEPTION.encode() in rewritten:
            raise ReconstructionError(f"unexpected obsolete exception-library binding: {path}")
        if tree.entries[resolved].mode != "100644":
            raise ReconstructionError(f"unexpected Sky1 descriptor mode: {resolved}")
        updates[resolved] = rewritten
    return updates


def adapt_source_libraries(repo: Path, candidate: str) -> str:
    """Make a child commit; do not move refs or rewrite imported inputs."""
    tree = GitTree(repo, candidate)
    updates = source_library_updates(tree)
    if not updates:
        return candidate
    with temp_dir(repo, "radxa-library-index-") as scratch:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(scratch) / "index"))

        def indexed(*args: str) -> str:
            return subprocess.run(["git", "-C", str(repo), *args], env=env,
                                  check=True, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True).stdout.strip()
        indexed("read-tree", candidate)
        for path, data in updates.items():
            oid = subprocess.run(["git", "-C", str(repo), "hash-object", "-w", "--stdin"],
                                 input=data, check=True, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE).stdout.decode().strip()
            indexed("update-index", "--add", "--cacheinfo", compatibility_update_mode(path, data), oid, path)
        result = indexed("write-tree")
    message = ("source: adapt Sky1 interfaces to selected EDK2\n\n"
               f"Source-Compatibility-Input: {candidate}\n"
               "Source-Compatibility-Upstream: d2fc49ac55eae1366d4fdf262129313d08b23d19\n"
               "Source-Compatibility-Upstream: 00a865d595591fef44dc9f23353f5c3d50152e04\n"
               "Source-Compatibility-Upstream: 5c6e9d475fe2943f2844ca879785c198588a30d0\n"
               "Source-Compatibility-Upstream: 3b61f4d266ba8e8a4c320a06295637b01f098c2b\n"
               "Source-Compatibility-Upstream: 95a7323e86126560e0a37b3801bc6c12a8428cda\n")
    if any(path.startswith(AML_OVERLAY) for path in updates):
        source = "src/" + AML_MODULE + "CodeGen/AmlCodeGen.c"
        message += (f"Source-Compatibility-AML-Header: {AML_PUBLIC_HEADER}={tree.entries[tree.resolve(AML_PUBLIC_HEADER)].oid}\n"
                    f"Source-Compatibility-AML-Vendor: {source}={tree.entries[tree.resolve(source)].oid}\n"
                    "Source-Compatibility-AML-Precedent: 82651824536e2c742b326c140417441735805706\n"
                    "Source-Compatibility-AML-Precedent: 9916f3e32fadf9119836fcf3c308e2322e6f16be\n")
    return git(repo, "commit-tree", result, "-p", candidate, "-m", message).stdout.strip()


def validate_source_libraries(repo: Path, candidate: str) -> None:
    """Existing checkpoints need explicit correction rather than silent ref movement."""
    if source_library_updates(GitTree(repo, candidate)):
        raise ReconstructionError("obsolete Sky1 interfaces; prepare a new candidate")
