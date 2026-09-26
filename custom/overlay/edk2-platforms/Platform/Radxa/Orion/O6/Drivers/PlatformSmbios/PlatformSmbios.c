/*
  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "PlatformSmbios.h"
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>

STATIC ADD_PLATFORM_SMBIOS_TABLE  *mPlatformSmbiosTableList[] = {
  AddSmbiosType0,
  AddSmbiosType1,
  AddSmbiosType2,
  AddSmbiosType3,
  AddSmbiosType4,
  AddSmbiosType7,
  AddSmbiosType32,
  NULL
};

STATIC CHAR8  *mPlatformSmbiosTableName[] = {
  "AddSmbiosType0",
  "AddSmbiosType1",
  "AddSmbiosType2",
  "AddSmbiosType3",
  "AddSmbiosType4",
  "AddSmbiosType7",
  "AddSmbiosType32",
  NULL
};

STATIC ADD_PLATFORM_SMBIOS_TABLE  *mPlatformSmbiosTableListFirmwareFixes[] = {
  AddSmbiosType0,
  AddSmbiosType1,
  AddSmbiosType2,
  AddSmbiosType3,
  AddSmbiosType32,
  NULL
};

STATIC CHAR8  *mPlatformSmbiosTableNameFirmwareFixes[] = {
  "AddSmbiosType0",
  "AddSmbiosType1",
  "AddSmbiosType2",
  "AddSmbiosType3",
  "AddSmbiosType32",
  NULL
};

EFI_STATUS
EFIAPI
PlatformSmbiosEntryPoint (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS           Status;
  UINTN                Index;
  EFI_SMBIOS_PROTOCOL  *Smbios;
  ADD_PLATFORM_SMBIOS_TABLE  **PlatformSmbiosTableList;
  CHAR8                      **PlatformSmbiosTableName;

  //
  // Find the SMBIOS protocol
  //
  Status = gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a] Fail to locate gEfiSmbiosProtocolGuid!\n", __FUNCTION__));
    return Status;
  }

  if (FixedPcdGetBool (PcdCustomFirmwareFixesEnable)) {
    PlatformSmbiosTableList = mPlatformSmbiosTableListFirmwareFixes;
    PlatformSmbiosTableName = mPlatformSmbiosTableNameFirmwareFixes;
  } else {
    PlatformSmbiosTableList = mPlatformSmbiosTableList;
    PlatformSmbiosTableName = mPlatformSmbiosTableName;
  }

  for (Index = 0; PlatformSmbiosTableList[Index]; Index++) {
    Status = PlatformSmbiosTableList[Index](Smbios);
    DEBUG (
      (DEBUG_INFO,
       "[Platfrom Smbios] Execute Add: %a, Status: %r\n",
       PlatformSmbiosTableName[Index], Status)
      );
  }

  return EFI_SUCCESS;
}
