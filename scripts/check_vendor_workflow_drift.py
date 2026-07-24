#!/usr/bin/env python3
"""Detect vendor CI workflow changes that need review on the build branch."""

from __future__ import annotations

import argparse
from copy import deepcopy
import json
import os
import time
from pathlib import Path
from typing import Any

from reconstruction_common import (
    ReconstructionError,
    for_each_ref,
    format_duration,
    git,
    load_json,
    main_wrapper,
    repo_root,
    resolve_ref,
    truthy,
    version_key,
)


HELP = """check-vendor-workflow-drift

No variables are required.

Optional variables:
  REVIEWED=0|1  With --refresh, confirm vendor workflow changes were reviewed
                and relevant changes were ported to the build branch.
  WRITE=0|1     With --refresh, update the audited baseline. Default: 0.
  V=0|1  Print every checked vendor workflow snapshot.

This check compares .github/workflows content in recorded vendor source refs
against the audited baselines in config/vendor-workflow-baseline.json. It does
not run or port vendor workflows. A mismatch means a vendor source update has
changed workflow content and the build branch CI should be reviewed before the
baseline is refreshed.

Use --refresh to preview the missing refs and their workflow snapshot groups.
After reviewing the changes and porting anything relevant, rerun with
REVIEWED=1 WRITE=1. Existing audited entries are never changed silently:
missing refs with identical workflow content join the matching entry, while a
new workflow snapshot gets a new baseline entry.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--refresh", default=os.environ.get("REFRESH", "0"))
    p.add_argument("--reviewed", default=os.environ.get("REVIEWED", "0"))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"), help="verbosity flag propagated from make")
    return p


def workflow_snapshot(repo: Path, ref: str, workflow_dir: str) -> tuple[str | None, dict[str, str]]:
    resolved = resolve_ref(repo, ref, check=False)
    if not resolved:
        raise ReconstructionError(f"vendor ref is unavailable locally: {ref}")
    tree_result = git(repo, "rev-parse", "--verify", "--quiet", f"{resolved}:{workflow_dir}", check=False)
    tree = tree_result.stdout.strip() if tree_result.returncode == 0 else None
    result = git(repo, "ls-tree", "-r", resolved, "--", workflow_dir, check=False)
    if result.returncode != 0:
        return tree, {}
    blobs: dict[str, str] = {}
    for line in result.stdout.splitlines():
        if "\t" not in line:
            continue
        meta, path = line.split("\t", 1)
        parts = meta.split()
        if len(parts) >= 3 and parts[1] == "blob":
            blobs[path] = parts[2]
    return tree, blobs


def vendor_refs(repo: Path, vendor: str) -> list[str]:
    if vendor != "radxa":
        raise ReconstructionError(f"unsupported vendor workflow drift check: {vendor}")
    return sorted(
        [
            ref
            for ref in for_each_ref(repo, "source/vendor/radxa")
            if ref.startswith("source/vendor/radxa/")
        ],
        key=version_key,
    )


def check_refs(repo: Path, entry: dict[str, Any], vendor: str) -> list[str]:
    configured_refs = entry.get("refs")
    if configured_refs is None:
        return vendor_refs(repo, vendor)
    if not isinstance(configured_refs, list) or not all(isinstance(ref, str) for ref in configured_refs):
        raise ReconstructionError("config/vendor-workflow-baseline.json: refs must be a list of strings")
    refs = sorted(set(configured_refs), key=version_key)
    missing = [ref for ref in refs if not resolve_ref(repo, ref, check=False)]
    if missing:
        raise ReconstructionError(
            "config/vendor-workflow-baseline.json references unavailable vendor refs: "
            + ", ".join(missing)
        )
    return refs


def diff_paths(expected: dict[str, str], actual: dict[str, str]) -> list[str]:
    problems: list[str] = []
    for path in sorted(set(expected) - set(actual)):
        problems.append(f"missing {path}")
    for path in sorted(set(actual) - set(expected)):
        problems.append(f"new {path}")
    for path in sorted(set(expected) & set(actual)):
        if expected[path] != actual[path]:
            problems.append(f"changed {path}: {expected[path]} -> {actual[path]}")
    return problems


def check_entry(repo: Path, entry: dict[str, Any], verbose: bool) -> tuple[str, list[str], list[str]]:
    vendor = str(entry.get("vendor", ""))
    baseline_ref = str(entry.get("baseline_ref", ""))
    workflow_dir = str(entry.get("workflow_dir", ".github/workflows"))
    expected_tree = entry.get("workflow_tree_id")
    expected_paths = entry.get("paths", {})
    if not isinstance(expected_paths, dict):
        raise ReconstructionError("config/vendor-workflow-baseline.json: paths must be an object")
    refs = check_refs(repo, entry, vendor)
    if not refs:
        return vendor, refs, [f"{vendor}: no vendor refs found for workflow drift check"]

    problems: list[str] = []
    for ref in refs:
        tree, blobs = workflow_snapshot(repo, ref, workflow_dir)
        path_diffs = diff_paths(expected_paths, blobs)
        if tree != expected_tree or path_diffs:
            detail = [f"{ref}: vendor workflow content differs from audited baseline"]
            if baseline_ref:
                detail.append(f"baseline ref: {baseline_ref}")
            detail.append(f"workflow tree: {tree or '<missing>'} != {expected_tree}")
            detail.extend(f"  {item}" for item in path_diffs)
            problems.append("\n".join(detail))
        elif verbose:
            print(f"vendor workflow baseline ok: {ref} ({tree})")
    return vendor, refs, problems


def refresh_missing_refs(repo: Path, data: dict[str, Any]) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    updated = deepcopy(data)
    entries = updated.get("checks", [])
    if not isinstance(entries, list) or not entries:
        raise ReconstructionError("config/vendor-workflow-baseline.json has no checks")

    additions: list[dict[str, Any]] = []
    vendors = sorted({str(entry.get("vendor", "")) for entry in entries if isinstance(entry, dict)})
    for vendor in vendors:
        vendor_entries = [
            entry
            for entry in entries
            if isinstance(entry, dict) and str(entry.get("vendor", "")) == vendor
        ]
        covered = {
            ref
            for entry in vendor_entries
            for ref in entry.get("refs", [])
            if isinstance(ref, str)
        }
        for ref in sorted(set(vendor_refs(repo, vendor)) - covered, key=version_key):
            workflow_dir = ".github/workflows"
            tree, paths = workflow_snapshot(repo, ref, workflow_dir)
            matching_entry = next(
                (
                    entry
                    for entry in vendor_entries
                    if entry.get("workflow_dir", ".github/workflows") == workflow_dir
                    and entry.get("workflow_tree_id") == tree
                    and entry.get("paths", {}) == paths
                ),
                None,
            )
            new_snapshot = matching_entry is None
            if matching_entry is None:
                matching_entry = {
                    "baseline_ref": ref,
                    "paths": paths,
                    "refs": [],
                    "vendor": vendor,
                    "workflow_dir": workflow_dir,
                    "workflow_tree_id": tree,
                }
                entries.append(matching_entry)
                vendor_entries.append(matching_entry)
            refs = matching_entry.setdefault("refs", [])
            refs.append(ref)
            refs[:] = sorted(set(refs), key=version_key)
            additions.append(
                {
                    "baseline_ref": matching_entry["baseline_ref"],
                    "new_snapshot": new_snapshot,
                    "ref": ref,
                    "tree": tree,
                }
            )

    entries.sort(
        key=lambda entry: (
            str(entry.get("vendor", "")),
            version_key(str(entry.get("baseline_ref", ""))),
        )
    )
    return updated, additions


def print_refresh_preview(additions: list[dict[str, Any]]) -> None:
    if not additions:
        print("vendor workflow baseline already covers every recorded ref")
        return
    print("vendor workflow baseline refresh preview:")
    for addition in additions:
        relation = "new workflow snapshot" if addition["new_snapshot"] else f"matches {addition['baseline_ref']}"
        print(f"  - {addition['ref']}: {relation} ({addition['tree'] or '<missing>'})")


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    data = load_json(repo, "config/vendor-workflow-baseline.json")
    if truthy(args.refresh):
        updated, additions = refresh_missing_refs(repo, data)
        print_refresh_preview(additions)
        if not additions:
            return
        if truthy(args.write) and not truthy(args.reviewed):
            raise ReconstructionError(
                "refusing to refresh vendor workflow baselines without REVIEWED=1; "
                "review the vendor workflow diff and port relevant build-branch CI changes first"
            )
        if not truthy(args.write):
            print("dry run; after review, rerun with REVIEWED=1 WRITE=1")
            return
        path = repo / "config" / "vendor-workflow-baseline.json"
        path.write_text(json.dumps(updated, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        data = updated
        print(f"updated vendor workflow baseline for {len(additions)} ref(s)")

    entries = data.get("checks", [])
    if not isinstance(entries, list) or not entries:
        raise ReconstructionError("config/vendor-workflow-baseline.json has no checks")
    problems: list[str] = []
    covered_refs: dict[str, set[str]] = {}
    for entry in entries:
        if not isinstance(entry, dict):
            raise ReconstructionError("config/vendor-workflow-baseline.json checks must be objects")
        vendor, refs, entry_problems = check_entry(repo, entry, verbose)
        covered_refs.setdefault(vendor, set()).update(refs)
        problems.extend(entry_problems)
    for vendor, refs in sorted(covered_refs.items()):
        missing = sorted(set(vendor_refs(repo, vendor)) - refs, key=version_key)
        if missing:
            problems.append(
                f"{vendor}: vendor workflow baseline does not cover recorded refs: "
                + ", ".join(missing)
            )
    if problems:
        details = "\n\n".join(problems)
        raise ReconstructionError(
            "vendor workflow drift detected; review and port relevant CI changes before updating the baseline:\n"
            f"{details}"
        )
    print(f"validated vendor workflow baseline in {format_duration(time.monotonic() - started)}")


if __name__ == "__main__":
    main_wrapper(main)
