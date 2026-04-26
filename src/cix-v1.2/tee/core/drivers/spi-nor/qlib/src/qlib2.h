/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib2.h
* @brief      This file contains QLIB definitions for W77Q 16Mb to 128Mb flash QLIB interface backward compatibility 
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB2_H__
#define __QLIB2_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef Q2_API

#if (QLIB_NUM_OF_DIES != 1)
#error "QLIB_NUM_OF_DIES must be 1 when Q2_API defined"
#endif
/************************************************************************************************************
 * This definition is used to support old W77Q (16Mb to 128Mb) API for function
 * QLIB_STATUS_T QLIB_AuthPlainAccess_Grant(QLIB_CONTEXT_T* qlibContext, U32 sectionID)
************************************************************************************************************/
#define QLIB_AuthPlainAccess_Grant QLIB_PlainAccessGrant

/************************************************************************************************************
 * This macro is used to support old W77Q (16Mb to 128Mb) API for function
 * QLIB_STATUS_T QLIB_AuthPlainAccess_Revoke(QLIB_CONTEXT_T* qlibContext, U32 sectionID)
************************************************************************************************************/
#define QLIB_AuthPlainAccess_Revoke(qlibContext, sectionID) \
    QLIB_PlainAccessRevoke(qlibContext, sectionID, QLIB_PA_REVOKE_ALL_ACCESS)

/************************************************************************************************************
 * This definition is used to support old W77Q (16Mb to 128Mb) API for function
 * QLIB_STATUS_T QLIB_PlainAccessEnable(QLIB_CONTEXT_T* qlibContext, U32 sectionID)
************************************************************************************************************/
#define QLIB_PlainAccessEnable QLIB_PlainAccessGrant

/************************************************************************************************************
 * This macro is used to support old W77Q (16Mb to 128Mb) API for function
 * QLIB_STATUS_T QLIB_Format(QLIB_CONTEXT_T* qlibContext, const KEY_T deviceMasterKey, BOOL eraseDataOnly)
************************************************************************************************************/
#define QLIB_Format(qlibContext, deviceMasterKey, eraseDataOnly) QLIB_Format(qlibContext, deviceMasterKey, eraseDataOnly, FALSE)

/************************************************************************************************************
 * This macro is used to support old W77Q (16Mb to 128Mb) API for function
 * QLIB_STATUS_T QLIB_ConfigDevice(QLIB_CONTEXT_T*                   qlibContext,
 *                                 const KEY_T                       deviceMasterKey,
 *                                 const KEY_T                       deviceSecretKey,
 *                                 const QLIB_SECTION_CONFIG_TABLE_T sectionTable,
 *                                 const KEY_ARRAY_T                 restrictedKeys,
 *                                 const KEY_ARRAY_T                 fullAccessKeys,
 *                                 const QLIB_WATCHDOG_CONF_T*       watchdogDefault,
 *                                 const QLIB_DEVICE_CONF_T*         deviceConf,
 *                                 const _128BIT                     suid)
************************************************************************************************************/
#define QLIB_ConfigDevice(qlibContext,                                  \
                          deviceMasterKey,                              \
                          deviceSecretKey,                              \
                          sectionTable,                                 \
                          restrictedKeys,                               \
                          fullAccessKeys,                               \
                          watchdogDefault,                              \
                          deviceConf,                                   \
                          suid)                                         \
    QLIB_ConfigDevice(qlibContext,                                      \
                      deviceMasterKey,                                  \
                      deviceSecretKey,                                  \
                      (const QLIB_SECTION_CONFIG_TABLE_T*)sectionTable, \
                      (const KEY_ARRAY_T*)restrictedKeys,               \
                      (const KEY_ARRAY_T*)fullAccessKeys,               \
                      NULL,                                             \
                      NULL,                                             \
                      (const QLIB_WATCHDOG_CONF_T*)watchdogDefault,     \
                      (const QLIB_DEVICE_CONF_T*)deviceConf,            \
                      suid)

/************************************************************************************************************
 * swap type as defined in QLIB old API for W77Q 16Mb to 128Mb flash
************************************************************************************************************/
typedef enum
{
    QLIB_SWAP_NO        = FALSE,           ///< do not perform swap after applying configuration
    QLIB_SWAP           = TRUE,            ///< perform swap after applying configuration
    QLIB_SWAP_AND_RESET = FALSE + TRUE + 1 ///< perform swap and then perform CPU reset
} QLIB_SWAP_T;

/************************************************************************************************************
 * This macro is used to support old W77Q (16Mb to 128Mb) API for function
 * QLIB_STATUS_T QLIB_ConfigSection(QLIB_CONTEXT_T*      qlibContext,
 *                                  U32                  sectionID,
 *                                  const QLIB_POLICY_T* policy,
 *                                  const U64*           digest,
 *                                  const U32*           crc,
 *                                  const U32*           newVersion,
 *                                  QLIB_SWAP_T          swap);
************************************************************************************************************/
#define QLIB_ConfigSection(qlibContext, sectionID, policy, digest, crc, newVersion, swap) \
    QLIB_ConfigSection(qlibContext,                                                       \
                       sectionID,                                                         \
                       policy,                                                            \
                       digest,                                                            \
                       crc,                                                               \
                       newVersion,                                                        \
                       swap == QLIB_SWAP_NO ? FALSE : TRUE,                               \
                       swap == QLIB_SWAP_AND_RESET ? QLIB_SECTION_CONF_ACTION__RESET : QLIB_SECTION_CONF_ACTION__RELOAD)

/************************************************************************************************************
 * This macro is used to support old W77Q 16Mb to 128Mb flash QLIB API for function
 * QLIB_STATUS_T QLIB_Watchdog_Get(QLIB_CONTEXT_T* qlibContext, U32* secondsPassed, U32* ticksResidue, BOOL* expired)
************************************************************************************************************/
#define QLIB_Watchdog_Get(qlibContext, secondsPassed, ticksResidue, expired) \
    QLIB2_Watchdog_Get(qlibContext, secondsPassed, ticksResidue, expired)

/************************************************************************************************************
 * @brief       W77Q (16Mb to 128Mb) function for status status of the Secure watchdog timer.
 *
 * @p secondsPassed, @p ticksResidue and @p expired parameters can be NULL and thus the function does not return them.
 *
 * @param[out]  qlibContext   [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[out]  secondsPassed The current value of the AWD timer (in seconds). If NULL, time is not returned.
 * @param[out]  ticksResidue  The residue of the AWD timer (in units of 64 tics of LF_OSC). If NULL, time is not returned.
 * @param[out]  expired       TRUE if AWD timer has expired. If NULL, expired indication is not returned.
 *
 * @return
 * QLIB_STATUS__OK = 0                  - no error occurred\n
 * QLIB_STATUS__INVALID_PARAMETER       - @p qlibContext is NULL\n
 * QLIB_STATUS__NOT_CONNECTED           - Need to perform connect using @ref QLIB_Connect function\n
 * QLIB_STATUS__(ERROR)                 - Other error
************************************************************************************************************/
QLIB_STATUS_T QLIB2_Watchdog_Get(QLIB_CONTEXT_T* qlibContext, U32* secondsPassed, U32* ticksResidue, BOOL* expired);

/************************************************************************************************************
 * Definition for deprecated compilation option QLIB_INIT_AFTER_Q2_POWER_UP
************************************************************************************************************/
#ifdef QLIB_INIT_AFTER_FLASH_POWER_UP
#define QLIB_INIT_AFTER_Q2_POWER_UP
#endif

#endif // Q2_API

#ifdef __cplusplus
}
#endif

#endif // __QLIB2_H__
