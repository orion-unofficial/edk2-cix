/*
  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "PlatformSmbios.h"
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>

#define STR(x) XSTR(x)
#define XSTR(x) #x
#define BIOS_VENDOR        "Radxa Computer (Shenzhen) Co., Ltd."
#define BIOS_RELEASE_DATE  STR (COMPILE_BUILD_DATE)

STATIC
UINTN
AppendAsciiSmbiosString (
  OUT CHAR8        *Destination,
  IN  CONST CHAR8  *Source
  )
{
  UINTN  Index;

  for (Index = 0; Source[Index] != '\0'; Index++) {
    Destination[Index] = Source[Index];
  }

  Destination[Index] = '\0';
  return Index + 1;
}

STATIC
UINTN
AppendUnicodeAsAsciiSmbiosString (
  OUT CHAR8         *Destination,
  IN  CONST CHAR16  *Source
  )
{
  UINTN  Index;

  for (Index = 0; Source[Index] != L'\0'; Index++) {
    Destination[Index] = (CHAR8)Source[Index];
  }

  Destination[Index] = '\0';
  return Index + 1;
}

EFI_STATUS
AddSmbiosType0 (
  IN EFI_SMBIOS_PROTOCOL  *Smbios
  )
{
  CONST CHAR16       *FirmwareVersion;
  UINTN              StringsSize;
  CHAR8              *StringWalker;
  EFI_STATUS         Status;
  EFI_SMBIOS_HANDLE  SmbiosHandle;
  SMBIOS_TABLE_TYPE0 *Type0;

  FirmwareVersion = (CONST CHAR16 *)PcdGetPtr (PcdFirmwareVersionString);
  StringsSize     = AsciiStrSize (BIOS_VENDOR) + StrLen (FirmwareVersion) + 1 + AsciiStrSize (BIOS_RELEASE_DATE) + 1;
  Type0           = AllocateZeroPool (sizeof (SMBIOS_TABLE_TYPE0) + StringsSize);
  if (Type0 == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Type0->Hdr.Type                         = EFI_SMBIOS_TYPE_BIOS_INFORMATION;
  Type0->Hdr.Length                       = sizeof (SMBIOS_TABLE_TYPE0);
  Type0->Hdr.Handle                       = SMBIOS_HANDLE_PI_RESERVED;
  Type0->Vendor                           = 1;
  Type0->BiosVersion                      = 2;
  Type0->BiosSegment                      = 0;
  Type0->BiosReleaseDate                  = 3;
  Type0->BiosSize                         = 0x7F;
  Type0->BiosCharacteristics.PciIsSupported = 1;
  Type0->BiosCharacteristics.PlugAndPlayIsSupported = 1;
  Type0->BiosCharacteristics.BiosIsUpgradable = 1;
  Type0->BiosCharacteristics.SelectableBootIsSupported = 1;
  Type0->BIOSCharacteristicsExtensionBytes[0] = 0x1;
  Type0->BIOSCharacteristicsExtensionBytes[1] = 0xC;
  Type0->SystemBiosMajorRelease = (PcdGet32 (PcdFirmwareRevision) >> 16) & 0xFF;
  Type0->SystemBiosMinorRelease = PcdGet32 (PcdFirmwareRevision) & 0xFF;
  Type0->EmbeddedControllerFirmwareMajorRelease = 0xFF;
  Type0->EmbeddedControllerFirmwareMinorRelease = 0xFF;

  StringWalker  = (CHAR8 *)(Type0 + 1);
  StringWalker += AppendAsciiSmbiosString (StringWalker, BIOS_VENDOR);
  StringWalker += AppendUnicodeAsAsciiSmbiosString (StringWalker, FirmwareVersion);
  StringWalker += AppendAsciiSmbiosString (StringWalker, BIOS_RELEASE_DATE);
  *StringWalker = '\0';

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status       = Smbios->Add (
                           Smbios,
                           NULL,
                           &SmbiosHandle,
                           (EFI_SMBIOS_TABLE_HEADER *)Type0
                           );
  FreePool (Type0);

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[%a]:[%dL] Smbios Type0 Table Log Failed! %r \n",
      __FUNCTION__,
      DEBUG_LINE_NUMBER,
      Status
      ));
  }

  return EFI_SUCCESS;
}
