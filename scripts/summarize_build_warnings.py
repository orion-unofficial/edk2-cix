#!/usr/bin/env python3

from __future__ import annotations

import argparse
import collections
import pathlib
import re
import sys
from dataclasses import dataclass


@dataclass(frozen=True)
class WarningClass:
    key: str
    title: str
    pattern: re.Pattern[str]
    disposition: str
    note: str


WARNING_CLASSES = (
    WarningClass(
        key="vfr_grammar_ambiguity",
        title="VFR grammar ambiguity",
        pattern=re.compile(r"VfrSyntax\.g, line .*ambiguous upon"),
        disposition="known-upstream",
        note=(
            "BaseTools grammar ambiguity warning from the imported upstream "
            "VFR compiler. Safe to treat as known noise on the current "
            "upstream-preserving path."
        ),
    ),
    WarningClass(
        key="devicepath_stringop_overflow",
        title="DevicePathUtilities stringop-overflow",
        pattern=re.compile(r"DevicePathUtilities\.c:.*-Wstringop-overflow"),
        disposition="triaged-upstream",
        note=(
            "Imported upstream edk2 warning in UefiDevicePathLib. Worth "
            "tracking, but not yet patched on the upstream-preserving path."
        ),
    ),
    WarningClass(
        key="platformconfig_duplicate_default",
        title="PlatformConfig duplicate default",
        pattern=re.compile(r"PlatformConfigHii\.i\(\d+\): WARNING: default"),
        disposition="custom-path-candidate",
        note=(
            "Radxa/CIX VFR checkbox default is specified twice. The warning is "
            "real, but removing the redundant default changes the generated "
            "IFR/HII payload, so it is not upstream-safe to patch here."
        ),
    ),
    WarningClass(
        key="iasl_warning",
        title="IASL / ACPI warning",
        pattern=re.compile(r"Warning\s+\d+\s+-"),
        disposition="needs-review",
        note=(
            "Vendor ACPI source warning from the current O6/CIX tables. These "
            "warnings were traced to Radxa/CIX ASL sources and should be "
            "reviewed table-by-table; many are not upstream-safe fixes because "
            "they would change emitted AML bytes or firmware behaviour."
        ),
    ),
    WarningClass(
        key="rwx_segment",
        title="RWX load segment",
        pattern=re.compile(r"LOAD segment with RWX permissions"),
        disposition="known-toolchain",
        note=(
            "Linker warning seen broadly across upstream and vendor debug DLL "
            "intermediates with the current firmware toolchain."
        ),
    ),
    WarningClass(
        key="lto_serial",
        title="LTO serialisation",
        pattern=re.compile(r"lto-wrapper: warning: using serial compilation"),
        disposition="informational",
        note=(
            "Performance warning from GCC LTO job scheduling. This is not a "
            "correctness failure."
        ),
    ),
)

IASL_CODE_DETAILS = {
    "3107": "reserved method must return a value",
    "3115": "not all control paths return a value",
    "3124": "switch expression defaults to Integer typing",
    "3134": "unreachable statement",
    "3144": "method local is set but never used",
    "3150": "empty resource template",
    "3175": "static OperationRegion declared inside control method",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Classify common edk2-cix build warnings from a captured build log."
    )
    parser.add_argument("log_file", type=pathlib.Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    lines = args.log_file.read_text(encoding="utf-8", errors="replace").splitlines()

    matched_indices: set[int] = set()
    classified: list[tuple[WarningClass, list[str]]] = []
    for warning_class in WARNING_CLASSES:
        hits = [
            line.rstrip()
            for index, line in enumerate(lines)
            if warning_class.pattern.search(line)
            and not (index in matched_indices or matched_indices.add(index))
        ]
        classified.append((warning_class, hits))

    other_warning_lines = []
    generic_warning = re.compile(r"\bwarning:|\bWARNING:")
    for index, line in enumerate(lines):
        if index in matched_indices:
            continue
        if generic_warning.search(line):
            other_warning_lines.append(line.rstrip())

    print("Known warning classes:")
    printed_any = False
    for warning_class, hits in classified:
        if not hits:
            continue
        printed_any = True
        print(
            f"- {warning_class.title}: {len(hits)} "
            f"({warning_class.disposition})"
        )
        print(f"  Note: {warning_class.note}")
        if warning_class.key == "iasl_warning":
            code_counts: collections.Counter[str] = collections.Counter()
            code_pattern = re.compile(r"Warning\s+(\d+)\s+-")
            for hit in hits:
                match = code_pattern.search(hit)
                if match:
                    code_counts[match.group(1)] += 1
            if code_counts:
                print("  Breakdown:")
                for code, count in sorted(
                    code_counts.items(), key=lambda item: (-item[1], item[0])
                ):
                    detail = IASL_CODE_DETAILS.get(code, "unclassified IASL warning")
                    print(f"    - {code}: {count} ({detail})")
        print(f"  Example: {hits[0]}")

    if not printed_any:
        print("- none detected")

    print()
    print("Other unmatched warning lines:")
    if other_warning_lines:
        preview = other_warning_lines[:10]
        for line in preview:
            print(f"- {line}")
        remaining = len(other_warning_lines) - len(preview)
        if remaining > 0:
            print(f"- ... plus {remaining} more unmatched warning line(s)")
    else:
        print("- none")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
