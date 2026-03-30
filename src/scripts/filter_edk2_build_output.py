#!/usr/bin/env python3
"""Reduce V=0 EDK2 build noise while preserving real failures."""

from __future__ import annotations

import os
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
PLATFORMCONFIG_DEFAULT_RE = re.compile(r"PlatformConfigHii\.i\(\d+\): WARNING: default")
PLATFORMCONFIG_CONTINUATION_RE = re.compile(r"^\s*: default value re-defined")
RWX_WARNING_RE = re.compile(r"LOAD segment with RWX permissions")
LTO_SERIAL_WARNING_RE = re.compile(r"^lto-wrapper: warning: using serial compilation")
LTO_SERIAL_NOTE_RE = re.compile(r"^lto-wrapper: note: see the .-flto. option documentation")
VFR_AMBIGUITY_RE = re.compile(r"^VfrSyntax\.g(?:, line \d+)?: warning: .*ambiguous upon ")
BUILDING_RE = re.compile(r"^Building \.\.\. (.+) \[([^\]]+)\]$")
SRC_PREFIX_RE = re.compile(r"^.*?/src/")

ARTEFACT_MODE = os.environ.get("ARTEFACT_MODE", "custom")


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


def rewrite_line(line: str) -> str:
    match = BUILDING_RE.match(line)
    if not match:
        return line
    path, arch = match.groups()
    path = SRC_PREFIX_RE.sub("", path, count=1)
    return f"Building {path} [{arch}]"


def main() -> int:
    buffered_iasl: list[str] = []
    suppress_platformconfig_continuation = False

    def flush_buffer() -> None:
        for buffered_line in buffered_iasl:
            print(buffered_line)
        buffered_iasl.clear()

    for raw_line in sys.stdin:
        line = raw_line.rstrip("\n")

        if suppress_platformconfig_continuation:
            suppress_platformconfig_continuation = False
            if PLATFORMCONFIG_CONTINUATION_RE.match(line):
                continue

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

        if ARTEFACT_MODE == "upstream" and PLATFORMCONFIG_DEFAULT_RE.search(line):
            suppress_platformconfig_continuation = True
            continue
        if (
            RWX_WARNING_RE.search(line)
            or LTO_SERIAL_WARNING_RE.search(line)
            or LTO_SERIAL_NOTE_RE.search(line)
            or VFR_AMBIGUITY_RE.search(line)
        ):
            continue

        if should_drop_line(line):
            continue

        print(rewrite_line(line))

    if buffered_iasl:
        flush_buffer()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
