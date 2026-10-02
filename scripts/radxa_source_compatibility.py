#!/usr/bin/env python3
"""Narrow Sky1 source adaptations for selected EDK2 library dependencies."""

from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess

from reconstruction_common import ReconstructionError, git, temp_dir
from validate_radxa13_source import GitTree

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
            indexed("update-index", "--cacheinfo", "100644", oid, path)
        result = indexed("write-tree")
    message = ("source: adapt Sky1 libraries to selected EDK2\n\n"
               f"Source-Compatibility-Input: {candidate}\n"
               "Source-Compatibility-Upstream: d2fc49ac55eae1366d4fdf262129313d08b23d19\n"
               "Source-Compatibility-Upstream: 00a865d595591fef44dc9f23353f5c3d50152e04\n"
               "Source-Compatibility-Upstream: 5c6e9d475fe2943f2844ca879785c198588a30d0\n")
    return git(repo, "commit-tree", result, "-p", candidate, "-m", message).stdout.strip()


def validate_source_libraries(repo: Path, candidate: str) -> None:
    """Existing checkpoints need explicit correction rather than silent ref movement."""
    if source_library_updates(GitTree(repo, candidate)):
        raise ReconstructionError("obsolete Sky1 library bindings; prepare a new candidate")
