#!/usr/bin/env python3
"""Execute the real MADT ordering code and model Linux ARM64 enumeration."""
import io
import os
import re
from pathlib import Path
from platform import machine
import tarfile
import tempfile
import subprocess
import unittest

from reconstruction_common import resolve_ref, unofficial_firmware_refs, unofficial_release_edk2_refs
from test_firmware_acpi_runtime import PREAMBLE, ROOT, execute, repo_source

PATH = 'custom/overlay/edk2-platforms/Platform/CIX/Sky1/Library/Acpi/CIX/AcpiMadtLibCIX/MadtGenerator.c'


def function(text, name):
    start = text.index('STATIC\nEFI_STATUS\n' + name + ' (')
    return text[start:text.index('\n}', start) + 2]


HARNESS = PREAMBLE + r'''
#include <stdlib.h>
#define TRUE 1
#define FALSE 0
#define EFI_ACPI_6_2_GIC_ENABLED 1
#define PLAT_CPU_COUNT 12
#define GET_MPID(c,k) (((c)<<8)|(k))
#define ARM_CORE_AFF0 0xff
#define ARM_CORE_AFF1 (0xff<<8)
#define ARM_CORE_AFF2 (0xff<<16)
#define ARM_CORE_AFF3 (0xffULL<<32)
#define ASSERT(x) assert(x)
typedef int BOOLEAN;
typedef struct {UINT64 MPIDR; UINT32 AcpiProcessorUid,Flags; void *CpcToken; UINT32 Cookie;} CM_ARM_GICC_INFO;
typedef CM_ARM_GICC_INFO EFI_ACPI_6_3_GIC_STRUCTURE;
static UINT8 boot_core;
static UINT32 writes;
static UINT64 ArmReadMpidr(void) {return ((UINT64)boot_core<<8)|(1ULL<<31);}
static BOOLEAN IsAcpiUidEqual(const void *a,const void *b,UINT32 i,UINT32 j) {
  (void)i;(void)j;
  return ((const CM_ARM_GICC_INFO*)a)->AcpiProcessorUid==((const CM_ARM_GICC_INFO*)b)->AcpiProcessorUid;
}
static BOOLEAN FindDuplicateValue(const void *p,UINT32 n,size_t size,BOOLEAN (*equal)(const void*,const void*,UINT32,UINT32)) {
  for(UINT32 i=0;i<n;i++)for(UINT32 j=0;j<i;j++)
    if(equal((const char*)p+i*size,(const char*)p+j*size,i,j))return TRUE;
  return FALSE;
}
static void AddGICC(EFI_ACPI_6_3_GIC_STRUCTURE *out,const CM_ARM_GICC_INFO *in,UINT8 rev) {
  assert(rev==5);*out=*in;writes++;
}
'''

MAIN = r'''
int main(void) {
  (void)fixes;(void)ArmReadMpidr;
  const UINT32 rank[12]={10,11,8,9,6,7,4,5,0,1,2,3};
  CM_ARM_GICC_INFO in[13],saved[13],out[13];
  UINT32 token[12];
  for(UINT32 i=0;i<12;i++) {
    // Deliberately reverse input order, use non-dense UIDs and unique metadata.
    UINT32 core=11-i;
    in[i]=(CM_ARM_GICC_INFO){core*0x100,100+(core+7)%12,1,&token[core],0xabc0+core};
  }
  in[12]=(CM_ARM_GICC_INFO){0xdead,999,1,NULL,42};
  for(boot_core=0;boot_core<12;boot_core++) {
    for(UINT32 mask=0;mask<4096;mask++) {
      for(UINT32 i=0;i<12;i++)in[i].Flags=(mask>>(11-i))&1;
      memcpy(saved,in,sizeof(in));writes=0;
      EFI_STATUS status=AddGICCList(out,in,12,5);
#ifdef ENABLE_CORE_ORDER_PERFORMANCE
      if(!(mask&(1u<<boot_core))) {assert(status==EFI_INVALID_PARAMETER && writes==0);continue;}
      assert(status==EFI_SUCCESS && writes==12);
      UINT32 expected[12],n=0;expected[n++]=boot_core;
      for(UINT32 i=0;i<12;i++)if(rank[i]!=boot_core)expected[n++]=rank[i];
      for(UINT32 i=0;i<12;i++)assert(memcmp(&out[i],&in[11-expected[i]],sizeof(out[i]))==0);
      // Linux assigns boot MPIDR to logical CPU0, then consumes enabled entries.
      UINT32 logical[12],count=1;logical[0]=boot_core;
      for(UINT32 i=0;i<12;i++)if(out[i].Flags && out[i].MPIDR!=boot_core*0x100)
        logical[count++]=(UINT32)(out[i].MPIDR/0x100);
      n=1;
      for(UINT32 i=0;i<12;i++)if(expected[i]!=boot_core && (mask&(1u<<expected[i])))
        assert(logical[n++]==expected[i]);
      assert(count==n);
#else
      (void)rank;
      assert(status==EFI_SUCCESS && writes==12);
      assert(memcmp(out,in,12*sizeof(*in))==0);
#endif
      assert(memcmp(in,saved,sizeof(in))==0);
    }
  }
#ifdef ENABLE_CORE_ORDER_PERFORMANCE
  boot_core=10;
  for(UINT32 i=0;i<12;i++)in[i].Flags=1;
  // Fuse-reduced topology: absent records are skipped without UID compaction.
  writes=0;assert(AddGICCList(out,in,8,5)==EFI_SUCCESS && writes==8);
  writes=0;assert(AddGICCList(out,in+2,10,5)==EFI_INVALID_PARAMETER && writes==0);
  writes=0;assert(AddGICCList(out,in,13,5)==EFI_INVALID_PARAMETER && writes==0);
  writes=0;assert(AddGICCList(out,in,0,5)==EFI_INVALID_PARAMETER && writes==0);
  boot_core=12;writes=0;assert(AddGICCList(out,in,12,5)==EFI_INVALID_PARAMETER && writes==0);
  boot_core=10;
  in[0].MPIDR=0xdead;writes=0;assert(AddGICCList(out,in,12,5)==EFI_INVALID_PARAMETER && writes==0);
  in[0].MPIDR=in[1].MPIDR;writes=0;assert(AddGICCList(out,in,12,5)==EFI_INVALID_PARAMETER && writes==0);
#endif
  in[0].AcpiProcessorUid=in[1].AcpiProcessorUid;
  writes=0;assert(AddGICCList(out,in,12,5)==EFI_INVALID_PARAMETER && writes==0);
  return 0;
}
'''


class FirmwareCoreOrderTests(unittest.TestCase):
    def test_madt_identity_boot_core_masks_and_linux_numbering(self):
        variants = {repo_source(PATH)}
        if not os.environ.get('SOURCE_TEST_REF') and not os.environ.get('SOURCE_TEST_ROOT'):
            for ref in unofficial_firmware_refs(ROOT):
                variants.add(subprocess.check_output(['git', '-C', str(ROOT), 'show', resolve_ref(ROOT, ref) + ':' + PATH], text=True))
        for text in variants:
            code = function(text, 'AddGICCList')
            for performance in (False, True):
                with self.subTest(performance=performance):
                    flags = '#define ENABLE_CORE_ORDER_PERFORMANCE 1\n' if performance else ''
                    execute(flags + HARNESS + code + MAIN)

    def test_complete_madt_module_compiles_with_real_edk2_headers(self):
        data = repo_source(PATH)
        inf = repo_source(PATH.rsplit('/', 1)[0] + '/AcpiMadtLibCIX.inf')
        declared_packages = re.findall(r'^\s+(\S+\.dec)\s*$', inf, re.M)
        classes = re.search(r'\[LibraryClasses\](.*?)(?=\n\[|\Z)', inf, re.S).group(1).split()
        platform_paths = {
            'AcpiPlatform.h': 'Platform/CIX/Sky1/Include/AcpiPlatform.h',
            'Include/AcpiCommon.h': 'Silicon/CIX/Sky1/Include/AcpiCommon.h',
            'Include/MemoryMap.h': 'Silicon/CIX/Sky1/Include/MemoryMap.h',
        }
        packages = ('MdePkg', 'MdeModulePkg', 'EmbeddedPkg', 'DynamicTablesPkg', 'ArmPkg')
        with tempfile.TemporaryDirectory(prefix='core-order-headers-') as temporary:
            root = Path(temporary)
            platform = root / 'platform'
            for name, path in platform_paths.items():
                target = platform / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(repo_source('src/edk2-platforms/' + path))
            unit = root / 'MadtGenerator.c'
            unit.write_text(data)
            for version in sorted(unofficial_release_edk2_refs(ROOT)):
                ref = 'source/base/edk2/' + version
                declarations = ''
                for path in declared_packages:
                    if path.startswith(('Platform/', 'Silicon/')):
                        declarations += repo_source('src/edk2-platforms/' + path)
                    else:
                        declarations += subprocess.check_output(['git', '-C', str(ROOT), 'show',
                                                                 resolve_ref(ROOT, ref) + ':' + path], text=True)
                for library in classes:
                    self.assertRegex(declarations, r'(?m)^\s*' + re.escape(library) + r'\|')
                archive = subprocess.check_output(['git', '-C', str(ROOT), 'archive', resolve_ref(ROOT, ref),
                                                   *[p + '/Include' for p in packages]])
                headers = root / version
                with tarfile.open(fileobj=io.BytesIO(archive)) as contents:
                    for member in contents:
                        if member.isfile():
                            target = headers / member.name
                            target.parent.mkdir(parents=True, exist_ok=True)
                            target.write_bytes(contents.extractfile(member).read())
                arch = 'AArch64' if machine().lower() in ('arm64', 'aarch64') else 'X64'
                for performance in (False, True):
                    with self.subTest(version=version, performance=performance):
                        command = ['cc', '-std=c11', '-Wall', '-Werror', '-fshort-wchar',
                                   '-c', '-o', str(root / 'MadtGenerator.o'), '-include', 'Uefi.h',
                                   '-I', str(platform), '-I', str(platform / 'Include'),
                                   '-I', str(headers / 'MdePkg/Include' / arch)]
                        for package in packages:
                            command += ['-I', str(headers / package / 'Include')]
                        if performance:
                            command += ['-DENABLE_CORE_ORDER_PERFORMANCE=1']
                        result = subprocess.run(command + [str(unit)], text=True, capture_output=True)
                        self.assertEqual(result.returncode, 0, result.stderr)

    def test_topology_preserves_vendor_uids_and_core_enable_associations(self):
        text = repo_source('custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/ConfigurationManagerDxe/ConfigurationManager.c')
        start = text.index('STATIC\nEFI_STATUS\nEFIAPI\nInitializeCmCixCpuTopoInfo (')
        code = text[start:text.index('\n}', start) + 2]
        harness = PREAMBLE + r'''
#include <stdlib.h>
#define EFIAPI
#define EfiBootServicesData 4
#define DEBUG(x) do {} while(0)
typedef struct {UINT32 Coreid,Uid;} CpuCore;
typedef struct {CpuCore *Core;UINT32 CoreNumber;} ClusterTopology;
typedef struct {ClusterTopology *ClusterTopo;UINT32 ClusterNumber;} SocketTopology;
typedef struct {SocketTopology *SocketTopo;} CpuTopology;
typedef struct {struct {UINT8 CoreEnable[12];} Cpu;} CONFIG_PARAMS_DATA_BLOCK;
typedef struct {UINT32 Coreid,Uid,Enable;} CIX_CPU_CORE;
typedef struct {CIX_CPU_CORE *Core;UINT32 CoreNumber,Uid;} CIX_CLUSTER_TOPO;
typedef struct {CIX_CLUSTER_TOPO *ClusterTopo;UINT32 ClusterNumber;} CM_CIX_CPU_TOPO_INFO;
typedef struct {CM_CIX_CPU_TOPO_INFO *CpuTopoInfo;} SKY1_PLATFORM_REPOSITORY_INFO;
typedef struct {SKY1_PLATFORM_REPOSITORY_INFO *PlatRepoInfo;} EDKII_CONFIGURATION_MANAGER_PROTOCOL;
static CM_CIX_CPU_TOPO_INFO mCpuTopoInfo;
static CpuTopology vendor;
static void GetValidCpuCoreNum(UINT8 *out) {*out=12;}
static void GetCpuTopology(CpuTopology **out) {*out=&vendor;}
static EFI_STATUS allocate(int kind,size_t n,void **out) {assert(kind==4);*out=malloc(n);assert(*out);return 0;}
static struct {EFI_STATUS (*AllocatePool)(int,size_t,void**);} services={allocate},*gBS=&services;
'''
        main = r'''
int main(void) {
  (void)fixes;
  const UINT32 physical[12]={10,11,0,1,2,3,4,5,6,7,8,9};
  static CpuCore cores[12];
  static ClusterTopology clusters[5]={{cores,2},{cores+2,4},{cores+6,2},{cores+8,2},{cores+10,2}};
  static SocketTopology socket={clusters,5};vendor.SocketTopo=&socket;
  SKY1_PLATFORM_REPOSITORY_INFO repo={0};EDKII_CONFIGURATION_MANAGER_PROTOCOL protocol={&repo};
  CONFIG_PARAMS_DATA_BLOCK config;
  for(UINT32 i=0;i<12;i++)cores[i]=(CpuCore){physical[i],100+i*3};
  for(UINT32 mask=0;mask<4096;mask++) {
    for(UINT32 i=0;i<12;i++)config.Cpu.CoreEnable[i]=(mask>>physical[i])&1;
    assert(InitializeCmCixCpuTopoInfo(&protocol,&config)==EFI_SUCCESS);
    assert(repo.CpuTopoInfo->ClusterNumber==5);
    UINT32 slot=0;
    for(UINT32 c=0;c<5;c++) {
      CIX_CLUSTER_TOPO *cluster=&repo.CpuTopoInfo->ClusterTopo[c];
      assert(cluster->Uid==c+1 && cluster->CoreNumber==clusters[c].CoreNumber);
      for(UINT32 i=0;i<cluster->CoreNumber;i++,slot++) {
        assert(cluster->Core[i].Coreid==cores[slot].Coreid);
        assert(cluster->Core[i].Uid==cores[slot].Uid);
        assert(cluster->Core[i].Enable==config.Cpu.CoreEnable[slot]);
      }
      free(cluster->Core);
    }
    free(repo.CpuTopoInfo->ClusterTopo);
  }
  return 0;
}
'''
        for performance in (False, True):
            with self.subTest(performance=performance):
                flags = '#define ENABLE_CORE_ORDER_PERFORMANCE 1\n' if performance else ''
                execute(flags + harness + code + main)


if __name__ == '__main__':
    unittest.main()
