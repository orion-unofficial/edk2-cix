#!/usr/bin/env python3
"""Resolve a version-pinned edk2-cix Debian package from GitHub Releases."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from typing import Any


GITHUB_REPOSITORY_RE = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
TAG_RE = re.compile(r"^[0-9][0-9A-Za-z.+:~_-]*$")


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
        raise RuntimeError(
            f"GitHub API request failed for {url}: {detail or exc}"
        ) from exc


def resolve_release_asset(
    github_repository: str,
    tag: str,
    token: str | None,
) -> dict[str, Any]:
    if not GITHUB_REPOSITORY_RE.fullmatch(github_repository):
        raise ValueError(
            f"GitHub repository must have owner/name form: {github_repository!r}"
        )
    if not TAG_RE.fullmatch(tag):
        raise ValueError(f"Invalid release tag: {tag!r}")

    release = github_request(
        "https://api.github.com/repos/"
        f"{github_repository}/releases/tags/{urllib.parse.quote(tag, safe='')}",
        token,
    )
    assets = release.get("assets", [])
    if not isinstance(assets, list):
        raise RuntimeError(
            f"Release metadata for tag {tag} does not contain an asset list."
        )

    expected_name = f"edk2-cix_{tag}_all.deb"
    exact_match = next(
        (
            asset
            for asset in assets
            if isinstance(asset, dict) and asset.get("name") == expected_name
        ),
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

    names = ", ".join(sorted(str(asset.get("name")) for asset in deb_assets))
    raise RuntimeError(
        f"Release tag {tag} has multiple .deb assets and none matched "
        f"{expected_name}: {names}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--github-repository", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument(
        "--token-env",
        default="GITHUB_TOKEN",
        help="Environment variable holding the GitHub token (default: GITHUB_TOKEN).",
    )
    args = parser.parse_args()

    try:
        asset = resolve_release_asset(
            args.github_repository,
            args.tag,
            os.environ.get(args.token_env),
        )
    except (RuntimeError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 2

    print(
        json.dumps(
            {
                "asset_api_url": asset.get("url"),
                "asset_download_url": asset.get("browser_download_url"),
                "asset_id": asset.get("id"),
                "asset_name": asset.get("name"),
                "tag": args.tag,
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
