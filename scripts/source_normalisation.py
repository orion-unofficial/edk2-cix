#!/usr/bin/env python3
"""Canonicalise editable source trees while preserving raw vendor snapshots."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path
from typing import Iterable, NamedTuple


NORMAL_FILE_MODES = {"100644", "100755"}
PATH_ARGUMENT_BUDGET = 32 * 1024


class NormalisationResult(NamedTuple):
    line_endings: int
    trailing_whitespace: int
    file_modes: int

    @property
    def changed(self) -> int:
        return self.line_endings + self.trailing_whitespace + self.file_modes


def path_batches(paths: Iterable[str]) -> list[list[str]]:
    batches: list[list[str]] = []
    batch: list[str] = []
    batch_size = 0
    for path in paths:
        if not path:
            continue
        path_size = len(path.encode("utf-8", errors="surrogateescape")) + 1
        if batch and batch_size + path_size > PATH_ARGUMENT_BUDGET:
            batches.append(batch)
            batch = []
            batch_size = 0
        batch.append(path)
        batch_size += path_size
    if batch:
        batches.append(batch)
    return batches


def tracked_entries(worktree: Path, paths: Iterable[str] = ()) -> list[tuple[str, str]]:
    selected = [path for path in paths if path]
    batches = path_batches(selected) if selected else [[]]
    output = bytearray()
    for batch in batches:
        command = ["git", "-C", str(worktree), "ls-files", "-s", "-z"]
        if batch:
            command.extend(["--", *batch])
        result = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if result.returncode != 0:
            detail = result.stderr.decode("utf-8", errors="replace").strip()
            raise RuntimeError(f"cannot enumerate source files for normalisation: {detail}")
        output.extend(result.stdout)

    entries: list[tuple[str, str]] = []
    for record in output.split(b"\0"):
        if not record:
            continue
        metadata, raw_path = record.split(b"\t", 1)
        mode = metadata.split(b" ", 1)[0].decode("ascii")
        path = raw_path.decode("utf-8", errors="surrogateescape")
        entries.append((mode, path))
    return entries


def modified_tracked_paths(worktree: Path) -> list[str]:
    result = subprocess.run(
        ["git", "-C", str(worktree), "diff", "--name-only", "-z"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"cannot identify checkout-dirty source files: {detail}")
    return [
        raw_path.decode("utf-8", errors="surrogateescape")
        for raw_path in result.stdout.split(b"\0")
        if raw_path
    ]


def attribute_inconsistent_paths(worktree: Path) -> list[str]:
    result = subprocess.run(
        ["git", "-C", str(worktree), "ls-files", "--eol", "-z"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"cannot inspect tracked source line endings: {detail}")

    paths: list[str] = []
    for record in result.stdout.split(b"\0"):
        if not record or b"\t" not in record:
            continue
        metadata, raw_path = record.split(b"\t", 1)
        fields = metadata.decode("ascii", errors="replace").split()
        if fields and fields[0] in {"i/crlf", "i/mixed"} and "eol=lf" in fields:
            paths.append(raw_path.decode("utf-8", errors="surrogateescape"))
    return paths


def normalise_worktree(
    worktree: Path,
    *,
    paths: Iterable[str] = (),
) -> NormalisationResult:
    """Canonicalise changed text and strip executable bits from non-scripts."""

    line_endings = 0
    trailing_whitespace = 0
    file_modes = 0
    changed_paths: list[str] = []

    for mode, relative_path in tracked_entries(worktree, paths):
        if mode not in NORMAL_FILE_MODES:
            continue
        path = worktree / relative_path
        data = path.read_bytes()
        if b"\0" in data:
            continue

        normalised = data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
        if normalised != data:
            line_endings += 1
        without_trailing = re.sub(rb"[ \t]+(?=\n|$)", b"", normalised)
        if without_trailing != normalised:
            trailing_whitespace += 1
        normalised = without_trailing
        path_changed = False
        if normalised != data:
            path.write_bytes(normalised)
            path_changed = True

        if mode == "100755" and not normalised.startswith(b"#!"):
            path.chmod(0o644)
            file_modes += 1
            path_changed = True

        if path_changed:
            changed_paths.append(relative_path)

    if changed_paths:
        for batch in path_batches(changed_paths):
            subprocess.run(
                ["git", "-C", str(worktree), "add", "-f", "--", *batch],
                check=True,
            )
    return NormalisationResult(line_endings, trailing_whitespace, file_modes)
