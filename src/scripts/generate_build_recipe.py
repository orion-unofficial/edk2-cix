#!/usr/bin/env python3
"""Generate a read-only HII rebuild recipe in a private experimental overlay."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import shlex
import shutil

MODULE = Path('edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/SystemInfoDxe')
BOOLS = ('ENABLE_FIRMWARE_FIXES', 'ENABLE_TF_A_FIXES',
         'ENABLE_EXPERIMENTAL_UEFI_SETTINGS', 'DEBUG_ON_UART3', 'UART3_ENABLE', 'DEBUG_VERBOSE')


def recipe(config: dict[str, str], release: str, board: str, product: str,
           distro: str) -> list[str]:
    if config['ARTEFACT_MODE'] != 'custom' or config['ENABLE_EXPERIMENTAL_UEFI_SETTINGS'] != 'TRUE':
        raise ValueError('the rebuild menu is exclusive to custom experimental firmware')
    args = {'RELEASE': release, 'ARTEFACT_MODE': 'custom', 'FIRMWARE_BOARD': board,
            'FIRMWARE_PRODUCT': product, 'FIRMWARE_TARGET': config['UEFI_TARGET'],
            'FIRMWARE_DISTRO': distro}
    for key in ('ENABLE_FIRMWARE_FIXES', 'ENABLE_CORE_ORDER', 'CIX_RELEASE',
                'ENABLE_TF_A_FIXES', 'ENABLE_EXPERIMENTAL_UEFI_SETTINGS',
                'DEBUG_ON_UART3', 'UART3_ENABLE', 'DEBUG_VERBOSE', 'DEBUG_PRINT_ERROR_LEVEL'):
        value = config.get(key, '')
        # Keep empty values explicit: several options distinguish unset from false.
        args[key] = value.lower() if key in BOOLS else value
    # These overrides affect the stored image and are accepted by the source builder.
    for key in ('BUILD_DATE', 'SOURCE_DATE_EPOCH', 'PM_CONFIG_SOURCE_DATE_EPOCH',
                'FIRMWARE_VERSION', 'MEM_CFG_MEMFREQ', 'O6_SMBIOS_BASEBOARD_ASSET_TAG',
                'O6_SMBIOS_CHASSIS_ASSET_TAG', 'SIGNING_CERT_SOURCE_DIR', 'REQUIRE_SIGNING_CERT_SOURCE'):
        if config.get(key):
            args[key] = config[key]
    if config['UEFI_TARGET'] == 'DEBUG' or config.get('DEBUG_VERBOSE') == 'TRUE':
        args['FORCE_DEBUG_BUILD'] = '1'
    return ['make build'] + [key + '=' + shlex.quote(value) for key, value in args.items()]


def uni_string(value: str) -> str:
    if any(ord(c) < 32 for c in value):
        raise ValueError('control characters are not permitted in a build recipe')
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def generate(overlay: Path, output: Path, config: dict[str, str], release: str,
             board: str, product: str, distro: str, build_commit: str) -> None:
    lines = recipe(config, release, board, product, distro)
    if not release:
        raise ValueError('FIRMWARE_REBUILD_RELEASE is required for the experimental rebuild menu')
    # Dereference source links before moving the overlay; their relative targets
    # otherwise point outside this private workspace. Never modify source inputs.
    if output.exists():
        shutil.rmtree(output)
    module = output / MODULE
    shutil.copytree(overlay / MODULE, module, symlinks=False)
    includes = MODULE.parent.parent / "Include"
    if (overlay / includes).is_dir():
        shutil.copytree(overlay / includes, output / includes, symlinks=False)
    title = 'Rebuild running firmware'
    help_text = ('Build settings, not saved setup values. Use this build checkout and its '
                 'source refs. Host tools and timestamps can change bytes.')
    # Component identities already appear in this same System Information form.
    # Keep the complete command and orchestration checkout, avoiding duplicate rows.
    display = lines + ['Build checkout: ' + (build_commit or 'direct source build')]
    uni = ['#langdef en-US "English"',
           '#string STR_BUILD_RECIPE_TITLE #language en-US ' + uni_string(title),
           '#string STR_BUILD_RECIPE_HELP #language en-US ' + uni_string(help_text)]
    # recipe() and the private-overlay selection enforce both custom and
    # experimental gates. This inline fragment needs no vendor CPP macros.
    hfr = ['subtitle text = STRING_TOKEN(STR_BUILD_RECIPE_TITLE);']
    for index, line in enumerate(display):
        token = f'STR_BUILD_RECIPE_{index:02d}'
        uni.append(f'#string {token} #language en-US ' + uni_string(line))
        hfr.append(f'  text help = STRING_TOKEN(STR_BUILD_RECIPE_HELP), text = STRING_TOKEN({token});')
    (module / 'BuildRecipe.uni').write_text('\n'.join(uni) + '\n')
    (module / 'BuildRecipe.hfr').write_text('\n'.join(hfr) + '\n')
    command = ' \\\n  '.join(lines) + '\n'
    (output / 'firmware-rebuild.txt').write_text(command)
    (output / 'firmware-rebuild.json').write_text(json.dumps({
        'schema': 1, 'build_commit': build_commit, 'command': command, 'config': config,
    }, indent=2) + '\n')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('overlay', 'output', 'config'):
        parser.add_argument('--' + name, type=Path, required=True)
    for name in ('release', 'board', 'product', 'distro'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--build-commit', default='')
    args = parser.parse_args()
    config = dict(line.split('=', 1) for line in args.config.read_text().splitlines())
    generate(args.overlay, args.output, config, args.release, args.board,
             args.product, args.distro, args.build_commit)


if __name__ == '__main__':
    main()
