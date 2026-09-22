/*
  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "PlatformSmbios.h"

#ifdef O6_SMBIOS_BASEBOARD_ASSET_TAG
#define TYPE2_ASSET_TAG_INDEX   4
#define TYPE2_LOCATION_INDEX    5
#define TYPE2_ASSET_TAG_STRING  O6_SMBIOS_BASEBOARD_ASSET_TAG "\0"
#else
#define TYPE2_ASSET_TAG_INDEX   0
#define TYPE2_LOCATION_INDEX    4
#define TYPE2_ASSET_TAG_STRING
#endif

#define TYPE2_STRINGS                                                                  \
  "Radxa Computer (Shenzhen) Co., Ltd.\0"   /* Manufacturer */                         \
  "Radxa Orion O6\0"                        /* Product Name */                         \
  "1.0\0"                                   /* Version */                              \
  TYPE2_ASSET_TAG_STRING                                                      \
  "Part Component\0"                        /* board location */

#pragma pack(1)
typedef struct {
  SMBIOS_TABLE_TYPE2    Base;
  UINT8                 Strings[sizeof (TYPE2_STRINGS)];
} PLATFORM_SMBIOS_TYPE2;
#pragma pack()

STATIC PLATFORM_SMBIOS_TYPE2  mPlatformDefaultType2 = {
  {
    {
      EFI_SMBIOS_TYPE_BASEBOARD_INFORMATION,
      sizeof (SMBIOS_TABLE_TYPE2),
      SMBIOS_HANDLE_BASE_BOARD,
    },
    1,
    2,
    3,
    0,
    TYPE2_ASSET_TAG_INDEX,
    {
      1,
    },
    TYPE2_LOCATION_INDEX,
    SMBIOS_HANDLE_CHASSIS,
    BaseBoardTypeMotherBoard,
    1,
    { SMBIOS_HANDLE_CLUSTER1 },
  },
  TYPE2_STRINGS
};

EFI_STATUS
AddSmbiosType2 (
  IN EFI_SMBIOS_PROTOCOL  *Smbios
  )
{
  EFI_STATUS               Status;
  EFI_SMBIOS_HANDLE        SmbiosHandle;
  EFI_SMBIOS_HANDLE        SmbiosHandleType4;
  EFI_SMBIOS_TABLE_HEADER  *Record;
  EFI_SMBIOS_TYPE          Type4;
  PLATFORM_SMBIOS_TYPE2    Type2Record;

  Type2Record       = mPlatformDefaultType2;
  Type4             = SMBIOS_TYPE_PROCESSOR_INFORMATION;
  SmbiosHandleType4 = SMBIOS_HANDLE_PI_RESERVED;
  Status            = Smbios->GetNext (Smbios, &SmbiosHandleType4, &Type4, &Record, NULL);
  if (!EFI_ERROR (Status) && (SmbiosHandleType4 != SMBIOS_HANDLE_PI_RESERVED)) {
    Type2Record.Base.ContainedObjectHandles[0] = SmbiosHandleType4;
  } else {
    DEBUG ((DEBUG_ERROR, "Fail to locate the handle of Smbios Type4 entry!\n"));
  }

  SmbiosHandle = SMBIOS_HANDLE_BASE_BOARD;
  Status       = Smbios->Add (
                           Smbios,
                           NULL,
                           &SmbiosHandle,
                           (EFI_SMBIOS_TABLE_HEADER *)&Type2Record
                           );

  if (EFI_ERROR (Status)) {
    DEBUG (
      (
       DEBUG_ERROR,
       "[%a]:[%dL] Smbios Type2 Table Log Failed! %r \n",
       __FUNCTION__,
       DEBUG_LINE_NUMBER,
       Status
      )
      );
  }

  return EFI_SUCCESS;
}
