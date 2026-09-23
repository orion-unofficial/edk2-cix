#!/usr/bin/env python3
"""Run the custom configuration protocols against malformed entry requests."""
import unittest

from test_firmware_update_safety import PRELUDE, function, run_c, source


class ConfigProtocolBoundsTests(unittest.TestCase):
    def test_option_parser_nulls_and_allocation_cleanup(self):
        for directory, prefix in (
            ('Silicon/CIX/Sky1/Library/ConfigParamsDataBlockLib', ''),
            ('Platform/CIX/Sky1/Library/PlatformConfigParamsDataBlockLib', 'Platform'),
        ):
            with self.subTest(protocol=prefix or 'Silicon'):
                text = source('custom/overlay/edk2-platforms/' + directory + '/' + prefix + 'ConfigParamsDataBlockLib.c')
                text = text.replace('PLATFORM_CONFIG_', 'CONFIG_').replace('ParsePlatformConfig', 'ParseConfig')
                harness = PRELUDE + r'''
#include <wchar.h>
#define MAX_PARAMS_OPTION_NUM 2
#define MAX_PARAMS_OPTION_STRING_SIZE 8
typedef wchar_t CHAR16;
typedef struct { UINT64 Value;CHAR16 String[8]; } CONFIG_PARAMS_DATA_OPTIONS;
static UINTN StrSize(const CHAR16 *p) {return (wcslen(p)+1)*sizeof(*p);}
static void *AllocateCopyPool(size_t n,const void *src) {void *p=allocate(n);if(p)memcpy(p,src,n);return p;}
static CHAR16 *StrStr(CHAR16 *haystack,const CHAR16 *needle) {return wcsstr(haystack,needle);}
static UINT64 StrDecimalToUint64(const CHAR16 *p) {return wcstoull(p,NULL,10);}
static UINT64 StrHexToUint64(const CHAR16 *p) {return wcstoull(p,NULL,16);}
static EFI_STATUS StrCpyS(CHAR16 *dst,UINTN n,const CHAR16 *src) {if(wcslen(src)>=n)return EFI_INVALID_PARAMETER;wcscpy(dst,src);return 0;}
'''
                harness += function(text, 'ParseConfigDataOption') + r'''
int main(void) {
 UINT32 num=0;CONFIG_PARAMS_DATA_OPTIONS options[2];
 assert(ParseConfigDataOption(NULL,&num,options)==EFI_INVALID_PARAMETER);
 assert(ParseConfigDataOption(L"0:on",NULL,options)==EFI_INVALID_PARAMETER);
 assert(ParseConfigDataOption(L"0:on",&num,NULL)==EFI_INVALID_PARAMETER);
 assert(!allocation_attempts && !outstanding);
 fail_allocation=1;assert(ParseConfigDataOption(L"0:on",&num,options)==EFI_OUT_OF_RESOURCES);
 fail_allocation=0;
 assert(!ParseConfigDataOption(L"0:off,0x1:on",&num,options));
 assert(num==2 && options[0].Value==0 && options[1].Value==1 && !outstanding);
 assert(ParseConfigDataOption(L"bad",&num,options)==EFI_UNSUPPORTED && !outstanding);
 assert(ParseConfigDataOption(L"0:too-long-text",&num,options)==EFI_INVALID_PARAMETER && !outstanding);
 assert(ParseConfigDataOption(L"0:off,1:on,2:auto",&num,options)==EFI_OUT_OF_RESOURCES && !outstanding);
 return 0;
}
'''
                run_c(self, harness)

    def test_initialization_allocation_and_install_failures(self):
        for directory, prefix in (
            ('Silicon/CIX/Sky1/Drivers/ConfigParamsManageDxe', ''),
            ('Platform/CIX/Sky1/Drivers/PlatformConfigParamsManageDxe', 'Platform'),
        ):
            with self.subTest(protocol=prefix or 'Silicon'):
                text = source('custom/overlay/edk2-platforms/' + directory + '/' + prefix + 'ConfigParamsManageDxe.c')
                header = source('src/edk2-platforms/' + directory.split('/Drivers/')[0] + '/Include/Protocol/' + prefix + 'ConfigParamsManageProtocol.h')
                self.assertRegex(header, r'CONFIG_PARAMS_DATA_BLOCK\s+\*Data;')
                text = text.replace('PLATFORM_CONFIG_', 'CONFIG_').replace('CIX_PLATFORM_', 'CIX_')
                text = text.replace('PlatformConfig', 'Config')
                harness = PRELUDE + r'''
#define POST_CODE(x) do {} while(0)
#define EFI_NATIVE_INTERFACE 0
#define CIX_CONFIG_PARAMS_MANAGE_PROTOCOL_VERSION 1
typedef void *EFI_HANDLE;
typedef void EFI_SYSTEM_TABLE;
typedef struct { UINT8 bytes[16]; } CONFIG_PARAMS_DATA_BLOCK;
typedef struct { UINT32 Id; } CONFIG_PARAMS_DATA_ENTRY;
typedef struct { UINT32 Version;void *Data,*Entry;UINT32 EntryNum;void *GetEntry,*FindEntry,*ModifyEntry; } CIX_CONFIG_PARAMS_MANAGE_PROTOCOL;
static CONFIG_PARAMS_DATA_BLOCK mConfigParamsDataBlock;
static CONFIG_PARAMS_DATA_ENTRY mConfigDataEntryTable[2];
static UINT32 mConfigDataEntryNum=2;
static void *ConfigDataGetEntry,*ConfigDataFindEntry,*ConfigDataModifyEntry;
static int hooks,installs,fail_install,gCixConfigParamsManageProtocolGuid;
static CIX_CONFIG_PARAMS_MANAGE_PROTOCOL *installed;
static void *AllocateCopyPool(size_t n,const void *src) {void *p=allocate(n);if(p)memcpy(p,src,n);return p;}
static void ConfigParamsHook(CONFIG_PARAMS_DATA_BLOCK *p) {assert(p);hooks++;}
static EFI_STATUS install(EFI_HANDLE *h,void *guid,int kind,void *p) {assert(p && guid==&gCixConfigParamsManageProtocolGuid);installs++;if(fail_install)return EFI_DEVICE_ERROR;installed=p;return 0;}
static struct { EFI_STATUS (*InstallProtocolInterface)(EFI_HANDLE*,void*,int,void*); } bs={install},*gBS=&bs;
'''
                harness += function(text, 'ConfigParamsManageDxeEntryPoint') + r'''
int main(void) {
 for(int n=1;n<=3;n++) {
  allocation_attempts=hooks=installs=0;fail_allocation=n;
  assert(ConfigParamsManageDxeEntryPoint(NULL,NULL)==EFI_OUT_OF_RESOURCES);
  assert(!outstanding && !hooks && !installs);
 }
 allocation_attempts=hooks=installs=fail_allocation=0;fail_install=1;
 assert(ConfigParamsManageDxeEntryPoint(NULL,NULL)==EFI_DEVICE_ERROR);
 assert(!outstanding && hooks==1 && installs==1);
 hooks=installs=fail_install=0;
 assert(!ConfigParamsManageDxeEntryPoint(NULL,NULL));
 assert(outstanding==3 && hooks==1 && installs==1 && installed->EntryNum==2);
 release(installed->Data);release(installed->Entry);release(installed);assert(!outstanding);return 0;
}
'''
                run_c(self, harness)

    def test_entry_indices_ranges_and_typed_write_widths(self):
        for directory, prefix in (
            ('Silicon/CIX/Sky1/Drivers/ConfigParamsManageDxe', ''),
            ('Platform/CIX/Sky1/Drivers/PlatformConfigParamsManageDxe', 'Platform'),
        ):
            with self.subTest(protocol=prefix or 'Silicon'):
                text = source('custom/overlay/edk2-platforms/' + directory + '/' + prefix + 'ConfigParamsManageDxe.c')
                header = source('src/edk2-platforms/' + directory.split('/Drivers/')[0] + '/Include/Protocol/' + prefix + 'ConfigParamsManageProtocol.h')
                self.assertRegex(header, r'CONFIG_PARAMS_DATA_BLOCK\s+\*Data;')
                # Normalize just the protocol's parallel type/function names.
                text = text.replace('PLATFORM_CONFIG_', 'CONFIG_').replace('CIX_PLATFORM_', 'CIX_')
                text = text.replace('PlatformConfigData', 'ConfigData').replace('ParsePlatformConfig', 'ParseConfig')
                harness = PRELUDE.replace('typedef int BOOLEAN;', 'typedef uint8_t BOOLEAN;') + r'''
#define CONST const
#define EFI_BUFFER_TOO_SMALL 0x8000000000000005ULL
#define EFI_COMPROMISED_DATA 0x8000000000000021ULL
#define MAX_PARAMS_OPTION_NUM 10
#define PARAMS_DATA_BOOLEAN_TYPE 1
#define PARAMS_DATA_MULTI_OPTION_TYPE 2
#define PARAMS_DATA_INTEGER_TYPE 3
#define PARAMS_DATA_STRING_TYPE 4
typedef struct { UINT8 bytes[16]; } CONFIG_PARAMS_DATA_BLOCK;
typedef struct { UINT32 Id,Offset,Size,Type; char *Option; } CONFIG_PARAMS_DATA_ENTRY;
typedef struct { UINT64 Value; } CONFIG_PARAMS_DATA_OPTIONS;
typedef struct { CONFIG_PARAMS_DATA_BLOCK *Data; CONFIG_PARAMS_DATA_ENTRY *Entry; UINT32 EntryNum; } CIX_CONFIG_PARAMS_MANAGE_PROTOCOL;
static int parse_calls,excess_options;
static char canonical_options[]="0;1";
static EFI_STATUS ParseConfigDataOption(char *option, UINT32 *num, CONFIG_PARAMS_DATA_OPTIONS *options) {
 assert(option==canonical_options);parse_calls++;*num=excess_options?11:2;
 options[0].Value=0;options[1].Value=1;return EFI_SUCCESS;
}
static UINT16 ReadUnaligned16(const UINT16 *p) { UINT16 v;memcpy(&v,p,2);return v; }
static UINT32 ReadUnaligned32(const UINT32 *p) { UINT32 v;memcpy(&v,p,4);return v; }
static UINT64 ReadUnaligned64(const UINT64 *p) { UINT64 v;memcpy(&v,p,8);return v; }
'''
                harness += '\n'.join(function(text, name) for name in ('ConfigDataGetEntry', 'ConfigDataFindEntry', 'ConfigDataModifyEntry'))
                harness += r'''
int main(void) {
 void *allocation=allocate(1);release(allocation);
 CONFIG_PARAMS_DATA_BLOCK data={{0}};
 CONFIG_PARAMS_DATA_ENTRY entries[]={{7,0,8,PARAMS_DATA_MULTI_OPTION_TYPE,canonical_options}},copy;
 CIX_CONFIG_PARAMS_MANAGE_PROTOCOL protocol={&data,entries,1};
 UINT8 buffer[16]={0,1};UINTN size=sizeof(buffer);
 assert(ConfigDataGetEntry(NULL,0,&copy,buffer,&size)==EFI_INVALID_PARAMETER);
 assert(ConfigDataFindEntry(NULL,7,&copy,buffer,&size)==EFI_INVALID_PARAMETER);
 assert(ConfigDataGetEntry(&protocol,1,&copy,buffer,&size)==EFI_INVALID_PARAMETER);
 assert(ConfigDataGetEntry(&protocol,UINT32_MAX,&copy,buffer,&size)==EFI_INVALID_PARAMETER);
 assert(ConfigDataGetEntry(&protocol,0,&copy,buffer,NULL)==EFI_INVALID_PARAMETER);
 size=0;assert(ConfigDataGetEntry(&protocol,0,NULL,NULL,&size)==EFI_BUFFER_TOO_SMALL && size==8);
 size=16;assert(!ConfigDataGetEntry(&protocol,0,&copy,buffer,&size) && size==8);
 assert(!ConfigDataFindEntry(&protocol,7,&copy,buffer,&size));
 assert(ConfigDataFindEntry(&protocol,8,&copy,buffer,&size)==EFI_NOT_FOUND);
 entries[0].Offset=UINT32_MAX;
 assert(ConfigDataGetEntry(&protocol,0,&copy,buffer,&size)==EFI_COMPROMISED_DATA);
 assert(ConfigDataFindEntry(&protocol,7,&copy,buffer,&size)==EFI_COMPROMISED_DATA);
 entries[0].Offset=0;entries[0].Size=UINT32_MAX;
 assert(ConfigDataGetEntry(&protocol,0,&copy,buffer,&size)==EFI_COMPROMISED_DATA);
 entries[0].Size=8;copy=entries[0];buffer[1]=1;
 assert(ConfigDataModifyEntry(NULL,&copy,buffer,1)==EFI_INVALID_PARAMETER);
 for(UINTN n=0;n<=9;n++) {
  parse_calls=0;memset(&data,0,sizeof(data));
  EFI_STATUS status=ConfigDataModifyEntry(&protocol,&copy,buffer+1,n);
  if(n==1 || n==2 || n==4 || n==8) {assert(!status && parse_calls==1 && data.bytes[0]==1);}
  else {assert(status==EFI_INVALID_PARAMETER && !parse_calls && !data.bytes[0]);}
 }
 copy.Option=NULL;assert(!ConfigDataModifyEntry(&protocol,&copy,buffer+1,1));
 copy.Offset=16;assert(ConfigDataModifyEntry(&protocol,&copy,buffer+1,1)==EFI_INVALID_PARAMETER);
 copy=entries[0];excess_options=1;
 assert(ConfigDataModifyEntry(&protocol,&copy,buffer+1,1)==EFI_COMPROMISED_DATA);excess_options=0;
 entries[0].Offset=15;copy=entries[0];
 assert(ConfigDataModifyEntry(&protocol,&copy,buffer+1,1)==EFI_INVALID_PARAMETER);
 entries[0].Offset=0;entries[0].Type=PARAMS_DATA_BOOLEAN_TYPE;copy=entries[0];
 assert(ConfigDataModifyEntry(&protocol,&copy,buffer+1,2)==EFI_INVALID_PARAMETER);
 assert(!ConfigDataModifyEntry(&protocol,&copy,buffer+1,1));
 entries[0].Type=PARAMS_DATA_STRING_TYPE;copy=entries[0];
 assert(!ConfigDataModifyEntry(&protocol,&copy,buffer,3));
 assert(!outstanding);return 0;
}
'''
                run_c(self, harness)


if __name__ == '__main__':
    unittest.main()
