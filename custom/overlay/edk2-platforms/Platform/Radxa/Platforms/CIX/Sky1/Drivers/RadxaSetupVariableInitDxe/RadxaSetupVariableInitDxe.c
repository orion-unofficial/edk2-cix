/** @file

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved

**/
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
#include "../../Include/RadxaSetupVar.h"

STATIC
VOID
ApplyBoardDeviceModelDefaults (
  OUT RADXA_SETUP_DATA  *RadxaSetupVar
  )
{
  CHAR16  *SystemProductName;

  RadxaSetupVar->PcieDeviceModel = RADXA_SETUP_PCIE_DEVICE_MODEL_LINUX;
  RadxaSetupVar->UsbDeviceModel  = RADXA_SETUP_USB_DEVICE_MODEL_LINUX;

  RadxaSetupVar->UsbGenericXhciVisible[0] = 1;
  RadxaSetupVar->UsbGenericXhciVisible[1] = 0;
  RadxaSetupVar->UsbGenericXhciVisible[2] = 1;
  RadxaSetupVar->UsbGenericXhciVisible[3] = 0;
  RadxaSetupVar->ThermalPowerModel = RADXA_SETUP_THERMAL_POWER_MODEL_VENDOR_ACPI;

  SystemProductName = (CHAR16 *)FixedPcdGetPtr (PcdSystemProductName);
  if (!StrCmp (L"Radxa Orion O6N", SystemProductName)) {
    RadxaSetupVar->UsbGenericXhciVisible[2] = 0;
  }
}

EFI_STATUS
EFIAPI
RadxaSetupVariableInitDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS        Status = EFI_SUCCESS;
  BOOLEAN           NeedsRewrite;
  UINTN             VarSize;
  RADXA_SETUP_DATA  RadxaSetupVar;

  NeedsRewrite = FALSE;
  VarSize = sizeof (RADXA_SETUP_DATA);
  ZeroMem (&RadxaSetupVar, sizeof (RadxaSetupVar));
  ApplyBoardDeviceModelDefaults (&RadxaSetupVar);

  Status = gRT->GetVariable (
                  RADXA_SETUP_VAR,
                  &gRadxaSetupVariableGuid,
                  NULL,
                  &VarSize,
                  &RadxaSetupVar
                  );
  if (EFI_ERROR (Status)) {
    //
    // Variable does not exist yet - create it
    //
    Status = gRT->SetVariable (
                    RADXA_SETUP_VAR,
                    &gRadxaSetupVariableGuid,
                    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                    sizeof (RADXA_SETUP_DATA),
                    &RadxaSetupVar
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: EfiSetVariable failed - %r\n", __FUNCTION__, Status));
      return Status;
    }
  } else {
    if (VarSize < sizeof (RADXA_SETUP_DATA)) {
      NeedsRewrite = TRUE;
    }

    if (NeedsRewrite) {
      Status = gRT->SetVariable (
                      RADXA_SETUP_VAR,
                      &gRadxaSetupVariableGuid,
                      EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                      sizeof (RADXA_SETUP_DATA),
                      &RadxaSetupVar
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: EfiSetVariable update failed - %r\n", __FUNCTION__, Status));
        return Status;
      }
    }

    DEBUG ((DEBUG_INFO, "%a: EfiGetVariable Success - %r\n", __FUNCTION__, Status));
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gRadxaSetupVariableGuid,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InstallMultipleProtocolInterfaces failed - %r\n", __FUNCTION__, Status));
    return Status;
  }

  return Status;
}
