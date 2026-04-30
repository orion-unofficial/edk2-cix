#!/usr/bin/env python3
"""Verify that the declared supported build matrix has matching refs."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
from typing import Any

from reconstruction_common import (
    ReconstructionError,
    git,
    load_json,
    main_wrapper,
    ref_exists,
    repo_root,
    tree_id,
    truthy,
)


HELP = """verify-build-matrix

No variables are required.

Optional variables:
  V=1  Print every expected release branch and required source ref.

Checks:
  - every release declared in config/build-matrix.json exists in config/releases.json
  - every declared release branch/ref exists locally
  - required base, vendor, component, and local refs exist
  - actual source/release branches are all declared in the build matrix
  - Radxa delta and rendered-base refs cover every declared EDK2 release
  - local-1.2.1 aliases have the same tree as their non-alias local branch
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--v", default=os.environ.get("V", "0"), help="verbosity flag propagated from make")
    return p


def release_values(matrix: dict[str, Any]) -> list[str]:
    values = [item["release"] for item in matrix.get("edk2_releases", [])]
    if not values:
        raise ReconstructionError("config/build-matrix.json does not declare any edk2_releases")
    return values


def expand_template(template: str, release: str) -> str:
    return template.format(release=release, edk2_ref=f"edk2-stable{release}")


def variant_releases(variant: dict[str, Any], all_releases: list[str]) -> list[str]:
    releases = variant.get("releases")
    if releases == "all":
        return all_releases
    if isinstance(releases, list):
        return releases
    if "release" in variant:
        return [variant["release"]]
    raise ReconstructionError(f"variant has no release selection: {variant.get('name', '<unnamed>')}")


def expected_from_matrix(matrix: dict[str, Any]) -> tuple[set[str], set[str], dict[str, str]]:
    all_releases = release_values(matrix)
    expected_releases: set[str] = set()
    expected_required_refs: set[str] = set()
    aliases: dict[str, str] = {}

    for variant in matrix.get("source_variants", []):
        branch_template = variant.get("branch_template")
        if not branch_template:
            raise ReconstructionError(f"source variant has no branch_template: {variant.get('name', '<unnamed>')}")
        for release in variant_releases(variant, all_releases):
            branch = expand_template(branch_template, release)
            expected_releases.add(branch)
            for required in variant.get("required_ref_templates", []):
                expected_required_refs.add(expand_template(required, release))
            alias_template = variant.get("alias_of_template")
            if alias_template:
                aliases[branch] = expand_template(alias_template, release)

    for variant in matrix.get("special_source_variants", []):
        branch = variant.get("branch")
        if not branch:
            raise ReconstructionError(f"special source variant has no branch: {variant.get('name', '<unnamed>')}")
        expected_releases.add(branch)
        expected_required_refs.update(variant.get("required_refs", []))
        if variant.get("alias_of"):
            aliases[branch] = variant["alias_of"]

    return expected_releases, expected_required_refs, aliases


def actual_source_release_refs(repo: Path) -> set[str]:
    result = git(repo, "for-each-ref", "--format=%(refname:short)", "refs/heads/source/release", check=False)
    if result.returncode != 0:
        return set()
    return {line for line in result.stdout.splitlines() if line}


def actual_refs(repo: Path, namespace: str) -> set[str]:
    result = git(repo, "for-each-ref", "--format=%(refname:short)", f"refs/heads/{namespace}", check=False)
    if result.returncode != 0:
        return set()
    return {line for line in result.stdout.splitlines() if line}


def local_tag_refs(repo: Path) -> set[str]:
    result = git(repo, "for-each-ref", "--format=%(refname:short)", "refs/tags/source/local", check=False)
    if result.returncode != 0:
        return set()
    return {line for line in result.stdout.splitlines() if line}


def require_manifested_release_entries(
    repo: Path,
    expected_releases: set[str],
    verbose: bool,
) -> list[str]:
    problems: list[str] = []
    releases = load_json(repo, "config/releases.json").get("releases", {})
    configured = set(releases)

    missing_config = sorted(expected_releases - configured)
    extra_config = sorted(configured - expected_releases)
    if missing_config:
        problems.append("config/releases.json is missing declared matrix releases:\n" + "\n".join(f"  - {r}" for r in missing_config))
    if extra_config:
        problems.append("config/releases.json contains releases not declared in config/build-matrix.json:\n" + "\n".join(f"  - {r}" for r in extra_config))

    actual = actual_source_release_refs(repo)
    missing_refs = sorted(expected_releases - actual)
    extra_refs = sorted(actual - expected_releases)
    if missing_refs:
        problems.append("missing source/release refs:\n" + "\n".join(f"  - {r}" for r in missing_refs))
    if extra_refs:
        problems.append("source/release refs are not declared in config/build-matrix.json:\n" + "\n".join(f"  - {r}" for r in extra_refs))

    for ref in sorted(expected_releases):
        if ref not in releases:
            continue
        if not ref_exists(repo, ref):
            continue
        expected_tree = releases[ref].get("tree_id")
        if expected_tree and tree_id(repo, ref) != expected_tree:
            problems.append(f"{ref}: tree ID differs from config/releases.json ({tree_id(repo, ref)} != {expected_tree})")
        if verbose:
            print(f"release ok: {ref}")

    return problems


def require_source_refs(
    repo: Path,
    matrix: dict[str, Any],
    expected_required_refs: set[str],
    verbose: bool,
) -> list[str]:
    problems: list[str] = []
    all_releases = release_values(matrix)
    expected_base_rendered = {f"source/base/rendered/edk2-stable{release}" for release in all_releases}
    expected_radxa = {f"source/delta/radxa/1.2.1/edk2-stable{release}" for release in all_releases}
    expected_local_tags = {f"source/unofficial/edk2-stable{release}" for release in all_releases}

    required = set(expected_required_refs)
    required.update(expected_base_rendered)
    required.update(expected_radxa)
    required.update(expected_local_tags)

    missing = sorted(ref for ref in required if not ref_exists(repo, ref))
    if missing:
        problems.append("required source refs are unavailable locally:\n" + "\n".join(f"  - {r}" for r in missing))

    actual_base_rendered = actual_refs(repo, "source/base/rendered")
    actual_radxa = actual_refs(repo, "source/delta/radxa/1.2.1")
    actual_local_tags = local_tag_refs(repo)

    if expected_base_rendered != actual_base_rendered:
        problems.append(
            "source/base/rendered refs do not match the matrix:\n"
            + diff_sets(expected_base_rendered, actual_base_rendered)
        )
    if expected_radxa != actual_radxa:
        problems.append(
            "source/delta/radxa/1.2.1 refs do not match the matrix:\n"
            + diff_sets(expected_radxa, actual_radxa)
        )
    missing_tags = expected_local_tags - actual_local_tags
    if missing_tags:
        problems.append("missing local compatibility tags:\n" + "\n".join(f"  - {r}" for r in sorted(missing_tags)))

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


def require_build_policy(matrix: dict[str, Any]) -> list[str]:
    problems: list[str] = []
    policies = load_json(repo_root(Path(__file__)), "config/policies.json").get("build_policy", {})
    for item in matrix.get("edk2_releases", []):
        policy = item.get("build_policy")
        if policy not in policies:
            problems.append(f"{item.get('edk2_ref', item.get('release'))}: unknown build_policy {policy!r}")
    return problems


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    matrix = load_json(repo, "config/build-matrix.json")

    expected_releases, expected_required_refs, aliases = expected_from_matrix(matrix)
    problems: list[str] = []
    problems.extend(require_manifested_release_entries(repo, expected_releases, verbose))
    problems.extend(require_source_refs(repo, matrix, expected_required_refs, verbose))
    problems.extend(require_alias_trees(repo, aliases))
    problems.extend(require_build_policy(matrix))

    if problems:
        raise ReconstructionError("build matrix verification failed:\n" + "\n\n".join(problems))

    print(
        f"validated build matrix: {len(release_values(matrix))} EDK2 releases, "
        f"{len(expected_releases)} release refs"
    )


if __name__ == "__main__":
    main_wrapper(main)
