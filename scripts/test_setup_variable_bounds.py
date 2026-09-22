#!/usr/bin/env python3
"""Run the actual network/system initializers against hostile variable sizes."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from reconstruction_common import show_file
from test_setup_migration import SOURCE

ROOT = Path(__file__).resolve().parents[1]


class SetupVariableBoundsTests(unittest.TestCase):
    def test_consumers_reject_truncated_or_failed_reads(self):
        local = os.environ.get('SOURCE_TEST_ROOT')
        text = (Path(local) / SOURCE).read_text() if local else show_file(
            ROOT, os.environ.get('SOURCE_TEST_REF', 'source/unofficial/1.3/current'), SOURCE).decode()
        self.assertEqual(text.count('Status = ReadPlatformSetupVariable (&PlatformSetupVar);'), 3)
        start = text.rindex('STATIC\n', 0, text.index('ReadPlatformSetupVariable ('))
        body = text[start:text.index('EFI_STATUS\nEFIAPI\nCheckCpuShareInfo (')]
        harness = r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#define STATIC static
#define OUT
#define EFI_ERROR(s) ((s) != 0)
#define EFI_COMPROMISED_DATA 4
#define PLATFORM_SETUP_VAR L"platform"
typedef int EFI_STATUS;
typedef size_t UINTN;
typedef struct { unsigned char bytes[8]; } PLATFORM_SETUP_DATA;
static int gPlatformSetupVariableGuid, read_status;
static size_t stored_size;
static int get(const wchar_t *name, void *guid, void *attr, UINTN *size, void *data) {
  (void)name;(void)guid;(void)attr;
  if (!read_status) { assert(stored_size <= *size); memset(data,1,stored_size); }
  *size=stored_size; return read_status;
}
static struct { int (*GetVariable)(const wchar_t*,void*,void*,UINTN*,void*); } services={get}, *gRT=&services;
'''
        harness += body + r'''
int main(void) {
  PLATFORM_SETUP_DATA data;
  for (stored_size=0;stored_size<sizeof(data);stored_size++)
    assert(ReadPlatformSetupVariable(&data)==EFI_COMPROMISED_DATA);
  assert(ReadPlatformSetupVariable(&data)==0);
  for (read_status=1;read_status<=3;read_status++) {
    stored_size=100000;
    assert(ReadPlatformSetupVariable(&data)==read_status);
  }
  return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='setup-reader-') as tmp:
            root = Path(tmp)
            (root / 'test.c').write_text(harness)
            result = subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                                     str(root / 'test.c'), '-o', str(root / 'test')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(root / 'test')], check=True, capture_output=True)

    def test_read_failures_do_not_overwrite_and_sizes_are_bounded(self):
        local = os.environ.get('SOURCE_TEST_ROOT')
        text = (Path(local) / SOURCE).read_text() if local else show_file(
            ROOT, os.environ.get('SOURCE_TEST_REF', 'source/unofficial/1.3/current'), SOURCE).decode()
        text = text[text.index('EFI_STATUS\nNetworkStackVariableInit ('):
                    text.index('EFI_STATUS\nEFIAPI\nPlatformSetupVariableInitDxeEntry (')]
        harness = r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#define VOID void
#define EFI_SUCCESS 0
#define EFI_NOT_FOUND 1
#define EFI_BUFFER_TOO_SMALL 2
#define EFI_DEVICE_ERROR 3
#define EFI_COMPROMISED_DATA 4
#define EFI_ERROR(s) ((s) != 0)
#define EFI_VARIABLE_NON_VOLATILE 1
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 2
#define DEBUG(x) do {} while (0)
#define ZeroMem(p,n) memset(p,0,n)
#define FixedPcdGet8(x) 1
#define NETWORK_STACK_VAR L"network"
#define SYSTEM_TABLE_VAR L"system"
typedef int EFI_STATUS;
typedef int BOOLEAN;
typedef size_t UINTN;
typedef struct { unsigned char Enable, Ipv4Pxe, Ipv6Pxe, Ipv4Http, Ipv6Http; } NETWORK_STACK;
typedef struct { unsigned char SystemTableSelect; } SYSTEM_TABLE;
static int IsRtcPowerfailure(void) { return 0; }
static int gEfiNetworkStackSetupGuid, gCixGlobalVariableGuid;
static int read_status, writes;
static size_t stored_size;
static int get(const wchar_t *name, void *guid, void *attr, UINTN *size, void *data) {
  (void)name;(void)guid;(void)attr;
  if (read_status == 0) { assert(stored_size <= *size); memset(data,0,stored_size); }
  *size=stored_size; return read_status;
}
static int set(const wchar_t *name, void *guid, int attr, UINTN size, void *data) {
  (void)guid;(void)attr;
  size_t expected = !wcscmp(name,L"network") ? sizeof(NETWORK_STACK) : sizeof(SYSTEM_TABLE);
  assert(size == expected); ++writes;
  if (!wcscmp(name,L"network") && !read_status && stored_size==1) {
    NETWORK_STACK *n=data; assert(n->Enable==0 && n->Ipv6Http==1);
  }
  return 0;
}
static struct { int (*GetVariable)(const wchar_t*,void*,void*,UINTN*,void*);
                int (*SetVariable)(const wchar_t*,void*,int,UINTN,void*); } services={get,set}, *gRT=&services;
'''
        if "IsRtcPowerfailure ()" not in text:
            harness = harness.replace("static int IsRtcPowerfailure(void) { return 0; }", "")
        harness += text + r'''
int main(void) {
  for (int kind=0;kind<2;kind++) {
    int (*init)(void)=kind ? SystemTableVariableInit : NetworkStackVariableInit;
    for (int error=2;error<=3;error++) {
      writes=0;stored_size=100000;read_status=error;
      assert(init()==error && writes==0);
    }
    writes=0;stored_size=100000;read_status=EFI_NOT_FOUND; assert(!init() && writes==1);
    writes=0;stored_size=0;read_status=0; assert(init()==EFI_COMPROMISED_DATA && writes==0);
    writes=0;stored_size=kind ? sizeof(SYSTEM_TABLE) : sizeof(NETWORK_STACK);read_status=0;
    assert(!init() && writes==0);
  }
  writes=0;stored_size=1;read_status=0; assert(!NetworkStackVariableInit() && writes==1);
  return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='setup-bounds-') as tmp:
            root = Path(tmp)
            (root / 'test.c').write_text(harness)
            result = subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                                     str(root / 'test.c'), '-o', str(root / 'test')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(root / 'test')], check=True, capture_output=True)


if __name__ == '__main__':
    unittest.main()
