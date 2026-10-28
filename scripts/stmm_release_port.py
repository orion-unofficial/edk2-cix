"""Apply the small CIX platform adaptations to otherwise release-native sources.

Called by the compile-stage runner after native DSC selection. Changes are made
on real source files with exact anchors and preserved original line endings.
No newer EDK2 library implementations are imported into historical releases.
"""
from __future__ import annotations

import hashlib
import os
import re
import subprocess
from pathlib import Path

from stmm_release_platform import SUPPORTED_EDK2


CRYPTO_PATH = 'CryptoPkg/Library/Include/CrtLibSupport.h'
# Exact upstream fix already retained by the CIX component (EDK2 ba43de142c8e).
CRYPTO_BEFORE = 'ca74a28d0003f42643cafc50a26efeef7da750ff6c9067db8985d1ea8ecabf37'
CRYPTO_AFTER = '51a1a7b11ba2227a3bf0e688d59d867522480ee783d3c0c4c88e07c8829800d1'


def replace(path: Path, old: str, new: str) -> None:
    raw = path.read_bytes()
    newline = b'\r\n' if b'\r\n' in raw else b'\n'
    before = old.encode().replace(b'\n', newline)
    after = new.encode().replace(b'\n', newline)
    if raw.count(before) != 1:
        raise ValueError('native source anchor missing or ambiguous: ' + str(path))
    path.write_bytes(raw.replace(before, after))


def apply(source: Path, release: str, crypto_header: bytes | None = None) -> dict:
    if release not in SUPPORTED_EDK2:
        raise ValueError('unsupported native source-port release')
    changed = []
    if release == '202608':
        path = source / CRYPTO_PATH
        if crypto_header is None or hashlib.sha256(crypto_header).hexdigest() != CRYPTO_AFTER:
            raise ValueError('missing exact reviewed native-width CryptoPkg fix')
        current = hashlib.sha256(path.read_bytes()).hexdigest()
        if current not in (CRYPTO_BEFORE, CRYPTO_AFTER):
            raise ValueError('CryptoPkg fix preimage differs from the reviewed upstream source')
        path.write_bytes(crypto_header)
        changed.append(CRYPTO_PATH)
    if release < '202502':
        directory = source / 'StandaloneMmPkg/Library/StandaloneMmCoreEntryPoint/Arm'
        header = directory / 'CixLegacyBootInfo.h'
        header.write_bytes((source / 'CixStandaloneMmPkg/LegacyBootInfo.h').read_bytes())
        include = '#include <Library/Arm/StandaloneMmCoreEntryPoint.h>'
        entry = directory / 'StandaloneMmCoreEntryPoint.c'
        hob = directory / 'CreateHobList.c'
        for path in (entry, hob):
            replace(path, include, include + '\n#include "CixLegacyBootInfo.h"')
        anchor = '  PayloadBootInfo = GetAndPrintBootinformation (SharedBufAddress);'
        replace(entry, anchor, '''  if (!CixValidateLegacyBootInfo (SharedBufAddress, SharedBufSize)) {
    Status = EFI_INVALID_PARAMETER;
    goto finish;
  }

''' + anchor)
        anchor = '  HobStart = CreateHobListFromBootInfo (&CpuDriverEntryPoint, PayloadBootInfo);'
        replace(entry, anchor, anchor + '''
  if (HobStart == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto finish;
  }
''')
        anchor = '  ASSERT (HobStart == (VOID *)(UINTN)PayloadBootInfo->SpHeapBase);'
        replace(hob, anchor, anchor + '''

  // CIX appends its NOR layout to the legacy boot records. Validate it before
  // entry, then expose exactly the packed bytes consumed by the vendor driver.
  {
    STATIC CONST EFI_GUID  NvramGuid = {
      0x4c8167fc, 0x5a9d, 0x468e, { 0xa9, 0x42, 0x86, 0x1f, 0xb5, 0x0a, 0xb5, 0x39 }
    };
    if (BuildGuidDataHob (&NvramGuid, (VOID *)CixLegacyFlashInfo (PayloadBootInfo), 36) == NULL) {
      return NULL;
    }
  }
''')
        core = source / 'StandaloneMmPkg/Core/StandaloneMmCore.c'
        if 'InitializeMmHobList (' in core.read_text():
            # Newer native cores require resource HOBs to describe non-MMRAM.
            # The secure ranges already have the dedicated MMRAM descriptor HOB.
            # Keep the core's overlap check; stop publishing a contradictory HOB.
            declarations = re.findall(r'^  EFI_RESOURCE_ATTRIBUTE_TYPE +Attributes;\n',
                                      hob.read_text(), re.M)
            if len(declarations) != 1:
                raise ValueError('missing native resource-HOB attributes declaration')
            replace(hob, declarations[0], '')
            text = hob.read_text()
            begin = '  // Build a resource descriptor Hob that describes the available physical\n'
            end = '  // Find the size of the GUIDed HOB with MP information\n'
            if text.count(begin) != 1 or text.count(end) != 1:
                raise ValueError('unrecognized native legacy resource HOB producer')
            block = text[text.index(begin):text.index(end)]
            if block.count('BuildResourceDescriptorHob (') != 1 or 'PayloadBootInfo->SpMemBase' not in block:
                raise ValueError('unexpected legacy resource HOB contents')
            replace(hob, block, '')
        dependency = source / 'StandaloneMmPkg/Core/Dependency.c'
        if 'EFI_DEP_REPLACE_TRUE' in dependency.read_text():
            # Mechanically generated exact upstream backport. Do not grant write
            # permission to the FV merely to allow DEPEX evaluation to mutate it.
            patch = source / 'CixStandaloneMmPkg/Patches/LegacyDepexReadOnly.patch'
            env = dict(os.environ, GIT_CEILING_DIRECTORIES=str(source.parent))
            command = ['git', '-C', str(source), 'apply']
            subprocess.run([*command, '--check', str(patch)], env=env, check=True)
            subprocess.run([*command, str(patch)], env=env, check=True)
            changed.append(str(dependency.relative_to(source)))
        changed.extend(str(path.relative_to(source)) for path in (header, entry, hob))
    return {name: hashlib.sha256((source / name).read_bytes()).hexdigest() for name in changed}
