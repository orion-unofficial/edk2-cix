#!/usr/bin/env python3
"""Run a durable release-native StandaloneMM compilation matrix.

Compilation is the first qualification stage. No production selector or source
ref is advanced by this command; successful cases remain built-not-qualified.
Run in a session-owned durable directory. Re-running resumes completed cases
only when the complete frozen plan still matches. Failed execution state is
preserved; repaired candidates require a new output directory.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from reconstruction_common import release_entry, resolve_ref
from stmm_release_platform import NATIVE_DSC, configure
from stmm_release_port import CRYPTO_PATH, apply as apply_ports
from stmm_vendor_control import configure as configure_control


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def save(path: Path, value: dict) -> None:
    temporary = path.with_suffix(path.suffix + '.new')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def git(repo: Path, *args: str) -> bytes:
    return subprocess.check_output(['git', '-C', str(repo), *args])


def archive(repo: Path, tree: str, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=False)
    process = subprocess.Popen(['git', '-C', str(repo), 'archive', tree], stdout=subprocess.PIPE)
    try:
        subprocess.run(['tar', '-xf', '-', '-C', str(destination)], stdin=process.stdout, check=True)
    finally:
        process.stdout.close()
        status = process.wait()
    if status:
        raise RuntimeError('source archive failed')


def compile_case(repo: Path, case: dict, directory: Path, image: str, jobs: int,
                 control_fd: bytes | None = None) -> dict:
    source = directory / 'source'
    if not source.exists():
        archive(repo, case['edk2_tree'], source)
        archive(repo, case['platform_tree'], source / 'CixStandaloneMmPkg')
    native = git(repo, 'show', case['firmware_commit'] + ':' + NATIVE_DSC)
    profile = configure(source, native, case['radxa'], case['edk2'])
    crypto = git(repo, 'cat-file', 'blob', case['crypto_header']) if case.get('crypto_header') else None
    profile['source_adaptations'] = apply_ports(source, case['edk2'], crypto)
    save(directory / 'profile.json', profile)
    # Fail closed if dependencies have not been materialised by the firmware tree.
    entries = git(repo, 'ls-tree', '-r', case['edk2_tree']).decode().splitlines()
    if any(line.startswith('160000 ') for line in entries):
        raise ValueError('selected EDK2 tree contains unresolved dependencies')
    vendor = source / 'CixStandaloneMmPkg' / profile['vendor_directory']
    manifest = json.loads((vendor / 'manifest.json').read_text())
    if manifest['radxa_release'] != case['radxa']:
        raise ValueError('vendor release differs from selected firmware')
    for name, expected in manifest['files'].items():
        if Path(name).name != name or digest((vendor / name).read_bytes()) != expected:
            raise ValueError('vendor component hash mismatch: ' + name)
    control_files = configure_control(source, control_fd, manifest) if control_fd is not None else {}
    command = '''set -eo pipefail
export WORKSPACE=/work/source TMPDIR=/work/tmp SOURCE_DATE_EPOCH=0
export GCC_AARCH64_PREFIX=aarch64-linux-gnu- GCC5_AARCH64_PREFIX=aarch64-linux-gnu-
cd "$WORKSPACE"
source edksetup.sh
make -C BaseTools -j"$JOBS"
build -p CixStandaloneMmPkg/CixStandaloneMm.dsc -a AARCH64 -t "$TOOLCHAIN" -b RELEASE -n "$JOBS"
aarch64-linux-gnu-gcc --version > /work/compiler.txt
'''
    (directory / 'tmp').mkdir(exist_ok=True)
    name = 'stmm-release-' + digest(str(directory).encode())[:16]
    with (directory / 'build.log').open('w') as log:
        subprocess.run(['docker', 'run', '--rm', '--name', name, '--network', 'none',
                        '--entrypoint', 'bash', '-v', str(directory) + ':/work',
                        '-e', f'JOBS={jobs}', '-e', 'TOOLCHAIN=' + profile['toolchain'],
                        image, '-c', command], stdout=log, stderr=log, check=True)
    fd = source / f"Build/StandaloneMm/RELEASE_{profile['toolchain']}/FV/BL32_AP_MM.fd"
    payload = fd.read_bytes()
    if len(payload) != 2 * 1024 * 1024:
        raise ValueError('unexpected StandaloneMM image size')
    for name in manifest['files']:
        if name.endswith('.efi') and payload.count((vendor / name).read_bytes()) != 1:
            raise ValueError('retained executable changed: ' + name)
    for name, expected in control_files.items():
        content = (source / 'CixStandaloneMmPkg/QualificationVendorStorage' / name).read_bytes()
        if digest(content) != expected or (name.endswith('.efi') and payload.count(content) != 1):
            raise ValueError('vendor control executable changed: ' + name)
    shutil.copyfile(fd, directory / 'BL32_AP_MM.fd')
    return {'status': 'built-not-qualified', 'fd_sha256': digest(payload),
            'profile': profile, 'vendor': manifest, 'control_storage': control_files,
            'compiler': (directory / 'compiler.txt').read_text()}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--image', required=True)
    parser.add_argument('--edk2', nargs='+', default=['202208', '202608', '202411', '202502'])
    parser.add_argument('--radxa', nargs='+', choices=['1.3.1', '1.2.4'], default=['1.3.1'])
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--platform-ref', default='source/unofficial/stmm/edk2-stable202608')
    parser.add_argument('--vendor-control-fd', type=Path,
                        help='pinned original vendor FD; build native core/CPU with vendor storage for parity tests')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('jobs must be positive')
    if args.vendor_control_fd and len(args.radxa) != 1:
        parser.error('a vendor control FD must select exactly one Radxa release')
    control_fd = args.vendor_control_fd.read_bytes() if args.vendor_control_fd else None
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with (output / 'runner.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        (output / 'runner.pid').write_text(str(os.getpid()) + '\n')
        image = subprocess.check_output(
            ['docker', 'image', 'inspect', '--format', '{{.Id}}', args.image], text=True).strip()
        platform_ref = resolve_ref(root, args.platform_ref)
        platform_tree = git(root, 'rev-parse', platform_ref + ':CixStandaloneMmPkg').decode().strip()
        cases = []
        for radxa in args.radxa:
            for edk2 in args.edk2:
                target = f'edk2-{edk2}/radxa-{radxa}/unofficial'
                _, entry = release_entry(root, target, require=True)
                commit = resolve_ref(root, entry['source_ref'])
                commit = git(root, 'rev-parse', commit).decode().strip()
                tree = git(root, 'rev-parse', commit + ':src/edk2').decode().strip()
                cases.append({'edk2': edk2, 'radxa': radxa, 'target': target,
                              'firmware_commit': commit, 'edk2_tree': tree, 'platform_tree': platform_tree,
                              'crypto_header': git(root, 'rev-parse',
                                                   platform_ref + ':' + CRYPTO_PATH).decode().strip()
                              if edk2 == '202608' else None})
        plan = {'schema_version': 1, 'image': image, 'cases': cases,
                'runner_sha256': digest(Path(__file__).read_bytes()),
                'platform_helper_sha256': digest(Path(__file__).with_name('stmm_release_platform.py').read_bytes()),
                'source_port_sha256': digest(Path(__file__).with_name('stmm_release_port.py').read_bytes()),
                'control_helper_sha256': digest(Path(__file__).with_name('stmm_vendor_control.py').read_bytes()),
                'control_fd_sha256': digest(control_fd) if control_fd is not None else None}
        plan_path = output / 'plan.json'
        if plan_path.exists() and json.loads(plan_path.read_text()) != plan:
            raise ValueError('frozen inputs changed; use a new output directory')
        save(plan_path, plan)
        snapshot = output / 'tools'
        snapshot.mkdir(exist_ok=True)
        for name in ('qualify_stmm_releases.py', 'stmm_release_platform.py', 'stmm_release_port.py',
                     'stmm_vendor_control.py'):
            shutil.copyfile(Path(__file__).with_name(name), snapshot / name)
        state_path = output / 'state.json'
        state = json.loads(state_path.read_text()) if state_path.exists() else {'cases': {}}
        failed = False
        for case in cases:
            key = case['edk2'] + '-radxa-' + case['radxa']
            previous = state['cases'].get(key, {})
            directory = output / key
            if previous.get('status') == 'built-not-qualified':
                if digest((directory / 'BL32_AP_MM.fd').read_bytes()) != previous['fd_sha256']:
                    raise ValueError('completed payload changed: ' + key)
                continue
            if previous:
                failed = True
                continue
            if directory.exists():
                raise ValueError('failed execution state retained; use a new output directory: ' + key)
            directory.mkdir()
            state.update(active_case=key, status='running', updated=datetime.now(timezone.utc).isoformat())
            state['cases'][key] = {'status': 'running'}
            save(state_path, state)
            started = time.monotonic()
            print('Starting ' + key, flush=True)
            try:
                result = compile_case(root, case, directory, image, args.jobs, control_fd)
            except (subprocess.CalledProcessError, ValueError, OSError) as exc:
                result = {'status': 'failed', 'error': str(exc)}
                failed = True
            result['elapsed_seconds'] = round(time.monotonic() - started, 1)
            state['cases'][key] = result
            save(directory / 'result.json', result)
            save(state_path, state)
            print(key + ': ' + result['status'], flush=True)
        state.update(active_case=None, status='compile-failures' if failed else 'compiled-not-qualified',
                     updated=datetime.now(timezone.utc).isoformat())
        save(state_path, state)
        if failed:
            sys.exit(1)


if __name__ == '__main__':
    main()
