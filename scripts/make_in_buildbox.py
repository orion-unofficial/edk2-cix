#!/usr/bin/env python3
"""Run retained variant recipes through the build-owned container runner."""

import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


def command(args, cwd):
    if 'buildbox-ccache-stats' in args:
        values = dict(arg.split('=', 1) for arg in args if '=' in arg and not arg.startswith('-'))
        if values.get('CCACHE_STATS_FORMAT', 'json') != 'json':
            raise ValueError('the internal buildbox Make bridge supports JSON cache statistics')
        directory = Path(cwd)
        for index, arg in enumerate(args):
            if arg in ('-C', '--directory'):
                directory /= args[index + 1]
            elif arg.startswith('--directory='):
                directory /= arg.split('=', 1)[1]
        result = [sys.executable, str(directory / 'scripts/ccache_stats.py'),
                  '--cache-dir', values.get('CCACHE_DIR') or os.environ.get('CCACHE_DIR', '/hosttmp/ccache')]
        if values.get('CCACHE_BIN'):
            result.extend(['--ccache-bin', values['CCACHE_BIN']])
        return result
    goals = {f'buildbox-{goal}': goal for goal in ('firmware-build', 'firmware-stage', 'zip', 'targz')}
    return ['/usr/bin/make', *(goals.get(arg, arg) for arg in args)]


def host_command(args, cwd, environ):
    """Keep the retained matrix on the host and select its distro per variant."""
    if not any(arg.startswith('buildbox-') for arg in args):
        return [environ.get('EDK2_CIX_HOST_MAKE', '/usr/bin/make'), *args], dict(environ)
    flags = shlex.split(environ.get('MAKEFLAGS', ''))
    inherited = flags[flags.index('--') + 1:] if '--' in flags else []
    inherited = [arg for arg in inherited if '=' in arg and not arg.startswith('MAKE=')]
    arguments = [*inherited, *args]
    values = dict(arg.split('=', 1) for arg in arguments if '=' in arg and not arg.startswith('-'))
    distro = values.get('FIRMWARE_DISTRO') or environ.get('EDK2_CIX_FIRMWARE_DISTRO', 'trixie')
    env = dict(environ)
    env['EDK2_CIX_FIRMWARE_DISTRO'] = distro
    image = (environ.get('EDK2_CIX_BUILDBOX_IMAGE_OVERRIDE') or values.get('BUILDBOX_IMAGE') or
             f'mcr.microsoft.com/devcontainers/base:{distro}')
    env['EDK2_CIX_BUILDBOX_IMAGE'] = image
    root = str(Path(environ['EDK2_CIX_FIRMWARE_ROOT']).resolve())
    workspace = environ['EDK2_CIX_VARIANT_WORKSPACE']

    def container_argument(arg):
        key, separator, value = arg.partition('=')
        path = value if separator else key
        if path == root or path.startswith(root + '/'):
            path = workspace + path[len(root):]
        return key + separator + path if separator else path

    runner = Path(environ['EDK2_CIX_BUILD_INFRA_ROOT']) / 'scripts/run_in_buildbox.sh'
    return [str(runner), '/build-infra/scripts/make_in_buildbox.py',
            *(container_argument(arg) for arg in arguments),
            'MAKE=/build-infra/scripts/make_in_buildbox.py'], env


def main():
    if sys.argv[1:] in (['--help'], ['-h']):
        print(__doc__ + '\nUsage: make_in_buildbox.py [Make arguments]')
        return
    if os.environ.get('EDK2_CIX_IN_BUILDBOX') != '1' and os.environ.get('EDK2_CIX_HOST_VARIANTS') != '1':
        raise SystemExit('make_in_buildbox.py requires the build-owned container runner')
    try:
        if os.environ.get('EDK2_CIX_IN_BUILDBOX') == '1':
            argv, env = command(sys.argv[1:], Path.cwd()), dict(os.environ)
        else:
            argv, env = host_command(sys.argv[1:], Path.cwd(), os.environ)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc
    if os.environ.get('EDK2_CIX_IN_BUILDBOX') != '1' and 'buildbox-ccache-stats' in sys.argv[1:]:
        result = subprocess.run(argv, env=env, stdout=subprocess.PIPE, text=True)
        if result.returncode:
            sys.stderr.write(result.stdout)
            raise SystemExit(result.returncode)
        # Use the retained formatter, keeping container lifecycle logs out of JSON.
        formatter = Path(env['EDK2_CIX_FIRMWARE_ROOT']) / 'scripts/format_ccache_stats.py'
        with tempfile.NamedTemporaryFile(mode='w', prefix='buildbox-stats-', suffix='.log') as log:
            log.write(result.stdout)
            log.flush()
            raise SystemExit(subprocess.call([sys.executable, str(formatter), '--format', 'json', log.name]))
    os.execve(argv[0], argv, env)


if __name__ == '__main__':
    main()
