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


# This command executes in the rendered buildbox. Keep compilation coverage of
# the unavailable signing path without bypassing any public packaging guard.
DEVELOPMENT_ROOT = Path('build-cache/untrusted-component-qualification')
DEVELOPMENT_BUILD = r'''
build_root="$PWD/build-cache/untrusted-component-qualification/$1"
package="$PWD/src/edk2-non-osi/Platform/CIX/Sky1/PackageTool"
mkdir -p "$build_root"
for directory in Keys certs Firmwares; do
    cp -a "$package/$directory" "$build_root/"
done
host_arch="$(uname -m)"
cross_compile=
if [[ "$host_arch" != aarch64 ]]; then
    cross_compile=aarch64-linux-gnu-
fi
make --no-print-directory -C src/tools/arm-trusted-firmware-fiptool HOST_ARCH="$host_arch" all
fix_args=()
if [[ "$1" == true ]]; then
    fix_args+=(--enable-tf-a-fixes)
fi
bash src/scripts/build_cix_release_bootloader2.sh \
    --tfa-dir "$PWD/src/cix-v1.2/tf-a" \
    --tee-dir "$PWD/src/cix-v1.2/tee" \
    --build-root "$build_root" \
    --fiptool "$PWD/src/tools/arm-trusted-firmware-fiptool/build/$host_arch/fiptool" \
    --output "$build_root/bootloader2-untrusted.img" \
    --cross-compile "$cross_compile" --jobs "$(nproc)" \
    --cache-root "$PWD/build-cache/cix-release" "${fix_args[@]}"
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worktree', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--distro', choices=('bookworm', 'trixie'),
                        default=os.environ.get('FIRMWARE_DISTRO', 'trixie'))
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
        # Invoke the retained helper directly, outside every firmware Make target.
        # Its private output directory is never mirrored as usable firmware.
        subprocess.run([str(worktree / 'scripts/run_in_buildbox.sh'),
                        'bash', '-euc', DEVELOPMENT_BUILD, 'trusted-component-check', fixes],
                       cwd=worktree, env=dict(os.environ,
                                              CIX_RELEASE='',
                                              EDK2_CIX_BUILDBOX_IMAGE=os.environ.get('EDK2_CIX_BUILDBOX_IMAGE') or f'mcr.microsoft.com/devcontainers/base:{args.distro}',
                                              EDK2_CIX_BUILDBOX_PLATFORM=os.environ.get('BUILDBOX_PLATFORM', 'linux/amd64'),
                                              EDK2_CIX_HOST_TMPDIR=os.environ.get('EDK2_CIX_HOST_TMPDIR', str(worktree / '.cache/edk2-cix/firmware/buildbox/tmp'))), check=True)
        image = worktree / DEVELOPMENT_ROOT / fixes / 'bootloader2-untrusted.img'
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
