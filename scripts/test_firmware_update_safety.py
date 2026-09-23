#!/usr/bin/env python3
"""Execute the selected custom updater with failing allocation and flash services."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from reconstruction_common import show_file

ROOT = Path(__file__).resolve().parents[1]
PLATFORM = 'edk2-platforms/Platform/CIX/Sky1/'


def source(path):
    local = os.environ.get('SOURCE_TEST_ROOT')
    if local:
        return (Path(local) / path).read_text()
    return show_file(ROOT, os.environ.get('SOURCE_TEST_REF', 'source/unofficial/1.3/current'), path).decode()


def function(text, name):
    match = re.search(r'(?m)^(?:STATIC\n)?(?:UINT8|UINT16|UINT32|BOOLEAN|EFI_STATUS|VOID|void)\n(?:EFIAPI\n)?' + name + r' \(', text)
    if match is None:
        raise AssertionError(f'missing {name}')
    end = text.index('\n}', text.index('\n{', match.end())) + 2
    return text[match.start():end] + '\n'


def run_c(test, body):
    with tempfile.TemporaryDirectory(prefix='firmware-update-') as directory:
        root = Path(directory)
        (root / 'test.c').write_text(body)
        compile_run = subprocess.run(
            ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
             '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
             str(root / 'test.c'), '-o', str(root / 'test')], capture_output=True, text=True)
        test.assertEqual(compile_run.returncode, 0, compile_run.stderr)
        result = subprocess.run([str(root / 'test')], capture_output=True, text=True)
        test.assertEqual(result.returncode, 0, result.stdout + result.stderr)


PRELUDE = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#define IN
#define OUT
#define STATIC static
#define EFIAPI
#define VOID void
#define TRUE 1
#define FALSE 0
#define EFI_SUCCESS 0
#define EFI_INVALID_PARAMETER 0x8000000000000002ULL
#define EFI_UNSUPPORTED 0x8000000000000003ULL
#define EFI_DEVICE_ERROR 0x8000000000000007ULL
#define EFI_OUT_OF_RESOURCES 0x8000000000000009ULL
#define EFI_NOT_FOUND 0x8000000000000014ULL
#define EFI_SECURITY_VIOLATION 0x800000000000001aULL
#define EFI_ERROR(s) (((uint64_t)(s) >> 63) != 0)
#define DEBUG(x) do {} while (0)
#define DebugPrint(...) do {} while (0)
#define Print(...) do {} while (0)
#define MAX_UINT8 UINT8_MAX
#define MAX_UINT32 UINT32_MAX
#define SIZE_4KB 4096
#define BIT0 1
#define BIT1 2
#define BIT2 4
#define BIT3 8
#define BIT4 16
#define BIT31 0x80000000U
#define OFFSET_OF(t,f) offsetof(t,f)
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define CopyMem(d,s,n) memcpy(d,s,n)
#define CompareMem(a,b,n) memcmp(a,b,n)
#define SetMem(p,n,v) memset(p,v,n)
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef size_t UINTN;
typedef char CHAR8;
typedef int BOOLEAN;
typedef uint64_t EFI_STATUS;
typedef EFI_STATUS (*EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS)(UINTN);
static int allocation_attempts, fail_allocation, outstanding;
static void *allocate(size_t n) {
  if (++allocation_attempts == fail_allocation) return NULL;
  void *p=calloc(1,n); assert(p); ++outstanding; return p;
}
static void release(void *p) { assert(p); --outstanding; assert(outstanding>=0); free(p); }
#define AllocatePool allocate
#define AllocateZeroPool allocate
#define AllocateRuntimePool allocate
#define AllocateRuntimeZeroPool allocate
#define FreePool release
'''


class FirmwareUpdateSafetyTests(unittest.TestCase):
    def protocol_header(self):
        return re.sub(r'(?m)^#include.*\n', '', source('src/' + PLATFORM + 'Include/Protocol/CixFwUpdateProtocol.h'))

    def test_protocol_flash_failures_bounds_and_cleanup(self):
        text = source('custom/overlay/' + PLATFORM + 'Drivers/FirmwareUpdateDxe/FwUpdateProtocolDxe.c')
        header = source('src/' + PLATFORM + 'Drivers/FirmwareUpdateDxe/FirmwareUpdate.h')
        header = header[header.index('#pragma pack(push)'):header.index('EFI_STATUS\nEFIAPI\nLocateNorFlashDiskIoProtocol')]
        functions = [
            'FlashRangeValid', 'FreeFirmwareUpdateState', 'ValidateFirmwareHeader',
            'CixFlashReadWrapper', 'ReadOnboardHeader', 'CixFirmwareUpdateInitialize',
            'CixFirmwareUpdateOnboardHeaderParse', 'IfNeedToUpdate', 'CixFirmwareVerify',
            'CixFlashWriteWrapper', 'CixFirmwareEntryUpdateWrapper', 'SplitAndWriteFirmware',
            'CixFirmwareFullUpdatewrapper', 'CixFirmwarePackageProgram',
            'CixFirmwareSingleProgram', 'CixFirmwareRawEntryUpdate',
            'ReadFwVersionOnboard', 'CixFirmwareGetVersion',
        ]
        harness = PRELUDE + self.protocol_header() + header + r'''
static unsigned char flash[8*1024*1024], image[8*1024*1024];
static int reads,writes,fail_read,fail_write,corrupt_read,fail_locate,abort_progress;
typedef struct disk {
 EFI_STATUS (*ReadDisk)(struct disk*,UINT32,UINT64,UINTN,void*);
 EFI_STATUS (*WriteDisk)(struct disk*,UINT32,UINT64,UINTN,void*);
} EFI_DISK_IO_PROTOCOL;
static EFI_STATUS read_disk(struct disk*d,UINT32 id,UINT64 off,UINTN n,void*buf) {
 assert(id==17 && off<=sizeof(flash) && n<=sizeof(flash)-off && buf);
 if (++reads==fail_read) return EFI_DEVICE_ERROR;
 memcpy(buf,flash+off,n); if(reads==corrupt_read && n) ((UINT8*)buf)[0]^=1;
 return 0;
}
static EFI_STATUS write_disk(struct disk*d,UINT32 id,UINT64 off,UINTN n,void*buf) {
 assert(id==17 && off<=sizeof(flash) && n<=sizeof(flash)-off && buf);
 if (++writes==fail_write) return EFI_DEVICE_ERROR;
 memcpy(flash+off,buf,n); return 0;
}
static EFI_DISK_IO_PROTOCOL disk={read_disk,write_disk}, *NorFlashDiskIo=&disk;
static UINT32 MediaId=17,FlashBlockSize=4096;
static UINTN FlashCapacity=sizeof(flash);
static CIX_FWUP_PRIVATE_DATA *pFwPrivateData;
static BOOLEAN IsOtaPackage;
static FIRMWARE_PROGRAM_STATUS FwProgStatus;
static UINT16 GlobalPercentage;
static EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS FimwareUpdateCallBack,ProgressFunc;
static EFI_STATUS LocateNorFlashDiskIoProtocol(void) { return fail_locate ? EFI_NOT_FOUND : 0; }
static EFI_STATUS progress(UINTN value) { assert(value<=100);return abort_progress?EFI_DEVICE_ERROR:0; }
'''
        harness += '\n'.join(function(text, name) for name in functions)
        harness += r'''
static FIRMWARE_HEADER *make_header(UINT8 *where) {
 FIRMWARE_HEADER *h=(void*)where; h->Signature=FIRMWARE_HEADER_SIGNATURE;h->Version=1;h->EntryCount=2;h->ControlFlag=0;
 h->EntryNode[0]=(FIRMWARE_ENTRY){3,0x400000,1488,0};
 h->EntryNode[1]=(FIRMWARE_ENTRY){7,0x406000,4096,0}; return h;
}
static void reset(void) {
 FreeFirmwareUpdateState();assert(outstanding==0);
 reads=writes=fail_read=fail_write=corrupt_read=fail_locate=abort_progress=0;
 fail_allocation=allocation_attempts=0;GlobalPercentage=0;ProgressFunc=progress;
 memset(flash,0,sizeof(flash));memset(image,0,sizeof(image));
 make_header(flash+FIRMWARE_HEADER_OFFSET);make_header(image+FIRMWARE_HEADER_OFFSET);
}
int main(void) {
 UINT8 block[4096]={1};UINT32 size;
 reset();assert(!CixFirmwarePackageProgram(image,sizeof(image),progress));assert(writes==64 && GlobalPercentage==100 && outstanding==0);
 for(int i=1;i<=64;i++) {
  reset();fail_write=i;assert(CixFirmwarePackageProgram(image,sizeof(image),progress)==FIRMWARE_RET_ERR_PROG);assert(writes==i && outstanding==0);
 }
 reset();fail_read=1;assert(CixFirmwarePackageProgram(image,sizeof(image),progress)==FIRMWARE_RET_READ_ERR);assert(!writes && !outstanding);
 reset();fail_read=2;assert(CixFirmwarePackageProgram(image,sizeof(image),progress)==FIRMWARE_RET_ERR_VERIFY);assert(writes==1 && !outstanding);
 reset();corrupt_read=2;assert(CixFirmwarePackageProgram(image,sizeof(image),progress)==FIRMWARE_RET_ERR_VERIFY);assert(writes==1 && !outstanding);
 reset();abort_progress=1;assert(CixFirmwarePackageProgram(image,sizeof(image),progress)==FIRMWARE_RET_ERR_PROG);assert(writes==1 && !outstanding);
 for(int i=1;i<=5;i++) {
  reset();fail_allocation=i;assert(CixFirmwarePackageProgram(image,sizeof(image),progress)!=0);assert(!outstanding);assert(writes==(i==5));
 }
 reset();fail_locate=1;assert(CixFirmwarePackageProgram(image,sizeof(image),NULL)==FIRMWARE_RET_NO_FLASH_PROG_PROTOCOL);assert(!writes);
 for(UINT32 n=0;n<sizeof(FIRMWARE_HEADER);n++) {
  reset();assert(CixFirmwarePackageProgram(image,n,NULL)!=0);assert(!writes && !outstanding);
 }
 reset();assert(CixFlashWriteWrapper(7,block,4096,0,UINT32_MAX)==FIRMWARE_RET_ERR_INPUT);assert(!writes);
 assert(EFI_ERROR(CixFlashReadWrapper(UINT32_MAX,4096,block)));assert(!reads);
 assert(SplitAndWriteFirmware(image,sizeof(image)+1)!=0);assert(!writes);
 // Malformed counts, truncated tables, overlapping/duplicate entries, bad alignment.
 reset();FIRMWARE_HEADER *h=(void*)(image+FIRMWARE_HEADER_OFFSET);
 h->EntryCount=0;assert(ValidateFirmwareHeader(h,4096,&size)!=0);
 h->EntryCount=UINT32_MAX;assert(ValidateFirmwareHeader(h,4096,&size)!=0);
 h=make_header((UINT8*)h);assert(ValidateFirmwareHeader(h,sizeof(FIRMWARE_HEADER),&size)!=0);
 h->EntryNode[1].Base=h->EntryNode[0].Base;assert(CixFirmwarePackageProgram(image,sizeof(image),NULL)==FIRMWARE_RET_ERR_LAYOUT);assert(!writes);
 h=make_header((UINT8*)h);h->EntryNode[1].Type=3;assert(CixFirmwarePackageProgram(image,sizeof(image),NULL)==FIRMWARE_RET_ERR_LAYOUT);assert(!writes);
 h=make_header((UINT8*)h);h->EntryNode[1].Base++;assert(CixFirmwarePackageProgram(image,sizeof(image),NULL)==FIRMWARE_RET_ERR_4KB_ALIGN);assert(!writes);
 h=make_header((UINT8*)h);h->EntryNode[1].Length=UINT32_MAX;assert(CixFirmwarePackageProgram(image,sizeof(image),NULL)==FIRMWARE_RET_ERR_SIZE);assert(!writes);
 // Alternate header and repeated calls release all state.
 reset();memmove(flash+FIRMWARE_HEADER_OFFSET_ALT,flash+FIRMWARE_HEADER_OFFSET,4096);memset(flash+FIRMWARE_HEADER_OFFSET,0,4096);
 memmove(image+FIRMWARE_HEADER_OFFSET_ALT,image+FIRMWARE_HEADER_OFFSET,4096);memset(image+FIRMWARE_HEADER_OFFSET,0,4096);
 assert(!CixFirmwareUpdateInitialize(image,sizeof(image),0));assert(pFwPrivateData->HeaderOffset==FIRMWARE_HEADER_OFFSET_ALT);
 assert(!CixFirmwareUpdateInitialize(image,sizeof(image),0));FreeFirmwareUpdateState();assert(!outstanding);
 // Both OTA entries must be checked before the first write.
 reset();h=make_header(image);h->ControlFlag=UPDATE_OTA_PACKAGE;
 h->EntryNode[1].Length++;assert((CixFirmwarePackageProgram(image,8192,NULL)&255)==FIRMWARE_RET_ERR_LAYOUT);assert(!writes && !outstanding);
 h=make_header(image);h->ControlFlag=UPDATE_OTA_PACKAGE;
 assert(!CixFirmwarePackageProgram(image,8192,NULL));assert(writes==2 && !outstanding);
 reset();h=make_header(image);h->ControlFlag=UPDATE_OTA_PACKAGE;
 assert(CixFirmwarePackageProgram(image,4096,NULL)==FIRMWARE_RET_ERR_SIZE);assert(!writes && !outstanding);
 // Raw memory config is a 4KiB block although the payload is shorter.
 reset();assert(CixFirmwareRawEntryUpdate(3,block,4096,ENTRY_WRITE,NULL)==0x300);assert(writes==1 && !outstanding);
 assert(CixFirmwareRawEntryUpdate(3,block,4096,ENTRY_READ,NULL)==0x300);assert(!outstanding);
 reset();assert((CixFirmwareRawEntryUpdate(5,block,4096,ENTRY_WRITE,NULL)&255)==FIRMWARE_RET_TYPE_NOT_FOUND);assert(!writes && !outstanding);
 assert((CixFirmwareRawEntryUpdate(3,NULL,4096,ENTRY_WRITE,NULL)&255)==FIRMWARE_RET_ERR_INPUT);
 assert((CixFirmwareRawEntryUpdate(3,block,4096,(ENTRY_UPDATE_METHOD)-1,NULL)&255)==FIRMWARE_RET_ERR_INPUT);
 reset();assert(!EFI_ERROR(CixFlashReadWrapper(sizeof(flash),0,block)));
 assert(CixFirmwareSingleProgram(3,image,sizeof(image),NULL)==0x300);assert(writes==1 && !outstanding);
 reset();assert((CixFirmwareSingleProgram(7,image,0,NULL)&255)==FIRMWARE_RET_ERR_INPUT);assert(!writes && !outstanding);
 reset();UINT32 off,b1,b2,b3;assert(CixFirmwareGetVersion(&off,&b1,&b2,&b3)==FIRMWARE_RET_READ_ERR);assert(!outstanding);
 return 0;
}
'''
        run_c(self, harness)

    def test_driver_mapping_cannot_widen_physical_flash_access(self):
        text = source('custom/overlay/' + PLATFORM + 'Drivers/FirmwareUpdateDxe/FwUpdateProtocolDxe.c')
        header = source('src/' + PLATFORM + 'Drivers/FirmwareUpdateDxe/FirmwareUpdate.h')
        header = header[header.index('#pragma pack(push)'):header.index('EFI_STATUS\nEFIAPI\nLocateNorFlashDiskIoProtocol')]
        harness = PRELUDE + header + r"""
#define SIZE_8MB (8U*1024*1024)
#define ByProtocol 1
typedef void *EFI_HANDLE;
typedef struct { UINT8 Index; } NOR_FLASH_DEVICE_PATH;
typedef NOR_FLASH_DEVICE_PATH EFI_DEVICE_PATH_PROTOCOL;
typedef struct { int dummy; } EFI_DISK_IO_PROTOCOL;
typedef struct { UINTN Size; struct { UINT32 BlockSize,MediaId; } Media; } NOR_FLASH_INSTANCE;
static NOR_FLASH_INSTANCE instance;
static EFI_DISK_IO_PROTOCOL disk,*NorFlashDiskIo;
static NOR_FLASH_DEVICE_PATH device={42};
static int gEfiDiskIoProtocolGuid,locate_error,handle_error;
static UINT32 MediaId,FlashBlockSize;
static UINTN FlashCapacity;
#define INSTANCE_FROM_DISKIO_THIS(p) (&instance)
static EFI_DEVICE_PATH_PROTOCOL *DevicePathFromHandle(void *h) { return &device; }
static BOOLEAN IsNorFlashDevicePath(EFI_DEVICE_PATH_PROTOCOL*p) { return p==&device; }
static EFI_STATUS handles(int kind,void*g,void*key,UINTN *n,EFI_HANDLE **p) {
 if(locate_error)return EFI_NOT_FOUND;
 *n=1;*p=allocate(sizeof(**p));(*p)[0]=&device;return 0;
}
static EFI_STATUS handle(void*h,void*g,void**p) { *p=&disk;return handle_error?EFI_DEVICE_ERROR:0; }
static struct { EFI_STATUS (*LocateHandleBuffer)(int,void*,void*,UINTN*,EFI_HANDLE**);
 EFI_STATUS (*HandleProtocol)(void*,void*,void**); } bs={handles,handle},*gBS=&bs;
"""
        harness += function(text, 'FlashRangeValid') + function(text, 'LocateNorFlashDiskIoProtocol') + r"""
int main(void) {
 instance.Size=16*1024*1024;instance.Media.BlockSize=4096;instance.Media.MediaId=17;
 assert(!LocateNorFlashDiskIoProtocol());assert(!outstanding && MediaId==17);
 assert(FlashCapacity==SIZE_8MB && !FlashRangeValid(SIZE_8MB,1));
 assert(FlashRangeValid(SIZE_8MB-1,1));
 instance.Size=4*1024*1024;assert(!LocateNorFlashDiskIoProtocol());assert(FlashCapacity==instance.Size && !outstanding);
 instance.Media.BlockSize=0;assert(EFI_ERROR(LocateNorFlashDiskIoProtocol()));assert(!NorFlashDiskIo && !FlashCapacity && !outstanding);
 handle_error=1;assert(EFI_ERROR(LocateNorFlashDiskIoProtocol()));assert(!NorFlashDiskIo && !outstanding);
 locate_error=1;assert(EFI_ERROR(LocateNorFlashDiskIoProtocol()));assert(!NorFlashDiskIo && !outstanding);
 return 0;
}
"""
        run_c(self, harness)

    def test_fmp_backup_and_restore_failures_propagate(self):
        text = source('custom/overlay/' + PLATFORM + 'Drivers/SystemFirmwareUpdate/SystemFirmwareReportDxe.c')
        if 'NvramBackup' not in function(text, 'SystemFirmwareUpdateWithProgress'):
            self.check_legacy_fmp_status(text)
            return
        status = source('custom/overlay/' + PLATFORM + 'Include/Library/CixFirmwareStatus.h')
        harness = PRELUDE + self.protocol_header() + function(status, 'CixFirmwareResultStatus') + r"""
#define SPI_VARIABLE_BASE 0x388000
#define SPI_VARIABLE_SIZE 4096
static CIX_FW_UPDATE_PROTOCOL updater,*FlashUpdateProtocol;
static int gCixFirmwareUpdateProtocolGuid, locate_error, reads,writes,programs,read_error,write_error,corrupt,program_status;
static EFI_STATUS locate(void*g,void*r,void**p) { *p=&updater;return locate_error?EFI_NOT_FOUND:0; }
static struct { EFI_STATUS (*LocateProtocol)(void*,void*,void**); } bs={locate},*gBS=&bs;
static EFI_STATUS LocateNorFlashDiskIoProtocol(void) { return locate_error?EFI_NOT_FOUND:0; }
static EFI_STATUS NorFlashDiskIoRead(UINT32 o,UINT32 n,void*p) {
 assert(o==SPI_VARIABLE_BASE && n==3*SPI_VARIABLE_SIZE);if(++reads==read_error)return EFI_DEVICE_ERROR;
 memset(p,0x5a,n);if(reads==corrupt)((UINT8*)p)[0]^=1;return 0;
}
static EFI_STATUS NorFlashDiskIoWrite(UINT32 o,UINT32 n,void*p) {
 assert(o==SPI_VARIABLE_BASE && n==3*SPI_VARIABLE_SIZE && ((UINT8*)p)[0]==0x5a);
 return ++writes==write_error?EFI_DEVICE_ERROR:0;
}
static UINT16 program(UINT8*p,UINT32 n,EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS cb) {
 assert(n==1);++programs;return program_status;
}
"""
        harness += function(text, 'SystemFirmwareUpdateWithProgress') + r"""
static void reset(void) {
 assert(!outstanding);reads=writes=programs=read_error=write_error=corrupt=program_status=locate_error=0;
 fail_allocation=allocation_attempts=0;updater.FirmwarePackageProgram=program;
}
int main(void) {
 UINT8 image=1;
 reset();assert(!SystemFirmwareUpdateWithProgress(&image,1,NULL));assert(programs==1 && reads==2 && writes==1 && !outstanding);
 reset();assert(SystemFirmwareUpdateWithProgress(&image,(UINTN)UINT32_MAX+1,NULL)==EFI_INVALID_PARAMETER);assert(!programs);
 reset();locate_error=1;assert(EFI_ERROR(SystemFirmwareUpdateWithProgress(&image,1,NULL)));assert(!programs);
 for(int i=1;i<=2;i++) {reset();fail_allocation=i;assert(SystemFirmwareUpdateWithProgress(&image,1,NULL)==EFI_OUT_OF_RESOURCES);assert(!programs && !writes && !outstanding);}
 reset();read_error=1;assert(EFI_ERROR(SystemFirmwareUpdateWithProgress(&image,1,NULL)));assert(!programs && !writes && !outstanding);
 reset();read_error=2;assert(EFI_ERROR(SystemFirmwareUpdateWithProgress(&image,1,NULL)));assert(programs==1 && writes==1 && !outstanding);
 reset();write_error=1;assert(EFI_ERROR(SystemFirmwareUpdateWithProgress(&image,1,NULL)));assert(programs==1 && !outstanding);
 reset();corrupt=2;assert(EFI_ERROR(SystemFirmwareUpdateWithProgress(&image,1,NULL)));assert(programs==1 && !outstanding);
 for(int error=1;error<256;error++) {reset();program_status=0x700|error;assert(EFI_ERROR(SystemFirmwareUpdateWithProgress(&image,1,NULL)));assert(writes==1 && !outstanding);}
 return 0;
}
"""
        run_c(self, harness)

    def check_legacy_fmp_status(self, text):
        header = source('custom/overlay/' + PLATFORM + 'Include/Library/CixFirmwareStatus.h')
        harness = PRELUDE + self.protocol_header() + function(header, 'CixFirmwareResultStatus') + r"""
static CIX_FW_UPDATE_PROTOCOL updater,*FlashUpdateProtocol;
static int gCixFirmwareUpdateProtocolGuid,locate_error,programs,program_status;
static EFI_STATUS locate(void*g,void*r,void**p) { *p=&updater;return locate_error?EFI_NOT_FOUND:0; }
static struct { EFI_STATUS (*LocateProtocol)(void*,void*,void**); } bs={locate},*gBS=&bs;
static UINT16 program(UINT8*p,UINT32 n,EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS cb) {
 assert(n==1);++programs;return program_status;
}
""" + function(text, 'SystemFirmwareUpdateWithProgress') + r"""
int main(void) {
 void*p=allocate(1);release(p);UINT8 image=1;updater.FirmwarePackageProgram=program;
 assert(!SystemFirmwareUpdateWithProgress(&image,1,NULL));assert(programs==1);
 assert(SystemFirmwareUpdateWithProgress(&image,(UINTN)UINT32_MAX+1,NULL)==EFI_INVALID_PARAMETER);
 assert(SystemFirmwareUpdateWithProgress(NULL,1,NULL)==EFI_INVALID_PARAMETER);assert(programs==1);
 locate_error=1;assert(EFI_ERROR(SystemFirmwareUpdateWithProgress(&image,1,NULL)));assert(programs==1);
 locate_error=0;
 for(int error=1;error<256;error++) {program_status=0x700|error;assert(EFI_ERROR(SystemFirmwareUpdateWithProgress(&image,1,NULL)));}
 return 0;
}
"""
        run_c(self, harness)

    def test_flash_library_callers_propagate_status_and_bound_serial_buffers(self):
        text = source('custom/overlay/' + PLATFORM + 'Library/FlashUpdateLib/FlashUpdate.c')
        header = source('custom/overlay/' + PLATFORM + 'Include/Library/CixFirmwareStatus.h')
        harness = PRELUDE + self.protocol_header() + function(header, 'CixFirmwareResultStatus') + r"""
#include <stdio.h>
#undef SetMem
#undef CopyMem
#define EFI_COMPROMISED_DATA 0x8000000000000021ULL
#define AsciiSPrint snprintf
#define AsciiStrSize(s) (strlen(s)+1)
#define SerialNum 0
typedef struct { CHAR8 *StrSerialNumber; } CIX_FASTBOOT_INFO_PROTOCOL;
typedef struct { EFI_STATUS (*GetSocInfo)(int,UINT32**,UINT32*); } CIX_SOC_INFO_PROTOCOL;
static CIX_FW_UPDATE_PROTOCOL updater,*FlashUpdateProtocol;
static CIX_FASTBOOT_INFO_PROTOCOL fastboot;
static CIX_SOC_INFO_PROTOCOL soc;
static UINT32 serial[2]={0x12345678,0x9abcdef0},serial_size=8;
static int gCixFirmwareUpdateProtocolGuid,gCixFastbootInfoProtocolGuid,gCixSocInfoProtocolGuid;
static int locate_error,fastboot_error,soc_error,programs,program_status,missing_serial;
static EFI_STATUS get_soc(int type,UINT32**p,UINT32*n) {
 *p=missing_serial?NULL:serial;*n=serial_size;return soc_error?EFI_DEVICE_ERROR:0;
}
static EFI_STATUS locate(void*g,void*r,void**p) {
 if(g==&gCixFastbootInfoProtocolGuid){*p=&fastboot;return fastboot_error?EFI_NOT_FOUND:0;}
 if(g==&gCixSocInfoProtocolGuid){*p=&soc;return 0;}
 assert(g==&gCixFirmwareUpdateProtocolGuid);*p=&updater;return locate_error?EFI_NOT_FOUND:0;
}
static void set_mem(void*p,UINTN n,UINT8 v) {memset(p,v,n);}
static void copy_mem(void*d,void*s,UINTN n) {memcpy(d,s,n);}
static struct { EFI_STATUS (*LocateProtocol)(void*,void*,void**);
 void (*SetMem)(void*,UINTN,UINT8);void (*CopyMem)(void*,void*,UINTN); } bs={locate,set_mem,copy_mem},*gBS=&bs;
static UINT16 program(UINT8*p,UINT32 n,EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS cb) {
 assert(n==1);++programs;return program_status;
}
static UINT16 single(UINT8 type,UINT8*p,UINT32 n,EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS cb) {
 assert(type==7);return program(p,n,cb);
}
""" + ''.join(function(text, name) for name in ('SystemFirmwareUpdate', 'SystemSingleFirmwareUpdate', 'GetSerialNum')) + r"""
int main(void) {
 void*p=allocate(1);release(p);UINT8 image=1;updater.FirmwarePackageProgram=program;updater.FirmwareSingleProgram=single;
 assert(!SystemFirmwareUpdate(&image,1));assert(!SystemSingleFirmwareUpdate(&image,7,1));assert(programs==2);
 assert(SystemSingleFirmwareUpdate(&image,256,1)==EFI_INVALID_PARAMETER);
 assert(SystemFirmwareUpdate(&image,(UINTN)UINT32_MAX+1)==EFI_INVALID_PARAMETER);
 assert(SystemFirmwareUpdate(NULL,1)==EFI_INVALID_PARAMETER);assert(programs==2);
 for(int error=1;error<256;error++) {program_status=0x700|error;
  assert(EFI_ERROR(SystemFirmwareUpdate(&image,1)));assert(EFI_ERROR(SystemSingleFirmwareUpdate(&image,7,1)));}
 char out[20];soc.GetSocInfo=get_soc;memset(out,0x5a,sizeof(out));
 assert(GetSerialNum(out,18)==EFI_INVALID_PARAMETER && out[0]==0x5a);
 assert(GetSerialNum(NULL,19)==EFI_INVALID_PARAMETER);
 fastboot.StrSerialNumber="12345678901234567890";
 assert(!GetSerialNum(out,19));assert(!strcmp(out,"0x1234567890123456") && out[19]==0x5a);
 fastboot_error=1;assert(!GetSerialNum(out,19));assert(!strcmp(out,"0x123456789abcdef0") && out[19]==0x5a);
 serial_size=4;assert(GetSerialNum(out,19)==EFI_COMPROMISED_DATA);
 serial_size=8;missing_serial=1;assert(GetSerialNum(out,19)==EFI_COMPROMISED_DATA);
 soc_error=1;assert(GetSerialNum(out,19)==EFI_NOT_FOUND);
 return 0;
}
"""
        run_c(self, harness)

    def test_vendor_status_words_are_efi_errors(self):
        header = source('custom/overlay/' + PLATFORM + 'Include/Library/CixFirmwareStatus.h')
        harness = PRELUDE + self.protocol_header() + function(header, 'CixFirmwareResultStatus')
        harness += r'''
int main(void) {
 void *p=allocate(1);release(p);
 for(int type=0;type<=255;type++) for(int status=0;status<=255;status++) {
  EFI_STATUS result=CixFirmwareResultStatus((UINT16)((type<<8)|status));
  assert(status ? EFI_ERROR(result) : result==EFI_SUCCESS);
 }
 return 0;
}
'''
        run_c(self, harness)


if __name__ == '__main__':
    unittest.main()
