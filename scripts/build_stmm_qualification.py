#!/usr/bin/env python3
"""Build the pinned, unqualified CIX hybrid StandaloneMM component in Docker.

This command cannot select or assemble a board firmware image. Its output is
for qualification; STMM=unofficial remains protected by the normal build gates.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from reconstruction_common import resolve_ref


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def build(repo: Path, output: Path, image: str, jobs: int) -> dict:
    records = json.loads((repo / 'config/refs-cix.json').read_text())['refs']
    records = [r for r in records if r.get('component') == 'standalone-mm']
    if len(records) != 1 or records[0].get('type') not in {'qualification-component', 'unofficial-component'}:
        raise ValueError('missing or ambiguous StandaloneMM qualification component')
    record = records[0]

    def git(*args):
        return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()

    if (git('rev-parse', resolve_ref(repo, record['ref'])) != record['object_id'] or
            git('rev-parse', record['object_id'] + '^{tree}') != record['tree_id']):
        raise ValueError('StandaloneMM source ref differs from its build pin')
    image_id = subprocess.check_output(['docker', 'image', 'inspect', '--format', '{{.Id}}', image], text=True).strip()
    output.mkdir(parents=True, exist_ok=False)
    source = output / 'source'
    source.mkdir()
    (output / 'tmp').mkdir()
    archive = subprocess.Popen(['git', '-C', str(repo), 'archive', record['object_id']], stdout=subprocess.PIPE)
    try:
        subprocess.run(['tar', '-xf', '-', '-C', str(source)], stdin=archive.stdout, check=True)
    finally:
        archive.stdout.close()
        status = archive.wait()
    if status:
        raise RuntimeError('failed to export StandaloneMM source')
    vendor = source / 'CixStandaloneMmPkg/Vendor'
    manifest = json.loads((vendor / 'manifest.json').read_text())
    if manifest.get('schema_version') != 1:
        raise ValueError('unsupported vendor component manifest')
    for name, expected in manifest['files'].items():
        if Path(name).name != name or sha256((vendor / name).read_bytes()) != expected:
            raise ValueError(f'vendor StandaloneMM component hash mismatch: {name}')
    epoch = git('show', '-s', '--format=%ct', record['object_id'])
    command = '''set -eo pipefail
export WORKSPACE=/work/source TMPDIR=/work/tmp GCC_AARCH64_PREFIX=aarch64-linux-gnu-
cd "$WORKSPACE"
source edksetup.sh
make -C BaseTools -j"$JOBS"
build -p CixStandaloneMmPkg/CixStandaloneMm.dsc -a AARCH64 -t GCC -b RELEASE -n "$JOBS"
aarch64-linux-gnu-gcc --version > /work/compiler.txt
'''
    with (output / 'build.log').open('w') as log:
        subprocess.run(['docker', 'run', '--rm', '--network', 'none', '--entrypoint', 'bash',
                        '-e', f'SOURCE_DATE_EPOCH={epoch}', '-e', f'JOBS={jobs}',
                        '-v', f'{output}:/work', image_id, '-c', command], stdout=log, stderr=log, check=True)
    fd = source / 'Build/StandaloneMm/RELEASE_GCC/FV/BL32_AP_MM.fd'
    payload = fd.read_bytes()
    if len(payload) != 2 * 1024 * 1024:
        raise ValueError('unexpected StandaloneMM FD size')
    for name in manifest['files']:
        if name.endswith('.efi') and payload.count((vendor / name).read_bytes()) != 1:
            raise ValueError(f'expected exactly one unchanged vendor executable: {name}')
    shutil.copyfile(fd, output / 'BL32_AP_MM.fd')
    report = {'status': 'built-not-qualified', 'source': record, 'buildbox_image': image_id,
              'source_date_epoch': int(epoch), 'compiler': (output / 'compiler.txt').read_text(),
              'fd_sha256': sha256(payload), 'fd_size': len(payload), 'vendor': manifest,
              'required_optee_stmm_ffa_version': 12}
    (output / 'build.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path, help='new durable build directory')
    parser.add_argument('--image', default='edk2-cix-stmm-qualification', help='image built with scripts/stmm.Dockerfile')
    parser.add_argument('--jobs', type=int, default=8)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    repo = Path(__file__).resolve().parents[1]
    report = build(repo, args.output.resolve(), args.image, args.jobs)
    print(f"Built qualification payload {report['fd_sha256']}; not approved for STMM=unofficial")


if __name__ == '__main__':
    main()
