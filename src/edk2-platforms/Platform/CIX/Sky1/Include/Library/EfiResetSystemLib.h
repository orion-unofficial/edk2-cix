/** @file
  Legacy reset-system library interface used by existing CIX platform drivers.

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __EFI_RESET_SYSTEM_LIB_H__
#define __EFI_RESET_SYSTEM_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiMultiPhase.h>

EFI_STATUS
EFIAPI
LibResetSystem (
  IN EFI_RESET_TYPE  ResetType,
  IN EFI_STATUS      ResetStatus,
  IN UINTN           DataSize,
  IN CHAR16          *ResetData OPTIONAL
  );

EFI_STATUS
EFIAPI
LibInitializeResetSystem (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

#endif
