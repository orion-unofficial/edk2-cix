#!/usr/bin/env python3

from __future__ import annotations

import argparse
import html.parser
import pathlib
import re
import sys
import urllib.parse


ASSET_TAG_ATTRS = {
    "iframe": "src",
    "img": "src",
    "link": "href",
    "script": "src",
    "source": "src",
    "audio": "src",
    "video": "src",
}

SEARCH_INDEX_PATTERN = re.compile(r'path_to_searchindex_js\s*=\s*"([^"]+)"')


class AssetReferenceParser(html.parser.HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.references: list[tuple[str, str, str]] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        self._collect_reference(tag, attrs)

    def handle_startendtag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        self._collect_reference(tag, attrs)

    def _collect_reference(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attr_name = ASSET_TAG_ATTRS.get(tag)
        if attr_name is None:
            return
        attr_map = dict(attrs)
        ref = attr_map.get(attr_name)
        if ref:
            self.references.append((tag, attr_name, ref))


def should_skip_reference(ref: str) -> bool:
    if not ref or ref.startswith("#"):
        return True
    parsed = urllib.parse.urlparse(ref)
    if parsed.scheme or parsed.netloc:
        return True
    return ref.startswith(("data:", "javascript:", "mailto:", "tel:"))


def resolve_reference(build_root: pathlib.Path, html_path: pathlib.Path, ref: str) -> pathlib.Path | None:
    if should_skip_reference(ref):
        return None

    parsed = urllib.parse.urlparse(ref)
    path = urllib.parse.unquote(parsed.path)
    if not path:
        return None

    if path.startswith("/"):
        candidate = build_root / path.lstrip("/")
    else:
        candidate = html_path.parent / path

    if candidate.exists():
        return candidate
    if candidate.is_dir():
        index_candidate = candidate / "index.html"
        if index_candidate.exists():
            return index_candidate
    if not candidate.suffix:
        index_candidate = candidate / "index.html"
        if index_candidate.exists():
            return index_candidate
    return None


def validate_html_file(build_root: pathlib.Path, html_path: pathlib.Path) -> list[str]:
    text = html_path.read_text(encoding="utf-8")
    parser = AssetReferenceParser()
    parser.feed(text)

    refs = list(parser.references)
    for match in SEARCH_INDEX_PATTERN.finditer(text):
        refs.append(("script-inline", "searchindex", match.group(1)))

    errors: list[str] = []
    for tag, attr, ref in refs:
        if should_skip_reference(ref):
            continue
        if resolve_reference(build_root, html_path, ref) is None:
            rel_html = html_path.relative_to(build_root)
            errors.append(f"{rel_html}: missing {tag} {attr}={ref!r}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate that built mdBook HTML references only existing local asset files."
    )
    parser.add_argument("build_dir", help="Path to the built documentation root")
    args = parser.parse_args()

    build_root = pathlib.Path(args.build_dir).resolve()
    if not build_root.is_dir():
        parser.error(f"{build_root} is not a directory")

    html_files = sorted(build_root.rglob("*.html"))
    if not html_files:
        parser.error(f"{build_root} does not contain any HTML files")

    errors: list[str] = []
    for html_path in html_files:
        errors.extend(validate_html_file(build_root, html_path))

    if errors:
        print("[docs-validate] Broken local asset references detected:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print(f"[docs-validate] Validated {len(html_files)} HTML files under {build_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
