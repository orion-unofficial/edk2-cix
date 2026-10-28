"""Prepare exact maintained native preimages; never compile or move source refs."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from reconstruction_common import show_file
from stmm_release_port import CRYPTO_AFTER, CRYPTO_PATH, apply


ROOT = Path(__file__).resolve().parents[1]
POLICY = json.loads((ROOT / 'config/stmm-ci.json').read_text())
FIXTURES = ROOT / 'scripts/tests/fixtures/stmm-source-port'
ARM = 'StandaloneMmPkg/Library/StandaloneMmCoreEntryPoint/Arm/'
DEPENDENCY = 'StandaloneMmPkg/Core/Dependency.c'
HEADER_SHA = 'a7e7ee25035a09602681758f5dcbfdff49390edad83752e704dce6a28701a816'
PATCH_SHA = '211c6af55216dde005fb17749e059579d0baea0abb1d04b9a522db88b4e1b85a'
# Reviewed byte-level postimages, including the original CRLF source endings.
EXPECTED = {
    '202208': {
        DEPENDENCY: 'd2495ed6f7b005532ceb9d73699177880dfc2c3a1b7144cbd570d1038a58761c',
        ARM + 'CixLegacyBootInfo.h': HEADER_SHA,
        ARM + 'StandaloneMmCoreEntryPoint.c': '86373a5b2d70522fb602e58b525782a3b95011a878efb58c205c08f29a882b72',
        ARM + 'CreateHobList.c': 'fe5e64689190809c91aed9e82d8e5e24a28130a9881c47ce9b7e315bfe93a579',
    },
    '202411': {
        ARM + 'CixLegacyBootInfo.h': HEADER_SHA,
        ARM + 'StandaloneMmCoreEntryPoint.c': 'ac5720758ec6cd17b30bd406610ef512d4cd9b8e0bfc54fd49107595223ac3c5',
        ARM + 'CreateHobList.c': 'e3a1a434ce83d0c104130cddec6075518714dfe0c5441492137cd6002ff6887b',
    },
    '202502': {},
    '202608': {CRYPTO_PATH: CRYPTO_AFTER},
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def stage(source, release):
    case = next(row for row in POLICY['cases'] if row['edk2'] == release)
    ref = case['maintained']['commit']
    files = [DEPENDENCY, 'StandaloneMmPkg/Core/StandaloneMmCore.c']
    if release < '202502':
        files += [ARM + name for name in ('StandaloneMmCoreEntryPoint.c', 'CreateHobList.c')]
    if release == '202608':
        files.append(CRYPTO_PATH)
    for name in files:
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(show_file(ROOT, ref, 'src/edk2/' + name))
    for name, target, expected in (
            ('LegacyBootInfo.h', 'LegacyBootInfo.h', HEADER_SHA),
            ('LegacyDepexReadOnly.patch', 'Patches/LegacyDepexReadOnly.patch', PATCH_SHA)):
        data = (FIXTURES / name).read_bytes()
        if digest(data) != expected:
            raise ValueError('reviewed legacy fixture hash differs: ' + name)
        path = source / 'CixStandaloneMmPkg' / target
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    return (show_file(ROOT, POLICY['components']['maintained']['stmm']['commit'], CRYPTO_PATH)
            if release == '202608' else None)


def snapshot(source):
    return {str(path.relative_to(source)): digest(path.read_bytes())
            for path in source.rglob('*') if path.is_file()}


class NativeSourcePortTests(unittest.TestCase):
    def test_exact_ports_on_all_four_maintained_native_preimages(self):
        for release in EXPECTED:
            with self.subTest(release=release), tempfile.TemporaryDirectory(prefix='stmm-port-') as temporary:
                source = Path(temporary)
                crypto = stage(source, release)
                before = snapshot(source)
                result = apply(source, release, crypto)
                self.assertEqual(result, EXPECTED[release])
                after = snapshot(source)
                changed = {name: value for name, value in after.items() if before.get(name) != value}
                self.assertEqual(changed, result)
                self.assertTrue(set(before) <= set(after))
                if release < '202502':
                    entry = (source / (ARM + 'StandaloneMmCoreEntryPoint.c')).read_text()
                    hob = (source / (ARM + 'CreateHobList.c')).read_text()
                    self.assertLess(entry.index('CixValidateLegacyBootInfo ('),
                                    entry.index('PayloadBootInfo = GetAndPrintBootinformation'))
                    self.assertIn('if (HobStart == NULL)', entry)
                    self.assertIn('CixLegacyFlashInfo (PayloadBootInfo), 36)', hob)
                    self.assertEqual('BuildResourceDescriptorHob (' in hob, release == '202208')
                    self.assertNotIn('EFI_DEP_REPLACE_TRUE', (source / DEPENDENCY).read_text())
                    for name in (ARM + 'StandaloneMmCoreEntryPoint.c', ARM + 'CreateHobList.c'):
                        data = (source / name).read_bytes()
                        self.assertIn(b'\r\n', data)
                        self.assertNotIn(b'\n', data.replace(b'\r\n', b''))

    def test_missing_ambiguous_legacy_anchors_and_reapplication_fail(self):
        for mutation in ('missing', 'duplicate', 'reapply'):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory(prefix='stmm-port-') as temporary:
                source = Path(temporary)
                stage(source, '202411')
                entry = source / (ARM + 'StandaloneMmCoreEntryPoint.c')
                anchor = b'  PayloadBootInfo = GetAndPrintBootinformation (SharedBufAddress);'
                if mutation == 'reapply':
                    apply(source, '202411')
                else:
                    replacement = b'/* changed */' if mutation == 'missing' else anchor + b'\r\n' + anchor
                    entry.write_bytes(entry.read_bytes().replace(anchor, replacement))
                message = 'resource-HOB attributes' if mutation == 'reapply' else 'anchor missing or ambiguous'
                with self.assertRaisesRegex(ValueError, message):
                    apply(source, '202411')

    def test_unreviewed_crypto_input_or_preimage_is_rejected(self):
        for mutation in ('missing-fix', 'wrong-fix', 'wrong-preimage'):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory(prefix='stmm-port-') as temporary:
                source = Path(temporary)
                crypto = stage(source, '202608')
                if mutation == 'missing-fix':
                    crypto = None
                elif mutation == 'wrong-fix':
                    crypto += b'changed'
                else:
                    (source / CRYPTO_PATH).write_bytes(b'unreviewed preimage')
                before = snapshot(source)
                with self.assertRaises(ValueError):
                    apply(source, '202608', crypto)
                self.assertEqual(snapshot(source), before)

    def test_wrong_depex_preimage_rejects_exact_patch_without_changing_it(self):
        with tempfile.TemporaryDirectory(prefix='stmm-port-') as temporary:
            source = Path(temporary)
            stage(source, '202208')
            path = source / DEPENDENCY
            data = path.read_bytes().replace(b'#define EFI_DEP_REPLACE_TRUE  0xff',
                                             b'#define EFI_DEP_REPLACE_TRUE  0xfe')
            path.write_bytes(data)
            before = path.read_bytes()
            patch = source / 'CixStandaloneMmPkg/Patches/LegacyDepexReadOnly.patch'
            with self.assertRaises(subprocess.CalledProcessError):
                apply(source, '202208')
            self.assertEqual(path.read_bytes(), before)
            self.assertEqual(digest(patch.read_bytes()), PATCH_SHA)

    def test_unrecognized_resource_hob_and_release_are_rejected(self):
        with tempfile.TemporaryDirectory(prefix='stmm-port-') as temporary:
            source = Path(temporary)
            stage(source, '202411')
            path = source / (ARM + 'CreateHobList.c')
            path.write_bytes(path.read_bytes().replace(b'PayloadBootInfo->SpMemBase', b'OtherMemoryBase'))
            with self.assertRaisesRegex(ValueError, 'resource HOB contents'):
                apply(source, '202411')
            before = snapshot(source)
            with self.assertRaisesRegex(ValueError, 'unsupported'):
                apply(source, '202501')
            self.assertEqual(snapshot(source), before)


if __name__ == '__main__':
    unittest.main()
