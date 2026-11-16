#!/usr/bin/env python3
"""Bounded StandaloneMM Actions preflight/native compile; never certifies firmware.

Supported caller: .github/workflows/stmm-release-matched.yaml. Local compiler preparation must be explicit.
Source selection is pinned and results never reuse a historical pass.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / 'config/stmm-ci.json'
HELPERS = ('stmm_ci.py',)
TESTS = ('test_stmm_ci.py',)
NATIVE_HELPERS = ('qualify_stmm_releases.py', 'stmm_release_platform.py',
                  'stmm_release_port.py', 'stmm_vendor_control.py', 'reconstruction_common.py',
                  'stmm_compiler.py', 'stmm-compiler.Dockerfile', 'stmm_compiler_probe.sh')
NATIVE_TESTS = ('test_stmm_release_platform.py', 'test_stmm_release_port.py',
                'test_stmm_vendor_control.py')
NATIVE_DSC = 'src/edk2-platforms/Platform/StandaloneMm/PlatformStandaloneMmPkg/PlatformStandaloneMmRpmb.dsc'
CRYPTO_PATH = 'CryptoPkg/Library/Include/CrtLibSupport.h'
REQUIRED_CRITERIA = (
    'input-validation', 'native-compile', 'full-source-image',
    'exact-fd-optee-flash-binding', 'vendor-signature-chain',
    'actual-c-producer-native-abi', 'authenticated-variable-tests', 'cold-persistence',
    'paired-nor-baseline-2047', 'paired-cuts-189-149-40',
    'complete-source-model-lint-policy-identity-helper', 'minimised-clone-rebuild-replay',
    'applicable-deterministic-replay', 'remote-coherence',
)
BLOCKED_CRITERIA = REQUIRED_CRITERIA[2:10]
STATES = {'qualified-candidate', 'not-integrated', 'integrated-unqualified', 'qualified-descendant'}
RESULTS = {'passed', 'failed', 'blocked', 'unverified', 'incomplete'}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def save(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n')


def git(*args: str) -> str:
    return subprocess.check_output(['git', '-C', str(ROOT), *args], stderr=subprocess.PIPE,
                                   text=True).strip()


def validate_policy(policy: dict) -> None:
    if (policy['schema_version'] != 1 or set(policy['qualification_states']) != STATES
            or policy['required_criteria'] != list(REQUIRED_CRITERIA)
            or policy['blocked_criteria'] != list(BLOCKED_CRITERIA)):
        raise ValueError('qualification policy changed; explicit adapter review required')
    keys = [case['key'] for case in policy['cases']]
    if not keys or len(set(keys)) != len(keys):
        raise ValueError('empty or duplicate case population')
    defects = policy['inherited_defects']
    if (defects['paired_power_cut_samples'], defects['recoveries'],
            defects['expected_matching_failures']) != (189, 149, 40):
        raise ValueError('paired storage acceptance changed')
    if policy['minimum_disk_gib'] < 8 or policy['minimum_memory_gib'] < 4:
        raise ValueError('resource entry guards cannot be weakened')


def object_pin(pin: dict) -> None:
    if (not re.fullmatch('[0-9a-f]{40}', pin['commit'])
            or not re.fullmatch('[0-9a-f]{40}', pin['tree'])):
        raise ValueError('source commit must be an exact object ID')
    try:
        if git('rev-parse', pin['commit'] + '^{commit}') != pin['commit']:
            raise ValueError('source pin must name a commit object')
        tree = git('rev-parse', pin['commit'] + '^{tree}')
    except subprocess.CalledProcessError as exc:
        raise ValueError('pinned source object unavailable; approved transfer required: ' + pin['commit']) from exc
    if tree != pin['tree']:
        raise ValueError('source tree differs from manifest')


def file_hashes(names: tuple[str, ...]) -> dict:
    return {name: sha((ROOT / 'scripts' / name).read_bytes()) for name in names}


def subject(policy: dict, case: dict, selection: str, stage: str = 'preflight') -> dict:
    selected = case.get(selection)
    if selected is None:
        raise ValueError('no full-source candidate exists for this release')
    allowed = ({'qualified-candidate'} if selection == 'candidate' else
               {'not-integrated', 'integrated-unqualified', 'qualified-descendant'})
    if selected['state'] not in allowed:
        raise ValueError('invalid state for selected source population')
    entries = json.loads((ROOT / 'config/refs-unofficial.json').read_text())['refs']
    entry = next(e for e in entries if e['ref'] == case['maintained']['ref'])
    if (entry['edk2_base'] != 'edk2-stable' + case['edk2']
            or entry['radxa_release'] != case['radxa']):
        raise ValueError('case is not eligible under the source manifest')
    object_pin(selected)
    # A maintained ref must agree with both this manifest and the source model.
    if selection == 'maintained':
        ref = selected['ref']
        if git('rev-parse', ref) != selected['commit']:
            raise ValueError('maintained ref moved; review and repin before running')
        if (entry['object_id'], entry['tree_id']) != (selected['commit'], selected['tree']):
            raise ValueError('maintained manifest mismatch')
    components = policy['components'][selection]
    for pin in components.values():
        object_pin(pin)
        if pin.get('ref') and git('rev-parse', pin['ref']) != pin['commit']:
            raise ValueError('component ref moved; review and repin')
    if selection == 'maintained':
        records = json.loads((ROOT / 'config/refs-cix.json').read_text())['refs']
        for pin in components.values():
            matches = [r for r in records if r['ref'] == pin['ref']]
            if (len(matches) != 1
                    or (matches[0]['object_id'], matches[0]['tree_id']) != (pin['commit'], pin['tree'])):
                raise ValueError('maintained component manifest mismatch')
    source = selected['commit']
    # Full source tree identity pins all materialised native dependencies, too.
    dependencies = {name: git('rev-parse', source + ':src/' + name)
                    for name in ('edk2', 'edk2-platforms', 'edk2-non-osi')}
    component = components['stmm']['commit']
    dependencies['platform'] = git('rev-parse', component + ':CixStandaloneMmPkg')
    dependencies['optee'] = components['optee']
    # Preflight must be publishable without the unintegrated native helper WIP.
    dependencies['native_dsc_blob'] = git('rev-parse', source + ':' + NATIVE_DSC)
    dependencies['crypto_header'] = (git('rev-parse', component + ':' + CRYPTO_PATH)
                                     if case['edk2'] == '202608' else None)
    helpers = HELPERS + (NATIVE_HELPERS if stage == 'native-compile' else ())
    tests = TESTS + (NATIVE_TESTS if stage == 'native-compile' else ())
    return {'key': case['key'], 'selection': selection, 'input_state': selected['state'],
            'source': selected, 'dependencies': dependencies, 'helpers': file_hashes(helpers),
            'test_suite': file_hashes(tests), 'manifest_sha256': sha(POLICY.read_bytes()),
            'harness_commit': git('rev-parse', 'HEAD'), 'radxa': case['radxa'],
            'edk2': case['edk2'], 'board': 'O6', 'target': 'RELEASE', 'distro': 'trixie',
            'source_date_epoch': 0, 'compiler_image': policy['compiler_image']}


def new_result(policy: dict, key: str, selection: str, stage: str) -> dict:
    return {'schema_version': 1, 'key': key, 'selection': selection, 'stage': stage,
            'status': 'unverified', 'qualification': 'incomplete', 'subject': None, 'requested_source': next((c.get(selection) for c in policy['cases'] if c['key'] == key), None),
            'run_id': os.environ.get('GITHUB_RUN_ID'), 'attempt': os.environ.get('GITHUB_RUN_ATTEMPT'),
            'criteria': {name: ('blocked' if name in policy['blocked_criteria'] else 'unverified')
                         for name in policy['required_criteria']},
            'blocked_workloads': policy['blocked_workloads'], 'errors': []}


def compile_native(policy: dict, identity: dict, output: Path, *, build_compiler=False,
                   local_compiler_image=None) -> dict:
    from stmm_compiler import OCI, PLATFORM, prepare

    image = policy['compiler_image']
    if not (build_compiler or local_compiler_image or isinstance(image, str) and OCI.fullmatch(image)):
        raise ValueError('select an immutable OCI registry digest, --build-compiler, or --local-compiler-image')
    # A compile-only recipe is still a new attempt, never a historical replay.
    if shutil.disk_usage(output).free < policy['minimum_disk_gib'] * 1024 ** 3:
        raise ValueError('insufficient disk; use a measured/sized runner, do not delete evidence')
    memory_kib = int(next(line.split()[1] for line in Path('/proc/meminfo').read_text().splitlines()
                          if line.startswith('MemAvailable:')))
    if memory_kib < policy['minimum_memory_gib'] * 1024 ** 2:
        raise ValueError('insufficient available memory')
    inspection = prepare(ROOT, policy, output.resolve(), build=build_compiler, retained=local_compiler_image)
    if shutil.disk_usage(output).free < policy['minimum_disk_gib'] * 1024 ** 3:
        raise ValueError('insufficient disk after preparing compiler image')
    from qualify_stmm_releases import compile_case
    from stmm_release_platform import NATIVE_DSC as compiler_dsc
    from stmm_release_port import CRYPTO_PATH as compiler_crypto
    if (compiler_dsc, compiler_crypto) != (NATIVE_DSC, CRYPTO_PATH):
        raise ValueError('native helper input paths changed; review adapter identity binding')
    deps = identity['dependencies']
    case = {'edk2': identity['edk2'], 'radxa': identity['radxa'],
            'firmware_commit': identity['source']['commit'], 'edk2_tree': deps['edk2'],
            'platform_tree': deps['platform'], 'crypto_header': deps['crypto_header']}
    directory = output / 'native'
    directory.mkdir()
    try:
        result = compile_case(ROOT, case, directory, inspection['id'], jobs=2, platform=PLATFORM)
    finally:
        # Compiler failures need the same available debug evidence as successes.
        debug = output / 'debug'
        debug.mkdir()
        hashes = {}
        for path in (directory / 'source/Build/StandaloneMm').rglob('*'):
            if path.is_file() and path.suffix.lower() in {'.elf', '.map'}:
                dest = debug / path.relative_to(directory / 'source/Build/StandaloneMm')
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, dest)
                hashes[str(dest.relative_to(output))] = sha(dest.read_bytes())
        save(output / 'debug-sha256.json', hashes)
    return result


def validate_native_output(output: Path, build: dict) -> None:
    native = output / 'native'
    fd = native / 'BL32_AP_MM.fd'
    if (build['status'] != 'built-not-qualified' or fd.stat().st_size != 2 * 1024 * 1024
            or sha(fd.read_bytes()) != build['fd_sha256']
            or json.loads((native / 'profile.json').read_text()) != build['profile']
            or (native / 'compiler.txt').read_text() != build['compiler']):
        raise ValueError('native compiler returned incomplete or mismatched output')


def run_case(policy: dict, key: str, selection: str, stage: str, output: Path, *,
             build_compiler=False, local_compiler_image=None) -> int:
    output.mkdir(parents=True, exist_ok=False)  # Never overwrite a failed attempt.
    result = new_result(policy, key, selection, stage)
    try:
        validate_policy(policy)
        case = next(c for c in policy['cases'] if c['key'] == key)
        identity = subject(policy, case, selection, stage)
        result['subject'] = identity
        save(output / 'inputs.json', identity)
        result['criteria']['input-validation'] = 'passed'
        result['status'] = 'passed'
        if stage == 'native-compile':
            if identity['input_state'] == 'not-integrated':
                raise ValueError('maintained native integration missing; compile cannot qualify this source')
            result['build'] = compile_native(policy, identity, output, build_compiler=build_compiler,
                                             local_compiler_image=local_compiler_image)
            validate_native_output(output, result['build'])
            result['criteria']['native-compile'] = 'passed'
    except (ValueError, KeyError, StopIteration, OSError, subprocess.SubprocessError) as exc:
        result['status'] = 'blocked' if isinstance(exc, (ValueError, KeyError, StopIteration)) else 'failed'
        result['criteria']['native-compile' if stage == 'native-compile' else 'input-validation'] = result['status']
        result['errors'].append(str(exc))
    # These stages deliberately cannot emit qualified-descendant, even on success.
    evidence = {}
    for name in ('inputs.json', 'image.json', 'native/BL32_AP_MM.fd', 'native/profile.json',
                 'native/compiler.txt', 'native/build.log', 'debug-sha256.json'):
        path = output / name
        if path.is_file():
            evidence[name] = sha(path.read_bytes())
    for path in output.glob('compiler-*'):
        if path.is_file():
            evidence[path.name] = sha(path.read_bytes())
    result['evidence_sha256'] = evidence
    save(output / 'result.json', result)
    print(json.dumps(result, sort_keys=True))
    return 0 if result['status'] == 'passed' else 1


def selected_case_keys(policy: dict, selection: str, case_key: str | None = None) -> list[str]:
    keys = [c['key'] for c in policy['cases'] if selection == 'maintained' or c.get('candidate')]
    if case_key is None:
        return keys
    if case_key not in keys:
        raise ValueError(f'case {case_key!r} is unavailable for {selection} selection')
    return [case_key]


def aggregate(policy: dict, selection: str, stage: str, directory: Path, job_results: dict, *,
              case_key: str | None = None) -> dict:
    """Fail closed on missing, duplicate, stale, or foreign job results."""
    validate_policy(policy)
    expected = set(selected_case_keys(policy, selection, case_key))
    rows = {}
    errors = []
    for path in sorted(directory.rglob('result.json')):
        try:
            row = json.loads(path.read_text())
            key = row['key']
            if (not isinstance(key, str) or not isinstance(row.get('criteria'), dict)
                    or not isinstance(row.get('evidence_sha256'), dict)
                    or not {'status', 'qualification', 'subject', 'stage', 'selection', 'run_id', 'attempt'} <= row.keys()
                    or not isinstance(row['status'], str)):
                raise ValueError('malformed result')
        except (ValueError, KeyError, TypeError):
            errors.append('malformed result: ' + str(path))
            continue
        if key not in expected or key in rows:
            errors.append('unexpected or duplicate result: ' + key)
            continue
        rows[key] = row
        try:
            required_evidence = {'inputs.json'}
            if stage == 'native-compile':
                required_evidence.update({'image.json', 'debug-sha256.json', 'native/BL32_AP_MM.fd',
                                          'native/profile.json', 'native/compiler.txt', 'native/build.log'})
            if not required_evidence <= row['evidence_sha256'].keys():
                raise ValueError('required evidence missing')
            if json.loads((path.parent / 'inputs.json').read_text()) != row['subject']:
                raise ValueError('input receipt differs from subject')
            for name, digest in row['evidence_sha256'].items():
                artifact = path.parent / name
                if not artifact.resolve().is_relative_to(path.parent.resolve()):
                    raise ValueError('artifact path escapes result directory')
                if sha(artifact.read_bytes()) != digest:
                    raise ValueError('artifact checksum differs')
            if stage == 'native-compile':
                from stmm_compiler import BUILD_EVIDENCE, LOCAL_EVIDENCE, validate_receipt

                image = json.loads((path.parent / 'image.json').read_text())
                validate_receipt(policy, image, hosted=os.environ.get('GITHUB_ACTIONS') == 'true')
                mode = image['mode']
                if mode != 'registry':
                    if (not LOCAL_EVIDENCE <= row['evidence_sha256'].keys()
                            or row['evidence_sha256']['compiler-probe.txt'] != image['probe_sha256']):
                        raise ValueError('compiler probe evidence differs')
                if mode == 'build':
                    if (not BUILD_EVIDENCE <= row['evidence_sha256'].keys()
                            or (path.parent / 'compiler-iid.txt').read_text().strip() != image['requested']):
                        raise ValueError('compiler build evidence differs')
                validate_native_output(path.parent, row['build'])
                debug = json.loads((path.parent / 'debug-sha256.json').read_text())
                for name, digest in debug.items():
                    artifact = (path.parent / name).resolve()
                    if (not artifact.is_relative_to((path.parent / 'debug').resolve())
                            or sha(artifact.read_bytes()) != digest):
                        raise ValueError('debug evidence differs')
            case = next(c for c in policy['cases'] if c['key'] == key)
            if (row['subject'] != subject(policy, case, selection, stage) or row['stage'] != stage
                    or row['selection'] != selection
                    or row['run_id'] != os.environ.get('GITHUB_RUN_ID')
                    or row['attempt'] != os.environ.get('GITHUB_RUN_ATTEMPT')):
                errors.append('stale or mismatched identity: ' + key)
        except (ValueError, KeyError, TypeError, AttributeError, OSError, subprocess.SubprocessError):
            errors.append('unverifiable identity/evidence: ' + key)
        if row['status'] not in RESULTS or row['status'] != 'passed':
            errors.append('stage did not pass: ' + key)
        required = ['input-validation'] + (['native-compile'] if stage == 'native-compile' else [])
        if any(row['criteria'].get(name) != 'passed' for name in required):
            errors.append('missing stage criterion: ' + key)
        untouched = set(policy['required_criteria']) - set(required)
        if (set(row['criteria']) != set(policy['required_criteria'])
                or any(row['criteria'][name] != ('blocked' if name in policy['blocked_criteria'] else 'unverified')
                       for name in untouched)):
            errors.append('unevaluated qualification criteria changed: ' + key)
        if row['qualification'] != 'incomplete':
            errors.append('unsupported qualification claim: ' + key)
    for key in sorted(expected - rows.keys()):
        errors.append('missing result (failed/cancelled/skipped job): ' + key)
    if (set(job_results) != {'plan', 'cases', 'download'}
            or any(value != 'success' for value in job_results.values())):
        errors.append('one or more workflow dependencies did not succeed')
    return {'schema_version': 1, 'stage': stage, 'selection': selection,
            'status': 'failed' if errors else 'passed', 'qualification': 'incomplete', 'case': case_key,
            'qualification_states': policy['qualification_states'],
            'required_criteria': policy['required_criteria'], 'results': rows,
            'outstanding': [c for c in policy['required_criteria']
                            if c not in ['input-validation'] + (['native-compile'] if stage == 'native-compile' else [])],
            'blocked_workloads': policy['blocked_workloads'], 'job_results': job_results, 'errors': errors}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['matrix', 'case', 'aggregate'])
    parser.add_argument('--selection', choices=['candidate', 'maintained'], required=True)
    parser.add_argument('--stage', choices=['preflight', 'native-compile'], default='preflight')
    parser.add_argument('--case')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--results', type=Path)
    compiler = parser.add_mutually_exclusive_group()
    compiler.add_argument('--build-compiler', action='store_true')
    compiler.add_argument('--local-compiler-image', metavar='IMAGE_ID')
    args = parser.parse_args()
    if ((args.build_compiler or args.local_compiler_image)
            and (args.command, args.stage) != ('case', 'native-compile')):
        parser.error('compiler options require case --stage native-compile')
    policy = json.loads(POLICY.read_text())
    validate_policy(policy)
    if args.command != 'case':
        try:
            keys = selected_case_keys(policy, args.selection, args.case)
        except ValueError as exc:
            parser.error(str(exc))
    if args.command == 'matrix':
        print(json.dumps({'include': [{'key': key} for key in keys]}))
    elif args.command == 'case':
        if not args.case or not args.output:
            parser.error('case requires --case and --output')
        sys.exit(run_case(policy, args.case, args.selection, args.stage, args.output,
                          build_compiler=args.build_compiler, local_compiler_image=args.local_compiler_image))
    else:
        if not args.results or not args.output:
            parser.error('aggregate requires --results and --output')
        result = aggregate(policy, args.selection, args.stage, args.results,
                           json.loads(os.environ.get('STMM_JOB_RESULTS', '{}')), case_key=args.case)
        save(args.output, result)
        summary = os.environ.get('GITHUB_STEP_SUMMARY')
        if summary:
            with open(summary, 'a') as handle:
                handle.write(f"## StandaloneMM {args.selection}: {args.stage}\n\n")
                handle.write(f"Stage: {result['status']}. Firmware qualification: incomplete.\n\n")
                for key, row in result['results'].items():
                    identity = row.get('subject') or {}
                    handle.write(f"- {key}: {row['status']}; {identity.get('input_state', 'unverified')}; "
                                 f"source {identity.get('source', {}).get('commit', 'unverified')}\n")
                handle.write('\nOutstanding: ' + ', '.join(result['outstanding']) + '\n\n')
                handle.write('See aggregate.json for exact identities, evaluated criteria and blockers.\n')
                for error in result['errors']:
                    handle.write('- ' + error + '\n')
        sys.exit(0 if result['status'] == 'passed' else 1)


if __name__ == '__main__':
    main()
