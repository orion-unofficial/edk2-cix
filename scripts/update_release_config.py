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
  BUILD_POLICY=<name>  Defaults to edk2-stable202208 for 202208, otherwise
                       post-edk2-stable202208.
  WRITE=0|1           Actually update config/build-matrix.json.
                       Without WRITE=1 this is a dry run.

After writing:
  1. integrate the missing source refs with make integrate-source-release
  2. create variant refs with make render-release-branch PERSIST=1 REBUILD=1 FORCE=1
  3. run make verify-build-matrix
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--edk2-release", default=os.environ.get("EDK2_RELEASE", ""))
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


def ensure_matrix_release(matrix: dict[str, Any], release: str, build_policy: str) -> bool:
    releases = matrix.setdefault("edk2_releases", [])
    for item in releases:
        if item.get("release") == release:
            changed = False
            if item.get("build_policy") != build_policy:
                item["build_policy"] = build_policy
                changed = True
            return changed
    releases.append({"release": release, "build_policy": build_policy})
    releases.sort(key=release_sort_key)
    return True


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    write = truthy(args.write)

    release, edk2_ref = normalise_edk2_release(args.edk2_release)
    build_policy = args.build_policy or ("edk2-stable202208" if release == "202208" else "post-edk2-stable202208")
    matrix_path = repo / "config" / "build-matrix.json"
    matrix = load_json(matrix_path)

    matrix_changed = ensure_matrix_release(matrix, release, build_policy)

    print(f"EDK2 release: {edk2_ref}")
    print(f"build policy: {build_policy}")
    print(f"matrix entry: {'added/updated' if matrix_changed else 'already present'}")
    print("variant render plans: synthesised at runtime from config/build-matrix.json")

    if not write:
        print("dry run only; rerun with WRITE=1 to update config files")
        return

    if matrix_changed:
        write_json(matrix_path, matrix)
        if verbose:
            print(f"updated {matrix_path.relative_to(repo)}")
    if not matrix_changed:
        print("no changes required")


if __name__ == "__main__":
    main_wrapper(main)
