#!/usr/bin/env python3
"""Exercise each distinct retained updater/setup/PPTT source variant in CI."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from reconstruction_common import for_each_ref, show_file

ROOT = Path(__file__).resolve().parents[1]
PLATFORM = 'edk2-platforms/Platform/CIX/Sky1/'
FILES = (
    'custom/overlay/edk2-platforms/Silicon/CIX/Sky1/Library/ConfigParamsDataBlockLib/ConfigParamsDataBlockLib.c',
    'custom/overlay/' + PLATFORM + 'Library/PlatformConfigParamsDataBlockLib/PlatformConfigParamsDataBlockLib.c',
    'src/edk2-platforms/Silicon/CIX/Sky1/Include/Protocol/ConfigParamsManageProtocol.h',
    'src/' + PLATFORM + 'Include/Protocol/PlatformConfigParamsManageProtocol.h',
    'custom/overlay/edk2-platforms/Silicon/CIX/Sky1/Drivers/ConfigParamsManageDxe/ConfigParamsManageDxe.c',
    'custom/overlay/' + PLATFORM + 'Drivers/PlatformConfigParamsManageDxe/PlatformConfigParamsManageDxe.c',
    'custom/overlay/' + PLATFORM + 'Drivers/FirmwareUpdateDxe/FwUpdateProtocolDxe.c',
    'src/' + PLATFORM + 'Drivers/FirmwareUpdateDxe/FirmwareUpdate.h',
    'src/' + PLATFORM + 'Include/Protocol/CixFwUpdateProtocol.h',
    'custom/overlay/' + PLATFORM + 'Include/Library/CixFirmwareStatus.h',
    'custom/overlay/' + PLATFORM + 'Drivers/SystemFirmwareUpdate/SystemFirmwareReportDxe.c',
    'custom/overlay/' + PLATFORM + 'Library/FlashUpdateLib/FlashUpdate.c',
    'custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/PlatformConfigDxe/PlatformConfigDxe.c',
    'custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/SeConfigUpdateDxe/SeConfigUpdateDxe.c',
    'src/' + PLATFORM + 'Library/Acpi/CIX/AcpiPpttLibCIX/PpttGenerator.c',
    'src/edk2/MdePkg/Include/IndustryStandard/Acpi63.h',
    'src/edk2/MdePkg/Include/IndustryStandard/Acpi64.h',
)


class SourceMaintenanceVariantTests(unittest.TestCase):
    @unittest.skipIf(os.environ.get('SOURCE_TEST_REF') or os.environ.get('SOURCE_TEST_ROOT'),
                     'The caller already selected an explicit source variant')
    def test_retained_interfaces_and_cache_structures(self):
        variants = {}
        refs = for_each_ref(ROOT, 'source/unofficial/')
        self.assertTrue(refs, 'No retained Unofficial source refs')
        for ref in refs:
            digest = hashlib.sha256()
            for path in FILES:
                data = show_file(ROOT, ref, path)
                digest.update(len(data).to_bytes(8, 'little'))
                digest.update(data)
            variants.setdefault(digest.hexdigest(), []).append(ref)
        for refs in variants.values():
            with self.subTest(refs=refs):
                result = subprocess.run(
                    [sys.executable, '-m', 'unittest', 'test_firmware_update_safety',
                     'test_setup_remaining_paths', 'test_pptt_cache_policy', 'test_config_protocol_bounds'],
                    cwd=ROOT / 'scripts',
                    env={**os.environ, 'SOURCE_TEST_REF': refs[0],
                         'TMPDIR': str(Path(tempfile.gettempdir()).resolve())},
                    capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
