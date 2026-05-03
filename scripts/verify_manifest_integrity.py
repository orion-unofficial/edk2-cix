#!/usr/bin/env python3
"""Validate tree-ID manifests after defaults expansion."""

from __future__ import annotations

import argparse
from pathlib import Path

from reconstruction_common import (
    BASE_TREE_MANIFEST,
    VARIANT_TREE_MANIFEST,
    ReconstructionError,
    base_tree_records,
    main_wrapper,
    ref_exists,
    ref_manifest_records,
    rendered_ref_records,
    release_branch_parts,
    release_entries,
    repo_root,
    tree_id,
    truthy,
)


HELP = """verify-manifest-integrity

No variables are required.

Optional variables:
  V=0|1  Print every validated manifest record.

Checks:
  - defaults-expanded variant records retain the expected stage-specific type
  - defaults-expanded base records retain component skeleton metadata
  - variant tree IDs match generated release entries
  - persisted generated cache refs, if present, match recorded tree IDs
"""


BASE_COMPONENT_TEMPLATES = {
    "src/edk2": "source/base/edk2/{edk2_ref}",
    "src/edk2-platforms": "source/base/edk2-platforms/{edk2_ref}",
    "src/edk2-non-osi": "source/base/edk2-non-osi/{edk2_ref}",
}


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--v", default="0", help="verbosity flag propagated from make")
    return p


def expected_variant_type(ref: str) -> str:
    parts = release_branch_parts(ref)
    if parts["stage"] == "upstream":
        return "rendered-upstream-radxa-release"
    if parts["stage"] == "vendor":
        return "rendered-vendor-release"
    return "rendered-release"


def validate_variant_manifest(repo: Path, verbose: bool) -> list[str]:
    problems: list[str] = []
    records = rendered_ref_records(repo)
    generated = release_entries(repo)
    raw_count = len(ref_manifest_records(repo, VARIANT_TREE_MANIFEST))
    if raw_count != len(records):
        problems.append(f"{VARIANT_TREE_MANIFEST}: duplicate or missing ref records after expansion")
    for ref, record in sorted(records.items()):
        if ref not in generated:
            problems.append(f"{VARIANT_TREE_MANIFEST}: record is not derivable from source refs: {ref}")
            continue
        expected_type = expected_variant_type(ref)
        if record.get("type") != expected_type:
            problems.append(f"{ref}: type is {record.get('type')!r}, expected {expected_type!r}")
        if record.get("immutable") is not True:
            problems.append(f"{ref}: immutable must expand to true")
        if not record.get("tree_id"):
            problems.append(f"{ref}: missing tree_id")
        elif generated[ref].get("tree_id") != record["tree_id"]:
            problems.append(f"{ref}: generated entry tree_id differs from manifest")
        if ref_exists(repo, ref) and tree_id(repo, ref) != record.get("tree_id"):
            problems.append(f"{ref}: persisted cache ref tree differs from manifest")
        if verbose:
            print(f"variant manifest ok: {ref} ({expected_type})")
    return problems


def validate_base_manifest(repo: Path, verbose: bool) -> list[str]:
    problems: list[str] = []
    records = base_tree_records(repo)
    raw_count = len(ref_manifest_records(repo, BASE_TREE_MANIFEST))
    if raw_count != len(records):
        problems.append(f"{BASE_TREE_MANIFEST}: duplicate or missing ref records after expansion")
    for ref, record in sorted(records.items()):
        edk2_ref = ref.rsplit("/", 1)[-1]
        if record.get("type") != "component-skeleton":
            problems.append(f"{ref}: type is {record.get('type')!r}, expected 'component-skeleton'")
        if record.get("component") != "rendered-edk2-base":
            problems.append(f"{ref}: component is {record.get('component')!r}, expected 'rendered-edk2-base'")
        if record.get("immutable") is not True:
            problems.append(f"{ref}: immutable must expand to true")
        actual_components = {item.get("path"): item.get("ref") for item in record.get("components", [])}
        expected_components = {
            path: ref_template.format(edk2_ref=edk2_ref)
            for path, ref_template in BASE_COMPONENT_TEMPLATES.items()
        }
        if actual_components != expected_components:
            problems.append(f"{ref}: component plan differs from expected EDK2 skeleton plan")
        for component_ref in expected_components.values():
            if not ref_exists(repo, component_ref):
                problems.append(f"{ref}: missing component ref {component_ref}")
        if not record.get("tree_id"):
            problems.append(f"{ref}: missing tree_id")
        if ref_exists(repo, ref) and tree_id(repo, ref) != record.get("tree_id"):
            problems.append(f"{ref}: persisted cache ref tree differs from manifest")
        if verbose:
            print(f"base manifest ok: {ref}")
    return problems


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    problems = validate_variant_manifest(repo, verbose)
    problems.extend(validate_base_manifest(repo, verbose))
    if problems:
        details = "\n".join(f"  - {problem}" for problem in problems)
        raise ReconstructionError(f"manifest integrity verification failed:\n{details}")
    print(f"validated manifest integrity: {len(rendered_ref_records(repo))} variant tree records, {len(base_tree_records(repo))} base tree records")


if __name__ == "__main__":
    main_wrapper(main)
