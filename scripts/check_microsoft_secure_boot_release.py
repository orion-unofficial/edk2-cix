#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys

from generate_microsoft_secure_boot_defaults import DEFAULT_MANIFEST_PATH, load_manifest


RELEASE_TAG_RE = re.compile(r"^v(?P<major>\d+)\.(?P<minor>\d+)\.(?P<patch>\d+)$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Check whether the pinned microsoft/secureboot_objects firmware release "
            "tag is still current."
        )
    )
    parser.add_argument("--manifest", type=pathlib.Path, default=DEFAULT_MANIFEST_PATH)
    parser.add_argument(
        "--mode",
        choices=("warn", "strict", "off"),
        default="warn",
        help="warn: print advisories but succeed; strict: fail on newer release or query error; off: skip the check",
    )
    return parser.parse_args()


def parse_release_tag(tag: str) -> tuple[int, int, int]:
    match = RELEASE_TAG_RE.fullmatch(tag)
    if not match:
        raise ValueError(f"Unsupported release tag format: {tag}")
    return tuple(int(match.group(name)) for name in ("major", "minor", "patch"))


def list_release_tags(repo_url: str) -> list[str]:
    result = subprocess.run(
        ["git", "ls-remote", "--tags", "--refs", repo_url],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        stderr = result.stderr.strip()
        if not stderr:
            stderr = f"git ls-remote exited with status {result.returncode}"
        raise RuntimeError(stderr)

    tags: list[str] = []
    for line in result.stdout.splitlines():
        _sha, ref = line.split("\t", 1)
        prefix = "refs/tags/"
        if not ref.startswith(prefix):
            continue
        tag = ref[len(prefix) :]
        if RELEASE_TAG_RE.fullmatch(tag):
            tags.append(tag)
    if not tags:
        raise RuntimeError(f"No firmware release tags matching {RELEASE_TAG_RE.pattern!r} found in {repo_url}")
    return tags


def main() -> int:
    args = parse_args()
    if args.mode == "off":
        print("Microsoft Secure Boot release freshness check skipped.")
        return 0

    manifest = load_manifest(args.manifest.resolve())
    repo_url = str(manifest["source_repo"])
    pinned_tag = str(manifest.get("source_tag", ""))
    if not pinned_tag:
        print(f"manifest {args.manifest} does not declare source_tag", file=sys.stderr)
        return 1

    try:
        pinned_version = parse_release_tag(pinned_tag)
        available_tags = list_release_tags(repo_url)
        latest_tag = max(available_tags, key=parse_release_tag)
        latest_version = parse_release_tag(latest_tag)
    except (RuntimeError, ValueError) as error:
        if args.mode == "strict":
            print(f"Microsoft Secure Boot release check failed: {error}", file=sys.stderr)
            return 1
        print(f"warning: Microsoft Secure Boot release check skipped: {error}", file=sys.stderr)
        return 0

    if latest_version > pinned_version:
        message = (
            f"Newer microsoft/secureboot_objects firmware release available: "
            f"pinned {pinned_tag}, latest {latest_tag}"
        )
        if args.mode == "strict":
            print(message, file=sys.stderr)
            return 1
        print(f"warning: {message}", file=sys.stderr)
        return 0

    print(f"Microsoft Secure Boot source tag {pinned_tag} is current.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
