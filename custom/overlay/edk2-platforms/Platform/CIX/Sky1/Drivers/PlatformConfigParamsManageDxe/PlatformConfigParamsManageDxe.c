/** @file

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PlatformConfigParamsManageDxe.h"

EFI_STATUS
EFIAPI
PlatformConfigDataGetEntry (
  IN     CIX_PLATFORM_CONFIG_PARAMS_MANAGE_PROTOCOL  *This,
  IN     UINT32                                      Index,
  IN OUT PLATFORM_CONFIG_PARAMS_DATA_ENTRY           *Entry,
  IN OUT VOID                                        *Buffer,
  IN OUT UINTN                                       *BufferSize
  )
{
  if ((This == NULL) || (This->Data == NULL) || (This->Entry == NULL) ||
      (Index >= This->EntryNum)) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((This->Entry[Index].Offset > sizeof (*This->Data)) ||
      (This->Entry[Index].Size > sizeof (*This->Data) - This->Entry[Index].Offset)) {
    return EFI_COMPROMISED_DATA;
  }

  if (*BufferSize < This->Entry[Index].Size) {
    *BufferSize = This->Entry[Index].Size;
    return EFI_BUFFER_TOO_SMALL;
  }

  if ((Buffer == NULL) || (Entry == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem ((VOID *)Buffer, (VOID *)(((UINT8 *)(This->Data)) + This->Entry[Index].Offset), This->Entry[Index].Size);
  CopyMem ((VOID *)Entry, (VOID *)(&This->Entry[Index]), sizeof (PLATFORM_CONFIG_PARAMS_DATA_ENTRY));

  *BufferSize = This->Entry[Index].Size;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PlatformConfigDataFindEntry (
  IN     CIX_PLATFORM_CONFIG_PARAMS_MANAGE_PROTOCOL  *This,
  IN     UINT32                                      Id,
  IN OUT PLATFORM_CONFIG_PARAMS_DATA_ENTRY           *Entry,
  IN OUT VOID                                        *Buffer,
  IN OUT UINTN                                       *BufferSize
  )
{
  UINT32  Index;

  if ((This == NULL) || (This->Data == NULL) || (This->Entry == NULL) ||
      (BufferSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < This->EntryNum; Index++) {
    if (Id == This->Entry[Index].Id) {
      if ((This->Entry[Index].Offset > sizeof (*This->Data)) ||
          (This->Entry[Index].Size > sizeof (*This->Data) - This->Entry[Index].Offset)) {
        return EFI_COMPROMISED_DATA;
      }

      if (*BufferSize < This->Entry[Index].Size) {
        *BufferSize = This->Entry[Index].Size;
        return EFI_BUFFER_TOO_SMALL;
      }

      if ((Buffer == NULL) || (Entry == NULL)) {
        return EFI_INVALID_PARAMETER;
      }

      CopyMem ((VOID *)Buffer, (VOID *)(((UINT8 *)(This->Data)) + This->Entry[Index].Offset), This->Entry[Index].Size);
      CopyMem ((VOID *)Entry, (VOID *)(&This->Entry[Index]), sizeof (PLATFORM_CONFIG_PARAMS_DATA_ENTRY));
      *BufferSize = This->Entry[Index].Size;

      break;
    }
  }

  if (Index == This->EntryNum) {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PlatformConfigDataModifyEntry (
  IN     CIX_PLATFORM_CONFIG_PARAMS_MANAGE_PROTOCOL  *This,
  IN     PLATFORM_CONFIG_PARAMS_DATA_ENTRY           *Entry,
  IN     VOID                                        *Buffer,
  IN     UINTN                                       BufferSize
  )
{
  EFI_STATUS                           Status = EFI_SUCCESS;
  UINT64                               Value;
  UINT32                               Num, Index;
  PLATFORM_CONFIG_PARAMS_DATA_OPTIONS  Options[MAX_PARAMS_OPTION_NUM];

  if ((This == NULL) || (This->Data == NULL) || (This->Entry == NULL) ||
      (Buffer == NULL) || (Entry == NULL) || (BufferSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  // The caller may hold a copy; use the provider's canonical constraints.
  for (Index = 0; Index < This->EntryNum; Index++) {
    if ((Entry->Id == This->Entry[Index].Id) &&
        (Entry->Offset == This->Entry[Index].Offset) &&
        (Entry->Size == This->Entry[Index].Size) &&
        (Entry->Type == This->Entry[Index].Type)) {
      break;
    }
  }

  if (Index == This->EntryNum) {
    return EFI_INVALID_PARAMETER;
  }

  Entry = &This->Entry[Index];
  if ((Entry->Offset > sizeof (*This->Data)) ||
      (Entry->Size > sizeof (*This->Data) - Entry->Offset) ||
      (BufferSize > Entry->Size)) {
    return EFI_INVALID_PARAMETER;
  }

  switch (Entry->Type) {
    case PARAMS_DATA_BOOLEAN_TYPE:
      if (BufferSize != sizeof (BOOLEAN)) {
        return EFI_INVALID_PARAMETER;
      }

      Status = ParsePlatformConfigDataOption (Entry->Option, &Num, Options);
      if (!EFI_ERROR (Status)) {
        if (Num != 2) {
          Status = EFI_DEVICE_ERROR;
        } else {
          for (Index = 0; Index < Num; Index++) {
            if (Options[Index].Value == *(BOOLEAN *)Buffer) {
              CopyMem ((VOID *)(((UINT8 *)(This->Data)) + Entry->Offset), (VOID *)Buffer, BufferSize);
              break;
            }
          }

          if (Index == Num) {
            Status = EFI_NOT_FOUND;
          }
        }
      }

      break;
    case PARAMS_DATA_MULTI_OPTION_TYPE:
      if ((BufferSize != sizeof (UINT8)) && (BufferSize != sizeof (UINT16)) &&
          (BufferSize != sizeof (UINT32)) && (BufferSize != sizeof (UINT64))) {
        return EFI_INVALID_PARAMETER;
      }

      Status = ParsePlatformConfigDataOption (Entry->Option, &Num, Options);
      if (!EFI_ERROR (Status)) {
        if (BufferSize == sizeof (UINT8)) {
          Value = *(UINT8 *)Buffer;
        } else if (BufferSize == sizeof (UINT16)) {
          Value = ReadUnaligned16 ((CONST UINT16 *)Buffer);
        } else if (BufferSize == sizeof (UINT32)) {
          Value = ReadUnaligned32 ((CONST UINT32 *)Buffer);
        } else if (BufferSize == sizeof (UINT64)) {
          Value = ReadUnaligned64 ((CONST UINT64 *)Buffer);
        }

        if (Num > MAX_PARAMS_OPTION_NUM) {
          return EFI_COMPROMISED_DATA;
        }

        for (Index = 0; Index < Num; Index++) {
          if (Options[Index].Value == Value) {
            CopyMem ((VOID *)(((UINT8 *)(This->Data)) + Entry->Offset), (VOID *)Buffer, BufferSize);
            break;
          }
        }

        if (Index == Num) {
          Status =  EFI_NOT_FOUND;
        }
      }

      break;
    case PARAMS_DATA_INTEGER_TYPE:
    case PARAMS_DATA_STRING_TYPE:
      CopyMem ((VOID *)(((UINT8 *)(This->Data)) + Entry->Offset), (VOID *)Buffer, BufferSize);
      break;
    default:
      Status = EFI_INVALID_PARAMETER;
      break;
  }

  return Status;
}

/**
  Entrypoint of Platform Config Params Manage Dxe Driver

  @param  ImageHandle[in]      The firmware allocated handle for the EFI image.
  @param  SystemTable[in]      A pointer to the EFI System Table.

  @retval EFI_SUCCESS          The config params manage protocol has been registered.
  @retval Others               Unexpected error happened.

**/
EFI_STATUS
EFIAPI
PlatformConfigParamsManageDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                                  Status           = EFI_SUCCESS;
  PLATFORM_CONFIG_PARAMS_DATA_BLOCK           *ConfigData      = NULL;
  PLATFORM_CONFIG_PARAMS_DATA_ENTRY           *ConfigDataEntry = NULL;
  CIX_PLATFORM_CONFIG_PARAMS_MANAGE_PROTOCOL  *ConfigManage    = NULL;

  POST_CODE (PlatformConfigParamsManageDxeStart);

  ConfigData      = AllocateCopyPool (sizeof (PLATFORM_CONFIG_PARAMS_DATA_BLOCK), &mPlatformConfigParamsDataBlock);
  ConfigDataEntry = AllocateCopyPool (sizeof (PLATFORM_CONFIG_PARAMS_DATA_ENTRY) * mPlatformConfigDataEntryNum, &mPlatformConfigDataEntryTable);

  ConfigManage = AllocateZeroPool (sizeof (CIX_PLATFORM_CONFIG_PARAMS_MANAGE_PROTOCOL));
  if ((ConfigData == NULL) || (ConfigDataEntry == NULL) || (ConfigManage == NULL)) {
    if (ConfigData != NULL) {
      FreePool (ConfigData);
    }
    if (ConfigDataEntry != NULL) {
      FreePool (ConfigDataEntry);
    }
    if (ConfigManage != NULL) {
      FreePool (ConfigManage);
    }
    return EFI_OUT_OF_RESOURCES;
  }

  // platform hook routines for configuration parameters initialization
  PlatformConfigParamsHook (ConfigData);

  ConfigManage->Version     = CIX_PLATFORM_CONFIG_PARAMS_MANAGE_PROTOCOL_VERSION;
  ConfigManage->Data        = ConfigData;
  ConfigManage->Entry       = ConfigDataEntry;
  ConfigManage->EntryNum    = mPlatformConfigDataEntryNum;
  ConfigManage->GetEntry    = PlatformConfigDataGetEntry;
  ConfigManage->FindEntry   = PlatformConfigDataFindEntry;
  ConfigManage->ModifyEntry = PlatformConfigDataModifyEntry;

  Status = gBS->InstallProtocolInterface (
         &ImageHandle,
         &gCixPlatformConfigParamsManageProtocolGuid,
         EFI_NATIVE_INTERFACE,
         ConfigManage
         );
  if (EFI_ERROR (Status)) {
    FreePool (ConfigManage);
    FreePool (ConfigDataEntry);
    FreePool (ConfigData);
  }

  POST_CODE (PlatformConfigParamsManageDxeEnd);

  return Status;
}
