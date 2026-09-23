#!/usr/bin/env python3
"""Rebuild a custom FD to its measured compressed size before signing the FIP."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

from bl33_layout import limits
from debug_build_policy import fd_size

OVERFLOW = re.compile(r"the required fv image size (0x[0-9a-f]+) exceeds the set fv image size (0x[0-9a-f]+)", re.I)


def resize_fdf(text: str, size: int) -> str:
    if size <= 0 or size % 0x1000:
        raise ValueError("FD size must be a positive multiple of 4096")
    result, sizes = re.subn(r"(DEFINE\s+SKY1_BL33_UEFI_FD_SIZE\s*=\s*)0x[0-9a-f]+",
                            lambda m: m[1] + hex(size), text, flags=re.I)
    result, blocks = re.subn(r"(DEFINE\s+SKY1_BL33_UEFI_FD_BLOCKS\s*=\s*)0x[0-9a-f]+",
                             lambda m: m[1] + hex(size // 0x1000), result, flags=re.I)
    if sizes == blocks == 0:
        # The retained 202208/1.2.1 board uses the same single-FV layout with
        # literal lengths instead of TARGET conditionals.
        result, sizes = re.subn(r"(?m)^(Size\s*=\s*)0x[0-9a-f]+(?=\|gArmTokenSpaceGuid.PcdFdSize)",
                                lambda m: m[1] + hex(size), text, flags=re.I)
        result, blocks = re.subn(r"(?m)^(NumBlocks\s*=\s*)0x[0-9a-f]+(?=\s*$)",
                                 lambda m: m[1] + hex(size // 0x1000), result, count=1, flags=re.I)
        result, regions = re.subn(r"(?m)^0x00000000\|0x[0-9a-f]+(?=\s*\ngArmTokenSpaceGuid.PcdFvBaseAddress)",
                                  '0x00000000|' + hex(size), result, flags=re.I)
        if (sizes, blocks, regions) == (1, 1, 1):
            return result
    if sizes != 2 or blocks != 2:
        raise ValueError("unreviewed custom FD definition; cannot resize it safely")
    return result


def required_size(output: str, current: int) -> int | None:
    matches = OVERFLOW.findall(output)
    if len(matches) != 1 or "FVMAIN_COMPACT.inf" not in output:
        return None
    needed, actual = (int(value, 16) for value in matches[0])
    if actual != current or needed <= current:
        return None
    # Never interpret a compiler/module error as a reason to retry with more FV.
    if re.search(r":\d+(?::\d+)?:\s+(?:fatal )?error:|error F002|Failed to build module", output):
        return None
    return (needed + 0xFFF) & ~0xFFF


def build(command: list[str], fdf: Path, target: str, workspace: Path, maximum: int) -> int:
    text = fdf.read_text()
    original_size = fd_size(text, target)
    # DEBUG retains all DEBUG code, but its fixed 4MiB padding is not a minimum
    # requirement. Start at the ordinary RELEASE capacity and measure it too.
    size = fd_size(text, "RELEASE") if target == "DEBUG" else original_size
    root = workspace / "bl33-sizing"
    root.mkdir(parents=True, exist_ok=True)
    attempts = []
    # A changed FD-size PCD can alter compression; remeasure, do not assume a
    # single resize suffices. Each retry grows by at least one 4KiB block.
    for attempt in range(1, 9):
        if size > maximum:
            raise ValueError(f"compressed BL33 needs an FD of at least 0x{size:x}; "
                             f"the supported flash space is only 0x{maximum:x}, before FIP overhead")
        args = list(command)
        if size != original_size:
            generated = root / f"{fdf.stem}-{size:x}.fdf"
            generated.write_text(resize_fdf(text, size))
            args += ["-f", str(generated)]
        log = root / f"attempt-{attempt}.log"
        with log.open("w") as stream, subprocess.Popen(
                args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, errors="replace") as process:
            assert process.stdout is not None
            for line in process.stdout:
                stream.write(line)
                print(line, end="", flush=True)
            result = process.wait()
        output = log.read_text()
        attempts.append({"fd_size": size, "command": args, "returncode": result, "log": str(log)})
        (root / "attempts.json").write_text(json.dumps(attempts, indent=2) + "\n")
        if result == 0:
            return 0
        needed = required_size(output, size)
        if needed is None:
            return result
        print(f"[bl33-size] Measured FV overflow; rebuilding with FD 0x{needed:x}. "
              "Final signed-FIP size and layout consent are checked before packaging.", flush=True)
        size = needed
    raise ValueError("BL33 compressed size did not converge after eight measured retries")


def main() -> None:
    from firmware_chain import ChainError
    from validate_firmware_chain import load_catalog, reference
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--board", required=True)
    parser.add_argument("--target", choices=("DEBUG", "RELEASE"), required=True)
    parser.add_argument("--reference-dir", type=Path, required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    try:
        if not command:
            raise ValueError("missing EDK2 build command")
        relative = Path(f"Platform/Radxa/Orion/{args.board}/{args.board}.fdf")
        paths = [Path(path) / relative for path in os.environ["PACKAGES_PATH"].split(os.pathsep)]
        fdf = next((path for path in paths if path.is_file()), None)
        if fdf is None:
            raise ValueError("cannot resolve the selected board FDF")
        selected = reference(args.reference_dir, load_catalog())
        sys.exit(build(command, fdf, args.target, Path(os.environ["WORKSPACE"]),
                       limits(selected)["maximum_fd_size"]))
    except (ChainError, OSError, ValueError) as exc:
        parser.exit(2, f"[bl33-size] REJECTED: {exc}\n")


if __name__ == "__main__":
    main()
