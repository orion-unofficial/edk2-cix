#!/usr/bin/env python3
"""Reduce V=0 EDK2 build noise while preserving real failures."""

from __future__ import annotations

import re
import sys

IASL_LOC_RE = re.compile(r"\.iiii\s+\d+:")
IASL_SUMMARY_RE = re.compile(
    r"^Compilation (?:successful|failed)\.\s+([0-9]+)\s+Errors?,\s+([0-9]+)\s+Warnings?,\s+([0-9]+)\s+Remarks?"
)
IASL_COMMAND_RE = re.compile(r'(^|[/\s"])iasl(?:\.exe)?(?:"|\s)')
STRIP_NOOP_RE = re.compile(r'^"?echo"?\s+--strip-unneeded -R \.eh_frame ')
STRIP_FLAGS_RE = re.compile(r"^--strip-unneeded -R \.eh_frame ")
CONFIG_COPY_RE = re.compile(r"^Copying \$EDK_TOOLS_PATH/Conf/.+template$")
EDK2_ENV_RE = re.compile(
    r"^(WORKSPACE|PACKAGES_PATH|EDK_TOOLS_PATH|CONF_PATH|PYTHON_COMMAND|PREBUILD)\s*="
)
EDK2_META_RE = re.compile(
    r"^(Processing meta-data|Architecture\(s\)\s*=|Build target\s*=|Toolchain\s*=|Active Platform\s*=)"
)


def should_drop_line(line: str) -> bool:
    if IASL_COMMAND_RE.search(line):
        return True
    if "objcopy not needed for " in line:
        return True
    if STRIP_NOOP_RE.match(line):
        return True
    if STRIP_FLAGS_RE.match(line):
        return True
    if line.startswith("Intel ACPI Component Architecture"):
        return True
    if line.startswith("ASL+ Optimizing Compiler/Disassembler version "):
        return True
    if line.startswith("Copyright (c) 2000 - ") and line.endswith("Intel Corporation"):
        return True
    if line.startswith("ASL Input:"):
        return True
    if line.startswith("AML Output:"):
        return True
    if line.startswith("Loading previous configuration from "):
        return True
    if line == "Using EDK2 in-source Basetools":
        return True
    if EDK2_ENV_RE.match(line):
        return True
    if EDK2_META_RE.match(line):
        return True
    if re.fullmatch(r"\.* done!", line):
        return True
    if line.startswith("Build environment: "):
        return True
    if line.startswith("Build start time: "):
        return True
    if CONFIG_COPY_RE.match(line):
        return True
    if line.startswith("     to /") and "/Conf/" in line:
        return True
    if line.startswith("/") and line.endswith(".dll") and "Build/" in line and "/DEBUG/" in line:
        return True
    match = IASL_SUMMARY_RE.match(line)
    if match and int(match.group(1)) == 0:
        return True
    return False


def main() -> int:
    buffered_iasl: list[str] = []

    def flush_buffer() -> None:
        for buffered_line in buffered_iasl:
            print(buffered_line)
        buffered_iasl.clear()

    for raw_line in sys.stdin:
        line = raw_line.rstrip("\n")

        if buffered_iasl:
            buffered_iasl.append(line)
            summary_match = IASL_SUMMARY_RE.match(line)
            if summary_match:
                if int(summary_match.group(1)) != 0:
                    flush_buffer()
                else:
                    buffered_iasl.clear()
                continue
            continue

        if IASL_LOC_RE.search(line):
            buffered_iasl.append(line)
            continue

        if should_drop_line(line):
            continue

        print(line)

    if buffered_iasl:
        flush_buffer()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
