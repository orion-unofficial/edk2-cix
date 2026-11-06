#!/usr/bin/env python3
"""Concise field-level diagnostics for the existing ACPI and FV audit gates."""
from __future__ import annotations

from typing import Any, Iterator


def changes(expected: Any, actual: Any, path: str = '', *, ignore: frozenset[str] = frozenset()) -> Iterator[str]:
    """Describe all differences without changing the audit's acceptance rules."""
    def display(value: Any) -> str:
        text = repr(value)
        return text if len(text) <= 72 else text[:69] + '...'

    if isinstance(expected, dict) and isinstance(actual, dict):
        for key in sorted(expected.keys() | actual.keys()):
            if key in ignore:
                continue
            label = f'{path}/{key}' if path else key
            if key not in actual:
                yield f'{label}: removed'
            elif key not in expected:
                yield f'{label}: added'
            else:
                yield from changes(expected[key], actual[key], label, ignore=ignore)
    elif isinstance(expected, list) and isinstance(actual, list):
        # FFS modules and FV entries are keyed by GUID, so movement does not
        # turn every subsequent record into a spurious changed module.
        if all(isinstance(item, dict) and 'guid' in item for item in expected + actual):
            old = {item['guid']: item for item in expected}
            new = {item['guid']: item for item in actual}
            if len(old) == len(expected) and len(new) == len(actual):
                yield from changes(old, new, path, ignore=ignore)
                return
        if len(expected) != len(actual):
            yield f'{path}/count: {len(expected)} -> {len(actual)}'
        for index, (old, new) in enumerate(zip(expected, actual)):
            yield from changes(old, new, f'{path}[{index}]', ignore=ignore)
    elif expected != actual:
        yield f'{path}: {display(expected)} -> {display(actual)}'


def print_summary(lines: list[str], *, limit: int = 12) -> None:
    print(f'  {len(lines)} changed fields/items (expected -> actual):')
    for line in lines[:limit]:
        print(f'  - {line}')
    if len(lines) > limit:
        print(f'  ... {len(lines) - limit} more; use --report-json for the complete audit report')
