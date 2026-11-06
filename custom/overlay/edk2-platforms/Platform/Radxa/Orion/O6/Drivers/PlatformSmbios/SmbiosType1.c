/*
  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "PlatformSmbios.h"
#include <Library/BaseCryptLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/SocInfoProtocol.h>

#define TYPE1_STRINGS                                                                         \
  "Radxa Computer (Shenzhen) Co., Ltd.\0"   /* Manufacturer */                                \
  "Radxa Orion O6\0"                        /* Product Name */                                \
  "1.0\0"                                   /* Version */                                     \
  "Radxa System Serial Number\0"            /* Serial number */                               \
  "Default\0"                               /* SKUNumber */                                   \
  "Orion O6\0"                              /* System Family */

#define TYPE1_UUID_URL_NAMESPACE_SIZE  16

STATIC CONST UINT8  mUuidV5UrlNamespace[TYPE1_UUID_URL_NAMESPACE_SIZE] = {
  0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1,
  0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
};

#pragma pack(1)
typedef struct {
  SMBIOS_TABLE_TYPE1    Base;
  UINT8                 Strings[sizeof (TYPE1_STRINGS)];
} PLATFORM_SMBIOS_TYPE1;
#pragma pack()

STATIC PLATFORM_SMBIOS_TYPE1  mPlatformDefaultType1 = {
  {
    {
      EFI_SMBIOS_TYPE_SYSTEM_INFORMATION,
      sizeof (SMBIOS_TABLE_TYPE1),
      SMBIOS_HANDLE_PI_RESERVED,
    },
    1,
    2,
    3,
    4,
    {
      0xFFFFFFFF,
      0xFFFF,
      0xFFFF,
      { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
    },
    SystemWakeupTypePowerSwitch,
    5,
    6,
  },
  TYPE1_STRINGS
};

STATIC
EFI_STATUS
GetSocSerialWords (
  OUT UINT32  SerialWords[2]
  )
{
  EFI_STATUS             Status;
  CIX_SOC_INFO_PROTOCOL  *SocInfoProtocol;
  UINT32                 *SocInfo;
  UINT32                 SocInfoSize;

  Status = gBS->LocateProtocol (
                  &gCixSocInfoProtocolGuid,
                  NULL,
                  (VOID **)&SocInfoProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: locate SocInfo protocol failed: %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = SocInfoProtocol->GetSocInfo (SerialNum, &SocInfo, &SocInfoSize);
  if (EFI_ERROR (Status) || (SocInfo == NULL) || (SocInfoSize < 2)) {
    DEBUG ((DEBUG_ERROR, "%a: GetSocInfo(SerialNum) failed: %r\n", __FUNCTION__, Status));
    return EFI_NOT_FOUND;
  }

  SerialWords[0] = SocInfo[0];
  SerialWords[1] = SocInfo[1];
  return EFI_SUCCESS;
}

STATIC
VOID
GuidFromRfc4122Bytes (
  OUT EFI_GUID     *Guid,
  IN CONST UINT8   Rfc4122Bytes[16]
  )
{
  Guid->Data1 = ((UINT32)Rfc4122Bytes[0] << 24) |
                ((UINT32)Rfc4122Bytes[1] << 16) |
                ((UINT32)Rfc4122Bytes[2] << 8) |
                Rfc4122Bytes[3];
  Guid->Data2 = (UINT16)(((UINT16)Rfc4122Bytes[4] << 8) | Rfc4122Bytes[5]);
  Guid->Data3 = (UINT16)(((UINT16)Rfc4122Bytes[6] << 8) | Rfc4122Bytes[7]);
  CopyMem (Guid->Data4, &Rfc4122Bytes[8], sizeof (Guid->Data4));
}

STATIC
EFI_STATUS
BuildSystemUuidFromSerialWords (
  IN  CONST UINT32  SerialWords[2],
  OUT EFI_GUID      *SystemUuid
  )
{
  CHAR8  NameBuffer[96];
  UINT8  HashInput[TYPE1_UUID_URL_NAMESPACE_SIZE + sizeof (NameBuffer)];
  UINT8  Digest[SHA1_DIGEST_SIZE];
  UINTN  NameLength;

  AsciiSPrint (
    NameBuffer,
    sizeof (NameBuffer),
    "https://radxa.com/orion/o6/system/%08x%08x",
    SerialWords[0],
    SerialWords[1]
    );
  NameLength = AsciiStrLen (NameBuffer);

  CopyMem (HashInput, mUuidV5UrlNamespace, TYPE1_UUID_URL_NAMESPACE_SIZE);
  CopyMem (&HashInput[TYPE1_UUID_URL_NAMESPACE_SIZE], NameBuffer, NameLength);
  if (!Sha1HashAll (HashInput, TYPE1_UUID_URL_NAMESPACE_SIZE + NameLength, Digest)) {
    return EFI_DEVICE_ERROR;
  }

  Digest[6] = (UINT8)((Digest[6] & 0x0F) | 0x50);
  Digest[8] = (UINT8)((Digest[8] & 0x3F) | 0x80);
  GuidFromRfc4122Bytes (SystemUuid, Digest);
  return EFI_SUCCESS;
}

STATIC
VOID
FormatSocSerialAscii (
  IN  CONST UINT32  SerialWords[2],
  OUT CHAR8         SerialBuffer[17]
  )
{
  AsciiSPrint (SerialBuffer, 17, "%08x%08x", SerialWords[0], SerialWords[1]);
}

EFI_STATUS
AddSmbiosType1 (
  IN EFI_SMBIOS_PROTOCOL  *Smbios
  )
{
  EFI_STATUS             Status;
  EFI_SMBIOS_HANDLE      SmbiosHandle;
  PLATFORM_SMBIOS_TYPE1  Type1Record;
  CHAR8                  SerialBuffer[17];
  UINT32                 SerialWords[2];
  UINTN                  StringNumber;
  VOID                   *StoredUuid;
  UINTN                  StoredUuidSize;

  Type1Record = mPlatformDefaultType1;
  StoredUuid  = NULL;

  Status = GetVariable2 (L"SystemUUID", &gCixGPNVGuid, &StoredUuid, &StoredUuidSize);
  if (!EFI_ERROR (Status) && (StoredUuidSize == sizeof (EFI_GUID))) {
    CopyMem (&Type1Record.Base.Uuid, StoredUuid, sizeof (EFI_GUID));
  } else if (!EFI_ERROR (GetSocSerialWords (SerialWords))) {
    FormatSocSerialAscii (SerialWords, SerialBuffer);
    Status = BuildSystemUuidFromSerialWords (SerialWords, &Type1Record.Base.Uuid);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: failed to derive SystemUUID from SoC serial: %r\n", __FUNCTION__, Status));
    }
  }

  if (StoredUuid != NULL) {
    FreePool (StoredUuid);
  }

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status       = Smbios->Add (
                           Smbios,
                           NULL,
                           &SmbiosHandle,
                           (EFI_SMBIOS_TABLE_HEADER *)&Type1Record
                           );

  if (EFI_ERROR (Status)) {
    DEBUG (
      (
       DEBUG_ERROR,
       "[%a]:[%dL] Smbios Type1 Table Log Failed! %r \n",
       __FUNCTION__,
       DEBUG_LINE_NUMBER,
       Status
      )
      );
    return Status;
  }

  if (!EFI_ERROR (GetSocSerialWords (SerialWords))) {
    FormatSocSerialAscii (SerialWords, SerialBuffer);
    StringNumber = 4;
    Status       = Smbios->UpdateString (Smbios, &SmbiosHandle, &StringNumber, SerialBuffer);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: failed to update system serial string: %r\n", __FUNCTION__, Status));
    }
  }

  return EFI_SUCCESS;
}
