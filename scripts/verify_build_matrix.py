#!/usr/bin/env python3
"""Verify that the declared supported build matrix has matching refs."""

from __future__ import annotations

import argparse
import os
import time
from pathlib import Path
from typing import Any
from reconstruction_common import (
    CACHE_BASE_EDK2_PREFIX,
    CACHE_RELEASE_PREFIX,
    EDK2_REFS_MANIFEST,
    SOURCE_TARGET_CACHE_MANIFEST,
    ReconstructionError,
    base_tree_records,
    format_duration,
    for_each_ref,
    git,
    load_json,
    edk2_ref_for_release,
    unofficial_release_branch_for_tag,
    unofficial_release_edk2_refs,
    unofficial_release_tag_for_branch,
    main_wrapper,
    matrix_release_branches,
    matrix_release_values,
    ref_exists,
    available_radxa_releases,
    release_entry_required_refs,
    source_target_ref_records,
    release_entries,
    repo_root,
    rev_parse,
    radxa_source_refs,
    resolve_ref,
    tree_id,
    truthy,
)


HELP = """verify-build-matrix

No variables are required.

Optional variables:
  V=0|1  Print every expected firmware source-target branch and required source ref.

Checks:
  - every derived firmware source target has a render plan
  - every firmware source-target branch/ref derived from source refs is renderable
  - required base, vendor, component, and unofficial refs exist
  - retained source/cache/release branches are all derivable from source refs
  - retained source/cache/release branches match config/refs-source-target-cache.json tree IDs
  - Radxa vendor/port source refs and regenerable rendered-base cache plans cover every supported EDK2 release
  - obsolete source/delta/unofficial refs are absent
  - unofficial release tags are reachable from retained source/unofficial branches
  - versioned unofficial aliases have the same tree as their non-alias branch
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--v", default=os.environ.get("V", "0"), help="verbosity flag propagated from make")
    return p


def expected_from_source_refs(repo: Path) -> tuple[set[str], set[str], dict[str, str]]:
    releases = release_entries(repo)
    expected_releases = set(releases)
    expected_required_refs: set[str] = set()
    for entry in releases.values():
        expected_required_refs.update(release_entry_required_refs(entry))
        edk2_ref = entry.get("edk2_release")
        if entry.get("unofficial_delta") and edk2_ref:
            expected_required_refs.add(f"source/unofficial/{edk2_ref}")
    _branches, aliases = matrix_release_branches(repo)
    return expected_releases, expected_required_refs, aliases


def actual_source_release_refs(repo: Path) -> set[str]:
    return set(for_each_ref(repo, "source/cache/release"))


def actual_refs(repo: Path, namespace: str) -> set[str]:
    return set(for_each_ref(repo, namespace))


def local_tag_refs(repo: Path) -> set[str]:
    result = git(repo, "for-each-ref", "--format=%(refname:lstrip=2)", "refs/tags/source/unofficial/edk2", check=False)
    if result.returncode != 0:
        return set()
    return {line for line in result.stdout.splitlines() if line}


def tag_reachable_from_local_branch(repo: Path, tag: str) -> bool:
    commit = rev_parse(repo, tag)
    expected_branch = unofficial_release_branch_for_tag(tag)
    result = git(
        repo,
        "merge-base",
        "--is-ancestor",
        commit,
        resolve_ref(repo, expected_branch),
        check=False,
    )
    return result.returncode == 0


def rendered_base_records(repo: Path) -> dict[str, dict[str, Any]]:
    return base_tree_records(repo)


def require_manifested_release_entries(
    repo: Path,
    expected_releases: set[str],
    verbose: bool,
) -> list[str]:
    problems: list[str] = []
    releases = release_entries(repo)
    configured = set(releases)

    missing_config = sorted(expected_releases - configured)
    extra_config = sorted(configured - expected_releases)
    if missing_config:
        problems.append("derived render plans are missing source-derived source targets:\n" + "\n".join(f"  - {r}" for r in missing_config))
    if extra_config:
        problems.append("derived render plans contain source targets not derivable from source refs:\n" + "\n".join(f"  - {r}" for r in extra_config))

    actual = actual_source_release_refs(repo)
    legacy_release_refs = actual_refs(repo, "source/release")
    legacy_base_refs = actual_refs(repo, "source/base/rendered")
    if legacy_release_refs or legacy_base_refs:
        problems.append(
            "legacy generated cache refs are not supported; generated refs must be under source/cache/**:\n"
            + "\n".join(f"  - {r}" for r in sorted(legacy_release_refs | legacy_base_refs))
        )
    extra_refs = sorted(actual - expected_releases)
    if extra_refs:
        problems.append("source/cache/release refs are not derivable from current source refs:\n" + "\n".join(f"  - {r}" for r in extra_refs))

    source_target_records = source_target_ref_records(repo)
    extra_records = sorted(set(source_target_records) - expected_releases)
    if extra_records:
        problems.append(
            f"config/{SOURCE_TARGET_CACHE_MANIFEST} contains source targets not derivable from current source refs:\n"
            + "\n".join(f"  - {r}" for r in extra_records)
        )

    for ref in sorted(actual & expected_releases):
        if ref not in releases:
            continue
        expected_tree = releases[ref].get("tree_id")
        if expected_tree and tree_id(repo, ref) != expected_tree:
            problems.append(f"{ref}: tree ID differs from config/{SOURCE_TARGET_CACHE_MANIFEST} ({tree_id(repo, ref)} != {expected_tree})")
        if verbose:
            print(f"retained source target ok: {ref}")

    if verbose:
        omitted = sorted(expected_releases - actual)
        for ref in omitted:
            print(f"generated source target ok: {ref}")

    return problems


def require_source_refs(
    repo: Path,
    all_releases: list[str],
    expected_required_refs: set[str],
    verbose: bool,
) -> list[str]:
    problems: list[str] = []
    expected_base_rendered = {f"{CACHE_BASE_EDK2_PREFIX}edk2-stable{release}" for release in all_releases}
    expected_radxa = {
        ref
        for ref in expected_required_refs
        if ref.startswith(("source/vendor/radxa/", "source/port/radxa/"))
    }
    expected_local_tags = {
        unofficial_release_tag_for_branch(ref)
        for ref in expected_required_refs
        if ref.startswith("source/unofficial/")
    }
    expected_local_branches = {
        unofficial_release_branch_for_tag(tag)
        for tag in expected_local_tags
    }
    expected_local_branches.add("source/unofficial/current")

    required = set(expected_required_refs)
    required.update(expected_base_rendered)
    required.update(expected_local_tags)

    missing = sorted(ref for ref in required if ref not in expected_base_rendered and not ref_exists(repo, ref))
    if missing:
        problems.append("required source refs are unavailable locally:\n" + "\n".join(f"  - {r}" for r in missing))

    actual_base_rendered = actual_refs(repo, "source/cache/base/edk2")
    actual_radxa = set(radxa_source_refs(repo))
    obsolete_radxa_delta = actual_refs(repo, "source/delta/radxa")
    obsolete_unofficial_delta = actual_refs(repo, "source/delta/unofficial")
    actual_local_tags = local_tag_refs(repo)
    actual_local_branches = actual_refs(repo, "source/unofficial")
    base_records = rendered_base_records(repo)

    missing_base_rendered = expected_base_rendered - actual_base_rendered
    extra_base_rendered = actual_base_rendered - expected_base_rendered
    non_regenerable_base = sorted(ref for ref in missing_base_rendered if ref not in base_records)
    if non_regenerable_base:
        problems.append(
            f"source/cache/base/edk2 cache refs are missing and not derivable from config/{EDK2_REFS_MANIFEST}:\n"
            + "\n".join(f"  - {r}" for r in non_regenerable_base)
        )
    missing_base_components: list[str] = []
    for ref in sorted(missing_base_rendered - set(non_regenerable_base)):
        for component in base_records[ref].get("components", []):
            component_ref = component.get("ref")
            if component_ref and not ref_exists(repo, component_ref):
                missing_base_components.append(f"{ref}: missing component {component_ref}")
    if missing_base_components:
        problems.append(
            "source/cache/base/edk2 cache refs are missing and cannot be regenerated because components are unavailable:\n"
            + "\n".join(f"  - {r}" for r in missing_base_components)
        )
    if extra_base_rendered:
        problems.append(
            "source/cache/base/edk2 cache refs do not match supported EDK2 source/base refs:\n"
            + "\n".join(f"  - {r}" for r in sorted(extra_base_rendered))
        )
    if expected_radxa != actual_radxa:
        problems.append(
            "Radxa vendor/port source refs do not match the matrix:\n"
            + diff_sets(expected_radxa, actual_radxa)
        )
    if obsolete_radxa_delta:
        problems.append(
            "obsolete source/delta/radxa refs remain; Radxa source is now recorded under source/vendor/radxa/** "
            "or source/port/radxa/**:\n"
            + "\n".join(f"  - {r}" for r in sorted(obsolete_radxa_delta))
        )
    if obsolete_unofficial_delta:
        problems.append(
            "obsolete source/delta/unofficial refs remain; unofficial source is now recorded directly under "
            "source/unofficial/**:\n"
            + "\n".join(f"  - {r}" for r in sorted(obsolete_unofficial_delta))
        )
    missing_tags = expected_local_tags - actual_local_tags
    if missing_tags:
        problems.append("missing unofficial release tags:\n" + "\n".join(f"  - {r}" for r in sorted(missing_tags)))
    extra_tags = actual_local_tags - expected_local_tags
    if extra_tags:
        problems.append("unofficial release tags are not used by any derived firmware source target:\n" + "\n".join(f"  - {r}" for r in sorted(extra_tags)))
    missing_local_branches = expected_local_branches - actual_local_branches
    if missing_local_branches:
        problems.append(
            "missing source/unofficial release branches:\n"
            + "\n".join(f"  - {r}" for r in sorted(missing_local_branches))
        )
    orphaned_tags = sorted(tag for tag in expected_local_tags & actual_local_tags if not tag_reachable_from_local_branch(repo, tag))
    if orphaned_tags:
        problems.append(
            "unofficial release tags are not reachable from any retained source/unofficial branch:\n"
            + "\n".join(f"  - {r}" for r in orphaned_tags)
        )

    if verbose:
        for ref in sorted(required):
            print(f"required ref ok: {ref}")

    return problems


def diff_sets(expected: set[str], actual: set[str]) -> str:
    lines: list[str] = []
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    if missing:
        lines.append("missing:")
        lines.extend(f"  - {item}" for item in missing)
    if extra:
        lines.append("extra:")
        lines.extend(f"  - {item}" for item in extra)
    return "\n".join(lines) if lines else "no differences"


def require_alias_trees(repo: Path, aliases: dict[str, str]) -> list[str]:
    problems: list[str] = []
    for alias, target in sorted(aliases.items()):
        if not ref_exists(repo, alias) or not ref_exists(repo, target):
            continue
        alias_tree = tree_id(repo, alias)
        target_tree = tree_id(repo, target)
        if alias_tree != target_tree:
            problems.append(f"{alias}: alias tree differs from {target} ({alias_tree} != {target_tree})")
    return problems


def require_non_cix_unofficial_source_targets(
    repo: Path,
    releases: list[str],
    expected_releases: set[str],
) -> list[str]:
    problems: list[str] = []
    unofficial_edk2 = unofficial_release_edk2_refs(repo)
    supported_edk2 = {edk2_ref_for_release(release) for release in releases}
    custom_radxa_releases = available_radxa_releases(repo, supported_edk2)

    for release in releases:
        edk2_ref = edk2_ref_for_release(release)
        if edk2_ref not in unofficial_edk2:
            continue
        for radxa in custom_radxa_releases:
            source_target = f"{CACHE_RELEASE_PREFIX}custom/edk2-{release}/radxa-{radxa}/unofficial"
            alias = f"{source_target}-{radxa}"
            missing = [ref for ref in (source_target, alias) if ref not in expected_releases]
            if missing:
                problems.append(
                    f"{edk2_ref}/radxa-{radxa}: missing non-CIX unofficial source target(s):\n"
                    + "\n".join(f"  - {ref}" for ref in missing)
                )
    return problems


def build_policy_for_release(release: str) -> str:
    return "edk2-stable202208" if release == "202208" else "post-edk2-stable202208"


def require_build_policy(releases: list[str]) -> list[str]:
    problems: list[str] = []
    policies = load_json(repo_root(Path(__file__)), "config/policies.json").get("build_policy", {})
    for release in releases:
        policy = build_policy_for_release(release)
        if policy not in policies:
            problems.append(f"edk2-stable{release}: unknown build_policy {policy!r}")
    return problems


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    releases = matrix_release_values(repo)

    expected_releases, expected_required_refs, aliases = expected_from_source_refs(repo)
    problems: list[str] = []
    problems.extend(require_manifested_release_entries(repo, expected_releases, verbose))
    problems.extend(require_source_refs(repo, releases, expected_required_refs, verbose))
    problems.extend(require_alias_trees(repo, aliases))
    problems.extend(require_non_cix_unofficial_source_targets(repo, releases, expected_releases))
    problems.extend(require_build_policy(releases))

    if problems:
        raise ReconstructionError("derived build matrix verification failed:\n" + "\n\n".join(problems))

    print(
        f"validated derived build matrix: {len(releases)} EDK2 releases, "
        f"{len(expected_releases)} firmware source-target refs "
        f"in {format_duration(time.monotonic() - started)}"
    )


if __name__ == "__main__":
    main_wrapper(main)
