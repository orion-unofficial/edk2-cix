/*
 * Custom path: keep the existing UART3 pinmux override behavior for O6 while
 * also applying the experimental SR-IOV setup variable before PCI
 * enumeration begins and the persisted firmware text-console mode before the
 * console drivers initialize.
 */

#if UART3_ENABLE
#ifdef DEBUG_MODE
#undef DEBUG_MODE
#endif
#else
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
#endif

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
#include "../../../../../../../../../src/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/PlatformEnvHookLib.c"
#undef PlatformEnvHook
#undef InitPinmux

STATIC
VOID
ApplyDebugUart3PinMuxOverride (
  VOID
  )
{
#if UART3_ENABLE
  UINTN  Index;

  //
  // The public pad definitions describe UART3_TXD/RXD as raw function 0, with
  // function 1 mapping the same pads to GPIO105/GPIO106. Force the imported
  // O6 pinmux table to select the UART function when UART3_ENABLE is enabled.
  //
  for (Index = 0; Index < ARRAY_SIZE (PinMuxCfgTable); Index++) {
    if ((PinMuxCfgTable[Index].Offset == IO_S0_UART3_TXD) ||
        (PinMuxCfgTable[Index].Offset == IO_S0_UART3_RXD))
    {
      PinMuxCfgTable[Index].FuncSel = IO_FUNC00;
    }
  }
#endif
}

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
  ApplyDebugUart3PinMuxOverride ();
  ApplyExperimentalConsoleModeOverride ();
  ApplyExperimentalSrIovSetupOverride ();

  ImportedPlatformEnvHook (ConfigData);
}
