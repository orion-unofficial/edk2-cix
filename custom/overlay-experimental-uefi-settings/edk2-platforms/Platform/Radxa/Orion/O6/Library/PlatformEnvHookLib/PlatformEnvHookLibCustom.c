/*
 * Experimental UEFI settings path: extend the maintained custom O6 env-hook
 * overlay with the SR-IOV setup variable before PCI enumeration begins and
 * the persisted firmware text-console mode before the console drivers
 * initialize.
 */

#include <Library/PcdLib.h>
#include "../../../../Platforms/CIX/Sky1/Include/ExperimentalConsoleModeSetupVar.h"
#include "../../../../Platforms/CIX/Sky1/Include/ExperimentalPcieSrIovSetupVar.h"

typedef struct {
  UINT32    Columns;
  UINT32    Rows;
} EXPERIMENTAL_TEXT_MODE_INFO;

STATIC CONST EXPERIMENTAL_TEXT_MODE_INFO  mExperimentalTextModes[] = {
  { 80, 25 },
  { 80, 50 },
  { 100, 31 }
};

#define InitPinmux ImportedInitPinmux
#define PlatformEnvHook ImportedPlatformEnvHook
#include "../../../../../../../../../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/PlatformEnvHookLib.c"
#undef PlatformEnvHook
#undef InitPinmux

STATIC
VOID
ApplyExperimentalSrIovSetupOverride (
  VOID
  )
{
  EFI_GUID                            ExperimentalSrIovGuid = EXPERIMENTAL_PCIE_SRIOV_SETUP_VARIABLE_GUID;
  EFI_STATUS                          Status;
  EXPERIMENTAL_PCIE_SRIOV_SETUP_DATA  ExperimentalSrIovSetup;
  BOOLEAN                             SrIovEnabled;
  UINTN                               VariableSize;

  VariableSize = sizeof (ExperimentalSrIovSetup);
  Status = gRT->GetVariable (
                  EXPERIMENTAL_PCIE_SRIOV_SETUP_VAR_NAME,
                  &ExperimentalSrIovGuid,
                  NULL,
                  &VariableSize,
                  &ExperimentalSrIovSetup
                  );
  if (EFI_ERROR (Status) || (VariableSize != sizeof (ExperimentalSrIovSetup))) {
    SrIovEnabled = PcdGetBool (PcdSrIovSupport);
  } else {
    SrIovEnabled = (ExperimentalSrIovSetup.SrIovSupport != 0);
  }

  Status = PcdSetBoolS (PcdSrIovSupport, SrIovEnabled);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to apply SR-IOV policy: %r\n", __FUNCTION__, Status));
  }
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
  ApplyExperimentalConsoleModeOverride ();
  ApplyExperimentalSrIovSetupOverride ();

  ImportedPlatformEnvHook (ConfigData);
}
