/*
 * Custom path: make the O6N UART3 pinmux track UART3_ENABLE without editing
 * the imported upstream source in place.
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

#define InitPinmux ImportedInitPinmux
#include "../../../../../../../../../src/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLib.c"
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
  // O6N pinmux table to select the UART function when UART3_ENABLE is enabled.
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

EFI_STATUS
EFIAPI
InitPinmux (
  IN OUT ENV_HOOK_PARAMS_DATA_BLOCK  *ConfigData
  )
{
  ApplyDebugUart3PinMuxOverride ();

  return ImportedInitPinmux (ConfigData);
}
