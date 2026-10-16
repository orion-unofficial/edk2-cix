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


def build(source: Path, cache: Path, compiler: str, jobs: int) -> tuple[dict, bytes]:
    source = source.resolve()
    vendor = source / 'CixStandaloneMmPkg/Vendor'
    manifest = json.loads(read_bounded(vendor / 'manifest.json', 65536))
    require(manifest.get('schema_version') == 1, 'unsupported hybrid vendor manifest')
    for name, expected in manifest['files'].items():
        require(Path(name).name == name and sha256(read_bounded(vendor / name, 1024 * 1024)) == expected,
                f'hybrid vendor component hash mismatch: {name}')
    inputs = {'source': tree_fingerprint(source, excluded_prefixes=('.git', '__pycache__')),
              'compiler': compiler_fingerprint(compiler + 'gcc'),
              'host_compiler': compiler_fingerprint('cc'),
              'helper': sha256(Path(__file__).read_bytes()), 'ffa_version': 12,
              'source_date_epoch': 0}
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
        shutil.copytree(source, workspace, symlinks=True)
        (output / 'tmp').mkdir(exist_ok=True)
        environment = dict(os.environ)
        for name in ('WORKSPACE', 'EDK_TOOLS_PATH', 'CONF_PATH', 'PACKAGES_PATH', 'BUILD_OUTPUT_BASE',
                     'MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES'):
            environment.pop(name, None)
        environment.update(WORKSPACE=str(workspace), TMPDIR=str(output / 'tmp'),
                           GCC_AARCH64_PREFIX=compiler, SOURCE_DATE_EPOCH='0', JOBS=str(jobs))
        command = '''set -eo pipefail
source edksetup.sh
make -C BaseTools -j"$JOBS"
build -p CixStandaloneMmPkg/CixStandaloneMm.dsc -a AARCH64 -t GCC -b RELEASE -n "$JOBS"
'''
        print('[cix-release] Building hybrid StandaloneMM', flush=True)
        with (output / 'build.log').open('w') as log:
            try:
                subprocess.run(['bash', '-c', command], cwd=workspace, env=environment,
                               stdout=log, stderr=log, check=True)
            except subprocess.CalledProcessError:
                print((output / 'build.log').read_text(), file=sys.stderr)
                raise
        payload = read_bounded(workspace / 'Build/StandaloneMm/RELEASE_GCC/FV/BL32_AP_MM.fd', 2 * 1024 * 1024)
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
    require(entry['radxa_release'] == '1.3.1', 'hybrid StandaloneMM currently supports Radxa 1.3.1 only')
    identity, payload = build(args.source, args.cache_root, args.cross_compile, args.jobs)
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
