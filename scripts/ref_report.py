#!/usr/bin/env python3
"""Report required source refs and generated cache refs for clean-up review."""

from __future__ import annotations

import argparse
from pathlib import Path

from reconstruction_common import (
    CACHE_BASE_EDK2_PREFIX,
    CACHE_RELEASE_PREFIX,
    ReconstructionError,
    base_tree_records,
    git,
    local_compatibility_branch_for_tag,
    matrix_release_values,
    ref_exists,
    release_entry_required_refs,
    source_target_ref_records,
    release_entries,
    repo_root,
    resolve_ref,
    rev_parse,
    truthy,
    worktree_paths,
    main_wrapper,
)

AGENT_SCRATCH_PREFIX = "co" "dex/"


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cleanup", action="store_true", help="include clean-up-oriented wording")
    p.add_argument("--v", default="0", help="verbosity flag propagated from make")
    return p


def refs_under(repo: Path, namespace: str) -> set[str]:
    result = git(repo, "for-each-ref", "--format=%(refname:lstrip=2)", namespace, check=False)
    if result.returncode != 0:
        return set()
    return {line for line in result.stdout.splitlines() if line}


def branch_tag_collisions(repo: Path) -> list[str]:
    branches = refs_under(repo, "refs/heads")
    tags = refs_under(repo, "refs/tags")
    return sorted(branches & tags)


def entry_required_refs(entry: dict) -> set[str]:
    return release_entry_required_refs(entry)


def required_source_refs(repo: Path) -> set[str]:
    refs: set[str] = set()
    for entry in release_entries(repo).values():
        refs.update(entry_required_refs(entry))
        edk2_ref = entry.get("edk2_release")
        if entry.get("unofficial_delta") and edk2_ref:
            refs.add(f"source/unofficial/{edk2_ref}")
    for record in base_tree_records(repo).values():
        for component in record.get("components", []):
            if component.get("ref"):
                refs.add(component["ref"])
    return refs


def local_tag_problems(repo: Path) -> list[str]:
    problems: list[str] = []
    for tag in sorted(refs_under(repo, "refs/tags/source/unofficial/edk2")):
        try:
            branch = local_compatibility_branch_for_tag(tag)
        except ReconstructionError as exc:
            problems.append(f"{tag}: {exc}")
            continue
        if not ref_exists(repo, branch):
            problems.append(f"{tag}: matching branch is missing: {branch}")
            continue
        result = git(repo, "merge-base", "--is-ancestor", rev_parse(repo, tag), resolve_ref(repo, branch), check=False)
        if result.returncode != 0:
            problems.append(f"{tag}: tag commit is not reachable from {branch}")
    return problems


def scratch_worktrees(repo: Path) -> list[str]:
    entries: list[str] = []
    for path, item in sorted(worktree_paths(repo).items()):
        branch = item.get("branch", "")
        if branch.startswith("refs/heads/"):
            branch = branch.removeprefix("refs/heads/")
        if (
            branch.startswith(AGENT_SCRATCH_PREFIX)
            or branch.startswith(CACHE_RELEASE_PREFIX)
            or branch.startswith(CACHE_BASE_EDK2_PREFIX)
        ):
            entries.append(f"{path} ({branch})")
    return entries


def print_section(title: str, lines: list[str], empty: str = "none") -> None:
    print(f"\n{title}")
    if lines:
        for line in lines:
            print(f"  - {line}")
    else:
        print(f"  {empty}")


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    releases = matrix_release_values(repo)
    source_targets = set(release_entries(repo))
    required = required_source_refs(repo)
    base_cache = set(base_tree_records(repo))
    source_target_cache = set(source_target_ref_records(repo))
    actual_release = refs_under(repo, "refs/heads/source/cache/release")
    actual_base_cache = refs_under(repo, "refs/heads/source/cache/base/edk2")
    legacy_cache = refs_under(repo, "refs/heads/source/release") | refs_under(repo, "refs/heads/source/base/rendered")
    stale_release = sorted(actual_release - source_targets)
    present_source_target_cache = sorted(actual_release & source_target_cache)
    present_base_cache = sorted(actual_base_cache & base_cache)
    unexpected_base_cache = sorted(actual_base_cache - base_cache)

    heading = "Clean-up Report" if args.cleanup else "Ref Report"
    print(heading)
    print(f"EDK2 releases: {len(releases)}")
    print(f"Firmware source targets derivable from source refs: {len(source_targets)}")
    print(f"Required non-cache source refs: {len(required)}")
    print(f"Generated base cache refs derivable from EDK2 refs: {len(base_cache)}")
    print(f"Generated source-target cache refs described or derivable: {len(source_target_cache)}")

    if verbose:
        print_section("Required non-cache source refs", sorted(required))

    print_section("Present generated source/cache/release refs", present_source_target_cache)
    print_section("Stale source/cache/release refs", stale_release)
    print_section("Present generated source/cache/base/edk2 refs", present_base_cache)
    print_section("Unexpected source/cache/base/edk2 refs", unexpected_base_cache)
    print_section("Unsupported legacy generated cache refs", sorted(legacy_cache))
    print_section("Branch/tag collisions", branch_tag_collisions(repo))
    print_section("Unofficial checkpoint tag issues", local_tag_problems(repo))
    print_section("Scratch/diagnostic worktrees", scratch_worktrees(repo))

    if args.cleanup:
        print("\nClean-up guidance")
        print("  - source/cache/release/** and source/cache/base/edk2/** refs listed as generated caches may be deleted after verify-build-matrix passes.")
        print("  - Do not delete required non-cache source refs, source/unofficial checkpoint branches, or source/unofficial/edk2/stable-* tags.")
        print("  - Treat scratch worktrees as informational only; confirm ownership before removing them.")


if __name__ == "__main__":
    main_wrapper(main)
