/*
 * Custom path: make the O6N UART3 pinmux track DEBUG_ON_UART3 without editing
 * the imported upstream source in place.
 */

#if DEBUG_ON_UART3
#ifdef DEBUG_MODE
#undef DEBUG_MODE
#endif
#else
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
#endif

#include "../../../../../../../../../src/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLib.c"
