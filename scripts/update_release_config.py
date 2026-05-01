#!/usr/bin/env python3
"""Seed supported-release metadata for a new EDK2 release."""

from __future__ import annotations

import argparse
import json
import os
import re
from pathlib import Path
from typing import Any

from reconstruction_common import ReconstructionError, main_wrapper, repo_root, truthy


HELP = """update-release-config

Required variables:
  EDK2_RELEASE  EDK2 release to add, e.g. 202605 or edk2-stable202605.

Optional variables:
  RADXA_RELEASE=1.2.1
  CIX_RELEASE=1.2
  LOCAL_VERSION=1.2.1
  BUILD_POLICY=<name>  Defaults to edk2-stable202208 for 202208, otherwise
                       post-edk2-stable202208.
  WRITE=1             Actually update config/build-matrix.json and
                       config/releases.json. Without WRITE=1 this is a dry run.

After writing:
  1. integrate the missing source refs with make integrate-source-release
  2. create variant refs with make render-release-branch PERSIST=1 REBUILD=1 FORCE=1
  3. run make verify-build-matrix
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--edk2-release", default=os.environ.get("EDK2_RELEASE", ""))
    p.add_argument("--radxa-release", default=os.environ.get("RADXA_RELEASE", "1.2.1"))
    p.add_argument("--cix-release", default=os.environ.get("CIX_RELEASE", "1.2"))
    p.add_argument("--local-version", default=os.environ.get("LOCAL_VERSION", "1.2.1"))
    p.add_argument("--build-policy", default=os.environ.get("BUILD_POLICY", ""))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def normalise_edk2_release(value: str) -> tuple[str, str]:
    if not value:
        raise ReconstructionError("EDK2_RELEASE is required")
    release = value.removeprefix("edk2-stable")
    if not re.fullmatch(r"\d{6}(?:\.\d+)?", release):
        raise ReconstructionError("EDK2_RELEASE must look like 202602 or edk2-stable202602")
    return release, f"edk2-stable{release}"


def release_sort_key(item: dict[str, Any]) -> tuple[int, int]:
    release = str(item["release"])
    main, _, suffix = release.partition(".")
    return int(main), int(suffix or 0)


def ensure_matrix_release(matrix: dict[str, Any], release: str, edk2_ref: str, build_policy: str) -> bool:
    releases = matrix.setdefault("edk2_releases", [])
    for item in releases:
        if item.get("release") == release:
            changed = False
            if item.get("edk2_ref") != edk2_ref:
                item["edk2_ref"] = edk2_ref
                changed = True
            if item.get("build_policy") != build_policy:
                item["build_policy"] = build_policy
                changed = True
            return changed
    releases.append({"release": release, "edk2_ref": edk2_ref, "build_policy": build_policy})
    releases.sort(key=release_sort_key)
    return True


def release_entries(release: str, edk2_ref: str, radxa: str, cix: str, local_version: str) -> dict[str, dict[str, Any]]:
    return {
        f"source/release/upstream/edk2-{release}/radxa-{radxa}": {
            "description": f"Upstream EDK2 {release} checkpoint with Radxa {radxa} vendor layer, rendered as ordinary files.",
            "edk2_release": edk2_ref,
            "local_delta": False,
            "radxa_release": radxa,
            "render": {
                "base": {"ref": f"source/base/rendered/{edk2_ref}"},
                "commit_message": f"render: upstream EDK2 {release} with Radxa {radxa} vendor layer",
                "remove_root_gitmodules": True,
                "steps": [{"delta": f"source/delta/radxa/{radxa}/{edk2_ref}"}],
            },
            "source_ref": f"source/release/upstream/edk2-{release}/radxa-{radxa}",
        },
        f"source/release/vendor/edk2-{release}/cix-{cix}/radxa-{radxa}": {
            "cix_release": cix,
            "description": f"Vendor EDK2 {release} baseline with Radxa {radxa} and CIX {cix} TF-A/OP-TEE components.",
            "edk2_release": edk2_ref,
            "local_delta": False,
            "radxa_release": radxa,
            "render": {
                "base": {"ref": f"source/release/upstream/edk2-{release}/radxa-{radxa}"},
                "commit_message": f"render: vendor EDK2 {release} with Radxa {radxa} and CIX {cix} components",
                "remove_root_gitmodules": True,
                "steps": [
                    {"component": {"path": f"src/cix-v{cix}/tf-a", "ref": f"source/component/cix/{cix}/tf-a"}},
                    {"component": {"path": f"src/cix-v{cix}/tee", "ref": f"source/component/cix/{cix}/op-tee"}},
                ],
            },
            "source_ref": f"source/release/vendor/edk2-{release}/cix-{cix}/radxa-{radxa}",
        },
        f"source/release/custom/edk2-{release}/cix-{cix}/radxa-{radxa}/local": {
            "cix_release": cix,
            "description": f"Firmware variant with EDK2 {release}, Radxa {radxa}, CIX {cix} components, and local changes.",
            "edk2_release": edk2_ref,
            "local_delta": True,
            "radxa_release": radxa,
            "render": {
                "base": {"ref": f"source/release/vendor/edk2-{release}/cix-{cix}/radxa-{radxa}"},
                "commit_message": f"render: firmware variant with EDK2 {release}, Radxa {radxa}, CIX {cix} components, and local changes",
                "remove_root_gitmodules": True,
                "steps": [{"delta": f"source/delta/local/{edk2_ref}"}],
            },
            "source_ref": f"source/release/custom/edk2-{release}/cix-{cix}/radxa-{radxa}/local",
        },
        f"source/release/custom/edk2-{release}/cix-{cix}/radxa-{radxa}/local-{local_version}": {
            "cix_release": cix,
            "description": f"Named firmware variant alias for local version {local_version} on EDK2 {release}.",
            "edk2_release": edk2_ref,
            "local_delta": True,
            "radxa_release": radxa,
            "render": {
                "base": {"ref": f"source/release/vendor/edk2-{release}/cix-{cix}/radxa-{radxa}"},
                "commit_message": f"render: named firmware variant alias for local version {local_version} on EDK2 {release}",
                "remove_root_gitmodules": True,
                "steps": [{"delta": f"source/delta/local/{edk2_ref}"}],
            },
        },
    }


def merge_release_entries(releases: dict[str, Any], entries: dict[str, dict[str, Any]]) -> list[str]:
    added: list[str] = []
    for branch, entry in entries.items():
        if branch not in releases:
            releases[branch] = entry
            added.append(branch)
    return added


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    write = truthy(args.write)

    release, edk2_ref = normalise_edk2_release(args.edk2_release)
    build_policy = args.build_policy or ("edk2-stable202208" if release == "202208" else "post-edk2-stable202208")
    matrix_path = repo / "config" / "build-matrix.json"
    releases_path = repo / "config" / "releases.json"
    matrix = load_json(matrix_path)
    release_config = load_json(releases_path)

    matrix_changed = ensure_matrix_release(matrix, release, edk2_ref, build_policy)
    entries = release_entries(release, edk2_ref, args.radxa_release, args.cix_release, args.local_version)
    added = merge_release_entries(release_config.setdefault("releases", {}), entries)

    print(f"EDK2 release: {edk2_ref}")
    print(f"build policy: {build_policy}")
    print(f"matrix entry: {'added/updated' if matrix_changed else 'already present'}")
    print(f"release entries added: {len(added)}")
    for branch in added:
        print(f"  - {branch}")

    if not write:
        print("dry run only; rerun with WRITE=1 to update config files")
        return

    if matrix_changed:
        write_json(matrix_path, matrix)
        if verbose:
            print(f"updated {matrix_path.relative_to(repo)}")
    if added:
        write_json(releases_path, release_config)
        if verbose:
            print(f"updated {releases_path.relative_to(repo)}")
    if not matrix_changed and not added:
        print("no changes required")


if __name__ == "__main__":
    main_wrapper(main)
