#!/usr/bin/env python3
"""Detect vendor CI workflow changes that need review on the build branch."""

from __future__ import annotations

import argparse
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
  V=0|1  Print every checked vendor workflow snapshot.

This check compares .github/workflows content in recorded vendor source refs
against the audited baseline in config/vendor-workflow-baseline.json. It does
not run or port vendor workflows. A mismatch means a vendor source update has
changed workflow content and the build branch CI should be reviewed before the
baseline is refreshed.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
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


def check_entry(repo: Path, entry: dict[str, Any], verbose: bool) -> list[str]:
    vendor = str(entry.get("vendor", ""))
    workflow_dir = str(entry.get("workflow_dir", ".github/workflows"))
    expected_tree = entry.get("workflow_tree_id")
    expected_paths = entry.get("paths", {})
    if not isinstance(expected_paths, dict):
        raise ReconstructionError("config/vendor-workflow-baseline.json: paths must be an object")
    refs = vendor_refs(repo, vendor)
    if not refs:
        return [f"{vendor}: no vendor refs found for workflow drift check"]

    problems: list[str] = []
    for ref in refs:
        tree, blobs = workflow_snapshot(repo, ref, workflow_dir)
        path_diffs = diff_paths(expected_paths, blobs)
        if tree != expected_tree or path_diffs:
            detail = [f"{ref}: vendor workflow content differs from audited baseline"]
            detail.append(f"workflow tree: {tree or '<missing>'} != {expected_tree}")
            detail.extend(f"  {item}" for item in path_diffs)
            problems.append("\n".join(detail))
        elif verbose:
            print(f"vendor workflow baseline ok: {ref} ({tree})")
    return problems


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    data = load_json(repo, "config/vendor-workflow-baseline.json")
    entries = data.get("checks", [])
    if not isinstance(entries, list) or not entries:
        raise ReconstructionError("config/vendor-workflow-baseline.json has no checks")
    problems: list[str] = []
    for entry in entries:
        if not isinstance(entry, dict):
            raise ReconstructionError("config/vendor-workflow-baseline.json checks must be objects")
        problems.extend(check_entry(repo, entry, verbose))
    if problems:
        details = "\n\n".join(problems)
        raise ReconstructionError(
            "vendor workflow drift detected; review and port relevant CI changes before updating the baseline:\n"
            f"{details}"
        )
    print(f"validated vendor workflow baseline in {format_duration(time.monotonic() - started)}")


if __name__ == "__main__":
    main_wrapper(main)
