#!/usr/bin/env python3
"""Narrow source adaptations for EDK2's moved AArch64 exception library."""

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


def adapt_exception_library(repo: Path, candidate: str) -> str:
    """Make a child commit; do not move refs or rewrite imported inputs."""
    tree = GitTree(repo, candidate)
    updates = exception_library_updates(tree)
    if not updates:
        return candidate
    with temp_dir(repo, "radxa-exception-index-") as scratch:
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
    message = ("source: adapt Sky1 exception library to selected EDK2\n\n"
               f"Source-Compatibility-Input: {candidate}\n"
               "Source-Compatibility-Upstream: d2fc49ac55eae1366d4fdf262129313d08b23d19\n")
    return git(repo, "commit-tree", result, "-p", candidate, "-m", message).stdout.strip()


def validate_exception_library(repo: Path, candidate: str) -> None:
    """Existing checkpoints need explicit correction rather than silent ref movement."""
    if exception_library_updates(GitTree(repo, candidate)):
        raise ReconstructionError("obsolete Sky1 exception-library binding; prepare a new candidate")
