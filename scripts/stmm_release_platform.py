#!/usr/bin/env python3
"""Adapt release-native StandaloneMM platform bindings to the CIX NOR layout.

Used by qualify_stmm_releases.py. This prepares compilation candidates only;
legacy CIX handoff adaptations and runtime qualification are separate gates.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import re


NATIVE_DSC = 'src/edk2-platforms/Platform/StandaloneMm/PlatformStandaloneMmPkg/PlatformStandaloneMmRpmb.dsc'
SUPPORTED_EDK2 = {'202208', '202411', '202502', '202608'}
STORAGE_PCDS = ('PcdFlashNvStorageVariableBase64', 'PcdFlashNvStorageFtwWorkingBase64',
                'PcdFlashNvStorageFtwSpareBase64', 'PcdFlashNvStorageVariableSize',
                'PcdFlashNvStorageFtwWorkingSize', 'PcdFlashNvStorageFtwSpareSize')


def replace_one(text: str, pattern: str, replacement: str) -> str:
    result, count = re.subn(pattern, lambda match: replacement, text, flags=re.MULTILINE)
    if count != 1:
        raise ValueError(f'expected one native platform match, found {count}: {pattern}')
    return result


def configure(source: Path, native_dsc: bytes, radxa: str, edk2: str) -> dict:
    """Keep native library implementations; change only CIX platform bindings."""
    if radxa not in {'1.2.4', '1.3.1'} or edk2 not in SUPPORTED_EDK2:
        raise ValueError('unsupported release identity')
    text = native_dsc.decode().replace('\r\n', '\n')
    cpu = re.findall(r'^\s*((?:ArmPkg|StandaloneMmPkg)/Drivers/StandaloneMmCpu/StandaloneMmCpu.inf)\s*$', text, re.M)
    if len(cpu) != 1 or not (source / cpu[0]).is_file():
        raise ValueError('missing or ambiguous native StandaloneMM CPU implementation')
    entry = re.findall(r'^\s*StandaloneMmCoreEntryPoint\|([^\n]+)', text, re.M)
    entries = {'StandaloneMmPkg/Library/StandaloneMmCoreEntryPoint/StandaloneMmCoreEntryPoint.inf',
               'ArmPkg/Library/ArmStandaloneMmCoreEntryPoint/ArmStandaloneMmCoreEntryPoint.inf'}
    if len(entry) != 1 or entry[0].strip() not in entries or not (source / entry[0].strip()).is_file():
        raise ValueError('missing or ambiguous native ARM core entry implementation')
    modern = any((source / package / 'Include/IndustryStandard/ArmFfaBootInfo.h').is_file()
                 for package in ('ArmPkg', 'MdePkg'))
    if modern != (edk2 >= '202502'):
        raise ValueError('source capabilities differ from the selected EDK2 release')
    if any(path.startswith('ArmPkg/') != modern for path in (cpu[0], entry[0].strip())):
        raise ValueError('native CPU/core and handoff capabilities differ')
    text = replace_one(text, r'^\s*FLASH_DEFINITION\s*=.*$',
                       '  FLASH_DEFINITION = CixStandaloneMmPkg/CixStandaloneMm.fdf')
    text = replace_one(text, r'^\s*OUTPUT_DIRECTORY\s*=.*$', '  OUTPUT_DIRECTORY = Build/StandaloneMm')
    text = replace_one(text, r'^\s*SUPPORTED_ARCHITECTURES\s*=.*$', '  SUPPORTED_ARCHITECTURES = AARCH64')
    text = replace_one(text, r'^\s*Drivers/OpTee/OpteeRpmbPkg/OpTeeRpmbFv.inf\s*$', '')
    if text.count('NULL|Drivers/OpTee/OpteeRpmbPkg/FixupPcd.inf') != 2:
        raise ValueError('unexpected native variable/FTW storage bindings')
    text = text.replace('NULL|Drivers/OpTee/OpteeRpmbPkg/FixupPcd.inf', 'NULL|CixStandaloneMmPkg/VariablePcd.inf')
    text = replace_one(text, r'^\s*gEfiSecurityPkgTokenSpaceGuid.PcdUserPhysicalPresence\|.*$',
                       '  gEfiSecurityPkgTokenSpaceGuid.PcdUserPhysicalPresence|FALSE')
    text = replace_one(text, r'^\s*gEfiMdeModulePkgTokenSpaceGuid.PcdVariableStoreSize\|.*$',
                       '  gEfiMdeModulePkgTokenSpaceGuid.PcdVariableStoreSize|0x28000')
    for name in STORAGE_PCDS:
        text = replace_one(text, rf'^\s*gEfiMdeModulePkgTokenSpaceGuid\.{name}\|.*$', '')
    policy = 'MdeModulePkg/Library/VarCheckPolicyLib/VarCheckPolicyLibStandaloneMm.inf'
    if not (source / policy).is_file():
        raise ValueError('native StandaloneMM variable policy constructor is unavailable')
    if policy not in text:
        needle = 'NULL|MdeModulePkg/Library/VarCheckUefiLib/VarCheckUefiLib.inf'
        if text.count(needle) != 1:
            raise ValueError('unexpected variable checks binding')
        text = text.replace(needle, needle + '\n      NULL|' + policy)
    text += '\n[PcdsPatchableInModule]\n'
    for name in STORAGE_PCDS:
        value = '0x28000' if name.endswith('Size') else '0'
        text += f'  gEfiMdeModulePkgTokenSpaceGuid.{name}|{value}\n'
    package = source / 'CixStandaloneMmPkg'
    fdf = (package / 'CixStandaloneMm.fdf').read_text()
    fdf = replace_one(fdf, r'^  INF ArmPkg/Drivers/StandaloneMmCpu/StandaloneMmCpu\.inf$',
                      '  INF ' + cpu[0])
    vendor = 'Vendor/1.2.4' if radxa == '1.2.4' else 'Vendor'
    if fdf.count('$(CIX_STMM_VENDOR_PATH)') != 6:
        raise ValueError('missing or ambiguous vendor FDF bindings')
    fdf = fdf.replace('$(CIX_STMM_VENDOR_PATH)', vendor)
    tools = (source / 'BaseTools/Conf/tools_def.template').read_text()
    toolchain = 'GCC' if re.search(r'^\*_GCC_AARCH64_CC_PATH\s*=', tools, re.M) else 'GCC5'
    if not re.search(rf'^\*_{toolchain}_AARCH64_CC_PATH\s*=', tools, re.M):
        raise ValueError('no supported native AArch64 GCC toolchain definition')
    (package / 'CixStandaloneMm.fdf').write_text(fdf)
    (package / 'CixStandaloneMm.dsc').write_text(text)
    return {'edk2_release': edk2, 'radxa_release': radxa,
            'native_dsc_sha256': hashlib.sha256(native_dsc).hexdigest(),
            'configured_dsc_sha256': hashlib.sha256(text.encode()).hexdigest(),
            'core_entry': entry[0].strip(), 'cpu': cpu[0], 'toolchain': toolchain,
            'required_optee_stmm_ffa_version': 12 if modern else 10,
            'vendor_directory': vendor, 'status': 'prepared-not-qualified'}
