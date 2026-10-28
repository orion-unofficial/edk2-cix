"""Check release-native bindings against the retained real source snapshots."""
from pathlib import Path
import json
import re
import tempfile
import unittest

from reconstruction_common import show_file
from stmm_release_platform import NATIVE_DSC, configure


ROOT = Path(__file__).resolve().parents[1]
POLICY = json.loads((ROOT / 'config/stmm-ci.json').read_text())


def stage(source, version):
    case = next(row for row in POLICY['cases'] if row['edk2'] == version)
    ref = case['maintained']['commit']
    native = show_file(ROOT, ref, NATIVE_DSC)
    paths = re.findall(r'(?:StandaloneMmCoreEntryPoint\||^\s*)((?:ArmPkg|StandaloneMmPkg)/[^\s|]+\.inf)',
                       native.decode(), re.M)
    paths += ['BaseTools/Conf/tools_def.template',
              'MdeModulePkg/Library/VarCheckPolicyLib/VarCheckPolicyLibStandaloneMm.inf']
    if version >= '202502':
        paths.append(('ArmPkg' if version == '202502' else 'MdePkg') +
                     '/Include/IndustryStandard/ArmFfaBootInfo.h')
    for name in paths:
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(show_file(ROOT, ref, 'src/edk2/' + name))
    package = source / 'CixStandaloneMmPkg'
    package.mkdir()
    (package / 'CixStandaloneMm.fdf').write_bytes(show_file(
        ROOT, POLICY['components']['maintained']['stmm']['commit'], 'CixStandaloneMmPkg/CixStandaloneMm.fdf'))
    return native


class NativePlatformTests(unittest.TestCase):
    def test_native_crypto_cpu_and_handoff_are_selected_together(self):
        for version in ('202208', '202411', '202502', '202608'):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as temporary:
                source = Path(temporary)
                native = stage(source, version)
                text = native.decode()
                package = source / 'CixStandaloneMmPkg'
                profile = configure(source, native, '1.3.1', version)
                self.assertEqual(profile['required_optee_stmm_ffa_version'], 10 if version < '202502' else 12)
                self.assertEqual(profile['toolchain'], 'GCC5' if version == '202208' else 'GCC')
                generated = (package / 'CixStandaloneMm.dsc').read_text()
                for line in text.splitlines():
                    if any(name in line for name in ('BaseCryptLib|', 'OpensslLib|', 'MbedTlsLib|', 'AuthVariableLib|')):
                        self.assertIn(line.strip(), generated)
                self.assertNotIn('OpTeeRpmbFv.inf', generated)
                self.assertNotIn('FixupPcd.inf', generated)
                self.assertIn('VarCheckPolicyLibStandaloneMm.inf', generated)
                for binding in ('CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLibNull/',
                                'StandaloneMmMmuLib|ArmPkg/Library/StandaloneMmMmuLib/',
                                'PeCoffExtraActionLib|StandaloneMmPkg/Library/StandaloneMmPeCoffExtraActionLib/'):
                    self.assertIn(binding, generated)
                self.assertIn(profile['cpu'], (package / 'CixStandaloneMm.fdf').read_text())
                wrong = '202608' if version < '202502' else '202208'
                with self.assertRaisesRegex(ValueError, 'capabilities differ'):
                    configure(source, native, '1.3.1', wrong)

    def test_both_vendor_sets_and_gcc5_fallback(self):
        for radxa in ('1.2.4', '1.3.1'):
            with self.subTest(radxa=radxa), tempfile.TemporaryDirectory() as temporary:
                source = Path(temporary)
                native = stage(source, '202411')
                tools = source / 'BaseTools/Conf/tools_def.template'
                tools.write_text('*_GCC5_AARCH64_CC_PATH = aarch64-linux-gnu-gcc\n')
                profile = configure(source, native, radxa, '202411')
                self.assertEqual(profile['toolchain'], 'GCC5')
                vendor = 'Vendor/1.2.4' if radxa == '1.2.4' else 'Vendor'
                self.assertEqual(profile['vendor_directory'], vendor)
                fdf = (source / 'CixStandaloneMmPkg/CixStandaloneMm.fdf').read_text()
                self.assertNotIn('$(CIX_STMM_VENDOR_PATH)', fdf)
                self.assertEqual(fdf.count('CixStandaloneMmPkg/' + vendor + '/'), 6)

    def test_malformed_native_bindings_are_rejected_before_writing(self):
        cpu = 'ArmPkg/Drivers/StandaloneMmCpu/StandaloneMmCpu.inf'
        entry = 'ArmPkg/Library/ArmStandaloneMmCoreEntryPoint/ArmStandaloneMmCoreEntryPoint.inf'
        mutations = (
            lambda text: text.replace(cpu, 'ArmPkg/Drivers/Missing.inf'),
            lambda text: text + '\n  ' + cpu + '\n',
            lambda text: text.replace(entry, '../escape.inf'),
            lambda text: text + '\n  StandaloneMmCoreEntryPoint|' + entry + '\n',
            lambda text: text.replace('OUTPUT_DIRECTORY', 'MISSING_OUTPUT'),
            lambda text: text.replace('NULL|Drivers/OpTee/OpteeRpmbPkg/FixupPcd.inf', 'NULL|Missing.inf', 1),
        )
        for index, mutate in enumerate(mutations):
            with self.subTest(index=index), tempfile.TemporaryDirectory() as temporary:
                source = Path(temporary)
                native = stage(source, '202608')
                fdf = source / 'CixStandaloneMmPkg/CixStandaloneMm.fdf'
                before = fdf.read_bytes()
                with self.assertRaises(ValueError):
                    configure(source, mutate(native.decode()).encode(), '1.3.1', '202608')
                self.assertEqual(fdf.read_bytes(), before)
                self.assertFalse(fdf.with_suffix('.dsc').exists())

    def test_invalid_fdf_and_missing_toolchain_are_rejected(self):
        for mutation in ('missing-cpu', 'duplicate-cpu', 'missing-vendor', 'extra-vendor', 'tools', 'policy'):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as temporary:
                source = Path(temporary)
                native = stage(source, '202608')
                fdf = source / 'CixStandaloneMmPkg/CixStandaloneMm.fdf'
                text = fdf.read_text()
                cpu = '  INF ArmPkg/Drivers/StandaloneMmCpu/StandaloneMmCpu.inf'
                if mutation == 'missing-cpu':
                    text = text.replace(cpu, '')
                elif mutation == 'duplicate-cpu':
                    text += '\n' + cpu + '\n'
                elif mutation == 'missing-vendor':
                    text = text.replace('$(CIX_STMM_VENDOR_PATH)', 'Vendor', 1)
                elif mutation == 'extra-vendor':
                    text += '\n$(CIX_STMM_VENDOR_PATH)\n'
                elif mutation == 'tools':
                    (source / 'BaseTools/Conf/tools_def.template').write_text('unsupported tools\n')
                else:
                    (source / 'MdeModulePkg/Library/VarCheckPolicyLib/VarCheckPolicyLibStandaloneMm.inf').unlink()
                fdf.write_text(text)
                with self.assertRaises(ValueError):
                    configure(source, native, '1.3.1', '202608')
                self.assertEqual(fdf.read_text(), text)
                self.assertFalse(fdf.with_suffix('.dsc').exists())

    def test_unsupported_release_and_mixed_handoff_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            native = stage(source, '202608')
            for radxa, edk2 in (('unknown', '202608'), ('1.3.1', '202501')):
                with self.subTest(radxa=radxa, edk2=edk2), self.assertRaisesRegex(ValueError, 'unsupported'):
                    configure(source, native, radxa, edk2)
            legacy = 'StandaloneMmPkg/Library/StandaloneMmCoreEntryPoint/StandaloneMmCoreEntryPoint.inf'
            path = source / legacy
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('legacy entry\n')
            mixed = native.replace(b'ArmPkg/Library/ArmStandaloneMmCoreEntryPoint/ArmStandaloneMmCoreEntryPoint.inf',
                                   legacy.encode())
            with self.assertRaisesRegex(ValueError, 'CPU/core and handoff'):
                configure(source, mixed, '1.3.1', '202608')


if __name__ == '__main__':
    unittest.main()
