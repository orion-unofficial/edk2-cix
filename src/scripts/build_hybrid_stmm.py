#!/usr/bin/env python3
"""Build and verify the source-selected hybrid StandaloneMM inside the buildbox."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

# This helper is also installed at src/scripts in rendered firmware trees.
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from cix_release_cache import compiler_fingerprint, tree_fingerprint  # noqa: E402
from stage_vendor_stmm import load_payload, read_bounded, require, sha256, verify_embedded, write_changed  # noqa: E402


def vendor_directory(source: Path, release: str) -> Path:
    require(release in {'1.2.4', '1.3.1'}, 'hybrid StandaloneMM supports Radxa 1.2.4 or 1.3.1 only')
    vendor = source / 'CixStandaloneMmPkg/Vendor'
    return vendor / release if release == '1.2.4' else vendor


def build(source: Path, cache: Path, compiler: str, jobs: int,
          release: str = '1.3.1', *, edk2_source: Path | None = None,
          native_dsc: Path | None = None, edk2_release: str | None = None) -> tuple[dict, bytes]:
    source = source.resolve()
    native = edk2_source is not None
    native_options = (edk2_source, native_dsc, edk2_release)
    require(all(value is not None for value in native_options) or all(value is None for value in native_options),
            'release-native builds require EDK2 source, platform DSC and release together')
    vendor = vendor_directory(source, release)
    manifest = json.loads(read_bounded(vendor / 'manifest.json', 65536))
    require(manifest.get('schema_version') == 1, 'unsupported hybrid vendor manifest')
    require(manifest.get('radxa_release') == release, 'hybrid vendor release mismatch')
    for name, expected in manifest['files'].items():
        require(Path(name).name == name and sha256(read_bounded(vendor / name, 1024 * 1024)) == expected,
                f'hybrid vendor component hash mismatch: {name}')
    inputs = {'source': tree_fingerprint(source, excluded_prefixes=('.git', '__pycache__')),
              'compiler': compiler_fingerprint(compiler + 'gcc'),
              'host_compiler': compiler_fingerprint('cc'),
              'helper': sha256(Path(__file__).read_bytes()), 'ffa_version': 12,
              'source_date_epoch': 0, 'radxa_release': release}
    toolchain = 'GCC'
    if native:
        from stmm_release_platform import configure
        from stmm_release_port import CRYPTO_PATH, apply as apply_ports
        require(edk2_release in {'202208', '202411', '202502', '202608'},
                'release-native StandaloneMM release has not entered qualification')
        inputs.update(edk2_release=edk2_release,
                      edk2_source=tree_fingerprint(edk2_source, excluded_prefixes=('.git', 'Build', 'Conf'),
                                                   excluded_suffixes=('.pyc',)),
                      native_dsc=sha256(native_dsc.read_bytes()),
                      native_helpers={name: sha256(Path(__file__).with_name(name).read_bytes())
                                      for name in ('stmm_release_platform.py', 'stmm_release_port.py')},
                      ffa_version=10 if edk2_release < '202502' else 12)
    key = sha256(json.dumps(inputs, sort_keys=True).encode())
    output = cache.resolve() / key
    fd = output / 'BL32_AP_MM.fd'
    receipt = output / 'build.json'
    if not (fd.is_file() and receipt.is_file() and
            sha256(fd.read_bytes()) == json.loads(receipt.read_text()).get('fd_sha256')):
        output.mkdir(parents=True, exist_ok=True)
        workspace = output / 'source'
        if workspace.exists():
            shutil.rmtree(workspace)
        if native:
            def ignore_generated(directory, names):
                excluded = {name for name in names if name.endswith('.pyc')}
                if Path(directory) == edk2_source:
                    excluded.update(set(names) & {'.git', 'Build', 'Conf'})
                return excluded

            shutil.copytree(edk2_source, workspace, symlinks=True,
                            ignore=ignore_generated)
            shutil.copytree(source / 'CixStandaloneMmPkg', workspace / 'CixStandaloneMmPkg', symlinks=True)
            profile = configure(workspace, native_dsc.read_bytes(), release, edk2_release)
            crypto = (source / CRYPTO_PATH).read_bytes() if edk2_release == '202608' else None
            profile['source_adaptations'] = apply_ports(workspace, edk2_release, crypto)
            toolchain = profile['toolchain']
            write_changed(output / 'native-profile.json', (json.dumps(profile, indent=2) + '\n').encode())
        else:
            shutil.copytree(source, workspace, symlinks=True)
        # BaseTools/BuildEnv writes BuildEnv.sh before copying its templates.
        # Native staging omits generated Conf contents, but setup needs the directory.
        (workspace / 'Conf').mkdir(exist_ok=True)
        (output / 'tmp').mkdir(exist_ok=True)
        environment = dict(os.environ)
        for name in ('WORKSPACE', 'EDK_TOOLS_PATH', 'CONF_PATH', 'PACKAGES_PATH', 'BUILD_OUTPUT_BASE',
                     'MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES'):
            environment.pop(name, None)
        environment.update(WORKSPACE=str(workspace), TMPDIR=str(output / 'tmp'),
                           GCC_AARCH64_PREFIX=compiler, GCC5_AARCH64_PREFIX=compiler,
                           TOOLCHAIN=toolchain, SOURCE_DATE_EPOCH='0', JOBS=str(jobs),
                           CIX_STMM_VENDOR_PATH=str(vendor.relative_to(source / 'CixStandaloneMmPkg')))
        command = '''set -eo pipefail
source edksetup.sh
make -C BaseTools -j"$JOBS"
build -p CixStandaloneMmPkg/CixStandaloneMm.dsc -a AARCH64 -t "$TOOLCHAIN" -b RELEASE -n "$JOBS" -D CIX_STMM_VENDOR_PATH="$CIX_STMM_VENDOR_PATH"
'''
        print('[cix-release] Building hybrid StandaloneMM', flush=True)
        with (output / 'build.log').open('w') as log:
            try:
                subprocess.run(['bash', '-c', command], cwd=workspace, env=environment,
                               stdout=log, stderr=log, check=True)
            except subprocess.CalledProcessError:
                print((output / 'build.log').read_text(), file=sys.stderr)
                raise
        payload = read_bounded(workspace / f'Build/StandaloneMm/RELEASE_{toolchain}/FV/BL32_AP_MM.fd', 2 * 1024 * 1024)
        write_changed(fd, payload)
        write_changed(receipt, (json.dumps(dict(inputs, fd_sha256=sha256(payload)), indent=2) + '\n').encode())
        shutil.rmtree(workspace)
    payload = read_bounded(fd, 2 * 1024 * 1024)
    require(len(payload) == 2 * 1024 * 1024, 'unexpected hybrid StandaloneMM size')
    for name in manifest['files']:
        if name.endswith('.efi'):
            require(payload.count((vendor / name).read_bytes()) == 1,
                    f'hybrid must contain exactly one unchanged vendor executable: {name}')
    return dict(inputs, fd_sha256=sha256(payload), fd_size=len(payload), vendor=manifest), payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--edk2-source', type=Path)
    parser.add_argument('--native-dsc', type=Path)
    parser.add_argument('--edk2-release')
    parser.add_argument('--cache-root', type=Path, required=True)
    parser.add_argument('--cross-compile', default='aarch64-linux-gnu-')
    parser.add_argument('--jobs', type=int, default=1)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--vendor-fip', type=Path, required=True)
    operation = parser.add_mutually_exclusive_group(required=True)
    operation.add_argument('--output', type=Path)
    operation.add_argument('--verify-tee', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    require(args.jobs > 0, 'jobs must be positive')
    entry, _ = load_payload(args.manifest, args.vendor_fip)
    release = entry['radxa_release']
    vendor = vendor_directory(args.source, release)
    manifest = json.loads(read_bounded(vendor / 'manifest.json', 65536))
    require(manifest.get('source_fd_sha256') == entry['fd_sha256'],
            'hybrid vendor services do not match the selected vendor StandaloneMM image')
    identity, payload = build(args.source, args.cache_root, args.cross_compile, args.jobs, release,
                              edk2_source=args.edk2_source, native_dsc=args.native_dsc,
                              edk2_release=args.edk2_release)
    report = {'schema_version': 1, 'status': 'verified', 'selection': 'unofficial', 'standalone_mm': identity}
    if args.output:
        write_changed(args.output, payload)
    else:
        tee = read_bounded(args.verify_tee, 8 * 1024 * 1024)
        report['embedded_offset'] = verify_embedded(tee, payload)
        report['source_built_tee_sha256'] = sha256(tee)
    if args.report:
        write_changed(args.report, (json.dumps(report, indent=2, sort_keys=True) + '\n').encode())
    print(f"[cix-release] Verified hybrid StandaloneMM {identity['fd_sha256']}")


if __name__ == '__main__':
    main()
