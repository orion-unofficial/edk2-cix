#!/usr/bin/env python3
"""Verify that requested RELEASE diagnostics survived compilation."""

import argparse
from pathlib import Path


BDS_MARKERS = (
    b"[BDS] ConnectDeviceClass begin",
    b"[BDS] RefreshAllBootOption begin",
    b"[BDS] BootLogoEnableLogo begin",
    b"[BDS] BootDiscoveryPolicyHandler begin",
    b"[BDS] HandleCapsules begin",
)


def check(build_dir, error_level):
    # Targeted progress markers retain INIT and also accept the boot-manager bit.
    if not error_level & 0x401:
        return False
    image = (build_dir / "AARCH64/BdsDxe.efi").read_bytes()
    missing = [marker.decode() for marker in BDS_MARKERS if marker not in image]
    if missing:
        raise ValueError("RELEASE diagnostics were compiled out: " + ", ".join(missing))
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--error-level", type=lambda value: int(value, 0), required=True)
    args = parser.parse_args()
    try:
        verified = check(args.build_dir, args.error_level)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"[release-debug] {exc}\n")
    if verified:
        print("[release-debug] Requested BDS diagnostics verified in the compiled EFI")


if __name__ == "__main__":
    main()
