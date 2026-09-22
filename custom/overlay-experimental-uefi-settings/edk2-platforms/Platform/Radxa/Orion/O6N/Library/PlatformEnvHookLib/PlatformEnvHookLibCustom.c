/*
 * Experimental UEFI settings path: keep the custom-path UART3 pinmux override
 * behavior for O6N while also applying the persisted firmware text-console
 * mode before the console drivers initialize.
 */

#include <Library/PcdLib.h>
#include "../../../../Platforms/CIX/Sky1/Include/ExperimentalConsoleModeSetupVar.h"

#define InitPinmux ImportedInitPinmux
#define PlatformEnvHook ImportedPlatformEnvHook
#include "../../../../../../../../../src/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLib.c"
#undef PlatformEnvHook
#undef InitPinmux

typedef struct {
  UINT32    Columns;
  UINT32    Rows;
} EXPERIMENTAL_TEXT_MODE_INFO;

STATIC CONST EXPERIMENTAL_TEXT_MODE_INFO  mExperimentalTextModes[] = {
  { 80, 25 },
  { 80, 50 },
  { 100, 31 }
};

STATIC
VOID
ApplyDebugUart3PinMuxOverride (
  VOID
  )
{
  STATIC PINMUX_CFG  DebugUart3PinMuxCfgTable[] = {
    { S0_DOMAIN, IO_S0_UART3_TXD, IO_FUNC00, PU_DEFAULT, PD_DEFAULT, ST_DEFAULT, DRV_STREN_DEFAULT }, // UART3_TXD
    { S0_DOMAIN, IO_S0_UART3_RXD, IO_FUNC00, PU_DEFAULT, PD_DEFAULT, ST_DEFAULT, DRV_STREN_DEFAULT }  // UART3_RXD
  };

  if (!FixedPcdGetBool (PcdAcpiUart3Enable)) {
    return;
  }

  //
  // The public pad definitions describe UART3_TXD/RXD as raw function 0, with
  // function 1 mapping the same pads to GPIO105/GPIO106. Re-apply those pads as
  // UART after the imported board pinmux table runs.
  //
  PinMuxInit (DebugUart3PinMuxCfgTable, ARRAY_SIZE (DebugUart3PinMuxCfgTable));
}

EFI_STATUS
EFIAPI
InitPinmux (
  IN OUT ENV_HOOK_PARAMS_DATA_BLOCK  *ConfigData
  )
{
  EFI_STATUS  Status;

  Status = ImportedInitPinmux (ConfigData);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // The imported O6N pinmux table uses the GPIO mapping for these pads. Apply
  // the UART-specific override afterward when the custom path requests it.
  //
  ApplyDebugUart3PinMuxOverride ();
  return EFI_SUCCESS;
}

STATIC
VOID
ApplyExperimentalConsoleModeOverride (
  VOID
  )
{
  EFI_GUID                              ConsoleModeGuid = EXPERIMENTAL_CONSOLE_MODE_SETUP_VARIABLE_GUID;
  EFI_STATUS                            Status;
  EXPERIMENTAL_CONSOLE_MODE_SETUP_DATA  ConsoleModeSetup;
  UINT8                                 ModeIndex;
  UINTN                                 VariableSize;

  VariableSize = sizeof (ConsoleModeSetup);
  Status = gRT->GetVariable (
                  EXPERIMENTAL_CONSOLE_MODE_SETUP_VAR_NAME,
                  &ConsoleModeGuid,
                  NULL,
                  &VariableSize,
                  &ConsoleModeSetup
                  );
  if (EFI_ERROR (Status) || (VariableSize != sizeof (ConsoleModeSetup))) {
    return;
  }

  ModeIndex = ConsoleModeSetup.SetupTextMode;
  if (ModeIndex >= ARRAY_SIZE (mExperimentalTextModes)) {
    ModeIndex = EXPERIMENTAL_CONSOLE_TEXT_MODE_80X25;
  }

  Status = PcdSet32S (PcdConOutColumn, mExperimentalTextModes[ModeIndex].Columns);
  if (!EFI_ERROR (Status)) {
    Status = PcdSet32S (PcdConOutRow, mExperimentalTextModes[ModeIndex].Rows);
  }
  if (!EFI_ERROR (Status)) {
    Status = PcdSet32S (PcdSetupConOutColumn, mExperimentalTextModes[ModeIndex].Columns);
  }
  if (!EFI_ERROR (Status)) {
    Status = PcdSet32S (PcdSetupConOutRow, mExperimentalTextModes[ModeIndex].Rows);
  }
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to apply console mode override: %r\n", __FUNCTION__, Status));
  }
}

VOID
EFIAPI
PlatformEnvHook (
  IN OUT ENV_HOOK_PARAMS_DATA_BLOCK  *ConfigData
  )
{
  EFI_STATUS  Status;
  VOID        *Registration;

  if (ConfigData == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: platform ENV hook routine failed to get config data\n", __FUNCTION__));
    return;
  }

  ApplyExperimentalConsoleModeOverride ();

  Status = InitPinmux (ConfigData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InitPinmux failed: %r\n", __FUNCTION__, Status));
  }

  Status = InitGpio (ConfigData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InitGpio failed: %r\n", __FUNCTION__, Status));
  }

  Status = UpdatePcdDmaDeviceLimit (ConfigData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: UpdatePcdDmaDeviceLimit failed: %r\n", __FUNCTION__, Status));
  }

  Status = WakeupSourceInit (ConfigData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: WakeupSourceInit failed: %r\n", __FUNCTION__, Status));
  }

  Status = OnboardDevicePowerOff (ConfigData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: OnboardDevicePowerOff failed: %r\n", __FUNCTION__, Status));
  }

  Status = RtcWakupEnable (ConfigData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RtcWakupEnable failed: %r\n", __FUNCTION__, Status));
  }

  EfiCreateProtocolNotifyEvent (
    &gRadxaSetupVariableGuid,
    TPL_CALLBACK,
    SetUFSPower,
    ConfigData,
    &Registration
    );

  EfiCreateProtocolNotifyEvent (
    &gEfiI2cMasterProtocolGuid,
    TPL_CALLBACK,
    InstallRtcProtocol,
    ConfigData,
    &Registration
    );
}
