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
DEBUGLINK_NOOP_RE = re.compile(r'^"?echo"?\s+--add-gnu-debuglink=')
DEBUGLINK_FLAGS_RE = re.compile(r"^--add-gnu-debuglink=")
CONFIG_COPY_RE = re.compile(r"^Copying \$EDK_TOOLS_PATH/Conf/.+template$")
EDK2_ENV_RE = re.compile(
    r"^(WORKSPACE|PACKAGES_PATH|EDK_TOOLS_PATH|CONF_PATH|PYTHON_COMMAND|PREBUILD)\s*="
)
EDK2_META_RE = re.compile(
    r"^[.]*\s*(Processing meta-data|Architecture\(s\)\s*=|Build target\s*=|Toolchain\s*=|Active Platform\s*=)"
)
PLATFORMCONFIG_DEFAULT_RE = re.compile(r"PlatformConfigHii\.i\(\d+\): WARNING: default")
PLATFORMCONFIG_CONTINUATION_RE = re.compile(r"^\s*: default value re-defined")
RWX_WARNING_RE = re.compile(r"LOAD segment with RWX permissions")
LTO_SERIAL_WARNING_RE = re.compile(r"^lto-wrapper: warning: using serial compilation")
LTO_SERIAL_NOTE_RE = re.compile(r"^lto-wrapper: note: see the .-flto. option documentation")
VFR_AMBIGUITY_RE = re.compile(r"^VfrSyntax\.g(?:, line \d+)?: warning: .*ambiguous upon ")
BUILDING_RE = re.compile(r"^Building \.\.\. (.+) \[([^\]]+)\]$")
SRC_PREFIX_RE = re.compile(r"^.*?/src/")
PROGRESS_LINE_RE = re.compile(r"^[.#]+(?: done!)?$")

ARTEFACT_MODE = os.environ.get("ARTEFACT_MODE", "custom")
VERBOSE = os.environ.get("V", os.environ.get("EDK2_CIX_VERBOSE", "0")) == "1"
QUIET_FILTERING = not VERBOSE
SUPPRESS_WARNINGS = ARTEFACT_MODE == "upstream" and not VERBOSE


def should_drop_line(line: str) -> bool:
    if IASL_COMMAND_RE.search(line):
        return True
    if "objcopy not needed for " in line:
        return True
    if STRIP_NOOP_RE.match(line):
        return True
    if STRIP_FLAGS_RE.match(line):
        return True
    if DEBUGLINK_NOOP_RE.match(line):
        return True
    if DEBUGLINK_FLAGS_RE.match(line):
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
    if PROGRESS_LINE_RE.match(line):
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
    if match:
        errors = int(match.group(1))
        warnings = int(match.group(2))
        remarks = int(match.group(3))
        if errors == 0 and warnings == 0 and remarks == 0:
            return True
    return False


def rewrite_line(line: str) -> str:
    match = BUILDING_RE.match(line)
    if not match:
        return line
    path, arch = match.groups()
    path = SRC_PREFIX_RE.sub("", path, count=1)
    return f"Building {path} [{arch}]"


def prefers_no_blank_before(line: str) -> bool:
    return (
        not line
        or line.startswith("Building ")
        or line.startswith("VfrCompile...")
        or line.startswith(".Architecture")
        or PROGRESS_LINE_RE.match(line) is not None
    )


def is_warning_line(line: str) -> bool:
    return "warning:" in line or "WARNING:" in line or "Warnings," in line


def main() -> int:
    if not QUIET_FILTERING:
        for raw_line in sys.stdin:
            sys.stdout.write(raw_line)
        return 0

    buffered_iasl: list[str] = []
    suppress_platformconfig_continuation = False
    pending_blank = False

    def flush_buffer() -> None:
        for buffered_line in buffered_iasl:
            print(buffered_line)
        buffered_iasl.clear()

    def emit_line(line: str) -> None:
        nonlocal pending_blank
        if not line:
            pending_blank = True
            return
        if pending_blank and not prefers_no_blank_before(line):
            print()
        pending_blank = False
        print(line)

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
                errors = int(summary_match.group(1))
                warnings = int(summary_match.group(2))
                remarks = int(summary_match.group(3))
                if errors != 0:
                    flush_buffer()
                elif SUPPRESS_WARNINGS:
                    buffered_iasl.clear()
                elif warnings != 0 or remarks != 0:
                    flush_buffer()
                else:
                    buffered_iasl.clear()
                continue
            continue

        if IASL_LOC_RE.search(line):
            buffered_iasl.append(line)
            continue

        if SUPPRESS_WARNINGS:
            if PLATFORMCONFIG_DEFAULT_RE.search(line):
                suppress_platformconfig_continuation = True
                continue
            if (
                RWX_WARNING_RE.search(line)
                or LTO_SERIAL_WARNING_RE.search(line)
                or LTO_SERIAL_NOTE_RE.search(line)
                or VFR_AMBIGUITY_RE.search(line)
                or is_warning_line(line)
            ):
                continue

        if should_drop_line(line):
            continue

        emit_line(rewrite_line(line))

    if buffered_iasl:
        flush_buffer()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
