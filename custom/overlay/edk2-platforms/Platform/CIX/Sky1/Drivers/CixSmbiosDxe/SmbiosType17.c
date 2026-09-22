/*
  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "CixSmbiosDxe.h"
#include <Protocol/EcPlatformProtocol.h>
#include <Protocol/MemOutputBuffer.h>

#define TYPE17_STRINGS                                                         \
  "Top - on board\0" /* location */                                            \
  "BANK 0\0"         /* bank description */                                    \
  "BANK 1\0"         /* bank description */                                    \
  "BANK 2\0"         /* bank description */                                    \
  "BANK 3\0"         /* bank description */                                    \
  "Samsung\0"        /* manufacturer */                                        \
  "K3LKCKC0BM-MGCP\0" /* part number */

#define RADXA_ORION_O6_PCB_SKU            4
#define RADXA_ORION_O6_SAMSUNG_MEMTYPE    11
#define RADXA_ORION_O6_REV_A_BOARD_REV    0
#define TYPE17_MANUFACTURER_STRING_INDEX  6
#define TYPE17_PART_NUMBER_STRING_INDEX   7

STATIC
BOOLEAN
IsValidatedSamsungO6RevA (
  VOID
  )
{
  EFI_STATUS               Status;
  EC_PLATFORM_PROTOCOL  *EcPlatformProtocol;
  EC_RESPONSE           EcResponse;
  UINTN                 PcbSku;
  UINTN                 MemType;

  Status = gBS->LocateProtocol (
                  &gCixEcPlatformProtocolGuid,
                  NULL,
                  (VOID **)&EcPlatformProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: EC platform protocol not found: %r\n", __FUNCTION__, Status));
    return FALSE;
  }

  Status = EcPlatformProtocol->Transfer (EcPlatformProtocol, EC_COMMAND_GET_BOARD_ID, NULL, &EcResponse);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: GET_BOARD_ID failed: %r\n", __FUNCTION__, Status));
    return FALSE;
  }

  PcbSku  = EcResponse.BoardId.Id.Sku + (EcResponse.BoardId.Id.SkuExt << 3);
  MemType = EcResponse.BoardId.Id.Memory + (EcResponse.BoardId.Id.MemExt << 3);
  return (PcbSku == RADXA_ORION_O6_PCB_SKU) &&
         (MemType == RADXA_ORION_O6_SAMSUNG_MEMTYPE) &&
         (EcResponse.BoardId.Id.Rev == RADXA_ORION_O6_REV_A_BOARD_REV);
}

#pragma pack(1)
typedef struct {
  SMBIOS_TABLE_TYPE17    Base;
  UINT8                  Strings[sizeof (TYPE17_STRINGS)];
} CIX_TYPE17;
#pragma pack()

// Memory device
STATIC CIX_TYPE17  mCixDefaultType17 = {
  {
    {
      // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_MEMORY_DEVICE,       // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE17),        // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    SMBIOS_HANDLE_MEMORY,                            // array to which this module belongs
    0xFFFE,                                          // no errors
    32,                                              // unknown total width
    32,                                              // unknown data width
    0,                                               // Size
    0x0b,                                            // Row of chips
    0,                                               // not part of a set
    1,                                               // Device locator
    2,                                               // bank 0
    MemoryTypeUnknown,                               // memory type
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // unbuffered
    0,                                               // speed
    0,                                               // varies between different production runs
    0,                                               // serial
    0,                                               // asset tag
    0,                                               // part number
    0,                                               // rank
  },
  TYPE17_STRINGS
};

EFI_STATUS
AddSmbiosType17 (
  IN EFI_SMBIOS_PROTOCOL           *Smbios,
  IN const MEM_INIT_OUTPUT_BUFFER  *MemoryInfo
  )
{
  EFI_STATUS         Status;
  EFI_SMBIOS_HANDLE  SmbiosHandle;
  UINT8              MemoryDevNum;
  UINT8              i;
  UINT16             MemorySize;
  CIX_TYPE17         Type17Record;

  Type17Record = mCixDefaultType17;
  MemoryDevNum = COUNT_MEMORY_DEVICE_NUMBER (MemoryInfo->ChannelMask);
  MemorySize   = MemoryInfo->TotalSize/MemoryDevNum;
  if (MemorySize >= 0x7fff) {
    Type17Record.Base.Size         = 0x7fff;
    Type17Record.Base.ExtendedSize = MemorySize;
  } else {
    Type17Record.Base.Size = MemorySize;
  }

  if (MemoryInfo->DdrType == DDR_TYPE_LPDDR4X) {
    Type17Record.Base.MemoryType = MemoryTypeLpddr4;
  } else if (MemoryInfo->DdrType == DDR_TYPE_LPDDR5) {
    Type17Record.Base.MemoryType = MemoryTypeLpddr5;
  }

  Type17Record.Base.Speed                      = MemoryInfo->MaxFreq*2;
  Type17Record.Base.Attributes                 = MemoryInfo->RanksPerChannel;
  Type17Record.Base.ConfiguredMemoryClockSpeed = MemoryInfo->MaxFreq*2;
  Type17Record.Base.MemoryTechnology           = MemoryTechnologyDram;
  if (IsValidatedSamsungO6RevA ()) {
    Type17Record.Base.Manufacturer = TYPE17_MANUFACTURER_STRING_INDEX;
    Type17Record.Base.PartNumber   = TYPE17_PART_NUMBER_STRING_INDEX;
    Type17Record.Base.MemoryOperatingModeCapability.Bits.VolatileMemory = 1;
  }

  for (i = 0; i < MemoryDevNum; i++) {
    Type17Record.Base.BankLocator = 2+i;
    SmbiosHandle                  = SMBIOS_HANDLE_PI_RESERVED;
    Status                        = Smbios->Add (
                                                   Smbios,
                                                   NULL,
                                                   &SmbiosHandle,
                                                   (EFI_SMBIOS_TABLE_HEADER *)&Type17Record
                                                   );
    if (EFI_ERROR (Status)) {
      DEBUG (
        (
         DEBUG_ERROR,
         "[%a]:[%dL] Smbios Type17 Table Log Failed! %r \n",
         __FUNCTION__,
         DEBUG_LINE_NUMBER,
         Status
        )
        );
    }
  }

  return EFI_SUCCESS;
}
