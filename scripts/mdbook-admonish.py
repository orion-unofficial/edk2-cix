#!/usr/bin/env python3

import html
import json
import re
import shlex
import sys


OPEN_RE = re.compile(
    r"^(?P<indent>[ \t]*)(?P<fence>`{3,}|~{3,})admonish(?:\s+(?P<args>.*))?\s*$"
)


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] == "supports":
        return 0

    try:
        ctx, book = json.load(sys.stdin)
    except Exception as exc:
        print(f"failed to parse mdBook preprocessor input: {exc}", file=sys.stderr)
        return 1

    if ctx.get("renderer") == "html":
        process_items(book.get("items", []))

    json.dump(book, sys.stdout)
    return 0


def process_items(items: list[dict]) -> None:
    for item in items:
        chapter = item.get("Chapter")
        if not chapter:
            continue
        chapter["content"] = rewrite_admonitions(chapter.get("content", ""))
        process_items(chapter.get("sub_items", []))


def rewrite_admonitions(text: str) -> str:
    lines = text.splitlines()
    output: list[str] = []
    counters: dict[str, int] = {}
    idx = 0

    while idx < len(lines):
        line = lines[idx]
        match = OPEN_RE.match(line)
        if not match:
            output.append(line)
            idx += 1
            continue

        indent = match.group("indent")
        fence = match.group("fence")
        args = parse_args(match.group("args") or "")
        idx += 1
        content_lines: list[str] = []

        while idx < len(lines):
            current = lines[idx]
            stripped = current.strip()
            if stripped and set(stripped) == {fence[0]} and len(stripped) >= len(fence):
                break
            content_lines.append(current)
            idx += 1

        if idx == len(lines):
            output.append(line)
            output.extend(content_lines)
            break

        output.extend(
            render_admonition(
                indent=indent,
                directive=args["directive"],
                title=args["title"],
                content="\n".join(content_lines),
                counters=counters,
            ).splitlines()
        )
        idx += 1

    return "\n".join(output) + ("\n" if text.endswith("\n") else "")


def parse_args(raw: str) -> dict[str, str | None]:
    directive = "note"
    title = None

    try:
        tokens = shlex.split(raw)
    except ValueError:
        tokens = raw.split()

    if tokens and "=" not in tokens[0]:
        directive = tokens.pop(0)

    for token in tokens:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if key == "title":
            title = value

    return {"directive": directive, "title": title}


def render_admonition(
    *,
    indent: str,
    directive: str,
    title: str | None,
    content: str,
    counters: dict[str, int],
) -> str:
    label = title or directive.replace("-", " ").title()
    anchor_base = slugify(label or "default")
    count = counters.get(anchor_base, 0)
    counters[anchor_base] = count + 1
    anchor_id = anchor_base if count == 0 else f"{anchor_base}-{count}"

    return f"""
{indent}<div id="{html.escape(anchor_id)}" class="admonition admonish-{html.escape(directive)}" role="note" aria-labelledby="{html.escape(anchor_id)}-title">
{indent}<div class="admonition-title">
{indent}<div id="{html.escape(anchor_id)}-title">
{indent}
{indent}{html.escape(label)}
{indent}
{indent}</div>
{indent}<a class="admonition-anchor-link" href="#{html.escape(anchor_id)}"></a>
{indent}</div>
{indent}<div>

{content}

{indent}</div>
{indent}</div>""".lstrip("\n")


def slugify(text: str) -> str:
    chars: list[str] = []
    for char in text:
        if char.isalnum() or char in {"_", "-"}:
            chars.append(char.lower())
        elif char.isspace():
            if not chars or chars[-1] == "-":
                continue
            chars.append("-")
    return "".join(chars).strip("-") or "default"


if __name__ == "__main__":
    raise SystemExit(main())
