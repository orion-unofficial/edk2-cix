#!/usr/bin/env python3
"""Describe the debug categories from the requested source checkpoint."""
import os
from pathlib import Path

from debug_build_policy import HEADER, debug_bits
from reconstruction_common import main_wrapper, release_entry, show_file


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    target, entry = release_entry(root, os.environ.get("RELEASE") or None)
    bits = debug_bits(show_file(root, entry["source_ref"], HEADER).decode())
    maximum = 0
    print(f"Debug categories for {target}:")
    for name, bit in bits.items():
        maximum |= bit
        print(f"  0x{bit:08X}  {name}")
    print(f"DEBUG_VERBOSE=false: default mask 0x80000040 (ordinary RELEASE gating).\n"
          f"DEBUG_VERBOSE=true: default mask 0x{maximum:08X} (logging only in RELEASE).\n"
          "FD capacity is adjusted from measured compressed size; the signed FIP is checked before packaging.\n"
          "DEBUG_ALLOW_LARGE_IMAGE=1 permits the audited larger slot only when needed.\n"
          "Unused category diagnostics warn without removing bits. All size and signing checks remain mandatory.")


if __name__ == "__main__":
    main_wrapper(main)
