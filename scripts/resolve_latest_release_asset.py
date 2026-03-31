#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
from typing import Any


TAG_RE = re.compile(r"^[0-9][0-9A-Za-z.+:~_-]*$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Resolve the latest plain upstream release tag from local git tags "
            "and optionally look up its Debian package asset from GitHub Releases."
        )
    )
    parser.add_argument("--repo-root", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--github-repository", help="GitHub repository in owner/name form.")
    parser.add_argument(
        "--token-env",
        default="GITHUB_TOKEN",
        help="Environment variable that holds the GitHub token (default: GITHUB_TOKEN).",
    )
    return parser.parse_args()


def run(argv: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(argv, check=True, text=True, capture_output=True)


def list_candidate_tags(repo_root: pathlib.Path) -> list[str]:
    result = run(["git", "-C", str(repo_root), "tag", "--list"])
    candidates: list[str] = []
    for raw_tag in result.stdout.splitlines():
        tag = raw_tag.strip()
        if not tag:
            continue
        if tag.startswith("mr/") or "/" in tag:
            continue
        if not TAG_RE.fullmatch(tag):
            continue
        candidates.append(tag)
    return candidates


def split_debian_version(version: str) -> tuple[int, str, str]:
    if ":" in version:
        epoch_text, remainder = version.split(":", 1)
    else:
        epoch_text, remainder = "0", version
    try:
        epoch = int(epoch_text)
    except ValueError as exc:
        raise ValueError(f"Invalid Debian epoch in version {version!r}") from exc

    if "-" in remainder:
        upstream_version, debian_revision = remainder.rsplit("-", 1)
    else:
        upstream_version, debian_revision = remainder, ""
    return epoch, upstream_version, debian_revision


def char_order(char: str) -> int:
    if not char:
        return 0
    if char == "~":
        return -1
    if char.isalpha():
        return ord(char)
    return ord(char) + 256


def compare_non_digit_part(lhs: str, rhs: str) -> int:
    index = 0
    while index < len(lhs) or index < len(rhs):
        lhs_char = lhs[index] if index < len(lhs) else ""
        rhs_char = rhs[index] if index < len(rhs) else ""
        lhs_order = char_order(lhs_char)
        rhs_order = char_order(rhs_char)
        if lhs_order != rhs_order:
            return -1 if lhs_order < rhs_order else 1
        index += 1
    return 0


def compare_digit_part(lhs: str, rhs: str) -> int:
    lhs_trimmed = lhs.lstrip("0")
    rhs_trimmed = rhs.lstrip("0")

    if len(lhs_trimmed) != len(rhs_trimmed):
        return -1 if len(lhs_trimmed) < len(rhs_trimmed) else 1
    if lhs_trimmed != rhs_trimmed:
        return -1 if lhs_trimmed < rhs_trimmed else 1
    return 0


def leading_non_digit_part(text: str) -> str:
    index = 0
    while index < len(text) and not text[index].isdigit():
        index += 1
    return text[:index]


def compare_version_component(lhs: str, rhs: str) -> int:
    lhs_remaining = lhs
    rhs_remaining = rhs

    while lhs_remaining or rhs_remaining:
        lhs_prefix = leading_non_digit_part(lhs_remaining)
        rhs_prefix = leading_non_digit_part(rhs_remaining)
        prefix_cmp = compare_non_digit_part(lhs_prefix, rhs_prefix)
        if prefix_cmp:
            return prefix_cmp
        lhs_remaining = lhs_remaining[len(lhs_prefix) :]
        rhs_remaining = rhs_remaining[len(rhs_prefix) :]

        lhs_digits = ""
        while lhs_remaining[:1].isdigit():
            lhs_digits += lhs_remaining[0]
            lhs_remaining = lhs_remaining[1:]
        rhs_digits = ""
        while rhs_remaining[:1].isdigit():
            rhs_digits += rhs_remaining[0]
            rhs_remaining = rhs_remaining[1:]

        digit_cmp = compare_digit_part(lhs_digits, rhs_digits)
        if digit_cmp:
            return digit_cmp

    return 0


def version_is_newer(lhs: str, rhs: str) -> bool:
    lhs_epoch, lhs_upstream, lhs_revision = split_debian_version(lhs)
    rhs_epoch, rhs_upstream, rhs_revision = split_debian_version(rhs)

    if lhs_epoch != rhs_epoch:
        return lhs_epoch > rhs_epoch

    upstream_cmp = compare_version_component(lhs_upstream, rhs_upstream)
    if upstream_cmp:
        return upstream_cmp > 0

    revision_cmp = compare_version_component(lhs_revision, rhs_revision)
    return revision_cmp > 0


def select_latest_tag(tags: list[str]) -> str:
    if not tags:
        raise ValueError("No plain version tags were found after filtering out namespaced tags.")

    latest = tags[0]
    for tag in tags[1:]:
        if version_is_newer(tag, latest):
            latest = tag
    return latest


def github_request(url: str, token: str | None) -> dict[str, Any]:
    headers = {"Accept": "application/vnd.github+json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(request) as response:
            return json.load(response)
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace").strip()
        message = detail or str(exc)
        raise RuntimeError(f"GitHub API request failed for {url}: {message}") from exc


def resolve_release_asset(
    github_repository: str,
    tag: str,
    token: str | None,
) -> dict[str, Any]:
    encoded_tag = urllib.parse.quote(tag, safe="")
    release = github_request(
        f"https://api.github.com/repos/{github_repository}/releases/tags/{encoded_tag}",
        token,
    )
    assets = release.get("assets", [])
    if not isinstance(assets, list):
        raise RuntimeError(f"Release metadata for tag {tag} does not contain an asset list.")

    expected_name = f"edk2-cix_{tag}_all.deb"
    exact_match = next(
        (asset for asset in assets if isinstance(asset, dict) and asset.get("name") == expected_name),
        None,
    )
    if exact_match is not None:
        return exact_match

    deb_assets = [
        asset
        for asset in assets
        if isinstance(asset, dict) and str(asset.get("name", "")).endswith(".deb")
    ]
    if len(deb_assets) == 1:
        return deb_assets[0]
    if not deb_assets:
        raise RuntimeError(f"Release tag {tag} does not publish a .deb asset.")

    asset_names = ", ".join(sorted(str(asset.get("name")) for asset in deb_assets))
    raise RuntimeError(
        f"Release tag {tag} has multiple .deb assets and none matched {expected_name}: {asset_names}"
    )


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()

    try:
        latest_tag = select_latest_tag(list_candidate_tags(repo_root))
    except (ValueError, subprocess.CalledProcessError) as exc:
        print(str(exc), file=sys.stderr)
        return 2

    payload: dict[str, Any] = {
        "tag": latest_tag,
    }

    if args.github_repository:
        token = os.environ.get(args.token_env)
        try:
            asset = resolve_release_asset(args.github_repository, latest_tag, token)
        except RuntimeError as exc:
            print(str(exc), file=sys.stderr)
            return 2
        payload.update(
            {
                "asset_id": asset.get("id"),
                "asset_name": asset.get("name"),
                "asset_api_url": asset.get("url"),
                "asset_download_url": asset.get("browser_download_url"),
            }
        )

    print(json.dumps(payload, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
