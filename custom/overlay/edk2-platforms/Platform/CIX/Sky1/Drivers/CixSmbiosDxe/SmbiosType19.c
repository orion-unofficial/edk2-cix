/*
  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "CixSmbiosDxe.h"

#define TYPE19_STRINGS  "\0" /* nothing */
#define SKY1_LOW_DRAM_LIMIT_MB  0x7800

#pragma pack(1)
typedef struct {
  SMBIOS_TABLE_TYPE19    Base;
  UINT8                  Strings[sizeof (TYPE19_STRINGS)];
} CIX_TYPE19;
#pragma pack()

// Memory array mapped address, this structure
// is overridden by InstallMemoryStructure
STATIC CIX_TYPE19  mCixDefaultType19 = {
  {
    {
      // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS,       // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE19),                      // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    0xFFFFFFFF,     // invalid, look at extended addr field
    0xFFFFFFFF,
    SMBIOS_HANDLE_MEMORY,     // handle
    1,
    0x080000000,     // starting addr of first 2GB
    0x100000000,     // ending addr of first 2GB
  },
  TYPE19_STRINGS
};

STATIC
EFI_STATUS
LogType19Range (
  IN EFI_SMBIOS_PROTOCOL  *Smbios,
  IN UINT64               StartingAddress,
  IN UINT64               RegionLength
  )
{
  EFI_STATUS         Status;
  EFI_SMBIOS_HANDLE  SmbiosHandle;
  CIX_TYPE19         Type19Record;

  if (RegionLength == 0) {
    return EFI_SUCCESS;
  }

  Type19Record = mCixDefaultType19;
  Type19Record.Base.ExtendedStartingAddress = StartingAddress;
  Type19Record.Base.ExtendedEndingAddress   = StartingAddress + RegionLength - 1;

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status       = Smbios->Add (
                           Smbios,
                           NULL,
                           &SmbiosHandle,
                           (EFI_SMBIOS_TABLE_HEADER *)&Type19Record
                           );

  if (EFI_ERROR (Status)) {
    DEBUG (
      (
       DEBUG_ERROR,
       "[%a]:[%dL] Smbios Type19 Table Log Failed! %r \n",
       __FUNCTION__,
       DEBUG_LINE_NUMBER,
       Status
      )
      );
  }

  return Status;
}

EFI_STATUS
AddSmbiosType19 (
  IN EFI_SMBIOS_PROTOCOL           *Smbios,
  IN const MEM_INIT_OUTPUT_BUFFER  *MemoryInfo
  )
{
  EFI_STATUS  Status;
  UINT64      LowRegionLength;
  UINT64      HighRegionLength;

  // Match the platform DRAM map: low memory stays in the 0x80000000 window
  // and anything above 0x7800 MB is remapped into the high DRAM aperture.
  if (MemoryInfo->AvailableSize > SKY1_LOW_DRAM_LIMIT_MB) {
    LowRegionLength  = ((UINT64)SKY1_LOW_DRAM_LIMIT_MB) << 20;
    HighRegionLength = ((UINT64)(MemoryInfo->AvailableSize - SKY1_LOW_DRAM_LIMIT_MB)) << 20;
  } else {
    LowRegionLength  = ((UINT64)MemoryInfo->AvailableSize) << 20;
    HighRegionLength = 0;
  }

  Status = LogType19Range (Smbios, PcdGet64 (PcdSystemMemoryBase), LowRegionLength);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return LogType19Range (Smbios, FixedPcdGet64 (PcdDramHighSpaceBase), HighRegionLength);
}
