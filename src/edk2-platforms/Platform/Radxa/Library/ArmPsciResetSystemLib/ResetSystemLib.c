/** @file
  Radxa reset-system library using PSCI calls while preserving vendor reset flows.

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>

#include <IndustryStandard/ArmStdSmc.h>

#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/EcLib.h>
#include <Library/GpioLib.h>
#include <Library/PcdLib.h>
#include <Library/ResetSystemLib.h>
#include <PlatformInfoTypes.h>

STATIC
VOID
IssuePsciReset (
  IN UINTN  SmcId
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;

  ArmSmcArgs.Arg0 = SmcId;
  ArmCallSmc (&ArmSmcArgs);

  DEBUG ((DEBUG_ERROR, "%a: PSCI reset call 0x%lx failed\n", __FUNCTION__, SmcId));
  CpuDeadLoop ();
}

VOID
EFIAPI
ResetCold (
  VOID
  )
{
  IssuePsciReset (ARM_SMC_ID_PSCI_SYSTEM_RESET);
}

VOID
EFIAPI
ResetWarm (
  VOID
  )
{
  ResetCold ();
}

VOID
EFIAPI
ResetShutdown (
  VOID
  )
{
  UINT8  BoardId;

  BoardId = FixedPcdGet8 (PcdBoardId);

  GpioConfig (FixedPcdGet8 (PcdPcieRootPort0PeResetPin), OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
  GpioConfig (FixedPcdGet8 (PcdPcieRootPort1PeResetPin), OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
  GpioConfig (FixedPcdGet8 (PcdPcieRootPort2PeResetPin), OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
  GpioConfig (FixedPcdGet8 (PcdPcieRootPort3PeResetPin), OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
  GpioConfig (FixedPcdGet8 (PcdPcieRootPort4PeResetPin), OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);

  DEBUG ((DEBUG_INFO, "%a: disable additional GPIOs for board ID %u\n", __FUNCTION__, (UINT32)BoardId));
  if (BoardId == TypeRadxaOrionO6) {
    GpioConfig (10, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (12, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (13, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (17, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (21, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (22, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (40, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (41, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (42, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (81, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
  } else if (BoardId == TypeRadxaOrionO6N) {
    GpioConfig (1, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (7, OUTPUT, INOUT_HIGH, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (13, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (14, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (23, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (40, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (41, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (42, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (73, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
    GpioConfig (81, OUTPUT, INOUT_LOW, INTERRUPT_DISABLE, INTERRUPT_TYPE_DEFAULT);
  }

  IssuePsciReset (ARM_SMC_ID_PSCI_SYSTEM_OFF);
}

VOID
EFIAPI
ResetPlatformSpecific (
  IN UINTN  DataSize,
  IN VOID   *ResetData
  )
{
  EC_PARAMS_FORCE_EC_RESET  Params;
  UINT8                    BoardId;

  (VOID)DataSize;
  (VOID)ResetData;
  BoardId = FixedPcdGet8 (PcdBoardId);
  if (BoardId == TypeRadxaOrionO6) {
    Params.Reserved = 0;
    ForceEcReset (&Params);

    DEBUG ((DEBUG_INFO, "%a: force EC reset\n", __FUNCTION__));
    CpuDeadLoop ();
  }

  ResetCold ();
}

VOID
EFIAPI
ResetSystem (
  IN EFI_RESET_TYPE  ResetType,
  IN EFI_STATUS      ResetStatus,
  IN UINTN           DataSize,
  IN VOID            *ResetData OPTIONAL
  )
{
  (VOID)ResetStatus;
  switch (ResetType) {
    case EfiResetWarm:
      ResetWarm ();
      break;
    case EfiResetCold:
      ResetCold ();
      break;
    case EfiResetShutdown:
      ResetShutdown ();
      break;
    case EfiResetPlatformSpecific:
      ResetPlatformSpecific (DataSize, ResetData);
      break;
    default:
      ASSERT (FALSE);
      return;
  }
}
