#!/usr/bin/env python3
"""Exercise active HII and SE setup callbacks with malformed/failed variables."""
import unittest

from test_firmware_update_safety import source, function, run_c

BASE = 'custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/'
PRELUDE = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#define IN
#define OUT
#define CONST const
#define VOID void
#define EFIAPI
#define EFI_SUCCESS 0
#define EFI_NOT_FOUND 1
#define EFI_BUFFER_TOO_SMALL 2
#define EFI_DEVICE_ERROR 3
#define EFI_COMPROMISED_DATA 4
#define EFI_UNSUPPORTED 5
#define EFI_INVALID_PARAMETER 6
#define EFI_ERROR(s) ((s)!=0)
#define EFI_VARIABLE_NON_VOLATILE 1
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 2
#define DEBUG(x) do {} while(0)
#define ZeroMem(p,n) memset(p,0,n)
#define FixedPcdGet8(x) 2
typedef int EFI_STATUS;
typedef int BOOLEAN;
typedef size_t UINTN;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef void *EFI_EVENT;
'''


class SetupRemainingPathsTests(unittest.TestCase):
    def test_browser_failures_never_write_uninitialized_stores(self):
        text = source(BASE + 'PlatformConfigDxe/PlatformConfigDxe.c')
        harness = PRELUDE + r'''
#define EFI_BROWSER_ACTION_CHANGED 1
#define EFI_BROWSER_ACTION_RETRIEVE 2
#define EFI_BROWSER_ACTION_REQUEST_NONE 0
#define EFI_IFR_TYPE_BOOLEAN 4
#define KEY_ENABLE_NETWORK_STACK 1
#define KEY_DISABLE_ACPI_CPPC 2
#define KEY_DISABLE_SMALL_CORE 3
#define KEY_ENABLE_ACPI_LPI0 4
#define NETWORK_STACK_VAR L"network"
#define PLATFORM_SETUP_VAR L"platform"
#define COMPLIANCE_VAR L"compliance"
typedef int EFI_HII_CONFIG_ACCESS_PROTOCOL;
typedef int EFI_BROWSER_ACTION;
typedef int EFI_QUESTION_ID;
typedef int EFI_BROWSER_ACTION_REQUEST;
typedef union { int b; } EFI_IFR_TYPE_VALUE;
typedef struct { UINT8 Enable,Ipv4Pxe,Ipv6Pxe,Ipv4Http,Ipv6Http; } NETWORK_STACK;
typedef struct { UINT8 CpuCppcType,CpuCoreEnable[12],CpuLpiState; } PLATFORM_SETUP_DATA;
typedef struct { UINT8 DisableCPPC,DisableSmallCores,EnableLPI0; } COMPLIANCE_VARSTORE_DATA;
static int gEfiNetworkStackSetupGuid,gPlatformSetupVariableGuid,gPlatformConfigFormSetGuid;
static int reads,writes,fail_read,fail_write;
static int HiiGetBrowserData(void*g,const wchar_t*n,size_t size,UINT8 *p) {
 ++reads;if(reads==fail_read)return 0;
 memset(p,0x5a,size);return 1;
}
static int HiiSetBrowserData(void*g,const wchar_t*n,size_t size,UINT8 *p,void *r) {
 ++writes;
 if(!wcscmp(n,L"network"))assert(((NETWORK_STACK*)p)->Enable==0x5a);
 if(!wcscmp(n,L"platform"))assert(((PLATFORM_SETUP_DATA*)p)->CpuCoreEnable[0]==0x5a);
 return writes!=fail_write;
}
'''
        harness += function(text, 'PlatformConfigCallback') + r'''
int main(void) {
 EFI_IFR_TYPE_VALUE value={.b=1};int request=9;
 assert(PlatformConfigCallback(NULL,99,1,4,&value,&request)==EFI_UNSUPPORTED);assert(!reads && !writes);
 assert(PlatformConfigCallback(NULL,1,1,4,NULL,&request)==EFI_INVALID_PARAMETER);
 assert(PlatformConfigCallback(NULL,1,1,4,&value,NULL)==EFI_INVALID_PARAMETER);
 assert(PlatformConfigCallback(NULL,1,1,3,&value,&request)==EFI_INVALID_PARAMETER);
 for(int question=1;question<=4;question++) for(int action=1;action<=2;action++) {
  if(question==1 && action==2)continue;
  reads=writes=fail_read=fail_write=0;value.b=1;
  assert(!PlatformConfigCallback(NULL,action,question,4,&value,&request));
  assert(writes==1 && request==0);int expected_reads=reads;
  for(int failure=1;failure<=expected_reads;failure++) {
   reads=writes=fail_write=0;fail_read=failure;value.b=1;
   assert(PlatformConfigCallback(NULL,action,question,4,&value,&request)==EFI_DEVICE_ERROR);assert(!writes);
  }
  reads=writes=fail_read=0;fail_write=1;value.b=1;
  assert(PlatformConfigCallback(NULL,action,question,4,&value,&request)==EFI_DEVICE_ERROR);assert(writes==1);
 }
 return 0;
}
'''
        run_c(self, harness)

    def test_se_variable_bounds_no_overwrite_or_mailbox_on_invalid_data(self):
        text = source(BASE + 'SeConfigUpdateDxe/SeConfigUpdateDxe.c')
        harness = PRELUDE + r'''
#define SE_CONFIG_SETUP_VAR_NAME L"se"
typedef struct { UINT8 MemTagExtension; } SE_CONFIG_SETUP_VAR;
static int gSeConfigSetupVariableGuid,read_status,write_status,writes,mailbox_calls,mailbox_error;
static size_t stored_size;
static UINT8 stored_value;
static int get(const wchar_t*n,void*g,void*a,UINTN*s,void*p) {
 if(!read_status){assert(stored_size<=*s);memset(p,stored_value,stored_size);}
 *s=stored_size;return read_status;
}
static int set(const wchar_t*n,void*g,int a,UINTN s,void*p) {
 ++writes;assert(s==sizeof(SE_CONFIG_SETUP_VAR));assert(((SE_CONFIG_SETUP_VAR*)p)->MemTagExtension==0);return write_status;
}
static struct { int (*GetVariable)(const wchar_t*,void*,void*,UINTN*,void*);
 int (*SetVariable)(const wchar_t*,void*,int,UINTN,void*); } services={get,set},*gRT=&services;
static int MboxMemTagExtensionControl(UINT16*p,UINT8*v) {
 ++mailbox_calls;if(mailbox_calls==1){assert(*p==0);*v=0;}else assert(*p==0x101);
 return mailbox_error;
}
'''
        harness += function(text, 'SeConfigUpdateCallback') + function(text, 'SeConfigSetupVariableInit') + r'''
int main(void) {
 for(int error=EFI_BUFFER_TOO_SMALL;error<=EFI_DEVICE_ERROR;error++) {
  read_status=error;stored_size=100000;writes=mailbox_calls=0;
  assert(SeConfigSetupVariableInit()==error && !writes);
  SeConfigUpdateCallback(NULL,NULL);assert(!mailbox_calls);
 }
 read_status=EFI_NOT_FOUND;writes=0;assert(!SeConfigSetupVariableInit() && writes==1);
 write_status=EFI_DEVICE_ERROR;assert(SeConfigSetupVariableInit()==EFI_DEVICE_ERROR);
 read_status=0;stored_size=0;writes=0;assert(SeConfigSetupVariableInit()==EFI_COMPROMISED_DATA && !writes);
 SeConfigUpdateCallback(NULL,NULL);assert(!mailbox_calls);
 stored_size=sizeof(SE_CONFIG_SETUP_VAR);assert(!SeConfigSetupVariableInit() && !writes);
 stored_value=2;SeConfigUpdateCallback(NULL,NULL);assert(!mailbox_calls);
 stored_value=1;SeConfigUpdateCallback(NULL,NULL);assert(mailbox_calls==2);
 mailbox_calls=0;mailbox_error=EFI_DEVICE_ERROR;SeConfigUpdateCallback(NULL,NULL);assert(mailbox_calls==1);
 return 0;
}
'''
        run_c(self, harness)


if __name__ == '__main__':
    unittest.main()
