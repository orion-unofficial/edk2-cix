/** @file

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/AcpiLib.h>
#include <Library/AslUpdateLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/PlatformConfigParamsManageProtocol.h>
#include "../../Platforms/CIX/Sky1/Include/RadxaSetupVar.h"

EFI_GUID  pAcpiPlatformTableStorageGuid = {
  0xc1bb2ead, 0xc76a, 0x45dd, { 0x90, 0xb5, 0xd4, 0x02, 0x55, 0x17, 0x0a, 0x9c }
};

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

VOID
EFIAPI
AcpiHookFunctionOnReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS          Status = EFI_SUCCESS;
  CIX_PLATFORM_CONFIG_PARAMS_MANAGE_PROTOCOL  *PlatformConfigManage;
  CHAR8               *SsdtTableId = "ORIONO6";
  CHAR16              *SystemProductName;
  RADXA_SETUP_DATA    RadxaSetupVar;
  UINTN               VarSize;

  DEBUG ((DEBUG_INFO, "Enter %a\n", __func__));

  VarSize = sizeof (RADXA_SETUP_DATA);
  if (FixedPcdGetBool (PcdCustomFirmwareFixesEnable)) {
    ZeroMem (&RadxaSetupVar, sizeof (RadxaSetupVar));
    ApplyBoardDeviceModelDefaults (&RadxaSetupVar);
  }
  Status = gRT->GetVariable (
                  RADXA_SETUP_VAR,
                  &gRadxaSetupVariableGuid,
                  NULL,
                  &VarSize,
                  &RadxaSetupVar
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: EfiGetVariable failed for gRadxaSetupVariableGuid - %r\n", __FUNCTION__, Status));
  }

  Status = gBS->LocateProtocol (&gCixPlatformConfigParamsManageProtocolGuid, NULL, (VOID **)&PlatformConfigManage);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: LocateProtocol failed: %r\n", __FUNCTION__, Status));
  }

  SystemProductName = (CHAR16 *)FixedPcdGetPtr (PcdSystemProductName);

  if (!StrCmp (L"Radxa Orion O6", SystemProductName)) {
    Status = UpdateSsdtNameAslCode ((UINT8 *)SsdtTableId, AsciiStrLen (SsdtTableId), SIGNATURE_32 ('E', 'C', 'F', 'M'), &(PlatformConfigManage->Data->EcFanMode), sizeof (PlatformConfigManage->Data->EcFanMode));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Update ECFM failed, Status=%r\n", __FUNCTION__, Status));
    }
  }

  Status = UpdateNameAslCode (SIGNATURE_32 ('S', 'C', 'M', 'S'), &(RadxaSetupVar.EnableAcpiScmi), sizeof (RadxaSetupVar.EnableAcpiScmi));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Update SCMS failed, Status=%r\n", __FUNCTION__, Status));
  }

  if (FixedPcdGetBool (PcdCustomFirmwareFixesEnable)) {
    Status = UpdateNameAslCode (SIGNATURE_32 ('P', 'C', 'D', 'M'), &(RadxaSetupVar.PcieDeviceModel), sizeof (RadxaSetupVar.PcieDeviceModel));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Update PCDM failed, Status=%r\n", __FUNCTION__, Status));
    }

    Status = UpdateNameAslCode (SIGNATURE_32 ('U', 'S', 'D', 'M'), &(RadxaSetupVar.UsbDeviceModel), sizeof (RadxaSetupVar.UsbDeviceModel));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Update USDM failed, Status=%r\n", __FUNCTION__, Status));
    }

    Status = UpdateNameAslCode (SIGNATURE_32 ('U', 'G', 'V', '0'), &(RadxaSetupVar.UsbGenericXhciVisible[0]), sizeof (RadxaSetupVar.UsbGenericXhciVisible[0]));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Update UGV0 failed, Status=%r\n", __FUNCTION__, Status));
    }

    Status = UpdateNameAslCode (SIGNATURE_32 ('U', 'G', 'V', '1'), &(RadxaSetupVar.UsbGenericXhciVisible[1]), sizeof (RadxaSetupVar.UsbGenericXhciVisible[1]));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Update UGV1 failed, Status=%r\n", __FUNCTION__, Status));
    }

    Status = UpdateNameAslCode (SIGNATURE_32 ('U', 'G', 'V', '2'), &(RadxaSetupVar.UsbGenericXhciVisible[2]), sizeof (RadxaSetupVar.UsbGenericXhciVisible[2]));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Update UGV2 failed, Status=%r\n", __FUNCTION__, Status));
    }

    Status = UpdateNameAslCode (SIGNATURE_32 ('U', 'G', 'V', '3'), &(RadxaSetupVar.UsbGenericXhciVisible[3]), sizeof (RadxaSetupVar.UsbGenericXhciVisible[3]));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Update UGV3 failed, Status=%r\n", __FUNCTION__, Status));
    }

    Status = UpdateNameAslCode (SIGNATURE_32 ('T', 'P', 'D', 'M'), &(RadxaSetupVar.ThermalPowerModel), sizeof (RadxaSetupVar.ThermalPowerModel));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Update TPDM failed, Status=%r\n", __FUNCTION__, Status));
    }
  }

  //
  // Close the event, so it will not be signalled again.
  //
  if (Event != NULL) {
    gBS->CloseEvent (Event);
  }

  DEBUG ((DEBUG_INFO, "Exit %a\n", __func__));
}

EFI_STATUS
EFIAPI
AcpiPlatformDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   ReadyToBootEvent;

  DEBUG ((DEBUG_INFO, "Acpi Platform Dxe Entry\n"));

  Status = LocateAndInstallAcpiFromFv (&pAcpiPlatformTableStorageGuid);
  DEBUG ((DEBUG_INFO, "Install Acpi table status: %r\n", Status));

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  AcpiHookFunctionOnReadyToBoot,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &ReadyToBootEvent
                  );
  ASSERT_EFI_ERROR (Status);

  DEBUG ((DEBUG_INFO, "HACK: disable PcdTestKeyUsed FrontPage warning\n"));
  PcdSetBoolS (PcdTestKeyUsed, FALSE);

  DEBUG ((DEBUG_INFO, "Acpi Platform Dxe Exit\n"));

  return EFI_SUCCESS;
}
