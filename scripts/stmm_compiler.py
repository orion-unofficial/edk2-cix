"""Prepare and verify the immutable compiler input for the STMM native adapter."""
import hashlib
import json
import re
import subprocess

RECIPE = 'stmm-compiler.Dockerfile'
PROBE = 'stmm_compiler_probe.sh'
DIGEST = re.compile(r'sha256:[0-9a-f]{64}')
OCI = re.compile(r'[a-z0-9][a-z0-9._:/-]*@sha256:[0-9a-f]{64}')
PLATFORM = 'linux/amd64'
LOCAL_EVIDENCE = {'compiler-probe.txt', 'compiler-docker.txt'}
BUILD_EVIDENCE = {'compiler-build.log', 'compiler-iid.txt', 'compiler-buildx.txt'}


def compiler_files(root, policy):
    files = [(root / 'scripts' / name).read_bytes() for name in (RECIPE, PROBE)]
    for data, key in zip(files, ('recipe_sha256', 'probe_sha256'), strict=True):
        if hashlib.sha256(data).hexdigest() != policy[key]:
            raise ValueError('local compiler recipe or probe differs from reviewed inputs')
    return files


def docker_config_sha256(config):
    if not isinstance(config, dict):
        raise ValueError('missing or malformed Docker runtime configuration')
    # Docker API versions differ in which default-valued fields they emit.
    active = {key: value for key, value in config.items() if value}
    return hashlib.sha256(json.dumps(active, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def validate_image(policy, image, *, hosted=False):
    if ((image['os'], image['architecture']) != ('linux', 'amd64')
            or not DIGEST.fullmatch(image['id'])
            or not re.fullmatch('[0-9a-f]{64}', image['docker_config_sha256'])
            or (image['descriptor_digest'] is not None and not DIGEST.fullmatch(image['descriptor_digest']))
            or not isinstance(image['diff_ids'], list) or not image['diff_ids']
            or not all(DIGEST.fullmatch(layer) for layer in image['diff_ids'])):
        raise ValueError('native compile lane requires a Linux amd64 compiler image with exact content IDs')
    if image['mode'] == 'registry':
        requested = policy['compiler_image']
        if (not isinstance(requested, str) or not OCI.fullmatch(requested)
                or image['requested'] != requested
                or not any(ref.endswith('@' + requested.split('@', 1)[1]) for ref in image['repo_digests'])):
            raise ValueError('pulled image does not match the registry digest')
        return

    local = policy['local_compiler']
    mode = image['mode']
    if not DIGEST.fullmatch(image['requested']):
        raise ValueError('local compiler image ID is invalid')
    if mode == 'retained':
        if (hosted or image['requested'] not in local['retained_image_ids']
                or image['id'] not in local['retained_image_ids']
                or (image['descriptor_digest'] is not None
                    and image['descriptor_digest'] not in local['retained_image_ids'])
                or image['diff_ids'] != local['retained_diff_ids']
                or image['docker_config_sha256'] != local['retained_docker_config_sha256']):
            raise ValueError('retained compiler image differs from the reviewed image')
    elif mode != 'build' or policy['compiler_image'] is not None:
        raise ValueError('unsupported compiler receipt mode')


def validate_receipt(policy, image, *, hosted=False):
    validate_image(policy, image, hosted=hosted)
    if (image['mode'] != 'registry'
            and image['probe_sha256'] != policy['local_compiler']['probe_output_sha256']):
        raise ValueError('local compiler toolchain probe differs')


def prepare(root, policy, output, *, build=False, retained=None):
    if build and retained is not None:
        raise ValueError('choose one local compiler input')
    registry = policy['compiler_image']
    mode = 'build' if build else 'retained' if retained else 'registry'
    if mode == 'registry':
        if not isinstance(registry, str) or not OCI.fullmatch(registry):
            raise ValueError('select an immutable OCI registry digest, --build-compiler, or --local-compiler-image')
        requested = registry
        subprocess.run(['docker', 'pull', '--platform', PLATFORM, requested], check=True, timeout=600)
    else:
        local = policy['local_compiler']
        recipe, probe = compiler_files(root, local)
        if mode == 'retained' and retained not in local['retained_image_ids']:
            raise ValueError('local compiler must use a reviewed immutable image ID')
        if mode == 'build' and registry is not None:
            raise ValueError('local compiler build conflicts with the registry pin')
        with (output / 'compiler-docker.txt').open('wb') as log:
            subprocess.run(['docker', 'version'], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
        requested = retained
        if mode == 'build':
            with (output / 'compiler-buildx.txt').open('wb') as log:
                subprocess.run(['docker', 'buildx', 'version'], stdout=log, stderr=subprocess.STDOUT,
                               check=True, timeout=30)
            with (output / 'compiler-build.log').open('wb') as log:
                subprocess.run(['docker', 'buildx', 'build', '--platform', PLATFORM, '--no-cache', '--load',
                                '--progress=plain', '--iidfile', 'compiler-iid.txt',
                                '--build-arg', 'SOURCE_DATE_EPOCH=' + str(local['source_date_epoch']), '-'],
                               cwd=output, input=recipe, stdout=log, stderr=subprocess.STDOUT,
                               check=True, timeout=900)
            requested = (output / 'compiler-iid.txt').read_text().strip()
            if not DIGEST.fullmatch(requested):
                raise ValueError('compiler build did not produce an immutable image ID')

    inspection, = json.loads(subprocess.check_output(['docker', 'image', 'inspect', requested], timeout=30))
    image = {'mode': mode, 'requested': requested, 'id': inspection['Id'],
             'repo_digests': inspection['RepoDigests'] or [],
             'architecture': inspection['Architecture'], 'os': inspection['Os'],
             'diff_ids': inspection['RootFS']['Layers'],
             'descriptor_digest': (inspection.get('Descriptor') or {}).get('digest'),
             'docker_config_sha256': docker_config_sha256(inspection['Config'])}
    receipt = output / 'image.json'
    receipt.write_text(json.dumps(image, indent=2, sort_keys=True) + '\n')
    validate_image(policy, image)
    if mode != 'registry':
        path = output / 'compiler-probe.txt'
        with path.open('wb') as log, (output / 'compiler-probe.stderr').open('wb') as errors:
            subprocess.run(['docker', 'run', '--rm', '--pull', 'never', '--platform', PLATFORM,
                            '--network', 'none', '--read-only', '--tmpfs', '/tmp:rw,exec,nosuid',
                            '--entrypoint', 'bash', '-i', image['id'], '-s'],
                           input=probe, stdout=log, stderr=errors, check=True, timeout=90)
        image['probe_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
        receipt.write_text(json.dumps(image, indent=2, sort_keys=True) + '\n')
        validate_receipt(policy, image)
    return image
