#!/usr/bin/env python3
"""Resolve the reviewed O6 DSDT CPU reference-performance conflict.

This is a pure source transform for the release-expansion host harness.  It
accepts only the 12 known CPU CPPC conflicts and returns only the reviewed
whole-file result.  Callers decide how to record a resolution in their private
repository; this module does not read or update Git refs.
"""

from __future__ import annotations

import hashlib
import re


DSDT_CPU_PATH = (
    "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/"
    "AcpiSocTables/Dsdt-CPU.asl"
)
ACCEPTED_SHA256 = "96eae7036838cadd3dbb9418bc2b7be0428c4f9b083f1e02ecd702b64fcd0bb3"
ACCEPTED_GIT_BLOB = "2d85fa826dbf41e92ee86e4af858d8924a7cf42a"

OLD_MACROS = (
    b"#ifdef ENABLE_FIRMWARE_FIXES\n"
    b"#define CIX_A720_REF_PERF  0x0C4E\n"
    b"#define CIX_A520_REF_PERF  0x04D8\n"
    b"#else\n"
    b"#define CIX_A720_REF_PERF  REF_PERF\n"
    b"#define CIX_A520_REF_PERF  REF_PERF\n"
    b"#endif\n"
)

# Preserve the exact reviewed spellings and spacing.  Fixes-off retains the
# vendor's cluster references while fixes-on follows the O6 core topology.
RESOLVED_MACROS = (
    b"#ifdef ENABLE_FIRMWARE_FIXES\n"
    b"#define CIX_A720_REF_PERF  0x0C4E\n"
    b"#define CIX_A520_REF_PERF  0x04D8\n"
    b"#define CIX_CPU_0_1_REF_PERF    CIX_A720_REF_PERF\n"
    b"#define CIX_CPU_2_3_REF_PERF    CIX_A520_REF_PERF\n"
    b"#define CIX_CPU_4_5_REF_PERF    CIX_A520_REF_PERF\n"
    b"#define CIX_CPU_6_7_REF_PERF    CIX_A720_REF_PERF\n"
    b"#define CIX_CPU_8_9_REF_PERF    CIX_A720_REF_PERF\n"
    b"#define CIX_CPU_10_11_REF_PERF  CIX_A720_REF_PERF\n"
    b"#else\n"
    b"#define CIX_CPU_0_1_REF_PERF    CORE_0_TO_3_REF_PERF\n"
    b"#define CIX_CPU_2_3_REF_PERF    CORE_0_TO_3_REF_PERF\n"
    b"#define CIX_CPU_4_5_REF_PERF    CORE_4_5_REF_PERF\n"
    b"#define CIX_CPU_6_7_REF_PERF    CORE_6_7_REF_PERF\n"
    b"#define CIX_CPU_8_9_REF_PERF    CORE_8_9_REF_PERF\n"
    b"#define CIX_CPU_10_11_REF_PERF  CORE_10_11_REF_PERF\n"
    b"#endif\n"
)

_MACRO_RE = re.compile(rb"(?m)^#ifdef ENABLE_FIRMWARE_FIXES\n.*?^#endif\n", re.S)
_CALL_RE = re.compile(rb"^(\s*CPPC_PACKAGE_INIT\s*\(.*,)\s*([A-Z0-9_]+)\)(\s*)$")
_DEVICE_RE = re.compile(rb"Device \((?:CPU|CP)(\d+)\)")
_VENDOR_REF = (
    b"CORE_0_TO_3_REF_PERF", b"CORE_0_TO_3_REF_PERF",
    b"CORE_0_TO_3_REF_PERF", b"CORE_0_TO_3_REF_PERF",
    b"CORE_4_5_REF_PERF", b"CORE_4_5_REF_PERF",
    b"CORE_6_7_REF_PERF", b"CORE_6_7_REF_PERF",
    b"CORE_8_9_REF_PERF", b"CORE_8_9_REF_PERF",
    b"CORE_10_11_REF_PERF", b"CORE_10_11_REF_PERF",
)
_FIXED_REF = (
    b"CIX_A720_REF_PERF", b"CIX_A720_REF_PERF",
    b"CIX_A520_REF_PERF", b"CIX_A520_REF_PERF",
    b"CIX_A520_REF_PERF", b"CIX_A520_REF_PERF",
    b"CIX_A720_REF_PERF", b"CIX_A720_REF_PERF",
    b"CIX_A720_REF_PERF", b"CIX_A720_REF_PERF",
    b"CIX_A720_REF_PERF", b"CIX_A720_REF_PERF",
)


class DsdtConflictError(ValueError):
    """The input is not the reviewed DSDT CPU conflict."""


def _git_blob_id(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()


def resolve_dsdt_cpu_conflict(raw: bytes) -> bytes:
    """Return reviewed DSDT bytes, rejecting every other conflict or file delta."""
    if not isinstance(raw, bytes):
        raise TypeError("DSDT source must be bytes")
    if b"\r" in raw:
        raise DsdtConflictError("unexpected DSDT line ending")

    output: list[bytes] = []
    state = "normal"
    cpu: int | None = None
    seen: set[int] = set()
    ours = theirs = b""
    groups = 0
    for line in raw.splitlines(keepends=True):
        if state == "normal":
            device = _DEVICE_RE.search(line)
            if device:
                cpu = int(device.group(1))
        if line.startswith(b"<<<<<<< "):
            if state != "normal":
                raise DsdtConflictError("nested conflict marker")
            state, ours, theirs = "ours", b"", b""
            groups += 1
        elif line == b"=======\n":
            if state != "ours":
                raise DsdtConflictError("unexpected conflict separator")
            state = "theirs"
        elif line.startswith(b">>>>>>> "):
            if state != "theirs" or not line.endswith(b"\n"):
                raise DsdtConflictError("unexpected conflict close")
            ours_call = _CALL_RE.fullmatch(ours.rstrip(b"\n"))
            theirs_call = _CALL_RE.fullmatch(theirs.rstrip(b"\n"))
            if (cpu is None or cpu not in range(12) or ours_call is None or
                    theirs_call is None):
                raise DsdtConflictError("conflict is not one CPU0..CPU11 CPPC call")
            if cpu in seen:
                raise DsdtConflictError(f"duplicate CPU{cpu} conflict")
            if (ours_call.group(1), ours_call.group(3)) != (theirs_call.group(1), theirs_call.group(3)):
                raise DsdtConflictError(f"CPU{cpu} variants differ beyond the reference symbol")
            if (ours_call.group(2), theirs_call.group(2)) != (_FIXED_REF[cpu], _VENDOR_REF[cpu]):
                raise DsdtConflictError(f"CPU{cpu} reference mapping differs")
            cluster = (cpu // 2) * 2
            output.append(ours_call.group(1) + b" CIX_CPU_" + str(cluster).encode("ascii") +
                          b"_" + str(cluster + 1).encode("ascii") + b"_REF_PERF)" +
                          ours_call.group(3) + b"\n")
            seen.add(cpu)
            state = "normal"
        elif state == "ours":
            ours += line
        elif state == "theirs":
            theirs += line
        else:
            output.append(line)

    if state != "normal" or groups != 12 or seen != set(range(12)):
        raise DsdtConflictError("expected exactly one complete conflict for each CPU0..CPU11")
    resolved = b"".join(output)
    if _MACRO_RE.findall(resolved) != [OLD_MACROS]:
        raise DsdtConflictError("legacy macro block differs from reviewed input")
    resolved = resolved.replace(OLD_MACROS, RESOLVED_MACROS, 1)
    if any(marker in resolved for marker in (b"<<<<<<<", b"=======", b">>>>>>>")):
        raise DsdtConflictError("unconsumed conflict marker")
    if (hashlib.sha256(resolved).hexdigest() != ACCEPTED_SHA256 or
            _git_blob_id(resolved) != ACCEPTED_GIT_BLOB):
        raise DsdtConflictError("resolved DSDT differs from reviewed whole-file identity")
    return resolved
