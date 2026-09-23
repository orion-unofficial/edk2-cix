#!/usr/bin/env python3
"""Exercise the actual custom ASL flag recipe with both firmware-fix states."""

import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from reconstruction_common import for_each_ref, show_file

ROOT = Path(__file__).resolve().parents[1]
HEADER = 'custom/overlay/edk2-platforms/Platform/CIX/Sky1/Include/AcpiGraph.h'


class CustomAslFlagsTests(unittest.TestCase):
    def test_graph_header_and_fix_gate_follow_effective_source_recipe(self):
        candidates = os.environ.get('SOURCE_ASL_CANDIDATES')
        refs = ([row['new'] for row in json.loads(Path(candidates).read_text())]
                if candidates else for_each_ref(ROOT, 'source/unofficial/'))
        with tempfile.TemporaryDirectory(prefix='custom-asl-flags-') as temporary:
            root = Path(temporary)
            header = root / HEADER
            header.parent.mkdir(parents=True)
            validator = root / 'layout.py'
            validator.write_text('print(1)\n')
            for ref in refs:
                source = show_file(ROOT, ref, 'src/Makefile').decode()
                start = source.index('\tif [[ "$(ARTEFACT_MODE)" == "custom" ]]; then \\\n\t\tlto_flag=')
                end = source.index('\tif [[ "$${#tool_def_overrides[@]}"', start)
                recipe = source[start:end]
                header.write_bytes(show_file(ROOT, ref, HEADER))
                tag = re.search(r'build\s+-a\s+AARCH64\s+-t\s+(\w+)', source).group(1)
                for mode in ('custom', 'upstream'):
                    for fixes in ('TRUE', 'FALSE'):
                        with self.subTest(ref=ref, mode=mode, fixes=fixes):
                            values = dict(ARTEFACT_MODE=mode, ENABLE_FIRMWARE_FIXES_NORMALIZED=fixes,
                                          ENABLE_CORE_ORDER_NORMALIZED='cix', V='0',
                                          CUSTOM_OVERLAY_ROOT=str(root / 'custom/overlay'),
                                          FIRMWARE_CHAIN_VALIDATOR=str(validator), REPO_ROOT=str(root),
                                          WORKSPACE=str(root), ARCHCC_FLAGS='', PLATFORM_FLAGS='',
                                          DEBUG_ALLOW_LARGE_IMAGE='0')
                            # Let Make expand its own functions, including the
                            # consent default, rather than approximating them
                            # with a regex that only understands $(NAME).
                            probe = root / 'recipe.mk'
                            probe.write_text(
                                'SHELL := bash\n.ONESHELL:\n.SHELLFLAGS := -eu -c\n'
                                + ''.join(f'{key} := {value}\n' for key, value in values.items())
                                + '.PHONY: check\ncheck:\n\t@tool_def_overrides=(); \\\n'
                                + recipe + '\tprintf "%s\\n" "$${tool_def_overrides[@]}"\n'
                            )
                            run = subprocess.run(['make', '--no-print-directory', '-f', str(probe)],
                                                 text=True, capture_output=True)
                            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                            flags = dict(line.strip().split(' = ', 1) for line in run.stdout.splitlines() if line.strip())
                            flags = {key.strip(): value for key, value in flags.items()}
                            if mode == 'upstream':
                                self.assertEqual(flags, {})
                                continue
                            debug = flags[f'DEBUG_{tag}_AARCH64_ASLPP_FLAGS']
                            release = flags[f'RELEASE_{tag}_AARCH64_ASLPP_FLAGS']
                            self.assertEqual(debug, release)
                            options = shlex.split(release.replace('DEF(GCC_ASLPP_FLAGS)', ''))
                            text = '#include <AcpiGraph.h>\n' + r'CIX_GRAPH_REMOTE3(\_SB.I2C1.PD10, "port@0", "endpoint@0", EP02)' + '\n'
                            result = subprocess.run(['cc', '-E', '-P', '-x', 'c', '-', *options],
                                                    input=text, text=True, capture_output=True)
                            self.assertEqual(result.returncode, 0, result.stderr)
                            if fixes == 'TRUE':
                                self.assertEqual(json.loads(result.stdout.strip()), r'\_SB.I2C1.PD10.EP02')
                            else:
                                self.assertIn(r'Package () { \_SB.I2C1.PD10, "port@0", "endpoint@0" }', result.stdout)
                                self.assertNotIn('-DENABLE_FIRMWARE_FIXES', release)


if __name__ == '__main__':
    unittest.main()
