"""Render the versioned StandaloneMM adapter without replacing firmware recipes."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re

FEATURE_REF = 'source/unofficial/stmm/integration-v1'


def compatibility(repo: Path) -> dict:
    from reconstruction_common import show_file
    return json.loads(show_file(repo, FEATURE_REF, 'profile.json'))


def selected(repo: Path, source_ref: str) -> bool:
    if not (repo / "config/refs-cix.json").is_file():
        return False
    from reconstruction_common import ref_manifest_records
    if not any(record.get('ref') == FEATURE_REF for record in ref_manifest_records(repo, 'refs-cix.json')):
        return False
    # VERSION in an old checkpoint can precede the release metadata overlay.
    from validate_release_inputs import baseline_release
    from reconstruction_common import show_file
    recipe = show_file(repo, source_ref, 'src/Makefile', check=False)
    # Earlier 1.2.4 recipes lack the reviewed signing/staging contract.
    return (baseline_release(repo, source_ref) in compatibility(repo)['firmware_releases']
            and b'CIX_RELEASE_CACHE_HELPER :=' in recipe
            and b'CIX_RELEASE_SIGNING_KEYS_DIR :=' in recipe)


def adapt_makefile(data: bytes, path: str) -> bytes:
    text = data.decode()
    # All retained recipes put selection guards before the first root variable.
    markers = {'Makefile': 'ROOT_MAKEFILE :=', '.github/local/Makefile.local': 'LOCAL_MAKEFILE :=',
               'src/Makefile': 'FIRMWARE_TARGET ?='}
    marker = markers[path]
    start = text.index(marker)
    guard = '''STMM ?= vendor
ifeq ($(filter $(strip $(STMM)),vendor unofficial),)
$(error STMM supports vendor or unofficial)
endif
ifeq ($(strip $(STMM)),unofficial)
ifneq ($(strip $(CIX_RELEASE)),unofficial)
$(error STMM=unofficial requires CIX_RELEASE=unofficial)
endif
ifneq ($(or $(FIRMWARE_BOARD),O6):$(or $(FIRMWARE_TARGET),RELEASE),O6:RELEASE)
$(error STMM=unofficial supports O6 RELEASE only)
endif
endif
ifneq ($(strip $(CIX_RELEASE)),)
ifeq ($(filter 1.2 v1.2 V1.2 unofficial,$(strip $(CIX_RELEASE))),)
$(error CIX_RELEASE supports 1.2 or unofficial)
endif
ifneq ($(or $(ARTEFACT_MODE),custom),custom)
$(error CIX_RELEASE requires ARTEFACT_MODE=custom)
endif
endif

'''
    root = {'Makefile': '.', '.github/local/Makefile.local': '../..', 'src/Makefile': '..'}[path]
    guard += f"""STMM_FEATURE_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/{root})
ifeq ($(strip $(STMM)),unofficial)
ifeq ($(wildcard $(STMM_FEATURE_ROOT)/src/cix-v1.2/stmm-unofficial/ArmPkg/Include/IndustryStandard/ArmFfaBootInfo.h)$(wildcard $(STMM_FEATURE_ROOT)/src/cix-v1.2/stmm-unofficial/MdePkg/Include/IndustryStandard/ArmFfaBootInfo.h),)
$(error STMM=unofficial requires the rendered modern secure-world component with FF-A boot information)
endif
endif

"""
    text = guard + text[start:]
    if '--stmm "$(STMM)"' not in text.split('FIRMWARE_LAYOUT_QUERY_ARGS =', 1)[-1].split('\n\n', 1)[0]:
        text = text.replace('FIRMWARE_LAYOUT_QUERY_ARGS = \\\n', 'FIRMWARE_LAYOUT_QUERY_ARGS = \\\n\t--stmm "$(STMM)" \\\n')
    text = text.replace('CIX_RELEASE="$(CIX_RELEASE)"', 'CIX_RELEASE="$(CIX_RELEASE)" STMM="$(STMM)"')
    if path != 'src/Makefile':
        return text.encode()
    text = text.replace('$(filter 1.2,$(CIX_RELEASE_NORMALIZED))', '$(filter 1.2 unofficial,$(CIX_RELEASE_NORMALIZED))')
    text = text.replace('ifeq ($(ARTEFACT_MODE):$(CIX_RELEASE_NORMALIZED),custom:1.2)',
                        'ifneq ($(and $(filter custom,$(ARTEFACT_MODE)),$(filter 1.2 unofficial,$(CIX_RELEASE_NORMALIZED))),)')
    text = text.replace('"$(CIX_RELEASE_NORMALIZED)" != "1.2"', '-z "$(CIX_RELEASE_NORMALIZED)"')
    text = text.replace('"$(CIX_RELEASE_NORMALIZED)" == "1.2"', '-n "$(CIX_RELEASE_NORMALIZED)"')
    text = re.sub(r'^CIX_RELEASE_V12_TEE_SOURCE_DIR := .*$',
                  'CIX_RELEASE_V12_TEE_SOURCE_DIR := $(CIX_RELEASE_SOURCE_DIR)/$(if $(filter unofficial,$(CIX_RELEASE_NORMALIZED)),tee-unofficial,tee)', text, flags=re.M)
    begin = text.index('CIX_RELEASE_BOOTLOADER2_HELPER :=')
    end = text.index('CIX_RELEASE_CACHE_HELPER :=', begin)
    text = text[:begin] + '''CIX_RELEASE_BOOTLOADER2_HELPER := scripts/build_cix_release_bootloader2.sh
CIX_RELEASE_STMM_HELPER := $(if $(filter unofficial,$(STMM)),scripts/build_hybrid_stmm.py,scripts/stage_vendor_stmm.py)
CIX_RELEASE_STMM_SOURCE := $(abspath $(CIX_RELEASE_SOURCE_DIR)/stmm-unofficial)
CIX_RELEASE_STMM_FFA_VERSION := $(if $(filter unofficial,$(STMM)),12,10)
CIX_RELEASE_STMM_MANIFEST := $(CIX_RELEASE_V12_PAYLOAD_DIR)/standalone-mm/manifest.json
CIX_RELEASE_STMM_ARGS = $(if $(filter unofficial,$(STMM)),--source "$(CIX_RELEASE_STMM_SOURCE)" --edk2-source "$(CIX_RELEASE_STMM_SOURCE)" --native-dsc "$(abspath stmm-integration-v1/PlatformStandaloneMmRpmb.dsc)" --edk2-release 202608 --cache-root "$(CIX_RELEASE_CACHE_ROOT)/stmm" --cross-compile "$(GCC_AARCH64_REAL_PREFIX)$(GCC5_AARCH64_REAL_PREFIX)" --jobs "$(HELPER_BUILD_JOBS)")
''' + text[end:]
    # Keep GCC5, board DSC/FDF, packaging and signing rules release-owned.
    stamp = text.index('# Hash the selected sources on every build')
    tail = text[stamp:]
    if '@python3 "$(CIX_RELEASE_STMM_HELPER)"' not in tail:
        tail = tail.replace('\t@python3 "$(CIX_RELEASE_CACHE_HELPER)"', '''\t@python3 "$(CIX_RELEASE_STMM_HELPER)" $(CIX_RELEASE_STMM_ARGS) \\
\t\t--manifest "$(CIX_RELEASE_STMM_MANIFEST)" \\
\t\t--vendor-fip "$(PACKAGE_TOOL)/Firmwares/bootloader2.img" \\
\t\t--output "$(call BUILD_STAGE_DIR,$*)/Firmwares/BL32_AP_EFI_STMM.fd"
\t@python3 "$(CIX_RELEASE_CACHE_HELPER)"''', 1)
    tail = tail.replace('@python3 "$(CIX_RELEASE_STMM_HELPER)" \\',
                        '@python3 "$(CIX_RELEASE_STMM_HELPER)" $(CIX_RELEASE_STMM_ARGS) \\')
    if '--stmm-ffa-version' not in tail:
        tail = tail.replace('--stmm-path "$(abspath $(call BUILD_STAGE_DIR,$*))/Firmwares/BL32_AP_EFI_STMM.fd" \\\n',
                            '--stmm-path "$(abspath $(call BUILD_STAGE_DIR,$*))/Firmwares/BL32_AP_EFI_STMM.fd" \\\n\t\t--stmm-ffa-version "$(CIX_RELEASE_STMM_FFA_VERSION)" \\\n')
    tail = re.sub(r'^\s*--stmm "\$\(STMM\)"[^\n]*\n', '', tail, flags=re.M)
    tail = tail.replace('--vendor-fip "$(abspath $(PACKAGE_TOOL)/Firmwares/bootloader2.img)" \\\n',
                        '--vendor-fip "$(abspath $(PACKAGE_TOOL)/Firmwares/bootloader2.img)" \\\n\t\t--stmm "$(STMM)" --stmm-source "$(CIX_RELEASE_STMM_SOURCE)" --stmm-edk2-release 202608 \\\n')
    text = text[:stamp] + tail
    if "'STMM=$(STMM)'" not in text:
        text = text.replace("'CIX_RELEASE=$(CIX_RELEASE_NORMALIZED)'", "'CIX_RELEASE=$(CIX_RELEASE_NORMALIZED)' \\\n\t\t'STMM=$(STMM)'")
    return text.encode()


def feature_files(repo: Path, source_ref: str) -> dict[str, tuple[str, bytes]]:
    from reconstruction_common import git, show_file
    result = {}
    listing = git(repo, 'ls-tree', '-r', FEATURE_REF).stdout.splitlines()
    for line in listing:
        meta, path = line.split('\t')
        if path.startswith('overlay/'):
            result[path.removeprefix('overlay/')] = (meta.split()[0], show_file(repo, FEATURE_REF, path))
    profile = compatibility(repo)
    for path in ('Makefile', '.github/local/Makefile.local', 'src/Makefile'):
        data = show_file(repo, source_ref, path)
        if hashlib.sha256(data).hexdigest() not in profile['recipe_preimages'][path]:
            from reconstruction_common import ReconstructionError
            raise ReconstructionError(f'unreviewed StandaloneMM recipe preimage: {source_ref}:{path}')
        result[path] = ('100644', adapt_makefile(data, path))
    # Preserve each release's compiler suffix and other validation contracts.
    for path in ('scripts/validate_make_inputs.py', 'scripts/firmware_layout.py'):
        text = show_file(repo, source_ref, path).decode()
        text = text.replace('VALID_CIX_RELEASES = {"1.2"}', 'VALID_CIX_RELEASES = {"1.2", "unofficial"}')
        text = text.replace('VALID_CIX_RELEASES = {"", "1.2"}', 'VALID_CIX_RELEASES = {"", "1.2", "unofficial"}')
        text = text.replace('normalized_cix_release == "1.2"', 'normalized_cix_release in VALID_CIX_RELEASES')
        if path.endswith('firmware_layout.py') and '    stmm: str' not in text:
            text = text.replace('    cix_release: str = ""', '    cix_release: str = ""\n    stmm: str = "vendor"')
            text = text.replace('parts.append("cix")', 'parts.append("cix_unofficial" if self.cix_release == "unofficial" else "cix")\n            if self.stmm == "unofficial":\n                parts.append("stmm_unofficial")')
            text = text.replace('            tokens.append("cix")', '            tokens.append("cix")\n            if self.stmm == "unofficial":\n                tokens.append("stmm_unofficial")')
            text = text.replace('        cix_release=getattr(args, "cix_release", ""),', '        cix_release=getattr(args, "cix_release", ""),\n        stmm=getattr(args, "stmm", "vendor"),')
            text = text.replace('    parser.add_argument("--cix-release")', '    parser.add_argument("--cix-release")\n    parser.add_argument("--stmm", choices=("vendor", "unofficial"), default="vendor")')
        result[path] = ('100755', text.encode())
    path = 'scripts/build_firmware_variants.py'
    text = show_file(repo, source_ref, path).decode()
    text = text.replace('"buildbox-firmware-stage",',
                        '"firmware-stage" if os.environ.get("EDK2_CIX_IN_BUILDBOX") == "1" else "buildbox-firmware-stage",')
    anchor = '    result = subprocess.run(\n        command,\n        cwd=repo_root,'
    replacement = """    if os.environ.get("EDK2_CIX_IN_BUILDBOX") == "1":
        command = ["python3", str(repo_root / "scripts/ccache_stats.py"), "--cache-dir", ccache_dir]
        if ccache_bin:
            command.extend(["--ccache-bin", ccache_bin])
""" + anchor
    text = text.replace(anchor, replacement, 1)
    result[path] = ('100755', text.encode())
    # A direct rendered-worktree build locates its owning build checkout.
    # Exported trees can provide the infrastructure location explicitly.
    result['scripts/run_in_buildbox.sh'] = ('100755', b'''#!/usr/bin/env bash
set -euo pipefail
firmware_root="$(cd -- "$(dirname -- "$0")/.." && pwd -P)"
infra_root="${EDK2_CIX_BUILD_INFRA_ROOT:-}"
if [[ -z "$infra_root" ]]; then
    common="$(git -C "$firmware_root" rev-parse --path-format=absolute --git-common-dir)"
    infra_root="$(dirname -- "$common")"
fi
if [[ "$infra_root" == "$firmware_root" || ! -x "$infra_root/scripts/run_in_buildbox.sh" ]]; then
    printf '%s\n' 'Set EDK2_CIX_BUILD_INFRA_ROOT to the build infrastructure checkout for container builds.' >&2
    exit 1
fi
export EDK2_CIX_FIRMWARE_ROOT="$firmware_root"
exec "$infra_root/scripts/run_in_buildbox.sh" "$@"
''')
    return result


def capability_problems(repo: Path, source_ref: str) -> list[str]:
    """Check the normal-world communication ABI, independent of the secure EDK2."""
    from reconstruction_common import show_file
    paths = (
        'src/edk2/MdePkg/Include/Protocol/MmCommunication2.h',
        'src/edk2/MdeModulePkg/Include/Guid/SmmVariableCommon.h',
        'src/edk2-non-osi/Platform/CIX/Sky1/Drivers/MmCommunicationDxe/ArmMmCommunication.efi',
        'src/edk2-non-osi/Platform/CIX/Sky1/Drivers/MmCommunicationDxe/ArmMmCommunication.inf',
    )
    problems = [f'missing normal-world MM communication prerequisite: {path}'
                for path in paths if not show_file(repo, source_ref, path, check=False)]

    def variable_abi(header):
        text = re.sub(r'/\*.*?\*/|//[^\n]*', '', header.decode(), flags=re.S)
        structures = re.findall(r'typedef\s+struct\s*\{.*?\}\s*\w+\s*;', text, re.S)
        functions = re.findall(r'#define\s+SMM_VARIABLE_FUNCTION_\w+[^\n]*', text)
        return [' '.join(item.split()) for item in structures + functions]

    normal = show_file(repo, source_ref, paths[1], check=False)
    modern = show_file(repo, 'source/unofficial/stmm/edk2-stable202608',
                       'MdeModulePkg/Include/Guid/SmmVariableCommon.h')
    if not variable_abi(normal) or variable_abi(normal) != variable_abi(modern):
        problems.append('normal-world variable message ABI differs from the modern StandaloneMM service')
    from validate_release_inputs import baseline_release
    release = baseline_release(repo, source_ref)
    row = compatibility(repo)['firmware_releases'][release]
    for key, path in (
        ('mm_communication_sha256', paths[2]),
        ('trusted_fip_sha256', 'src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/Firmwares/bootloader2.img'),
    ):
        if hashlib.sha256(show_file(repo, source_ref, path)).hexdigest() != row[key]:
            problems.append(f'unsupported {release} MM communication or vendor-service identity: {path}')
    return problems
