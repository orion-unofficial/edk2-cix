#!/usr/bin/env python3
"""Execute firmware memory-ownership and CPU/SCMI mapping code with mock services."""
import os
import json
import posixpath
import re
from pathlib import Path
import subprocess
import tempfile
import unittest

from reconstruction_common import resolve_ref

ROOT = Path(__file__).resolve().parents[1]
PREFIX = 'custom/overlay/edk2-platforms/Platform/CIX/Sky1/'


def source(path):
    return repo_source(PREFIX + path)


def repo_source(path):
    local = os.environ.get('SOURCE_TEST_ROOT')
    if local:
        return (Path(local) / path).read_text()
    ref = resolve_ref(ROOT, os.environ.get('SOURCE_TEST_REF', 'source/unofficial/1.3/current'))
    for _ in range(8):
        entry = subprocess.check_output(['git', '-C', str(ROOT), 'ls-tree',
                                         ref, '--', path], text=True)
        text = subprocess.check_output(['git', '-C', str(ROOT), 'show',
                                        ref + ':' + path], text=True)
        if not entry.startswith('120000 '):
            return text
        path = posixpath.normpath(posixpath.join(posixpath.dirname(path), text))
    raise AssertionError('overlay symlink cycle')


def preprocess(text, fixes):
    text = re.sub(r'^\s*#include[^\n]*', '', text, flags=re.M)
    return subprocess.check_output(['cc', '-E', '-P', '-x', 'c', '-'] +
                                   (['-DENABLE_FIRMWARE_FIXES=1'] if fixes else []),
                                   input=text, text=True)


def execute(text):
    with tempfile.TemporaryDirectory(prefix='firmware-acpi-runtime-') as tmp:
        root = Path(tmp)
        (root/'test.c').write_text(text)
        result = subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(root/'test.c'), '-o', str(root/'test')], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)
        subprocess.run([str(root/'test')], check=True, capture_output=True)


PREAMBLE = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#define STATIC static
#define CONST const
#define VOID void
#define IN
#define EFI_SUCCESS 0
#define EFI_INVALID_PARAMETER 1
#define EFI_ERROR(x) ((x)!=0)
#define FixedPcdGetBool(x) fixes
#define MAX_UINT64 UINT64_MAX
typedef uint64_t UINT64;
typedef unsigned int UINT32;
typedef unsigned char UINT8;
typedef size_t UINTN;
typedef char CHAR8;
typedef int EFI_STATUS;
static int fixes;
'''


class FirmwareAcpiRuntimeTests(unittest.TestCase):
    def test_dsp_mailbox_expansion_is_in_the_effective_custom_overlay(self):
        text = source('Drivers/AcpiSocTables/Dsdt-Dsp.asl')
        original = preprocess(text, False)
        fixed = preprocess(text, True)
        self.assertIn('"mboxes", Package (4)', original)
        self.assertNotIn('"txdb"', original)
        self.assertIn('"mboxes", Package (8)', fixed)
        self.assertIn('"txdb"', fixed)
        self.assertIn('"rxdb"', fixed)

    def test_gicc_initialisation_compiles_and_maps_every_core_order(self):
        text = repo_source('custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/ConfigurationManagerDxe/ConfigurationManager.c')
        start = text.index('STATIC\nEFI_STATUS\nEFIAPI\nInitializeCmArmGiccInfo (')
        end = text.index('\n}', start) + 2
        function = text[start:end]
        functions = {function}
        if not os.environ.get('SOURCE_TEST_ROOT') and not os.environ.get('SOURCE_TEST_REF'):
            refs = subprocess.check_output(['git', '-C', str(ROOT), 'for-each-ref',
                                            '--format=%(refname)', 'refs/heads/source/unofficial/'], text=True).splitlines()
            for ref in refs:
                text = subprocess.check_output(['git', '-C', str(ROOT), 'show', ref +
                                                ':custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/ConfigurationManagerDxe/ConfigurationManager.c'], text=True)
                start = text.index('STATIC\nEFI_STATUS\nEFIAPI\nInitializeCmArmGiccInfo (')
                functions.add(text[start:text.index('\n}', start) + 2])
        harness = PREAMBLE + r'''
#include <stdlib.h>
#define EFIAPI
#define PLAT_CPU_COUNT 12
#define PLAT_GIC_CPU_INTERFACE {{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1},{.Flags=1}}
#define EfiBootServicesData 4
#define EFI_ACPI_6_2_GIC_ENABLED 1
#define DEBUG(x) do {} while(0)
#define ASSERT(x) assert(x)
#define ZeroMem(p,n) memset(p,0,n)
#define CM_NULL_TOKEN NULL
#define CppcEnable 1
#define PcdGetBool(pcd) fixes
typedef void *CM_OBJECT_TOKEN;
typedef struct {UINT32 CPUInterfaceNumber,AcpiProcessorUid,Flags;CM_OBJECT_TOKEN CpcToken;} CM_ARM_GICC_INFO;
typedef struct {UINT32 Uid,Coreid,Enable;} CIX_CPU_CORE;
typedef struct {UINT32 CoreNumber;CIX_CPU_CORE *Core;} CIX_CLUSTER_TOPO;
typedef struct {UINT32 ClusterNumber;CIX_CLUSTER_TOPO *ClusterTopo;} CM_CIX_CPU_TOPO_INFO;
typedef struct {CM_CIX_CPU_TOPO_INFO *CpuTopoInfo;CM_ARM_GICC_INFO *GicCInfo;UINT32 CpuUidtoCoreNumberMap[12],CpuCpcInfo[12];} SKY1_PLATFORM_REPOSITORY_INFO;
typedef struct {SKY1_PLATFORM_REPOSITORY_INFO *PlatRepoInfo;} EDKII_CONFIGURATION_MANAGER_PROTOCOL;
static void GetValidCpuCoreNum(UINT8 *out) {*out=12;}
static int allocate(int kind,size_t size,void **out) {assert(kind==4);*out=malloc(size);assert(*out);return 0;}
static struct {int (*AllocatePool)(int,size_t,void **);} services={allocate}, *gBS=&services;
'''
        for mode in ('CIX', 'CONVENTIONAL', 'PERFORMANCE'):
            defines = '' if mode == 'CIX' else '#define ENABLE_CORE_ORDER_' + mode + ' 1\n'
            mapping = {'CIX': [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1],
                       'CONVENTIONAL': list(range(12)),
                       'PERFORMANCE': [8, 9, 10, 11, 4, 5, 6, 7, 2, 3, 0, 1]}[mode]
            main = '''
int main(void) {
  (void)fixes;
  UINT32 map[12]={''' + ','.join(map(str, mapping)) + '''};
  CIX_CPU_CORE cores[12]; CIX_CLUSTER_TOPO cluster={12,cores};
  CM_CIX_CPU_TOPO_INFO topo={1,&cluster};
  SKY1_PLATFORM_REPOSITORY_INFO repo={.CpuTopoInfo=&topo};
  EDKII_CONFIGURATION_MANAGER_PROTOCOL protocol={&repo};
  for(unsigned int i=0;i<12;i++) cores[i]=(CIX_CPU_CORE){map[i],i,i!=7};
  assert(InitializeCmArmGiccInfo(&protocol)==0);
  for(unsigned int i=0;i<12;i++) {
    unsigned int slot=''' + ('i' if mode == 'CIX' else 'map[i]') + ''';
    assert(repo.GicCInfo[slot].AcpiProcessorUid==map[i]);
    assert(repo.CpuUidtoCoreNumberMap[slot]==i);
    assert(repo.GicCInfo[slot].Flags==(i!=7));
    if(repo.GicCInfo[slot].CpcToken != CM_NULL_TOKEN)
      assert(repo.GicCInfo[slot].CpcToken == &repo.CpuCpcInfo[i]);
    if(fixes && REQUIRE_CORRECTED_TOKENS)
      assert(repo.GicCInfo[slot].CpcToken == &repo.CpuCpcInfo[i]);
    if(!fixes && REQUIRE_GATED_NULL_TOKENS)
      assert(repo.GicCInfo[slot].CpcToken == CM_NULL_TOKEN);
  }
  free(repo.GicCInfo);return 0;
}
'''
            for variant in functions:
                selected_function = preprocess(defines + variant, False)
                corrected = int(bool(re.search(r'\b(?:Fixed)?PcdGetBool\s*\(\s*PcdCustomFirmwareFixesEnable\s*\)', selected_function)))
                tokens = ('#define REQUIRE_CORRECTED_TOKENS ' + str(corrected) + '\n'
                          + '#define REQUIRE_GATED_NULL_TOKENS ' + str(corrected) + '\n')
                for enabled in (0, 1):
                    with self.subTest(order=mode, fixes=enabled, corrected=corrected):
                        invocation = main.replace('(void)fixes;', 'fixes=' + str(enabled) + ';')
                        execute(defines + tokens + harness + variant + invocation)

    def test_gpio_ownership_and_ec_thermal_limit(self):
        prefix = 'custom/overlay/edk2-platforms/Platform/Radxa/Orion/'
        for board in ('O6', 'O6N'):
            base = prefix + board + '/Drivers/AcpiPlatfomTables/'
            for name in ('UsbPwr.asl', 'Wireless.asl'):
                text = repo_source(base + name)
                stock, fixed = (preprocess(text, flag) for flag in (False, True))
                self.assertIn('PinGroupFunction', stock)
                self.assertNotIn('PinGroupFunction', fixed)
                self.assertEqual(stock.count('GpioIo'), fixed.count('GpioIo'))
                # Removing pin claims must not change voltage, polarity or policy.
                self.assertEqual(re.findall(r'Package\s*\(\)\s*\{[^{}]*\}', stock),
                                 re.findall(r'Package\s*\(\)\s*\{[^{}]*\}', fixed))
        base = prefix + 'O6/Drivers/AcpiPlatfomTables/'
        camera = preprocess(repo_source(base + 'MipiCamera.asl'), True)
        self.assertIn('"pinctrl_cam0_hw"', camera)
        self.assertNotIn('"pinctrl_cam1_hw"', camera)
        mux = preprocess(repo_source(base + 'RadxaO6Iomux.asl'), True)
        groups = mux[mux.index('PinGroup ("pinctrl_edp0"'):mux.index('PinGroup ("pinctrl_cam1_hw"')]
        for pad in ('DP2_DIGON', 'I2S3_DATA_OUT0', 'I2S3_DATA_OUT1'):
            self.assertNotIn('CIX_PAD_' + pad, groups)
        self.assertIn('CIX_PAD_DP2_BLON', groups)
        self.assertIn('CIX_PAD_I2S3_MCLK', groups)
        ec = repo_source(base + 'EC.asl')
        # ECTZ is guarded by the board's existing EC thermal feature.
        ec = '#define EC_FAN_SUPPORT 1\n#define EC_THERMAL_SUPPORT 1\n' + ec
        fixed = preprocess(ec, True)
        if 'ThermalZone(ECTZ)' not in fixed:
            self.fail('ECTZ test must exercise the enabled thermal zone')
        zone = fixed[fixed.index('ThermalZone(ECTZ)'):]
        self.assertIn('Name (_STR, Unicode ("EC"))', zone)
        self.assertIn('Name (_CRT, 0x0E80)', zone)

    def test_gpio3_labels_match_the_vendor_hardware_bit_assignments(self):
        hardware = repo_source('src/edk2-platforms/Silicon/CIX/Sky1/Include/IoConfig.h')
        self.assertRegex(hardware, r'#define\s+BITMAP_DP2_BLON\s+BIT15\b')
        self.assertRegex(hardware, r'#define\s+BITMAP_DP2_DIGON\s+BIT16\b')
        paths = [PREFIX + 'Drivers/AcpiSocTables/Dsdt-Gpio.asl']
        paths += ['custom/overlay/edk2-platforms/Platform/Radxa/Orion/' + board +
                  '/Drivers/AcpiPlatfomTables/GpioLines.asl' for board in ('O6', 'O6N')]
        for path in paths:
            text = repo_source(path)
            for fixes in (False, True):
                rendered = preprocess(text, fixes)
                marker = 'Device (GPI3)' if path.endswith('Dsdt-Gpio.asl') else 'Scope (\\_SB.GPI3)'
                block = rendered[rendered.index(marker):]
                names = re.search(r'Name\s*\((?:GPIN|GPIO),\s*Package\s*\(\)\s*\{([^}]+)\}', block)
                labels = re.findall(r'"([^"]*)"', names.group(1))
                with self.subTest(path=path, fixes=fixes):
                    self.assertEqual(len(labels), 17)
                    self.assertEqual(labels[15:17], ['DP2_BLON', 'DP2_DIGON'] if fixes else
                                     ['DP2_DIGON', 'DP2_BLON'])

    def test_graph_paths_survive_preprocessing_with_an_absolute_root(self):
        header = source('Include/AcpiGraph.h')
        text = header + r'''
CIX_GRAPH_REMOTE4(\_SB.DP00, "port@0", "port@0", "endpoint@0", EP00)
CIX_GRAPH_REMOTE3(\_SB.I2C1.PD10, "port@0", "endpoint@0", EP02)
'''
        for fixes in (False, True):
            result = subprocess.run(['cc', '-E', '-P', '-x', 'c', '-'] +
                                    (['-DENABLE_FIRMWARE_FIXES=1'] if fixes else []),
                                    input=text, text=True, capture_output=True, check=True)
            if fixes:
                self.assertEqual([json.loads(line) for line in result.stdout.splitlines() if line.strip()],
                                 [r'\_SB.DP00.EP00', r'\_SB.I2C1.PD10.EP02'])
            else:
                self.assertIn(r'Package () { \_SB.DP00,', result.stdout)

    def test_shared_runtime_hobs_exclude_exact_ramoops_interval(self):
        text = source('Library/MemoryInitPeiLib/MemoryInitPeiLib.c')
        start = text.index('STATIC\nVOID\nBuildSharedMemoryHobs')
        end = text.index('\n}', start) + 2
        execute(PREAMBLE + r'''
#define RAMOOPS_RES_BASE 0x83D00000U
#define RAMOOPS_RES_SIZE 0x000A0000U
#define EfiRuntimeServicesData 6
static UINT64 allocations[3][2];
static int count;
static void BuildMemoryAllocationHob(UINT64 base, UINT64 size, int type) {
  assert(count<3 && size>0 && type==6);
  allocations[count][0]=base;allocations[count++][1]=size;
}
''' + text[start:end] + r'''
int main(void) {
  fixes=1; BuildSharedMemoryHobs(0x82500000,0x1f00000);
  assert(count==2);
  assert(allocations[0][0]==0x82500000 && allocations[0][1]==0x1800000);
  assert(allocations[1][0]==0x83da0000 && allocations[1][1]==0x660000);
  count=0;fixes=0;BuildSharedMemoryHobs(0x82500000,0x1f00000);
  assert(count==1 && allocations[0][0]==0x82500000 && allocations[0][1]==0x1f00000);
  count=0;fixes=1;BuildSharedMemoryHobs(0x85f00000,0x100000);
  assert(count==1 && allocations[0][0]==0x85f00000 && allocations[0][1]==0x100000);
  count=0;BuildSharedMemoryHobs(RAMOOPS_RES_BASE,RAMOOPS_RES_SIZE);assert(count==0);
  return 0;
}
''')

    def test_scmi_domains_follow_actual_uids_in_every_supported_order(self):
        text = source('Library/Acpi/CIX/AcpiSsdtCpuTopologyLibCIX/SsdtCpuTopologyGenerator.c')
        start = text.index('STATIC CONST UINT8  mPhysicalScmiDomains')
        end = text.index('\nSTATIC\nEFI_STATUS\nEFIAPI\nCreateTopologyFromCpuTopoInfo', start)
        execute(PREAMBLE + r'''
#define PLAT_CPU_COUNT 12
typedef void *AML_OBJECT_NODE_HANDLE;
typedef struct {UINT32 Uid,Coreid,Enable;} CIX_CPU_CORE;
typedef struct {UINT32 CoreNumber;CIX_CPU_CORE *Core;} CIX_CLUSTER_TOPO;
typedef struct {UINT32 ClusterNumber;CIX_CLUSTER_TOPO *ClusterTopo;} CM_CIX_CPU_TOPO_INFO;
static UINT64 domains[12];static int count;
static int AmlCodeGenScope(const char *name,void *parent,void **out) {assert(!strcmp(name,"PMMX"));*out=parent;return 0;}
static int WriteAslName(char lead,UINT32 index,char *name) {snprintf(name,5,"%c%03X",lead,index);return 0;}
static int AmlCodeGenNameInteger(const char *name,UINT64 value,void *parent,void *out) {
  (void)parent;(void)out;assert(name[0]=='D');unsigned int index;assert(sscanf(name+1,"%x",&index)==1 && index<12);
  domains[index]=value;count++;return 0;
}
''' + text[start:end] + r'''
int main(void) {
  const unsigned int maps[3][12]={{2,3,4,5,6,7,8,9,10,11,0,1},{0,1,2,3,4,5,6,7,8,9,10,11},{8,9,10,11,4,5,6,7,2,3,0,1}};
  const unsigned int expected_physical[12]={2,2,2,2,5,5,6,6,3,3,4,4};
  CIX_CPU_CORE cores[12];CIX_CLUSTER_TOPO cluster={12,cores};CM_CIX_CPU_TOPO_INFO info={1,&cluster};
  for(unsigned int mode=0;mode<3;mode++) {
    for(unsigned int i=0;i<12;i++) cores[i]=(CIX_CPU_CORE){maps[mode][i],i,1};
    fixes=1;count=0;assert(!CreateAmlScmiDomains(&info,NULL));assert(count==12);
    for(unsigned int i=0;i<12;i++) assert(domains[maps[mode][i]]==expected_physical[i]);
    cores[7].Enable=0;assert(!CreateAmlScmiDomains(&info,NULL));assert(domains[maps[mode][7]]==UINT64_MAX);
  }
  fixes=0;count=0;assert(!CreateAmlScmiDomains(&info,NULL));assert(count==0);
  fixes=1;cores[0].Uid=12;assert(CreateAmlScmiDomains(&info,NULL)==EFI_INVALID_PARAMETER);
  cores[0].Uid=0;cores[0].Coreid=12;assert(CreateAmlScmiDomains(&info,NULL)==EFI_INVALID_PARAMETER);
  return 0;
}
''')


if __name__ == '__main__':
    unittest.main()
