/** @file
  Custom RELEASE logging without enabling assertion or diagnostic-only code.

  This header extension is selected only for custom RELEASE builds
  with DEBUG_VERBOSE=true. Keep the version-matched upstream header and all
  RELEASE preprocessor gates; override only printing and disable debug helpers
  at compile time. Do not expose this directory to imported upstream builds.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef CIX_RELEASE_LOGGING_DEBUG_LIB_H_
#define CIX_RELEASE_LOGGING_DEBUG_LIB_H_

#if !defined (MDEPKG_NDEBUG) || !defined (NDEBUG)
#error Custom RELEASE logging requires MDEPKG_NDEBUG and NDEBUG
#endif

// The upstream worker retains its fixed-level mask and argument evaluation rules.
#undef DEBUG
#define DEBUG(Expression)                 \
  do {                                    \
    if (DebugPrintEnabled ()) {           \
      _DEBUG_PRINT Expression;            \
    }                                     \
  } while (FALSE)

// Eliminate these blocks even without whole-program optimisation.
#undef DEBUG_CODE_BEGIN
#define DEBUG_CODE_BEGIN()  do { if (FALSE) { do { } while (FALSE)
#undef DEBUG_CODE_END
#define DEBUG_CODE_END()    } } while (FALSE)
#undef DEBUG_CLEAR_MEMORY
#define DEBUG_CLEAR_MEMORY(Address, Length)  do { } while (FALSE)

#endif
