#!/usr/bin/env python3
"""Run the firmware CPPC/GICC initialization path with host service mocks."""
import os
import re
import subprocess
import unittest

from test_firmware_acpi_runtime import PREAMBLE, ROOT, execute, repo_source
from reconstruction_common import (
    active_unofficial_source_ref, unofficial_checkpoints_by_edk2,
    unofficial_source_ref, resolve_ref,
)

PATH = 'edk2-platforms/Platform/CIX/Sky1/Drivers/ConfigurationManagerDxe/ConfigurationManager.c'


def function(text, name):
    start = text.index('\n' + name + ' (')
    return 'STATIC\nEFI_STATUS\nEFIAPI' + text[start:text.index('\n}', start) + 2]


HARNESS = PREAMBLE + r'''
#include <stdlib.h>
#define EFIAPI
#define FALSE 0
#define TRUE 1
#define PLAT_CPU_COUNT 12
#define PLAT_GIC_CPU_INTERFACE {{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1}}
#define PLAT_CPC_INFO_PCC {200,201,202,203,204,205,206,207,208,209,210,211}
#define EfiBootServicesData 4
#define EFI_ACPI_6_2_GIC_ENABLED 1
#define DEBUG(x) do {} while(0)
#define ASSERT(x) assert(x)
#define ZeroMem(p,n) memset(p,0,n)
#define CopyMem(d,s,n) memcpy(d,s,n)
#define CM_NULL_TOKEN NULL
#define CPPC_DISABLE 0
#define CPPC_PCC 1
#define CPPC_FAST_CHANNEL 2
#define EVT_NOTIFY_SIGNAL 3
#define TPL_CALLBACK 4
typedef void *EFI_EVENT;
typedef UINT32 CM_ARCH_COMMON_CPC_INFO;
typedef UINT32 CM_ARM_CPC_INFO;
typedef int CpuTopology;
typedef struct {struct {UINT32 CpuCppcType;} Misc;} CONFIG_PARAMS_DATA_BLOCK;
typedef struct {CONFIG_PARAMS_DATA_BLOCK *Data;} CIX_CONFIG_PARAMS_MANAGE_PROTOCOL;
typedef void *CM_OBJECT_TOKEN;
typedef struct {UINT32 CPUInterfaceNumber,AcpiProcessorUid,Flags;CM_OBJECT_TOKEN CpcToken;} CM_ARM_GICC_INFO;
typedef struct {UINT32 Uid,Coreid,Enable;} CIX_CPU_CORE;
typedef struct {UINT32 CoreNumber;CIX_CPU_CORE *Core;} CIX_CLUSTER_TOPO;
typedef struct {UINT32 ClusterNumber;CIX_CLUSTER_TOPO *ClusterTopo;} CM_CIX_CPU_TOPO_INFO;
typedef struct {CM_CIX_CPU_TOPO_INFO *CpuTopoInfo;CM_ARM_GICC_INFO *GicCInfo;UINT32 CpuUidtoCoreNumberMap[12];CM_ARCH_COMMON_CPC_INFO CpuCpcInfo[12];} SKY1_PLATFORM_REPOSITORY_INFO;
typedef struct {SKY1_PLATFORM_REPOSITORY_INFO *PlatRepoInfo;} EDKII_CONFIGURATION_MANAGER_PROTOCOL;
static CONFIG_PARAMS_DATA_BLOCK config;
static CIX_CONFIG_PARAMS_MANAGE_PROTOCOL config_manage={&config};
static int gCixConfigParamsManageProtocolGuid, gEfiEventExitBootServicesGuid;
static int event_count;
static void SetCpuMaxFreqOnExitBootService(void) {}
static void GetValidCpuCoreNum(UINT8 *out) {*out=12;}
static EFI_STATUS GetCpuTopology(CpuTopology **out) {*out=NULL;return 0;}
static EFI_STATUS InitializeCmCixCpuTopoInfo(const EDKII_CONFIGURATION_MANAGER_PROTOCOL *p, CONFIG_PARAMS_DATA_BLOCK *c) {(void)p;assert(c==&config);return 0;}
static EFI_STATUS InitializeCmArmCpcInfo(const EDKII_CONFIGURATION_MANAGER_PROTOCOL *p) {for(UINT32 i=0;i<12;i++)p->PlatRepoInfo->CpuCpcInfo[i]=100+i;return 0;}
static EFI_STATUS allocate(int kind,size_t size,void **out) {assert(kind==4);*out=malloc(size);assert(*out);return 0;}
static EFI_STATUS locate(void *guid,void *registration,void **out) {assert(guid==&gCixConfigParamsManageProtocolGuid);assert(!registration);*out=&config_manage;return 0;}
static EFI_STATUS event(int type,int tpl,void (*callback)(void),void *context,void *guid,EFI_EVENT *out) {assert(type==EVT_NOTIFY_SIGNAL && tpl==TPL_CALLBACK);assert(callback==SetCpuMaxFreqOnExitBootService);assert(!context && guid==&gEfiEventExitBootServicesGuid);*out=NULL;event_count++;return 0;}
static struct {EFI_STATUS (*AllocatePool)(int,size_t,void **);EFI_STATUS (*LocateProtocol)(void*,void*,void**);EFI_STATUS (*CreateEventEx)(int,int,void(*)(void),void*,void*,EFI_EVENT*);} services={allocate,locate,event}, *gBS=&services;
'''


class FirmwareCppcRuntimeTests(unittest.TestCase):
    def test_physical_tokens_and_disabled_mode_in_every_core_order(self):
        variants = {}
        if os.environ.get('SOURCE_TEST_ROOT') or os.environ.get('SOURCE_TEST_REF'):
            variants[os.environ.get('SOURCE_TEST_REF', 'local')] = (
                repo_source('custom/overlay/' + PATH), repo_source('src/' + PATH))
        else:
            refs = {
                active_unofficial_source_ref(ROOT, radxa, edk2)
                or unofficial_source_ref(ROOT, radxa, edk2)
                for edk2, checkpoints in unofficial_checkpoints_by_edk2(ROOT).items()
                for radxa in checkpoints
            }
            self.assertTrue(refs, 'no selected Unofficial source checkpoints')
            for ref in sorted(refs):
                resolved = resolve_ref(ROOT, ref)
                variants[ref] = tuple(subprocess.check_output(
                    ['git', '-C', str(ROOT), 'show', resolved + ':' + prefix + PATH], text=True)
                    for prefix in ('custom/overlay/', 'src/'))
        seen = set()
        for ref, (custom, vendor) in variants.items():
            gicc = function(custom, 'InitializeCmArmGiccInfo')
            init = function(custom, 'InitializePlatformRepository')
            vendor_tokens = 'GicCInfo[GicCIndex].CpcToken' in function(vendor, 'InitializeCmArmGiccInfo')
            key = (gicc, init, vendor_tokens)
            if key in seen:
                continue
            seen.add(key)
            # This is the actual global initializer, not a test-selected CPPC state.
            state = re.search(r'BOOLEAN\s+CppcEnable\s*=\s*TRUE\s*;', custom).group().replace('BOOLEAN', 'static int')
            for order, mapping in (
                    ('CIX', [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1]),
                    ('CONVENTIONAL', list(range(12))),
                    ('PERFORMANCE', [8, 9, 10, 11, 4, 5, 6, 7, 2, 3, 0, 1])):
                for fixes in ((0, 1) if order == 'CIX' else (1,)):
                    with self.subTest(ref=ref, order=order, fixes=fixes):
                        flags = '' if order == 'CIX' else '#define ENABLE_CORE_ORDER_' + order + ' 1\n'
                        main = r'''
int main(void) {
  fixes=FIXES;
  UINT32 map[12]={MAPPING};
  CIX_CPU_CORE cores[12];
  CIX_CLUSTER_TOPO clusters[3]={{4,cores},{4,cores+4},{4,cores+8}};
  CM_CIX_CPU_TOPO_INFO topo={3,clusters};
  SKY1_PLATFORM_REPOSITORY_INFO repo={.CpuTopoInfo=&topo};
  EDKII_CONFIGURATION_MANAGER_PROTOCOL protocol={&repo};
  for(UINT32 i=0;i<12;i++) cores[i]=(CIX_CPU_CORE){map[i],i,i!=7};
  for(UINT32 mode=CPPC_DISABLE;mode<=CPPC_FAST_CHANNEL;mode++) {
    CppcEnable=TRUE;
    config.Misc.CpuCppcType=mode;
    event_count=0;
    memset(repo.CpuCpcInfo,0,sizeof(repo.CpuCpcInfo));
    assert(InitializePlatformRepository(&protocol)==EFI_SUCCESS);
    assert(event_count==(mode==CPPC_DISABLE));
    for(UINT32 i=0;i<12;i++) {
      UINT32 slot=SLOT;
      assert(repo.GicCInfo[slot].AcpiProcessorUid==map[i]);
      assert(repo.CpuUidtoCoreNumberMap[slot]==i);
      assert(repo.GicCInfo[slot].Flags==(i!=7));
      int token_expected=fixes ? mode!=CPPC_DISABLE : VENDOR_TOKENS;
      assert(repo.GicCInfo[slot].CpcToken==(token_expected ? (CM_OBJECT_TOKEN)&repo.CpuCpcInfo[i] : CM_NULL_TOKEN));
      assert(repo.CpuCpcInfo[i]==(mode==CPPC_PCC ? 200+i : mode==CPPC_FAST_CHANNEL ? 100+i : 0));
    }
    free(repo.GicCInfo);
  }
  return 0;
}
'''
                        for name, value in {'FIXES': str(fixes), 'MAPPING': ','.join(map(str, mapping)),
                                            'SLOT': 'i' if order == 'CIX' else 'map[i]',
                                            'VENDOR_TOKENS': str(int(vendor_tokens))}.items():
                            main = main.replace(name, value)
                        execute(flags + HARNESS + state + gicc + init + main)


if __name__ == '__main__':
    unittest.main()
