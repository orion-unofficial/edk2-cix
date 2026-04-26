/*
  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "PlatformSmbios.h"

#ifdef O6_SMBIOS_CHASSIS_ASSET_TAG
#define TYPE3_ASSET_TAG_INDEX   3
#define TYPE3_ASSET_TAG_STRING  O6_SMBIOS_CHASSIS_ASSET_TAG "\0"
#else
#define TYPE3_ASSET_TAG_INDEX   0
#define TYPE3_ASSET_TAG_STRING
#endif

#define TYPE3_STRINGS                                                                    \
  "Radxa Computer (Shenzhen) Co., Ltd.\0" /* Manufacturer */                             \
  "1.0\0"                                 /* Version */                                  \
  TYPE3_ASSET_TAG_STRING

#pragma pack(1)
typedef struct {
  SMBIOS_TABLE_TYPE3    Base;
  UINT8                 Strings[sizeof (TYPE3_STRINGS)];
} PLATFORM_SMBIOS_TYPE3;
#pragma pack()

STATIC CONST PLATFORM_SMBIOS_TYPE3  mPlatformDefaultType3 = {
  {
    {
      EFI_SMBIOS_TYPE_SYSTEM_ENCLOSURE,
      sizeof (SMBIOS_TABLE_TYPE3),
      SMBIOS_HANDLE_CHASSIS,
    },
    1,
    MiscChassisTypeUnknown,
    2,
    0,
    TYPE3_ASSET_TAG_INDEX,
    ChassisStateUnknown,
    ChassisStateSafe,
    ChassisStateSafe,
    ChassisSecurityStatusNone,
    {
      0,
      0,
      0,
      0,
    },
    0,
    0,
    0,
  },
  TYPE3_STRINGS
};

EFI_STATUS
AddSmbiosType3 (
  IN EFI_SMBIOS_PROTOCOL  *Smbios
  )
{
  EFI_STATUS         Status;
  EFI_SMBIOS_HANDLE  SmbiosHandle;

  SmbiosHandle = SMBIOS_HANDLE_CHASSIS;
  Status       = Smbios->Add (
                           Smbios,
                           NULL,
                           &SmbiosHandle,
                           (EFI_SMBIOS_TABLE_HEADER *)&mPlatformDefaultType3
                           );

  if (EFI_ERROR (Status)) {
    DEBUG (
      (
       DEBUG_ERROR,
       "[%a]:[%dL] Smbios Type3 Table Log Failed! %r \n",
       __FUNCTION__,
       DEBUG_LINE_NUMBER,
       Status
      )
      );
  }

  return EFI_SUCCESS;
}
