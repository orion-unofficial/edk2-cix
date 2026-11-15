"""Exercise maintained STMM Make rules and assembly boundaries on primary sources."""
import os
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

import test_firmware_reconfiguration as firmware
from reconstruction_common import show_file, unofficial_component_steps
from stmm_integration import feature_files, native_rules, selected

ROOT = Path(__file__).resolve().parents[1]
REFS = tuple(f'source/unofficial/1.3.2/edk2-stable{version}' for version in ('202208', '202608'))


class NativeStmmRulesTests(unittest.TestCase):
    def test_renderer_only_assembles_components(self):
        for ref in REFS:
            with self.subTest(ref=ref):
                self.assertTrue(native_rules(ROOT, ref))
                self.assertFalse(selected(ROOT, ref))
                self.assertEqual(feature_files(ROOT, ref), {})
                steps = unofficial_component_steps(ROOT, ref)
                self.assertTrue(all('component' in step for step in steps))
                self.assertIn('src/cix-v1.2/stmm-unofficial', [step['component']['path'] for step in steps])
                for path in ('src/stmm-integration-v1/selection.mk', 'src/stmm-integration-v1/payload.mk'):
                    self.assertEqual(show_file(ROOT, REFS[0], path), show_file(ROOT, REFS[1], path))

    def test_selection_default_guards_payload_and_ffa_rules(self):
        for ref in REFS:
            with self.subTest(ref=ref), patch.dict(os.environ, SOURCE_TEST_REF=ref):
                fixture = firmware.FirmwareReconfigurationTests()
                fixture.setUp()
                try:
                    root = fixture.root
                    component = root / 'src/cix-v1.2/stmm-unofficial'
                    for name in ('ArmPkg/Include/IndustryStandard/ArmFfaBootInfo.h', 'CixStandaloneMmPkg/CixStandaloneMm.dsc', 'CixStandaloneMmPkg/BuildRule202608.template'):
                        path = component / name
                        path.parent.mkdir(parents=True, exist_ok=True)
                        path.touch()
                    probe = root / 'probe.mk'
                    probe.write_text('probe:\n\t@printf "%s\\n" "$(STMM)" "$(CIX_RELEASE_STMM_HELPER)" "$(CIX_RELEASE_STMM_FFA_VERSION)" "$(CIX_RELEASE_V12_TEE_SOURCE_DIR)"\n')
                    command = [fixture.make, '--no-print-directory', '-f', 'Makefile', '-f', str(probe), 'probe']

                    def run(**values):
                        args = {**fixture.base, **values}
                        return subprocess.run(command + [f'{key}={value}' for key, value in args.items()],
                                              cwd=root / 'src', capture_output=True, text=True, timeout=20)

                    for cix, stmm, helper, ffa, tee in (
                            ('', 'vendor', 'stage_vendor_stmm.py', '10', '/tee'),
                            ('1.2', 'vendor', 'stage_vendor_stmm.py', '10', '/tee'),
                            ('unofficial', 'vendor', 'stage_vendor_stmm.py', '10', '/tee-unofficial'),
                            ('unofficial', 'unofficial', 'build_hybrid_stmm.py', '12', '/tee-unofficial')):
                        result = run(CIX_RELEASE=cix, **({'STMM': stmm} if cix else {}))
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertEqual(result.stdout.splitlines()[:3], [stmm, 'scripts/' + helper, ffa])
                        self.assertTrue(result.stdout.splitlines()[3].endswith(tee))
                    for values, error in (
                            ({'CIX_RELEASE': '', 'STMM': 'unofficial'}, 'requires CIX_RELEASE=unofficial'),
                            ({'CIX_RELEASE': '1.2', 'STMM': 'unofficial'}, 'requires CIX_RELEASE=unofficial'),
                            ({'CIX_RELEASE': 'unofficial', 'STMM': 'other'}, 'supports vendor or unofficial'),
                            ({'CIX_RELEASE': '', 'STMM': '%'}, 'supports vendor or unofficial'),
                            ({'CIX_RELEASE': 'unofficial', 'STMM': 'unoff%'}, 'supports vendor or unofficial'),
                            ({'CIX_RELEASE': 'unofficial', 'STMM': 'vendor unofficial'}, 'supports vendor or unofficial'),
                            ({'CIX_RELEASE': 'unofficial', 'STMM': 'unofficial', 'FIRMWARE_BOARD': 'O6N'}, 'O6 RELEASE only'),
                            ({'CIX_RELEASE': 'unofficial', 'STMM': 'unofficial', 'FIRMWARE_TARGET': 'DEBUG'}, 'O6 RELEASE only'),
                            ({'CIX_RELEASE': 'unofficial', 'STMM': 'unofficial', 'DSC': 'edk2-platforms/Platform/Radxa/Orion/O6N/O6N.dsc'}, 'select the O6 DSC')):
                        result = run(**values)
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn(error, result.stderr)
                    (component / 'CixStandaloneMmPkg/CixStandaloneMm.dsc').unlink()
                    result = run(CIX_RELEASE='unofficial', STMM='unofficial')
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('requires the rendered stmm-unofficial component', result.stderr)
                finally:
                    fixture.doCleanups()


if __name__ == '__main__':
    unittest.main()
