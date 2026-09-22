#!/usr/bin/env python3
"""Reject metadata-only Radxa relabelling and mismatched vendor boot payloads."""

from __future__ import annotations

import argparse
import json
import posixpath
from pathlib import Path

from reconstruction_common import (
    ReconstructionError, main_wrapper, release_entries, release_entry,
    release_metadata_ref, show_file, source_target_name, load_ref_records,
)
from source_lifecycle import tree_entries
from debug_build_policy import fdf_paths

ROOT = Path(__file__).resolve().parents[1]
FIRMWARES = "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/Firmwares/"
# These are vendor runtime payloads, not regenerated memory/PM configuration or
# the custom BL33. Final packaging independently verifies the resulting chain.
LOCKED_PAYLOADS = ("bootloader1.img", "bootloader2.img", "sfh_fw.bin",
                   "ec_fw.bin", "se_config.bin", "trustzone_config.bin")


def source_fdf(repo: Path, ref: str, board: str, experimental: bool = False) -> str:
    """Read the effective custom board layout, including tracked mirror links."""
    for path in fdf_paths(board, experimental):
        if path in tree_entries(repo, ref, (path,)):
            break
    else:
        raise ReconstructionError(f"missing board layout: {ref}:{board}")
    for _ in range(16):
        entry = tree_entries(repo, ref, (path,)).get(path)
        if entry is None:
            raise ReconstructionError(f"missing board layout: {ref}:{path}")
        data = show_file(repo, ref, path).decode()
        if entry.mode != "120000":
            return data
        path = posixpath.normpath(posixpath.join(posixpath.dirname(path), data))
        if path.startswith(("/", "../")):
            break
    raise ReconstructionError(f"invalid board layout mirror: {ref}:{path}")


def baseline_release(repo: Path, ref: str) -> str:
    # Explicit release checkpoints bind a reviewed vendor port. Their VERSION
    # file can precede the final release_metadata transformation. Generic older
    # compatibility refs have no such binding: their actual baseline applies.
    records = load_ref_records(repo)
    record = next((r for r in records if r["ref"] == ref), {})
    if record.get("radxa_release"):
        return record["radxa_release"]
    if record.get("type") == "unofficial-line-tip":
        matches = {r["radxa_release"] for r in records
                   if r.get("radxa_release") and r.get("tree_id") == record.get("tree_id")}
        if len(matches) == 1:
            return matches.pop()
    return show_file(repo, ref, "VERSION").decode().strip()


def input_problems(repo: Path, entry: dict) -> list[str]:
    if not entry.get("unofficial_delta"):
        return []
    ref = entry["source_ref"]
    radxa = entry["radxa_release"]
    problems = []
    baseline = baseline_release(repo, ref)
    if baseline != radxa:
        problems.append(f"source checkpoint {ref} is based on Radxa {baseline}, not requested {radxa}; "
                        "a release_metadata step cannot supply missing vendor changes")
    vendor = release_metadata_ref(repo, radxa, entry["edk2_release"])
    if vendor is None:
        return problems + [f"no vendor reference for Radxa {radxa}"]
    source_files = tree_entries(repo, ref, (FIRMWARES,))
    vendor_files = tree_entries(repo, vendor, (FIRMWARES,))
    for name in LOCKED_PAYLOADS:
        path = FIRMWARES + name
        actual, expected = source_files.get(path), vendor_files.get(path)
        # Some older releases do not ship trustzone_config.bin at all.
        if name == "trustzone_config.bin" and actual is None and expected is None:
            continue
        if actual is None or expected is None or actual.object_id != expected.object_id:
            problems.append(f"{name} does not match requested Radxa {radxa} vendor input")
    return problems


def validate_inputs(repo: Path, entry: dict) -> None:
    problems = input_problems(repo, entry)
    if problems:
        raise ReconstructionError("release input provenance failed:\n  - " + "\n  - ".join(problems))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release")
    parser.add_argument("--inventory", action="store_true")
    args = parser.parse_args()
    if args.inventory:
        result = []
        for branch, entry in sorted(release_entries(ROOT).items()):
            target = source_target_name(branch)
            if not target.endswith("/unofficial") or "/cix-" in target:
                continue
            result.append({"release": target, "problems": input_problems(ROOT, entry)})
        print(json.dumps(result, indent=2))
        return
    _, entry = release_entry(ROOT, args.release)
    validate_inputs(ROOT, entry)


if __name__ == "__main__":
    main_wrapper(main)
