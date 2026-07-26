#!/usr/bin/env python3
"""Add a compatibility toolchain tag when an EDK2 release removes it."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


KEY_RE = re.compile(r"^(?P<indent>\s*)(?P<key>[^#\s][^=]*?)(?P<spacing>\s*)=(?P<value>.*)$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools-def", type=Path, required=True)
    parser.add_argument("--source-tag", required=True)
    parser.add_argument("--compat-tag", required=True)
    return parser.parse_args()


def toolchain_key(key: str, tag: str) -> bool:
    return f"_{tag}_" in key


def missing_required_keys(keys: set[str]) -> list[str]:
    required_suffixes = ("_AARCH64_CC_PATH", "_AARCH64_CC_FLAGS", "_AARCH64_DLINK_FLAGS")
    return [
        suffix
        for suffix in required_suffixes
        if not any(key.endswith(suffix) for key in keys)
    ]


def add_compatibility_tag(path: Path, source_tag: str, compat_tag: str) -> int:
    text = path.read_text(encoding="utf-8")
    parsed: list[tuple[str, re.Match[str]]] = []
    keys: set[str] = set()

    for line in text.splitlines():
        match = KEY_RE.match(line)
        if match is None:
            continue
        key = match.group("key").rstrip()
        keys.add(key)
        parsed.append((line, match))

    compat_keys = {key for key in keys if toolchain_key(key, compat_tag)}
    if compat_keys:
        missing = missing_required_keys(compat_keys)
        if missing:
            joined = ", ".join(missing)
            raise ValueError(
                f"{path}: existing {compat_tag} tag is incomplete; missing: {joined}"
            )
        return 0

    source_marker = f"_{source_tag}_"
    aliases: list[str] = []
    for line, match in parsed:
        key = match.group("key").rstrip()
        if not toolchain_key(key, source_tag):
            continue
        compat_key = key.replace(source_marker, f"_{compat_tag}_", 1)
        aliases.append(
            f"{match.group('indent')}{compat_key}{match.group('spacing')}={match.group('value')}"
        )

    alias_keys = {
        KEY_RE.match(line).group("key").rstrip()
        for line in aliases
        if KEY_RE.match(line) is not None
    }
    missing = missing_required_keys(alias_keys)
    if missing:
        joined = ", ".join(missing)
        raise ValueError(
            f"{path}: {source_tag} does not provide required compatibility keys: {joined}"
        )

    block = [
        "",
        "##################",
        f"# {compat_tag} compatibility alias generated from {source_tag}",
        "##################",
        *aliases,
        "",
    ]
    with path.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(block))
    return len(aliases)


def main() -> int:
    args = parse_args()
    try:
        count = add_compatibility_tag(
            args.tools_def.resolve(),
            args.source_tag,
            args.compat_tag,
        )
    except (OSError, ValueError) as exc:
        print(f"[toolchain-compat] ERROR: {exc}")
        return 1
    if count:
        print(
            f"[toolchain-compat] Added {args.compat_tag} compatibility tag "
            f"from {count} {args.source_tag} definitions"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
