#!/usr/bin/env python3
"""Compile the real migration functions with mocked variable services."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = 'custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/PlatformSetupVariableInitDxe/PlatformSetupVariableInitDxe.c'


class SetupMigrationTests(unittest.TestCase):
    def test_variable_failure_ordering_and_bounds(self):
        source_root = os.environ.get('SOURCE_TEST_ROOT')
        text = (Path(source_root) / SOURCE).read_text() if source_root else subprocess.check_output(
            ['git', '-C', str(ROOT), 'show', os.environ.get('SOURCE_TEST_REF', 'source/unofficial/1.3/current') + ':' + SOURCE], text=True)
        migration = text[text.index('STATIC\nEFI_STATUS\nApplyCustomFirmwareSetupMigrations'):text.index('\nVOID\nEFIAPI\nUpdateConfigParams')]
        setup = text[text.index('EFI_STATUS\nPlatformSetupVariableInit ('):text.index('EFI_STATUS\nNetworkStackVariableInit (')]
        harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#define STATIC static
#define IN
#define OUT
#define EFI_SUCCESS 0
#define EFI_NOT_FOUND 1
#define EFI_BUFFER_TOO_SMALL 2
#define EFI_DEVICE_ERROR 3
#define EFI_ERROR(x) ((x) != 0)
#define EFI_VARIABLE_NON_VOLATILE 1
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 2
#define DEBUG(x) do {} while (0)
#define DebugPrint(...) do {} while (0)
#define DEBUG_INFO 0
#define DEBUG_ERROR 0
#define CUSTOM_LPI_DEFAULT_MIGRATION_VAR L"marker"
#define PLATFORM_SETUP_VAR L"setup"
#define FixedPcdGetBool(x) fixes
#define FixedPcdGet8(x) default_lpi
#define VOID void
#define ZeroMem(p,n) safe_zero(p,n)
typedef unsigned long UINTN;
typedef unsigned int UINT32;
typedef unsigned char UINT8;
typedef int EFI_STATUS;
typedef struct { UINT8 CpuLpiState; UINT8 reserved[15]; } PLATFORM_SETUP_DATA;
static int gCixGlobalVariableGuid, gPlatformSetupVariableGuid;
static int fixes, default_lpi, marker_status, marker_value, setting_read, setting_fail, marker_fail;
static unsigned long marker_size, setting_size;
static int writes[4], write_count;
static PLATFORM_SETUP_DATA stored;
static void safe_zero(void *p, size_t n) { assert(n == sizeof(stored)); memset(p,0,n); }
static int IsRtcPowerfailure(void) { return 0; }
static void ConstructSetupVariable(PLATFORM_SETUP_DATA *p) { p->CpuLpiState=default_lpi; }
static int get(const wchar_t *name, void *guid, void *attrs, UINTN *size, void *data) {
  (void)guid; (void)attrs;
  if (!wcscmp(name,L"marker")) { *size=marker_size; *(UINT8*)data=marker_value; return marker_status; }
  *size=setting_size; if (!setting_read) memcpy(data,&stored,sizeof(stored)); return setting_read;
}
static int set(const wchar_t *name, void *guid, int attrs, UINTN size, void *data) {
  (void)guid; (void)attrs;
  assert(write_count < 4);
  if (!wcscmp(name,L"marker")) { writes[write_count++]=2; assert(size==1 && *(UINT8*)data==1); return marker_fail; }
  writes[write_count++]=1; assert(size==sizeof(stored));
  if (!setting_fail) memcpy(&stored,data,size);
  return setting_fail;
}
static struct { int (*GetVariable)(const wchar_t*,void*,void*,UINTN*,void*);
                int (*SetVariable)(const wchar_t*,void*,int,UINTN,void*); } services={get,set}, *gRT=&services;
static void reset(void) {
  fixes=1;default_lpi=2;marker_status=EFI_NOT_FOUND;marker_value=0;marker_size=1;
  setting_read=setting_fail=marker_fail=0;setting_size=sizeof(stored);write_count=0;
  memset(&stored,0,sizeof(stored));
}
'''
        if 'IsRtcPowerfailure ()' not in setup:
            harness = harness.replace('static int IsRtcPowerfailure(void) { return 0; }', '')
        harness += migration + setup + r'''
int main(void) {
  reset(); assert(!PlatformSetupVariableInit()); assert(write_count==2 && writes[0]==1 && writes[1]==2); assert(stored.CpuLpiState==2);
  reset(); setting_fail=EFI_DEVICE_ERROR; assert(!PlatformSetupVariableInit()); assert(write_count==1 && writes[0]==1); assert(stored.CpuLpiState==0);
  reset(); marker_fail=EFI_DEVICE_ERROR; assert(!PlatformSetupVariableInit()); assert(write_count==2 && stored.CpuLpiState==2);
  write_count=0; marker_fail=0; assert(!PlatformSetupVariableInit()); assert(write_count==1 && writes[0]==2);
  reset(); stored.CpuLpiState=1; assert(!PlatformSetupVariableInit()); assert(write_count==1 && writes[0]==2 && stored.CpuLpiState==1);
  reset(); marker_status=EFI_SUCCESS;marker_value=1;assert(!PlatformSetupVariableInit());assert(write_count==0 && stored.CpuLpiState==0);
  reset(); marker_status=EFI_DEVICE_ERROR;assert(!PlatformSetupVariableInit());assert(write_count==0);
  reset(); marker_status=EFI_SUCCESS;marker_size=0;assert(!PlatformSetupVariableInit());assert(write_count==0);
  reset(); marker_status=EFI_SUCCESS;marker_value=9;assert(!PlatformSetupVariableInit());assert(write_count==0);
  reset(); fixes=0;assert(!PlatformSetupVariableInit());assert(write_count==0);
  reset(); setting_read=EFI_BUFFER_TOO_SMALL;setting_size=100000;assert(!PlatformSetupVariableInit());assert(write_count==2 && stored.CpuLpiState==2);
  reset(); setting_size=1;assert(!PlatformSetupVariableInit());assert(write_count==2 && stored.CpuLpiState==2);
  reset(); setting_read=EFI_NOT_FOUND;setting_fail=EFI_DEVICE_ERROR;assert(PlatformSetupVariableInit()==EFI_DEVICE_ERROR);assert(write_count==1);
  return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='setup-migration-') as tmp:
            path = Path(tmp)
            (path/'test.c').write_text(harness)
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(path/'test.c'), '-o', str(path/'test')], check=True, capture_output=True)
            subprocess.run([str(path/'test')], check=True, capture_output=True)


if __name__ == '__main__':
    unittest.main()
