#!/usr/bin/env python3
"""Compile both curated trusted-firmware variants and verify their CIX-rooted FIPs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from firmware_chain import Certificate, parse_fip, validate_fip
from validate_firmware_chain import CIX_SIGNING_KEYS, PACKAGE, load_catalog, preflight, reference


# This command executes in the rendered buildbox and exercises the same helper
# used by CIX_RELEASE=1.2 firmware builds.
QUALIFICATION_ROOT = Path('build-cache/trusted-component-qualification')
DELEGATED_KEYS = (
    ("trusted-key-cert", "302", "trusted_world_privatekey.pem"),
    ("soc-fw-key-cert", "501", "bl31_privatekey.pem"),
    ("tos-fw-key-cert", "901", "bl32_privatekey.pem"),
)
DEVELOPMENT_BUILD = r'''
build_root="$PWD/build-cache/trusted-component-qualification/$1"
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
    --signing-keys-dir "$PWD/custom/signing-keys/cix-1.2" \
    --fiptool "$PWD/src/tools/arm-trusted-firmware-fiptool/build/$host_arch/fiptool" \
    --output "$build_root/bootloader2.img" \
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
    preflight(package, selected, '1.2', 'custom')
    records = []
    for fixes in ('false', 'true'):
        # Verify the helper's real signed output before a full-image build.
        subprocess.run([str(worktree / 'scripts/run_in_buildbox.sh'),
                        'bash', '-euc', DEVELOPMENT_BUILD, 'trusted-component-check', fixes],
                       cwd=worktree, env=dict(os.environ,
                                              CIX_RELEASE='',
                                              EDK2_CIX_BUILDBOX_IMAGE=os.environ.get('EDK2_CIX_BUILDBOX_IMAGE') or f'mcr.microsoft.com/devcontainers/base:{args.distro}',
                                              EDK2_CIX_BUILDBOX_PLATFORM=os.environ.get('BUILDBOX_PLATFORM', 'linux/amd64'),
                                              EDK2_CIX_HOST_TMPDIR=os.environ.get('EDK2_CIX_HOST_TMPDIR', str(worktree / '.cache/edk2-cix/firmware/buildbox/tmp'))), check=True)
        image = worktree / QUALIFICATION_ROOT / fixes / 'bootloader2.img'
        result = validate_fip(image.read_bytes(), 'trusted',
                              selected['trusted_root_spki_sha256'], selected['trusted_counter'])
        entries = parse_fip(image.read_bytes())
        delegations = {}
        for certificate, extension, key_name in DELEGATED_KEYS:
            published_spki = subprocess.check_output(
                ['openssl', 'pkey', '-in', str(worktree / CIX_SIGNING_KEYS / key_name),
                 '-pubout', '-outform', 'DER', '-passin', 'pass:'], stderr=subprocess.DEVNULL)
            embedded_spki = Certificate(entries[certificate]).extension(extension)
            if embedded_spki != published_spki:
                raise RuntimeError(f'{certificate} is not delegated to the pinned {key_name}')
            delegations[certificate] = hashlib.sha256(embedded_spki).hexdigest()
        records.append({'tf_a_fixes': fixes, 'compilation': 'passed',
                        'certificate_chain': 'verified', 'image_size': image.stat().st_size,
                        'delegations': delegations, 'fip': result})
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({'status': 'verified', 'variants': records}, indent=2) + '\n')


if __name__ == '__main__':
    main()
