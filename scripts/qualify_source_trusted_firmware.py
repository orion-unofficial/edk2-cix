#!/usr/bin/env python3
"""Compile curated trusted components and require their untrusted FIP to fail.

This is development compilation coverage, not production-image qualification.
Only the rejection report is suitable for CI publication.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

from firmware_chain import ChainError, validate_fip
from validate_firmware_chain import PACKAGE, load_catalog, preflight, reference


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worktree', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    worktree = args.worktree.resolve()
    package = worktree / PACKAGE
    selected = reference(package, load_catalog())
    try:
        preflight(package, selected, '1.2', 'custom')
    except ChainError as exc:
        if 'UEFI OEM key' not in str(exc):
            raise
    else:
        raise RuntimeError('expected incompatible Stage 3 signing policy was not rejected')
    records = []
    for fixes in ('false', 'true'):
        # Invoke the development component target only. Full-flash/OTA and
        # public build entry points independently reject this selection.
        subprocess.run([str(worktree / 'scripts/run_in_buildbox.sh'),
                        'make', '--no-print-directory', '-C', 'src',
                        'Build/O6/RELEASE_GCC/Firmwares/bootloader2.img',
                        'ARTEFACT_MODE=custom', 'CIX_RELEASE=1.2',
                        'FIRMWARE_BOARD=O6', 'FIRMWARE_TARGET=RELEASE',
                        'ENABLE_TF_A_FIXES=' + fixes],
                       cwd=worktree, env=dict(os.environ,
                                              EDK2_CIX_BUILDBOX_PLATFORM=os.environ.get('BUILDBOX_PLATFORM', 'linux/amd64'),
                                              EDK2_CIX_HOST_TMPDIR=os.environ.get('EDK2_CIX_HOST_TMPDIR', str(worktree / '.cache/edk2-cix/firmware/buildbox/tmp'))), check=True)
        image = worktree / 'src/Build/O6/RELEASE_GCC/Firmwares/bootloader2.img'
        try:
            validate_fip(image.read_bytes(), 'trusted', selected['trusted_root_spki_sha256'], selected['trusted_counter'])
        except ChainError as exc:
            if 'trust anchor' not in str(exc):
                raise RuntimeError('compiled FIP failed for an unexpected reason') from exc
            records.append({'tf_a_fixes': fixes, 'compilation': 'passed', 'flash_qualification': 'rejected', 'reason': str(exc)})
        else:
            raise RuntimeError('untrusted source-built FIP incorrectly passed vendor-chain qualification')
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({'status': 'expected-rejection-confirmed', 'variants': records}, indent=2) + '\n')


if __name__ == '__main__':
    main()
