/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib.c
* @brief      This file contains QLIB main interface
*
* ### project qlib
*
************************************************************************************************************/
#define __QLIB_C__
// Prevent unused macro warning

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define NO_Q2_API_H
#include "qlib.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 DEFINITIONS                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_LMOTS_SHA256_N24_W4_PARAM_SET_ID 0x0007u
#define QLIB_LMS_SHA256_M24_H20_PARAM_SET_ID  0x000Du
#define QLIB_LMS_SHA256_M24_H10_PARAM_SET_ID  0x000Bu

#define QLIB_JEDEC_ID_CAPACITY_256Mb 0x19u

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 MACROS                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_DEVICE_INITIALIZED(qlibContext) ((qlibContext)->busInterface.busMode != QLIB_BUS_MODE_INVALID)

#ifdef Q2_API
#ifdef QLIB_INIT_AFTER_FLASH_POWER_UP
#define QLIB_INIT_AFTER_Q2_POWER_UP
#endif
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                  TYPES                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
typedef enum
{
    QLIB_LOAD_ACLR_ANY      = 0,          // load SSPR to ACLR
    QLIB_LOAD_ACLR_PLAIN_RD = (1u << 0u), // load SSPR to ACLR only if plain read is configured for section
    QLIB_LOAD_ACLR_PLAIN_WR = (1u << 1u), // load SSPR to ACLR only if plain write is configured for section
    QLIB_LOAD_ACLR_NON_AUTH = (1u << 2u), // load SSPR to ACLR only if non-authenticated plain access is configured for section
    QLIB_LOAD_ACLR_NON_AUTH_PLAIN_RD =
        ((U8)QLIB_LOAD_ACLR_NON_AUTH |
         (U8)QLIB_LOAD_ACLR_PLAIN_RD), // load SSPR to ACLR only if non-authenticated plain read is configured for section
    QLIB_LOAD_ACLR_NON_AUTH_PLAIN_WR =
        ((U8)QLIB_LOAD_ACLR_NON_AUTH |
         (U8)QLIB_LOAD_ACLR_PLAIN_WR), // load SSPR to ACLR only if non-authenticated plain write is configured for section
} QLIB_LOAD_ACLR_T;

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                        LOCAL FUNCTION PROTOTYPES                                        */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

static QLIB_STATUS_T QLIB_ConfigDeviceSingleDie_L(QLIB_CONTEXT_T*             qlibContext,
                                                  QLIB_FLASH_CONFIG_T*        flashCfg,
                                                  QLIB_DIE_CONFIG_T*          dieCfg,
                                                  QLIB_SECTIONS_CONF_TABLE_T* cfgSectionArray);
static QLIB_STATUS_T QLIB_GetDeviceConfigSingleDie_L(QLIB_CONTEXT_T*             qlibContext,
                                                     QLIB_FLASH_CONFIG_T*        flashCfg,
                                                     QLIB_DIE_CONFIG_T*          dieCfg,
                                                     QLIB_SECTIONS_CONF_TABLE_T* cfgSectionArray);
static QLIB_STATUS_T QLIB_GetDefaultDieConfig_L(QLIB_CONTEXT_T*             qlibContext,
                                                QLIB_FLASH_CONFIG_T*        flashCfg,
                                                QLIB_DIE_CONFIG_T*          dieCfg,
                                                QLIB_SECTIONS_CONF_TABLE_T* cfgSectionArray);
static QLIB_STATUS_T QLIB_waitReadyAndInitBusMode_L(QLIB_CONTEXT_T* qlibContext);
static QLIB_STATUS_T QLIB_PlainAccessGrant_L(QLIB_CONTEXT_T* qlibContext, U32 sectionID, QLIB_LOAD_ACLR_T condition);
static QLIB_STATUS_T QLIB_GetTargetFlash_L(QLIB_HW_VER_T* hwVer, U32* target);
static QLIB_STATUS_T QLIB_SetInterface_L(QLIB_CONTEXT_T*   qlibContext,
                                         QLIB_BUS_FORMAT_T busFormat,
                                         BOOL              configFlash) __RAM_SECTION;
#if !defined EXCLUDE_Q2_4_BYTES_ADDRESS_MODE || !defined EXCLUDE_FAST_READ_DUMMY_CONFIG
static QLIB_STATUS_T                                     QLIB_SyncSPI_L(QLIB_CONTEXT_T* qlibContext);
#endif

#ifdef Q2_API
#ifdef __cplusplus
extern "C" {
#endif
QLIB_STATUS_T QLIB2_Watchdog_Get(QLIB_CONTEXT_T* qlibContext, U32* secondsPassed, U32* ticksResidue, BOOL* expired);
#ifdef __cplusplus
}
#endif
#endif
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_InitLib(QLIB_CONTEXT_T* qlibContext)
{
    void* userData;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);

    /*-----------------------------------------------------------------------------------------------------*/
    /* We must keep user data untouched since could be set before calling QLIB_InitLib                     */
    /*-----------------------------------------------------------------------------------------------------*/
    userData = QLIB_GetUserData(qlibContext);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Clear globals                                                                                       */
    /*-----------------------------------------------------------------------------------------------------*/
    (void)memset(qlibContext, 0, sizeof(QLIB_CONTEXT_T));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Initiate the standard module                                                                        */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_InitLib(qlibContext));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Initiate the secure module                                                                          */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SEC_InitLib(qlibContext));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Initiate the transport manager module with configured user data                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_SetUserData(qlibContext, userData);
    QLIB_STATUS_RET_CHECK(QLIB_TM_Init(qlibContext));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_SetInterface(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T busFormat)
{
    return QLIB_SetInterface_L(qlibContext, busFormat, TRUE);
}

QLIB_STATUS_T QLIB_NotifyInterface(QLIB_CONTEXT_T*      qlibContext,
                                   QLIB_BUS_FORMAT_T    busFormat,
                                   QLIB_STD_ADDR_MODE_T addrMode,
                                   U32                  dummyCycles)
{
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET((addrMode == QLIB_STD_ADDR_MODE__3_BYTE) ||
                        ((Q2_4_BYTES_ADDRESS_MODE(qlibContext) != 0u) && (addrMode == QLIB_STD_ADDR_MODE__4_BYTE)),
                    QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET((W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) == 0u) ||
                        ((dummyCycles > 0u) && (dummyCycles <= SPI_FLASH__EXTENDED_CONFIGURATION_MAX_DUMMY_CONFIG)),
                    QLIB_STATUS__INVALID_PARAMETER);

    QLIB_STATUS_RET_CHECK(QLIB_SetInterface_L(qlibContext, busFormat, FALSE));
    qlibContext->fastReadDummy = (U8)(dummyCycles);
    qlibContext->addrMode      = addrMode;
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_InitDevice(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T busFormat)
{
    QLIB_HW_VER_T hwVer = {0};

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);

    /********************************************************************************************************
     * Make sure to be synchronized with the flash device, exit power down mode, detect SPI mode
     * and command extension mode, make sure flash is ready and there is no connectivity issues
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_waitReadyAndInitBusMode_L(qlibContext));

    QLIB_STATUS_RET_CHECK(QLIB_GetHWVersion(qlibContext, &hwVer));

    // set configuration table according to detected device
    QLIB_STATUS_RET_CHECK(QLIB_Cfg_Init(qlibContext, &hwVer));

#if !defined EXCLUDE_Q2_4_BYTES_ADDRESS_MODE || !defined EXCLUDE_FAST_READ_DUMMY_CONFIG
    // sync SPI volatile parameters. After QLIB SW reset these are set back to their pre-reset value.
    QLIB_STATUS_RET_CHECK(QLIB_SyncSPI_L(qlibContext));
#endif

#if QLIB_NUM_OF_DIES > 1
    if (W77Q_MULTI_DIE(qlibContext) != 0u)
    {
#if !defined QLIB_INIT_AFTER_FLASH_POWER_UP
        QLIB_REG_ESSR_T essr;
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_ESSR_UNSIGNED(qlibContext, &essr));
        qlibContext->activeDie = READ_VAR_FIELD(essr.asUint64, QLIB_REG_ESSR__DIE_ID);
#endif

        // detect how many DIEs actually exist
#ifdef W77Q_BYPASS_JEDEC_CAPACITY_MULTIDIE
#if QLIB_NUM_OF_DIES == 4
        (void)QLIB_STD_SetActiveDie(qlibContext, 3u, TRUE);
        if (qlibContext->activeDie == 3u)
        {
            qlibContext->MaxDieId = 3u;
        }
        else
#endif
        {
            (void)QLIB_STD_SetActiveDie(qlibContext, 1u, TRUE);
            qlibContext->MaxDieId = qlibContext->activeDie;
        }
#else  // W77Q_BYPASS_JEDEC_CAPACITY_MULTIDIE
        QLIB_ASSERT_RET(hwVer.std.capacity >= QLIB_JEDEC_ID_CAPACITY_256Mb, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
        qlibContext->MaxDieId = (1 << (hwVer.std.capacity - QLIB_JEDEC_ID_CAPACITY_256Mb)) - 1u;
#endif // W77Q_BYPASS_JEDEC_CAPACITY_MULTIDIE
    }
#endif

#ifndef QLIB_INIT_AFTER_FLASH_POWER_UP
    // reset flash to be sure all volatile registers are loaded
    QLIB_STATUS_RET_CHECK(QLIB_STD_ResetFlash(qlibContext, FALSE));
#endif

    /*-----------------------------------------------------------------------------------------------------*/
    /* synchronize the lib state with the flash state                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SEC_SyncState(qlibContext));

    // Set new bus format
    QLIB_STATUS_RET_CHECK(QLIB_SetInterface(qlibContext, busFormat));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_Read(QLIB_CONTEXT_T* qlibContext, U8* buf, U32 sectionID, U32 offset, U32 size, BOOL secure, BOOL auth)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(0u < size, QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
    QLIB_ASSERT_RET((offset + size) >= size, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);

    if (TRUE == secure)
    {
        QLIB_ASSERT_RET((W77Q_VAULT(qlibContext) != 0u) || (QLIB_SECTION_ID_VAULT != sectionID), QLIB_STATUS__INVALID_PARAMETER);
        QLIB_ASSERT_RET((offset + size) <= QLIB_CALC_SECTION_SIZE(qlibContext, sectionID), QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
        return QLIB_SEC_Read(qlibContext, buf, sectionID, offset, size, auth);
    }
    else
    {
        U32 section = QLIB_FALLBACK_SECTION(qlibContext, sectionID);
        QLIB_ASSERT_RET(sectionID < QLIB_SECTION_ID_VAULT, QLIB_STATUS__INVALID_PARAMETER);
        QLIB_ASSERT_RET((offset + size) <= _QLIB_MAX_LEGACY_OFFSET(qlibContext), QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
        QLIB_ASSERT_RET((offset + size) <= QLIB_CALC_SECTION_SIZE(qlibContext, section), QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
#if QLIB_NUM_OF_DIES > 1
        QLIB_ASSERT_RET(qlibContext->activeDie == QLIB_INIT_DIE_ID || qlibContext->addrMode == QLIB_STD_ADDR_MODE__4_BYTE,
                        QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
#endif
        if ((QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[section].plainEnabled & QLIB_SECTION_PLAIN_EN_RD) == 0u)
        {
            QLIB_STATUS_RET_CHECK(QLIB_PlainAccessGrant_L(qlibContext,
                                                          section,
                                                          W77Q_CMD_PA_GRANT_REVOKE(qlibContext) != 0u
                                                              ? QLIB_LOAD_ACLR_PLAIN_RD
                                                              : QLIB_LOAD_ACLR_NON_AUTH_PLAIN_RD));
        }
        return QLIB_STD_Read(qlibContext, buf, QLIB_MAKE_LOGICAL_ADDRESS(qlibContext, sectionID, offset), size);
    }
}

QLIB_STATUS_T QLIB_Write(QLIB_CONTEXT_T* qlibContext, const U8* buf, U32 sectionID, U32 offset, U32 size, BOOL secure)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(0u < size, QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
    QLIB_ASSERT_RET((offset + size) >= size, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);

    if (TRUE == secure)
    {
        QLIB_ASSERT_RET((W77Q_VAULT(qlibContext) != 0u) || (QLIB_SECTION_ID_VAULT != sectionID), QLIB_STATUS__INVALID_PARAMETER);
        QLIB_ASSERT_RET((offset + size) <= QLIB_CALC_SECTION_SIZE(qlibContext, sectionID), QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
        return QLIB_SEC_Write(qlibContext, buf, sectionID, offset, size);
    }
    else
    {
        U32 section = QLIB_FALLBACK_SECTION(qlibContext, sectionID);
        QLIB_ASSERT_RET(sectionID < QLIB_SECTION_ID_VAULT, QLIB_STATUS__INVALID_PARAMETER);
        QLIB_ASSERT_RET((offset + size) <= _QLIB_MAX_LEGACY_OFFSET(qlibContext), QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
        QLIB_ASSERT_RET((offset + size) <= QLIB_CALC_SECTION_SIZE(qlibContext, section), QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
#if QLIB_NUM_OF_DIES > 1
        QLIB_ASSERT_RET(qlibContext->activeDie == QLIB_INIT_DIE_ID || qlibContext->addrMode == QLIB_STD_ADDR_MODE__4_BYTE,
                        QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
#endif
        if ((QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[section].plainEnabled & QLIB_SECTION_PLAIN_EN_WR) == 0u)
        {
            QLIB_STATUS_T ret =
                QLIB_PlainAccessGrant_L(qlibContext,
                                        section,
                                        W77Q_CMD_PA_GRANT_REVOKE(qlibContext) != 0u ? QLIB_LOAD_ACLR_PLAIN_WR
                                                                                    : QLIB_LOAD_ACLR_NON_AUTH_PLAIN_WR);
            QLIB_ASSERT_RET((ret == QLIB_STATUS__OK) ||
                                ((QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[sectionID].plainEnabled &
                                  QLIB_SECTION_PLAIN_EN_WR) != 0u),
                            ret);
        }
        return QLIB_STD_Write(qlibContext, buf, QLIB_MAKE_LOGICAL_ADDRESS(qlibContext, sectionID, offset), size);
    }
}

QLIB_STATUS_T QLIB_Erase(QLIB_CONTEXT_T* qlibContext, U32 sectionID, U32 offset, U32 size, BOOL secure)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(0u < size, QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
    QLIB_ASSERT_RET((offset + size) >= size, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);

    if (TRUE == secure)
    {
        QLIB_ASSERT_RET((W77Q_VAULT(qlibContext) != 0u) || (QLIB_SECTION_ID_VAULT != sectionID), QLIB_STATUS__INVALID_PARAMETER);
        QLIB_ASSERT_RET((offset + size) <= QLIB_CALC_SECTION_SIZE(qlibContext, sectionID), QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
        return QLIB_SEC_Erase(qlibContext, sectionID, offset, size);
    }
    else
    {
        U32 section = QLIB_FALLBACK_SECTION(qlibContext, sectionID);
        QLIB_ASSERT_RET(sectionID < QLIB_SECTION_ID_VAULT, QLIB_STATUS__INVALID_PARAMETER);
        QLIB_ASSERT_RET((offset + size) <= _QLIB_MAX_LEGACY_OFFSET(qlibContext), QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
        QLIB_ASSERT_RET((offset + size) <= QLIB_CALC_SECTION_SIZE(qlibContext, section), QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
#if QLIB_NUM_OF_DIES > 1
        QLIB_ASSERT_RET(qlibContext->activeDie == QLIB_INIT_DIE_ID || qlibContext->addrMode == QLIB_STD_ADDR_MODE__4_BYTE,
                        QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
#endif

        if ((QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[section].plainEnabled & QLIB_SECTION_PLAIN_EN_WR) == 0u)
        {
            QLIB_STATUS_RET_CHECK(QLIB_PlainAccessGrant_L(qlibContext,
                                                          section,
                                                          W77Q_CMD_PA_GRANT_REVOKE(qlibContext) != 0u
                                                              ? QLIB_LOAD_ACLR_PLAIN_WR
                                                              : QLIB_LOAD_ACLR_NON_AUTH_PLAIN_WR));
        }
        return QLIB_STD_Erase(qlibContext, QLIB_MAKE_LOGICAL_ADDRESS(qlibContext, sectionID, offset), size);
    }
}

QLIB_STATUS_T QLIB_EraseSection(QLIB_CONTEXT_T* qlibContext, U32 sectionID, BOOL secure)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(!((QLIB_SECTION_ID_VAULT == sectionID) && (QLIB_VAULT_GET_SIZE(qlibContext) == 0u)),
                    QLIB_STATUS__DEVICE_PRIVILEGE_ERR);

    if (FALSE == secure)
    {
        QLIB_ASSERT_RET(sectionID < QLIB_SECTION_ID_VAULT, QLIB_STATUS__INVALID_PARAMETER);
        if ((QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[sectionID].plainEnabled & QLIB_SECTION_PLAIN_EN_WR) == 0u)
        {
            QLIB_STATUS_RET_CHECK(QLIB_PlainAccessGrant_L(qlibContext,
                                                          sectionID,
                                                          W77Q_CMD_PA_GRANT_REVOKE(qlibContext) != 0u
                                                              ? QLIB_LOAD_ACLR_PLAIN_WR
                                                              : QLIB_LOAD_ACLR_NON_AUTH_PLAIN_WR));
        }
    }
    else
    {
        QLIB_ASSERT_RET((W77Q_VAULT(qlibContext) != 0u) || (QLIB_SECTION_ID_VAULT != sectionID), QLIB_STATUS__INVALID_PARAMETER);
    }

    return QLIB_SEC_EraseSection(qlibContext, sectionID, secure);
}

QLIB_STATUS_T QLIB_Suspend(QLIB_CONTEXT_T* qlibContext)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).isSuspended == 0u, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    QLIB_STATUS_RET_CHECK(QLIB_STD_EraseSuspend(qlibContext));

#if QLIB_NUM_OF_DIES > 1
    qlibContext->suspendDie = qlibContext->activeDie;
#endif
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_Resume(QLIB_CONTEXT_T* qlibContext)
{
#if QLIB_NUM_OF_DIES > 1
    U32 dieId;
#endif
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
#if QLIB_NUM_OF_DIES > 1
    QLIB_STATUS_RET_CHECK(QLIB_STD_SetActiveDie(qlibContext, qlibContext->suspendDie, FALSE));
#endif
    QLIB_STATUS_RET_CHECK(QLIB_STD_EraseResume(qlibContext, FALSE));

    QLIB_ACTIVE_DIE_STATE(qlibContext).isSuspended = 0u;
    QLIB_ACTIVE_DIE_STATE(qlibContext).mcInSync    = 0u;
#if QLIB_NUM_OF_DIES > 1
    for (dieId = 0; dieId < QLIB_GET_NUM_OF_DIES(qlibContext); dieId++)
    {
        if (qlibContext->dieState[dieId].isSuspended == 1u)
        {
            // clear errors for all dies that we used while they were suspended
            QLIB_STATUS_RET_CHECK(QLIB_STD_SetActiveDie(qlibContext, dieId, TRUE));
        }
        qlibContext->dieState[dieId].isSuspended = 0u;
        qlibContext->dieState[dieId].mcInSync    = 0u;
    }
    QLIB_STATUS_RET_CHECK(QLIB_STD_SetActiveDie(qlibContext, qlibContext->suspendDie, FALSE));
#endif

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_Power(QLIB_CONTEXT_T* qlibContext, QLIB_POWER_T power)
{
    U32 die;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_STATUS_RET_CHECK(QLIB_STD_Power(qlibContext, power));

    /*-----------------------------------------------------------------------------------------------------*/
    /* During power down SW could try to communicate and increment counters                                */
    /*-----------------------------------------------------------------------------------------------------*/
    if (QLIB_POWER_UP == power)
    {
        for (die = 0; die < QLIB_NUM_OF_DIES; die++)
        {
            qlibContext->dieState[die].mcInSync = 0u;
        }
    };

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_ResetFlash(QLIB_CONTEXT_T* qlibContext)
{
    U32 dmc;
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

#if QLIB_NUM_OF_DIES > 1
    QLIB_STATUS_RET_CHECK(QLIB_STD_SetActiveDie(qlibContext, QLIB_INIT_DIE_ID, FALSE));
#endif
    /*-----------------------------------------------------------------------------------------------------*/
    /* Save previous value of DMC. DMC is incremented on flash reset                                       */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__synch_MC(qlibContext));
    dmc = qlibContext->dieState[QLIB_INIT_DIE_ID].mc[DMC];

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform reset                                                                                       */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_ResetFlash(qlibContext, TRUE));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Sync qlib state after reset                                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SEC_SyncAfterFlashReset(qlibContext));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Verify reset occurred by testing DMC new value                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__synch_MC(qlibContext));
    QLIB_ASSERT_RET(dmc < qlibContext->dieState[QLIB_INIT_DIE_ID].mc[DMC], QLIB_STATUS__COMMAND_IGNORED);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_Format(QLIB_CONTEXT_T* qlibContext, const KEY_T deviceMasterKey, BOOL eraseDataOnly, BOOL factoryDefault)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(FALSE == eraseDataOnly || QLIB_KEY_MNGR__IS_KEY_VALID(deviceMasterKey), QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_Format(qlibContext, deviceMasterKey, eraseDataOnly, factoryDefault);
}

QLIB_STATUS_T QLIB_GetNotifications(QLIB_CONTEXT_T* qlibContext, QLIB_NOTIFICATIONS_T* notifs)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(NULL != notifs, QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_GetNotifications(qlibContext, notifs);
}

QLIB_STATUS_T QLIB_PerformMaintenance(QLIB_CONTEXT_T* qlibContext)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_PerformMCMaint(qlibContext);
}

#ifdef DEPRECATED_CONFIGURATION_API
QLIB_STATUS_T QLIB_ConfigDevice(QLIB_CONTEXT_T*                   qlibContext,
                                const KEY_T                       deviceMasterKey,
                                const KEY_T                       deviceSecretKey,
                                const QLIB_SECTION_CONFIG_TABLE_T sectionTable[QLIB_NUM_OF_DIES],
                                const KEY_ARRAY_T                 restrictedKeys[QLIB_NUM_OF_DIES],
                                const KEY_ARRAY_T                 fullAccessKeys[QLIB_NUM_OF_DIES],
                                const QLIB_LMS_KEY_ARRAY_T        lmsKeys[QLIB_NUM_OF_DIES],
                                const KEY_T                       preProvisionedMasterKey,
                                const QLIB_WATCHDOG_CONF_T*       watchdogDefault,
                                const QLIB_DEVICE_CONF_T*         deviceConf,
                                const _128BIT                     suid)
{
    U32                        die;
    U32                        section;
    QLIB_FLASH_CONFIG_T        flashCfg                          = {0};
    QLIB_DIE_CONFIG_T          dieCfg                            = {0};
    QLIB_SECTIONS_CONF_TABLE_T cfgSectionArray[QLIB_NUM_OF_DIES] = {0};

#ifndef Q2_API
    for (die = 0; (U8)die < QLIB_GET_NUM_OF_DIES(qlibContext); ++die)
    {
        QLIB_ASSERT_RET((NULL == deviceConf) || (NULL == sectionTable) ||
                            (QLIB_VAULT_CFG_TO_SIZE(deviceConf->vaultSize[die]) == sectionTable[die][QLIB_SECTION_ID_VAULT].size),
                        QLIB_STATUS__INVALID_PARAMETER);
    }
#endif // Q2_API

    QLIB_STATUS_RET_CHECK(
        QLIB_GetDeviceConfigMultiDie(qlibContext, &flashCfg, &dieCfg, cfgSectionArray, QLIB_GET_NUM_OF_DIES(qlibContext)));

    if (NULL != deviceMasterKey)
    {
        (void)memcpy(dieCfg.deviceMasterKey, deviceMasterKey, sizeof(KEY_T));
    }
    if (NULL != deviceSecretKey)
    {
        (void)memcpy(dieCfg.deviceSecretKey, deviceSecretKey, sizeof(KEY_T));
    }
    if (NULL != preProvisionedMasterKey)
    {
        (void)memcpy(dieCfg.preProvisionedMasterKey, preProvisionedMasterKey, sizeof(KEY_T));
    }
    if (NULL != watchdogDefault)
    {
        (void)memcpy(&flashCfg.watchdogDefault, watchdogDefault, sizeof(QLIB_WATCHDOG_CONF_T));
    }
    if (NULL != suid)
    {
        (void)memcpy(dieCfg.suid, suid, sizeof(_128BIT));
    }
    if (NULL != deviceConf)
    {
        (void)memcpy(&dieCfg.resetResp, &deviceConf->resetResp, sizeof(QLIB_RESET_RESPONSE_T));
        dieCfg.safeFB            = deviceConf->safeFB;
        dieCfg.speculCK          = deviceConf->speculCK;
        dieCfg.nonSecureFormatEn = deviceConf->nonSecureFormatEn;
        dieCfg.rngPAEn           = deviceConf->rngPAEn;
        dieCfg.ctagModeMulti     = deviceConf->ctagModeMulti;
        dieCfg.devcfgLock        = deviceConf->lock;
        dieCfg.bootFailReset     = deviceConf->bootFailReset;

        (void)memcpy(&flashCfg.pinMux, &deviceConf->pinMux, sizeof(QLIB_PIN_MUX_T));
        flashCfg.stdAddrSize.addrLen  = deviceConf->stdAddrSize.addrLen;
        flashCfg.stdAddrSize.addrMode = deviceConf->stdAddrSize.addrMode;
        flashCfg.fastReadDummyCycles  = deviceConf->fastReadDummyCycles;
#ifndef Q2_API
        flashCfg.dqsDisable = deviceConf->dqsDisable;
#endif // Q2_API
    }

    for (die = 0u; (U8)die < QLIB_GET_NUM_OF_DIES(qlibContext); ++die)
    {
        for (section = 0u; section < QLIB_NUM_OF_SECTIONS; ++section)
        {
            QLIB_SECTION_CONF_POST_ACTIONS_T postActions = {0};

            if (NULL != sectionTable)
            {
                cfgSectionArray[die].sectionConfigTable[section].baseAddr = sectionTable[die][section].baseAddr;
                cfgSectionArray[die].sectionConfigTable[section].size     = sectionTable[die][section].size;
                (void)memcpy(&cfgSectionArray[die].sectionConfigTable[section].sectionConfig.policy,
                             &sectionTable[die][section].policy,
                             sizeof(QLIB_POLICY_T));
                cfgSectionArray[die].sectionConfigTable[section].sectionConfig.crc    = sectionTable[die][section].crc;
                cfgSectionArray[die].sectionConfigTable[section].sectionConfig.digest = sectionTable[die][section].digest;
            }
            cfgSectionArray[die].sectionConfigTable[section].sectionConfig.postActions = postActions;
            if (NULL != deviceConf)
            {
                if (section < QLIB_NUM_OF_MAIN_SECTIONS)
                {
                    cfgSectionArray[die].sectionConfigTable[section].resetPA = deviceConf->resetPA[die][section];
                }
            }
            if (NULL != restrictedKeys)
            {
                (void)memcpy((U8*)cfgSectionArray[die].sectionConfigTable[section].restrictedKey,
                             (U8*)restrictedKeys[die][section],
                             sizeof(KEY_T));
            }
            if (NULL != fullAccessKeys)
            {
                (void)memcpy((U8*)cfgSectionArray[die].sectionConfigTable[section].fullAccessKey,
                             (U8*)fullAccessKeys[die][section],
                             sizeof(KEY_T));
            }
            if (NULL != lmsKeys)
            {
                (void)memcpy((U8*)cfgSectionArray[die].sectionConfigTable[section].lmsKey,
                             (U8*)lmsKeys[die][section],
                             sizeof(QLIB_LMS_KEY_T));
            }
        }
    }

    return QLIB_ConfigDeviceMultiDie(qlibContext, &flashCfg, &dieCfg, cfgSectionArray, QLIB_GET_NUM_OF_DIES(qlibContext));
}
#endif // DEPRECATED_CONFIGURATION_API

QLIB_STATUS_T QLIB_ConfigDeviceMultiDie(QLIB_CONTEXT_T*             qlibContext,
                                        QLIB_FLASH_CONFIG_T*        flashCfg,
                                        QLIB_DIE_CONFIG_T*          dieCfg,
                                        QLIB_SECTIONS_CONF_TABLE_T* cfgSectionArray,
                                        U8                          numOfDies)
{
    U32 die;

    QLIB_ASSERT_RET(numOfDies == QLIB_GET_NUM_OF_DIES(qlibContext), QLIB_STATUS__INVALID_PARAMETER);

    for (die = 0; (U8)die < QLIB_GET_NUM_OF_DIES(qlibContext); ++die)
    {
#if QLIB_NUM_OF_DIES > 1
        QLIB_STATUS_RET_CHECK(QLIB_SetActiveDie(qlibContext, die));
#endif

        QLIB_STATUS_RET_CHECK(QLIB_ConfigDeviceSingleDie_L(qlibContext,
                                                           flashCfg,
                                                           dieCfg,
                                                           (cfgSectionArray != NULL) ? &cfgSectionArray[die] : NULL));
    }

    return QLIB_STATUS__OK;
}

#ifdef DEPRECATED_CONFIGURATION_API
QLIB_STATUS_T QLIB_GetDeviceConfig(QLIB_CONTEXT_T*       qlibContext,
                                   QLIB_WATCHDOG_CONF_T* watchdogDefault,
                                   QLIB_DEVICE_CONF_T*   deviceConf)
{
    QLIB_FLASH_CONFIG_T        flashCfg                          = {0};
    QLIB_DIE_CONFIG_T          dieCfg[QLIB_NUM_OF_DIES]          = {0};
    QLIB_SECTIONS_CONF_TABLE_T cfgSectionArray[QLIB_NUM_OF_DIES] = {0};
    U32                        die;
    U32                        section;
#ifndef Q2_API
    U32 size;
#endif // Q2_API

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != watchdogDefault, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != deviceConf, QLIB_STATUS__INVALID_PARAMETER);

    (void)memset(deviceConf, 0, sizeof(QLIB_DEVICE_CONF_T));

    QLIB_STATUS_RET_CHECK(
        QLIB_GetDeviceConfigMultiDie(qlibContext, &flashCfg, dieCfg, cfgSectionArray, QLIB_GET_NUM_OF_DIES(qlibContext)));

    (void)memset(watchdogDefault, 0, sizeof(QLIB_WATCHDOG_CONF_T));
    (void)memset(deviceConf, 0, sizeof(QLIB_DEVICE_CONF_T));

    (void)memcpy(watchdogDefault, &flashCfg.watchdogDefault, sizeof(QLIB_WATCHDOG_CONF_T));

    (void)memcpy(&deviceConf->resetResp, &dieCfg[QLIB_INIT_DIE_ID].resetResp, sizeof(QLIB_RESET_RESPONSE_T));
    deviceConf->safeFB            = dieCfg[QLIB_INIT_DIE_ID].safeFB;
    deviceConf->speculCK          = dieCfg[QLIB_INIT_DIE_ID].speculCK;
    deviceConf->nonSecureFormatEn = dieCfg[QLIB_INIT_DIE_ID].nonSecureFormatEn;
    (void)memcpy(&deviceConf->pinMux, &flashCfg.pinMux, sizeof(QLIB_PIN_MUX_T));
    (void)memcpy(&deviceConf->stdAddrSize, &flashCfg.stdAddrSize, sizeof(QLIB_STD_ADDR_SIZE_T));
    deviceConf->rngPAEn       = dieCfg[QLIB_INIT_DIE_ID].rngPAEn;
    deviceConf->ctagModeMulti = dieCfg[QLIB_INIT_DIE_ID].ctagModeMulti;
    deviceConf->lock          = dieCfg[QLIB_INIT_DIE_ID].devcfgLock;
    deviceConf->bootFailReset = dieCfg[QLIB_INIT_DIE_ID].bootFailReset;
    for (die = 0; (U8)die < QLIB_GET_NUM_OF_DIES(qlibContext); ++die)
    {
        for (section = 0; section < QLIB_NUM_OF_MAIN_SECTIONS; ++section)
        {
            deviceConf->resetPA[die][section] = cfgSectionArray[die].sectionConfigTable[section].resetPA;
        }
#ifndef Q2_API
        if (W77Q_VAULT(qlibContext) != 0u)
        {
            QLIB_STATUS_RET_CHECK(
                QLIB_SEC_GetSectionConfiguration(qlibContext, QLIB_SECTION_ID_VAULT, NULL, &size, NULL, NULL, NULL, NULL));
            deviceConf->vaultSize[die] = QLIB_VAULT_SIZE_TO_CFG(size);
        }
#endif // Q2_API
    }
    deviceConf->fastReadDummyCycles = flashCfg.fastReadDummyCycles;
#ifndef Q2_API
    deviceConf->dqsDisable = flashCfg.dqsDisable;
#endif // Q2_API

    return QLIB_STATUS__OK;
}
#endif // DEPRECATED_CONFIGURATION_API

QLIB_STATUS_T QLIB_GetDeviceConfigMultiDie(QLIB_CONTEXT_T*             qlibContext,
                                           QLIB_FLASH_CONFIG_T*        flashCfg,
                                           QLIB_DIE_CONFIG_T*          dieCfg,
                                           QLIB_SECTIONS_CONF_TABLE_T* cfgSectionArray,
                                           U8                          numOfDies)
{
    QLIB_FLASH_CONFIG_T* pFlashCfg;
    QLIB_DIE_CONFIG_T*   pDieCfg;
    U8                   origDie = qlibContext->activeDie;
    QLIB_STATUS_T        ret     = QLIB_STATUS__OK;
#if QLIB_NUM_OF_DIES > 1
    U8 nextDie;
#endif

    QLIB_ASSERT_RET(numOfDies == QLIB_GET_NUM_OF_DIES(qlibContext), QLIB_STATUS__INVALID_PARAMETER);

    do
    {
        pFlashCfg = (QLIB_INIT_DIE_ID == qlibContext->activeDie) ? flashCfg : NULL;
        pDieCfg   = (QLIB_INIT_DIE_ID == qlibContext->activeDie) ? dieCfg : NULL;
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_GetDeviceConfigSingleDie_L(qlibContext,
                                                                   pFlashCfg,
                                                                   pDieCfg,
                                                                   &cfgSectionArray[qlibContext->activeDie]),
                                   ret,
                                   error);

#if QLIB_NUM_OF_DIES > 1
        nextDie = (qlibContext->activeDie + 1u) % QLIB_GET_NUM_OF_DIES(qlibContext);
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_SetActiveDie(qlibContext, nextDie), ret, error);
#endif
    } while (qlibContext->activeDie != origDie);

    return QLIB_STATUS__OK;
error:
#if QLIB_NUM_OF_DIES > 1
    QLIB_SetActiveDie(qlibContext, origDie);
#endif
    return ret;
}

QLIB_STATUS_T QLIB_GetSectionConfiguration(QLIB_CONTEXT_T* qlibContext,
                                           U32             sectionID,
                                           U32*            baseAddr,
                                           U32*            size,
                                           QLIB_POLICY_T*  policy,
                                           U64*            digest,
                                           U32*            crc,
                                           U32*            version)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    return QLIB_SEC_GetSectionConfiguration(qlibContext, sectionID, baseAddr, size, policy, digest, crc, version);
}

#ifdef DEPRECATED_CONFIGURATION_API
QLIB_STATUS_T QLIB_ConfigSection(QLIB_CONTEXT_T*            qlibContext,
                                 U32                        sectionID,
                                 const QLIB_POLICY_T*       policy,
                                 const U64*                 digest,
                                 const U32*                 crc,
                                 const U32*                 newVersion,
                                 BOOL                       swap,
                                 QLIB_SECTION_CONF_ACTION_T action)
{
    QLIB_SECTION_CONF_T sectionConfig = {0};

    QLIB_POLICY_T currentPolicy  = {0};
    U64           currentDigest  = 0u;
    U32           currentCrc     = 0u;
    U32           currentVersion = 0u;

    if ((NULL != policy) || (NULL != crc) || (NULL != digest) || (NULL != newVersion))
    {
        QLIB_STATUS_RET_CHECK(QLIB_GetSectionConfiguration(qlibContext,
                                                           sectionID,
                                                           NULL,
                                                           NULL,
                                                           &currentPolicy,
                                                           &currentDigest,
                                                           &currentCrc,
                                                           &currentVersion));
    }

    (void)memcpy(&sectionConfig.policy, (NULL != policy) ? policy : &currentPolicy, sizeof(QLIB_POLICY_T));
    sectionConfig.crc                = (NULL != crc) ? *crc : currentCrc;
    sectionConfig.digest             = (NULL != digest) ? *digest : currentDigest;
    sectionConfig.version            = (NULL != newVersion) ? *newVersion : currentVersion;
    sectionConfig.postActions.swap   = BOOLEAN_TO_INT(swap);
    sectionConfig.postActions.reload = (QLIB_SECTION_CONF_ACTION__RELOAD == action) ? 1u : 0u;
    sectionConfig.postActions.reset  = (QLIB_SECTION_CONF_ACTION__RESET == action) ? 1u : 0u;

    return QLIB_ConfigDeviceSection(qlibContext, sectionID, &sectionConfig);
}
#endif // DEPRECATED_CONFIGURATION_API

QLIB_STATUS_T QLIB_ConfigDeviceSection(QLIB_CONTEXT_T* qlibContext, U32 sectionID, QLIB_SECTION_CONF_T* sectionConfig)
{
    QLIB_SECTION_CONF_ACTION_T action;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET((W77Q_VAULT(qlibContext) != 0u) || (QLIB_SECTION_ID_VAULT != sectionID), QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != sectionConfig, QLIB_STATUS__INVALID_PARAMETER);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform Section Config                                                                              */
    /*-----------------------------------------------------------------------------------------------------*/
    action = (1u == sectionConfig->postActions.reset)
                 ? QLIB_SECTION_CONF_ACTION__RESET
                 : ((1u == sectionConfig->postActions.reload) ? QLIB_SECTION_CONF_ACTION__RELOAD : QLIB_SECTION_CONF_ACTION__NO);

    QLIB_STATUS_RET_CHECK(QLIB_SEC_ConfigSection(qlibContext,
                                                 sectionID,
                                                 &sectionConfig->policy,
                                                 &sectionConfig->digest,
                                                 &sectionConfig->crc,
                                                 &sectionConfig->version,
                                                 INT_TO_BOOLEAN(sectionConfig->postActions.swap),
                                                 action));

    if ((W77Q_SET_SCR_MODE(qlibContext) == 0u) && (action != QLIB_SECTION_CONF_ACTION__NO))
    {
        /*-------------------------------------------------------------------------------------------------*/
        /* Reopen the session to this section, after 'set_SCRn' revokes access privileges to the section   */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_SEC_OpenSession(qlibContext, sectionID, QLIB_SESSION_ACCESS_FULL, TRUE));
    }
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_ConfigAccess(QLIB_CONTEXT_T* qlibContext, U32 sectionID, BOOL readEnable, BOOL writeEnable)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform configuration                                                                               */
    /*-----------------------------------------------------------------------------------------------------*/
    return QLIB_SEC_ConfigAccess(qlibContext, sectionID, readEnable, writeEnable);
}

QLIB_STATUS_T QLIB_LoadKey(QLIB_CONTEXT_T* qlibContext, U32 sectionID, const KEY_T key, BOOL fullAccess)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_LoadKey(qlibContext, sectionID, key, fullAccess);
}

QLIB_STATUS_T QLIB_RemoveKey(QLIB_CONTEXT_T* qlibContext, U32 sectionID, BOOL fullAccess)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_RemoveKey(qlibContext, sectionID, fullAccess);
}

QLIB_STATUS_T QLIB_Connect(QLIB_CONTEXT_T* qlibContext)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_STATUS_RET_CHECK(QLIB_TM_Connect(qlibContext));

#if QLIB_NUM_OF_DIES > 1
    qlibContext->activeDie = QLIB_INIT_DIE_ID;
#endif
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_Disconnect(QLIB_CONTEXT_T* qlibContext)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(!QLIB_KEY_MNGR__SESSION_IS_OPEN(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

#if QLIB_NUM_OF_DIES > 1
    QLIB_STATUS_RET_CHECK(QLIB_SetActiveDie(qlibContext, QLIB_INIT_DIE_ID));
#endif
    QLIB_STATUS_RET_CHECK(QLIB_TM_Disconnect(qlibContext));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_OpenSession(QLIB_CONTEXT_T* qlibContext, U32 sectionID, QLIB_SESSION_ACCESS_T sessionAccess)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    return QLIB_SEC_OpenSession(qlibContext, sectionID, sessionAccess, TRUE);
}

QLIB_STATUS_T QLIB_CloseSession(QLIB_CONTEXT_T* qlibContext, U32 sectionID)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    return QLIB_SEC_CloseSession(qlibContext, sectionID);
}

QLIB_STATUS_T QLIB_PlainAccessGrant(QLIB_CONTEXT_T* qlibContext, U32 sectionID)
{
    return QLIB_PlainAccessGrant_L(qlibContext, sectionID, QLIB_LOAD_ACLR_ANY);
}

QLIB_STATUS_T QLIB_PlainAccessRevoke(QLIB_CONTEXT_T* qlibContext, U32 sectionID, QLIB_PA_REVOKE_TYPE_T revokeType)
{
    QLIB_POLICY_T policy = {0};

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_SECTION_ID_VAULT > sectionID, QLIB_STATUS__INVALID_PARAMETER);

    /*-----------------------------------------------------------------------------------------------------*/
    /* check if the section is authenticated plain access                                                  */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_GetSectionConfiguration(qlibContext, sectionID, NULL, NULL, &policy, NULL, NULL, NULL));
    QLIB_ASSERT_RET((W77Q_CMD_PA_GRANT_REVOKE(qlibContext) != 0u) || (policy.authPlainAccess == 1u), QLIB_STATUS__NOT_SUPPORTED);

    if (((revokeType == QLIB_PA_REVOKE_ALL_ACCESS) &&
         (QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[sectionID].plainEnabled != QLIB_SECTION_PLAIN_EN_NO)) ||
        ((QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[sectionID].plainEnabled & QLIB_SECTION_PLAIN_EN_WR) != 0u))

    {
        QLIB_STATUS_RET_CHECK(QLIB_SEC_PlainAccess_Revoke(qlibContext, sectionID, revokeType));

        /*-------------------------------------------------------------------------------------------------*/
        /* Set plain access state in QLIB                                                                  */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[sectionID].plainEnabled =
            (revokeType == QLIB_PA_REVOKE_ALL_ACCESS || policy.plainAccessReadEnable == 0u) ? QLIB_SECTION_PLAIN_EN_NO
                                                                                            : QLIB_SECTION_PLAIN_EN_RD;
    }
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CheckIntegrity(QLIB_CONTEXT_T* qlibContext, U32 sectionID, QLIB_INTEGRITY_T integrityType)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    return QLIB_SEC_CheckIntegrity(qlibContext, sectionID, integrityType);
}

QLIB_STATUS_T QLIB_CalcCDI(QLIB_CONTEXT_T* qlibContext, _256BIT nextCdi, const _256BIT prevCdi, U32 sectionId)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(NULL != nextCdi, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionId, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET((W77Q_VAULT(qlibContext) != 0u) || (QLIB_SECTION_ID_VAULT != sectionId), QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_CalcCDI(qlibContext, nextCdi, prevCdi, sectionId);
}

QLIB_STATUS_T QLIB_Watchdog_ConfigSet(QLIB_CONTEXT_T* qlibContext, const QLIB_WATCHDOG_CONF_T* watchdogCFG)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(NULL != watchdogCFG, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(qlibContext->activeDie == QLIB_INIT_DIE_ID, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    return QLIB_SEC_Watchdog_ConfigSet(qlibContext, watchdogCFG);
}

QLIB_STATUS_T QLIB_Watchdog_ConfigGet(QLIB_CONTEXT_T* qlibContext, QLIB_WATCHDOG_CONF_T* watchdogCFG)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(NULL != watchdogCFG, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(qlibContext->activeDie == QLIB_INIT_DIE_ID, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    return QLIB_SEC_Watchdog_ConfigGet(qlibContext, watchdogCFG);
}

#ifdef Q2_API
QLIB_STATUS_T QLIB2_Watchdog_Get(QLIB_CONTEXT_T* qlibContext, U32* secondsPassed, U32* ticksResidue, BOOL* expired)
{
    AWDTSR_T AWDTSR = 0;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(qlibContext->activeDie == QLIB_INIT_DIE_ID, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Secure command is ignored if power is down or suspended                                             */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).isPoweredDown == 0u, QLIB_STATUS__COMMAND_IGNORED);
    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).isSuspended == 0u, QLIB_STATUS__COMMAND_IGNORED);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_AWDTSR(qlibContext, &AWDTSR));

    if (NULL != secondsPassed)
    {
        *secondsPassed = (U32)READ_VAR_FIELD(AWDTSR, QLIB_REG_AWDTSR__AWDT_VAL(qlibContext));
    }

    if (NULL != ticksResidue)
    {
        *ticksResidue = (U32)READ_VAR_FIELD(AWDTSR, QLIB_REG_AWDTSR__AWDT_RES(qlibContext));
    }

    if (NULL != expired)
    {
        *expired = (1u == READ_VAR_FIELD(AWDTSR, QLIB_REG_AWDTSR__AWDT_EXP_S)) ? TRUE : FALSE;
    }

    return QLIB_STATUS__OK;
}
#endif

QLIB_STATUS_T QLIB_Watchdog_Touch(QLIB_CONTEXT_T* qlibContext)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(qlibContext->activeDie == QLIB_INIT_DIE_ID, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    return QLIB_SEC_Watchdog_Touch(qlibContext);
}

QLIB_STATUS_T QLIB_Watchdog_Trigger(QLIB_CONTEXT_T* qlibContext)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(qlibContext->activeDie == QLIB_INIT_DIE_ID, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    return QLIB_SEC_Watchdog_Trigger(qlibContext);
}

QLIB_STATUS_T QLIB_Watchdog_Get(QLIB_CONTEXT_T* qlibContext, U32* milliSecondsPassed, BOOL* expired)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(qlibContext->activeDie == QLIB_INIT_DIE_ID, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    return QLIB_SEC_Watchdog_Get(qlibContext, milliSecondsPassed, expired);
}

QLIB_STATUS_T QLIB_GetId(QLIB_CONTEXT_T* qlibContext, QLIB_ID_T* id_p)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(NULL != id_p, QLIB_STATUS__INVALID_PARAMETER);

        QLIB_STATUS_RET_CHECK(QLIB_STD_GetId(qlibContext, &(id_p->std)));
    QLIB_STATUS_RET_CHECK(QLIB_SEC_GetId(qlibContext, &(id_p->sec)));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_GetHWVersion(QLIB_CONTEXT_T* qlibContext, QLIB_HW_VER_T* hwVer)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(NULL != hwVer, QLIB_STATUS__INVALID_PARAMETER);

        QLIB_STATUS_RET_CHECK(QLIB_STD_GetHwVersion(qlibContext, &(hwVer->std)));
        hwVer->info.isSingleDie = (hwVer->std.capacity > QLIB_JEDEC_ID_CAPACITY_256Mb ? FALSE : TRUE);
        hwVer->info.flashSize   = DEVICE_ID_TO_TARGET_SIZE(hwVer->std.deviceID);
        hwVer->info.voltage     = (hwVer->std.memoryType == 0x4Au) || (hwVer->std.memoryType == 0x4Bu) ||
                                      (hwVer->std.memoryType == 0x4Cu) || (hwVer->std.memoryType == 0x4Du)
                                      ? QLIB_TARGET_VOLTAGE_3_3V
                                      : QLIB_TARGET_VOLTAGE_1_8V;

    QLIB_STATUS_RET_CHECK(QLIB_SEC_GetHWVersion(qlibContext, &(hwVer->sec)));

    switch (hwVer->sec.revision)
    {
        case 0:
            hwVer->info.revision = QLIB_TARGET_REVISION_A;
            break;
        case 1:
            hwVer->info.revision = QLIB_TARGET_REVISION_B;
            break;
        case 2:
            hwVer->info.revision = QLIB_TARGET_REVISION_C;
            break;
        default:
            hwVer->info.revision = QLIB_TARGET_REVISION_UNKNOWN;
            break;
    }
    QLIB_STATUS_RET_CHECK(QLIB_GetTargetFlash_L(hwVer, &hwVer->info.target));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_GetStatus(QLIB_CONTEXT_T* qlibContext)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    return QLIB_SEC_GetStatus(qlibContext);
}

void* QLIB_GetUserData(QLIB_CONTEXT_T* qlibContext)
{
    if (qlibContext == NULL)
    {
        return NULL;
    }
    else
    {
        return qlibContext->userData;
    }
}

void QLIB_SetUserData(QLIB_CONTEXT_T* qlibContext, void* userData)
{
    if (qlibContext != NULL)
    {
        qlibContext->userData = userData;
    }
}

QLIB_STATUS_T QLIB_ExportState(QLIB_CONTEXT_T* qlibContext, QLIB_SYNC_OBJ_T* syncObject)
{
    U32 die;
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(NULL != syncObject, QLIB_STATUS__INVALID_PARAMETER);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Fill the synchronization object                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    (void)memset(syncObject, 0, sizeof(QLIB_SYNC_OBJ_T));
    (void)memcpy(&syncObject->busInterface, &qlibContext->busInterface, sizeof(qlibContext->busInterface));
    syncObject->busInterface.busIsLocked = FALSE;

    syncObject->resetStatus = qlibContext->resetStatus;
    syncObject->addrSize    = qlibContext->addrSize;
    syncObject->addrMode    = qlibContext->addrMode;

    for (die = 0; die < QLIB_NUM_OF_DIES; die++)
    {
        (void)memcpy(syncObject->sectionsState[die],
                     qlibContext->dieState[die].sectionsState,
                     sizeof(qlibContext->dieState[die].sectionsState));
        syncObject->vaultSize[die] = qlibContext->dieState[die].vaultSize;
        (void)memcpy((void*)syncObject->wid[die], (const void*)qlibContext->dieState[die].wid, sizeof(QLIB_WID_T));
    }
    (void)memcpy(syncObject->cfgBitArr, qlibContext->cfgBitArr, sizeof(qlibContext->cfgBitArr));
    syncObject->detectedDeviceID = qlibContext->detectedDeviceID;
    syncObject->fastReadDummy    = qlibContext->fastReadDummy;
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_ImportState(QLIB_CONTEXT_T* qlibContext, const QLIB_SYNC_OBJ_T* syncObject)
{
    BOOL busIsLocked = FALSE;
    U32  die;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != syncObject, QLIB_STATUS__INVALID_PARAMETER);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Update the lib context                                                                              */
    /*-----------------------------------------------------------------------------------------------------*/
    busIsLocked = qlibContext->busInterface.busIsLocked;
    (void)memcpy(&qlibContext->busInterface, &syncObject->busInterface, sizeof(qlibContext->busInterface));
    qlibContext->busInterface.busIsLocked = busIsLocked;
    qlibContext->resetStatus              = syncObject->resetStatus;
    qlibContext->addrSize                 = syncObject->addrSize;
    qlibContext->addrMode                 = syncObject->addrMode;

    for (die = 0; die < QLIB_NUM_OF_DIES; die++)
    {
        (void)memcpy(qlibContext->dieState[die].sectionsState,
                     syncObject->sectionsState[die],
                     sizeof(qlibContext->dieState[die].sectionsState));
        qlibContext->dieState[die].vaultSize = syncObject->vaultSize[die];
        (void)memcpy((void*)qlibContext->dieState[die].wid, (const void*)syncObject->wid[die], sizeof(QLIB_WID_T));
    }
    (void)memcpy(qlibContext->cfgBitArr, syncObject->cfgBitArr, sizeof(qlibContext->cfgBitArr));
    qlibContext->detectedDeviceID = syncObject->detectedDeviceID;
    qlibContext->fastReadDummy    = syncObject->fastReadDummy;

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_GetVersion(QLIB_SW_VERSION_T* versionInfo)
{
    versionInfo->qlibVersion = QLIB_VERSION;
#ifdef Q2_API
    versionInfo->qlibTarget.isSingleDie = TRUE;
#if (QLIB_TARGET == w77q32jw_revB)
    versionInfo->qlibTarget.flashSize = QLIB_TARGET_SIZE_32Mb;
    versionInfo->qlibTarget.revision  = QLIB_TARGET_REVISION_C;
#elif ((QLIB_TARGET == w77q64jw_revA) || (QLIB_TARGET == w77q64jv_revA))
    versionInfo->qlibTarget.flashSize = QLIB_TARGET_SIZE_64Mb;
    versionInfo->qlibTarget.revision  = QLIB_TARGET_REVISION_A;
#elif ((QLIB_TARGET == w77q128jw_revA) || (QLIB_TARGET == w77q128jv_revA))
    versionInfo->qlibTarget.flashSize = QLIB_TARGET_SIZE_128Mb;
    versionInfo->qlibTarget.revision  = QLIB_TARGET_REVISION_A;
#elif ((QLIB_TARGET == w77q25nw_Ind_revB) || (QLIB_TARGET == w77q25nw_Auto_revB) || (QLIB_TARGET == w77t25nw_Ind_revB) || \
       (QLIB_TARGET == w77t25nw_Auto_revB))
    versionInfo->qlibTarget.flashSize = QLIB_TARGET_SIZE_256Mb;
    versionInfo->qlibTarget.revision  = QLIB_TARGET_REVISION_B;
#else
    versionInfo->qlibTarget.flashSize = QLIB_TARGET_SIZE_UNKNOWN;
    versionInfo->qlibTarget.revision  = QLIB_TARGET_REVISION_UNKNOWN;
#endif
#if ((QLIB_TARGET == w77q128jv_revA) || (QLIB_TARGET == w77q64jv_revA))
    versionInfo->qlibTarget.voltage = QLIB_TARGET_VOLTAGE_3_3V;
#elif ((QLIB_TARGET == w77q32jw_revB) || (QLIB_TARGET == w77q128jw_revA) || (QLIB_TARGET == w77q64jw_revA) ||             \
       (QLIB_TARGET == w77q25nw_Ind_revB) || (QLIB_TARGET == w77q25nw_Auto_revB) || (QLIB_TARGET == w77t25nw_Ind_revB) || \
       (QLIB_TARGET == w77t25nw_Auto_revB))

    versionInfo->qlibTarget.voltage = QLIB_TARGET_VOLTAGE_1_8V;
#else
    versionInfo->qlibTarget.voltage   = QLIB_TARGET_VOLTAGE_UNKNOWN;
#endif
#else
    versionInfo->qlibTarget = (U32)(QLIB_TARGET);
#endif
    return QLIB_STATUS__OK;
}


QLIB_STATUS_T QLIB_SetActiveDie(QLIB_CONTEXT_T* qlibContext, U8 die)
{
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(QLIB_NUM_OF_DIES > die, QLIB_STATUS__INVALID_PARAMETER);

    if (die != qlibContext->activeDie)
    {
#ifdef QLIB_RESUME_ON_DIE_SELECT
        // before leaving the previous die, resume suspend
        {
            if (QLIB_ACTIVE_DIE_STATE(qlibContext).isPoweredDown == 0u && QLIB_ACTIVE_DIE_STATE(qlibContext).isSuspended == 1u)
            {
                QLIB_STATUS_RET_CHECK(QLIB_Resume(qlibContext));
            }
        }
#endif //QLIB_RESUME_ON_DIE_SELECT
        QLIB_STATUS_RET_CHECK(QLIB_STD_SetActiveDie(qlibContext, die, FALSE));
        qlibContext->activeDie = die;
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_GetResetStatus(QLIB_CONTEXT_T* qlibContext, QLIB_RESET_STATUS_T* resetStatus)
{
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != resetStatus, QLIB_STATUS__INVALID_PARAMETER);

    *resetStatus = qlibContext->resetStatus;

    return QLIB_STATUS__OK;
}

#ifndef EXCLUDE_Q2_4_BYTES_ADDRESS_MODE
QLIB_STATUS_T QLIB_SetAddressMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T addrMode)
{
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(Q2_4_BYTES_ADDRESS_MODE(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_STATUS_RET_CHECK(QLIB_STD_SetAddressMode(qlibContext, addrMode));
    return QLIB_STATUS__OK;
}
QLIB_STATUS_T QLIB_SetPowerUpAddressMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T addrMode)
{
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(Q2_4_BYTES_ADDRESS_MODE(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_STATUS_RET_CHECK(QLIB_STD_SetPowerUpAddressMode(qlibContext, addrMode));
    return QLIB_STATUS__OK;
}
#endif

#ifndef EXCLUDE_FAST_READ_DUMMY_CONFIG
QLIB_STATUS_T QLIB_SetFastReadDummyCycles(QLIB_CONTEXT_T* qlibContext, U32 dummyCycles)
{
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET((dummyCycles > 0u) && (dummyCycles <= SPI_FLASH__EXTENDED_CONFIGURATION_MAX_DUMMY_CONFIG),
                    QLIB_STATUS__INVALID_PARAMETER);
    QLIB_STATUS_RET_CHECK(QLIB_STD_SetFastReadDummyCycles(qlibContext, (U8)dummyCycles));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_SetPowerUpFastReadDummyCycles(QLIB_CONTEXT_T* qlibContext, U32 dummyCycles)
{
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET((dummyCycles > 0u) && (dummyCycles <= SPI_FLASH__EXTENDED_CONFIGURATION_MAX_DUMMY_CONFIG),
                    QLIB_STATUS__INVALID_PARAMETER);
    QLIB_STATUS_RET_CHECK(QLIB_STD_SetPowerUpFastReadDummyCycles(qlibContext, (U8)dummyCycles));
    return QLIB_STATUS__OK;
}
#endif


#ifdef QLIB_SIGN_DATA_BY_FLASH
QLIB_STATUS_T QLIB_Sign(QLIB_CONTEXT_T* qlibContext, U32 sectionID, U8* dataIn, U32 dataSize, _256BIT signature)
{
    QLIB_POLICY_T policy;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(NULL != dataIn, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(0u != dataSize, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != signature, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET((W77Q_VAULT(qlibContext) != 0u) || (QLIB_SECTION_ID_VAULT != sectionID), QLIB_STATUS__INVALID_PARAMETER);
    if (W77Q_SUPPORT_LMS_ATTESTATION(qlibContext) != 0u)
    {
        return QLIB_STATUS__NOT_SUPPORTED;
    }
    else
    {
        /*-------------------------------------------------------------------------------------------------*/
        /* Verify section is not rollback protected since both active and inactive partitions are erased   */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_SEC_GetSectionConfiguration(qlibContext, sectionID, NULL, NULL, &policy, NULL, NULL, NULL));
        QLIB_ASSERT_RET(policy.rollbackProt == 0u, QLIB_STATUS__INVALID_PARAMETER);

        /*-------------------------------------------------------------------------------------------------*/
        /* Secure command is ignored if power is down or suspended                                         */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).isPoweredDown == 0u, QLIB_STATUS__COMMAND_IGNORED);
        QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).isSuspended == 0u, QLIB_STATUS__COMMAND_IGNORED);

        return QLIB_SEC_SignVerify(qlibContext, sectionID, dataIn, dataSize, signature, FALSE);
    }
}

QLIB_STATUS_T QLIB_Verify(QLIB_CONTEXT_T* qlibContext, U32 sectionID, U8* dataIn, U32 dataSize, const _256BIT signature)
{
    _256BIT tempSignature;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != (void*)qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(NULL != (void*)dataIn, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(0u != dataSize, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != signature, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET((W77Q_VAULT(qlibContext) != 0u) || (QLIB_SECTION_ID_VAULT != sectionID), QLIB_STATUS__INVALID_PARAMETER);
    if (W77Q_SUPPORT_LMS_ATTESTATION(qlibContext) != 0u)
    {
        return QLIB_STATUS__NOT_SUPPORTED;
    }
    else
    {
        /*-------------------------------------------------------------------------------------------------*/
        /* Open session is required for Verify                                                             */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_ASSERT_RET(QLIB_KEY_MNGR__SESSION_IS_OPEN(qlibContext), QLIB_STATUS__DEVICE_SESSION_ERR);

        /*-----------------------------------------------------------------------------------------------------*/
        /* Copy the signature to a temporary variable as the QLIB_SEC_SignVerify gets a non-const signature    */
        /*-----------------------------------------------------------------------------------------------------*/
        (void)memcpy(tempSignature, signature, sizeof(tempSignature));
        return QLIB_SEC_SignVerify(qlibContext, sectionID, dataIn, dataSize, tempSignature, TRUE);
    }
}

#endif

#if !defined EXCLUDE_LMS && !defined Q2_API
QLIB_STATUS_T                        QLIB_SendLMSCommand(QLIB_CONTEXT_T*                  qlibContext,
                                                         const QLIB_LMS_MSG_DATA_STRUCT_T lmsMsg,
                                                         const QLIB_LMS_KEY_ID_T          keyId,
                                                         const QLIB_LMS_SIG_BUFFER_T      lmsSig,
                                                         U32                              sectionID)
{
    U32       lmsCmd[QLIB_LMS_COMMAND_SIZE / sizeof(U32)] = {0};
    U8*       pLmsCommand                                 = (U8*)lmsCmd;
    const U8* pLmsSig                                     = lmsSig;
    U32       leafIndex;
    U32       lmotsParameterSetId;
    U32       lmsParameterSetId;

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_SUPPORT_LMS(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET(NULL != lmsMsg, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != lmsSig, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);

    (void)memcpy(pLmsCommand, lmsMsg, sizeof(QLIB_LMS_MSG_DATA_STRUCT_T));
    pLmsCommand += sizeof(QLIB_LMS_MSG_DATA_STRUCT_T);
    (void)memcpy(pLmsCommand, keyId, sizeof(QLIB_LMS_KEY_ID_T));
    pLmsCommand += sizeof(QLIB_LMS_KEY_ID_T);

    // q
    (void)memcpy((U8*)(&leafIndex), pLmsSig, sizeof(U32));
    pLmsSig += sizeof(U32);
    *pLmsCommand++ = (uint8_t)((leafIndex >> 24u) & 0xFFu);
    *pLmsCommand++ = (uint8_t)((leafIndex >> 16u) & 0xFFu);
    *pLmsCommand++ = (uint8_t)((leafIndex >> 8u) & 0xFFu);
    *pLmsCommand++ = (uint8_t)(leafIndex & 0xFFu);

    // lms ots type
    (void)memcpy((U8*)(&lmotsParameterSetId), pLmsSig, sizeof(U32));
    pLmsSig += sizeof(U32);
    QLIB_ASSERT_RET(lmotsParameterSetId == QLIB_LMOTS_SHA256_N24_W4_PARAM_SET_ID, QLIB_STATUS__INVALID_PARAMETER);

    // nonce
    (void)memcpy(pLmsCommand, pLmsSig, QLIB_LMS_PARAM_N);
    pLmsCommand += QLIB_LMS_PARAM_N;
    pLmsSig += QLIB_LMS_PARAM_N;

    // ots signature
    (void)memcpy(pLmsCommand, pLmsSig, QLIB_LMS_PARAM_P * QLIB_LMS_PARAM_N);
    pLmsCommand += (QLIB_LMS_PARAM_P * QLIB_LMS_PARAM_N);
    pLmsSig += (QLIB_LMS_PARAM_P * QLIB_LMS_PARAM_N);

    // lms type
    (void)memcpy((U8*)(&lmsParameterSetId), pLmsSig, sizeof(U32));
    QLIB_ASSERT_RET(lmsParameterSetId == QLIB_LMS_SHA256_M24_H20_PARAM_SET_ID, QLIB_STATUS__INVALID_PARAMETER);
    pLmsSig += sizeof(U32);

    // path
    (void)memcpy(pLmsCommand, pLmsSig, QLIB_LMS_TREE_HEIGHT * QLIB_LMS_PARAM_N);

    return QLIB_SEC_SendLMSCommand(qlibContext, lmsCmd, sizeof(lmsCmd), sectionID);
}
#endif

#ifndef EXCLUDE_MEM_COPY
QLIB_STATUS_T QLIB_MemCpy(QLIB_CONTEXT_T* qlibContext, U32 sectionID, U32 dest, U32 src, U32 size)
{
    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_MEM_COPY(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET(0u < size, QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
    QLIB_ASSERT_RET((dest + size) >= size, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET((src + size) >= size, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_MemCopy(qlibContext, dest, src, size, sectionID);
}
#endif

#ifndef EXCLUDE_MEM_CRC
QLIB_STATUS_T QLIB_MemCRC(QLIB_CONTEXT_T* qlibContext, U32* crc32, U32 sectionID, U32 offset, U32 size)
{
    QLIB_POLICY_T policy = {0};
    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_MEM_CRC(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET(0u < size, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);
    /********************************************************************************************************
     * Read current section version tag and check policy
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_GetSectionConfiguration(qlibContext, sectionID, NULL, NULL, &policy, NULL, NULL, NULL));
    QLIB_ASSERT_RET((offset + size) <= QLIB_CALC_SECTION_SIZE(qlibContext, sectionID), QLIB_STATUS__PARAMETER_OUT_OF_RANGE);

    return QLIB_SEC_MemCrc(qlibContext, crc32, sectionID, offset, size);
}
#endif

#ifndef EXCLUDE_SECURE_LOG
QLIB_STATUS_T QLIB_SecureLogRead(QLIB_CONTEXT_T* qlibContext, U8* buf, U32* addr, U32 sectionID, BOOL secure)
{
    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_SECURE_LOG(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(!((FALSE == secure) && (QLIB_SECTION_ID_VAULT == sectionID)), QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_SecureLogRead(qlibContext, buf, addr, sectionID, secure);
}

QLIB_STATUS_T QLIB_SecureLogWrite(QLIB_CONTEXT_T* qlibContext, const U8* buf, U32 sectionID, U32 size, BOOL secure)
{
    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_SECURE_LOG(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET(0u < size, QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
    QLIB_ASSERT_RET(QLIB_NUM_OF_SECTIONS > sectionID, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(!((FALSE == secure) && (QLIB_SECTION_ID_VAULT == sectionID)), QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_SecureLogWrite(qlibContext, buf, sectionID, size, secure);
}
#endif

#ifndef EXCLUDE_W77Q_RNG_FEATURE
QLIB_STATUS_T QLIB_GetRandom(QLIB_CONTEXT_T* qlibContext, U8* random, U32 randomSize)
{
    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_RNG_FEATURE(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET(NULL != random, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(0u != randomSize, QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_GetRandom(qlibContext, random, randomSize);
}
#endif

#if !defined EXCLUDE_LMS_ATTESTATION && !defined Q2_API
QLIB_STATUS_T                                    QLIB_LMS_Attest_SetPrivateKey(QLIB_CONTEXT_T*                 qlibContext,
                                                                               const LMS_ATTEST_PRIVATE_SEED_T seed,
                                                                               const LMS_ATTEST_KEY_ID_T       keyId)
{
    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_SUPPORT_LMS_ATTESTATION(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET(NULL != seed, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != keyId, QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_LMS_Attest_SetPrivateKey(qlibContext, seed, keyId);
}

QLIB_STATUS_T QLIB_LMS_Attest_GetPublicKey(QLIB_CONTEXT_T*    qlibContext,
                                           LMS_ATTEST_CHUNK_T pubKey,
                                           LMS_ATTEST_CHUNK_T pubCache[],
                                           U32                pubCacheLen)
{
    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER)
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_SUPPORT_LMS_ATTESTATION(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET(NULL != pubKey, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(0u == pubCacheLen || NULL != pubCache, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(pubCacheLen <= QLIB_LMS_ATTEST_ALL_NODES, QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_SEC_LMS_Attest_GetPublicKey(qlibContext, pubKey, pubCache, pubCacheLen);
}

QLIB_STATUS_T QLIB_LMS_Attest_Sign(QLIB_CONTEXT_T*              qlibContext,
                                   const U8*                    msg,
                                   U32                          msgSize,
                                   const LMS_ATTEST_NONCE_T     nonce,
                                   LMS_ATTEST_CHUNK_T           pubCache[],
                                   U32                          pubCacheLen,
                                   QLIB_LMS_ATTEST_SIG_BUFFER_T signature,
                                   LMS_ATTEST_KEY_ID_T          wKeyId)
{
    QLIB_OTS_SIG_T sig;
    U32            lmotsParameterSetId = QLIB_LMOTS_SHA256_N24_W4_PARAM_SET_ID;
    U32            lmsParameterSetId   = QLIB_LMS_SHA256_M24_H10_PARAM_SET_ID;
    U8*            pSigBuffer          = signature;
    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_SUPPORT_LMS_ATTESTATION(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET(NULL != msg, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(0u < msgSize, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != nonce, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != signature, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != wKeyId, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(0u == pubCacheLen || NULL != pubCache, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(pubCacheLen <= QLIB_LMS_ATTEST_ALL_NODES, QLIB_STATUS__INVALID_PARAMETER);

    /********************************************************************************************************
     * Sign message
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_SEC_LMS_Attest_Sign(qlibContext, msg, msgSize, nonce, pubCache, pubCacheLen, &sig));

    /********************************************************************************************************
     * Serialize signature in output buffer
    ********************************************************************************************************/
    (void)memcpy(pSigBuffer, (const U8*)(&sig.leafNum), sizeof(U32));
    pSigBuffer += sizeof(U32);
    (void)memcpy(pSigBuffer, (const U8*)(&lmotsParameterSetId), sizeof(U32));
    pSigBuffer += sizeof(U32);
    (void)memcpy(pSigBuffer, nonce, sizeof(LMS_ATTEST_NONCE_T));
    pSigBuffer += sizeof(LMS_ATTEST_NONCE_T);
    (void)memcpy(pSigBuffer, sig.otsSig, sizeof(LMS_ATTEST_OTS_SIG_T));
    pSigBuffer += sizeof(LMS_ATTEST_OTS_SIG_T);
    (void)memcpy(pSigBuffer, (const U8*)(&lmsParameterSetId), sizeof(uint32_t));
    pSigBuffer += sizeof(U32);
    (void)memcpy(pSigBuffer, sig.path, sizeof(LMS_ATTEST_CHUNK_T) * QLIB_LMS_ATTEST_TREE_HEIGHT);

    (void)memcpy(wKeyId, sig.keyId, sizeof(LMS_ATTEST_KEY_ID_T));
    return QLIB_STATUS__OK;
}
#endif

QLIB_STATUS_T QLIB_IsKeyProvisioned(QLIB_CONTEXT_T* qlibContext, QLIB_KID_TYPE_T keyIdType, U32 sectionID, BOOL* isProvisioned)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;

    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(NULL != isProvisioned, QLIB_STATUS__INVALID_PARAMETER);

    if (W77Q_CMD_GET_KEYS_STATUS(qlibContext) != 0u)
    {
        status = QLIB_SEC_IsKeyProvisioned(qlibContext, keyIdType, sectionID, isProvisioned);
    }
    else
    {
        status = QLIB_STATUS__NOT_SUPPORTED;
    }
    return status;
}

#if !defined EXCLUDE_W77Q_ECC && !defined Q2_API
QLIB_STATUS_T QLIB_GetEccStatus(QLIB_CONTEXT_T* qlibContext, QLIB_ECC_STATUS_T* eccStatus, QLIB_ADVANCED_ECC_T* advancedEcc)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_ECC(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET(NULL != eccStatus, QLIB_STATUS__INVALID_PARAMETER);

    return QLIB_STD_GetEccStatus(qlibContext, eccStatus, advancedEcc);
}

QLIB_STATUS_T QLIB_ClearAECCR(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_ECC(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    return QLIB_CMD_PROC__clear_AECCR(qlibContext);
}
#endif

#if !defined EXCLUDE_W77Q_INTERRUPTS && !defined Q2_API
QLIB_STATUS_T QLIB_ConfigInterrupts(QLIB_CONTEXT_T* qlibContext, QLIB_INTERRUPT_T* intTypes, BOOL enable)
{
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_INTERRUPTS(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    return QLIB_STD_ConfigInterrupts(qlibContext, intTypes, enable);
}

QLIB_STATUS_T QLIB_ClearInterrupts(QLIB_CONTEXT_T* qlibContext, QLIB_INTERRUPT_T* intTypes)
{
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_INTERRUPTS(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    return QLIB_STD_ClearInterrupts(qlibContext, intTypes);
}

QLIB_STATUS_T QLIB_ReadInterruptsState(QLIB_CONTEXT_T* qlibContext, QLIB_INTERRUPT_T* intState)
{
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(W77Q_INTERRUPTS(qlibContext) != 0u, QLIB_STATUS__NOT_SUPPORTED);
    QLIB_ASSERT_RET(NULL != intState, QLIB_STATUS__INVALID_PARAMETER);
    return QLIB_STD_ReadInterruptState(qlibContext, intState);
}
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                             LOCAL FUNCTIONS                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

static QLIB_STATUS_T QLIB_ConfigDeviceSingleDie_L(QLIB_CONTEXT_T*             qlibContext,
                                                  QLIB_FLASH_CONFIG_T*        flashCfg,
                                                  QLIB_DIE_CONFIG_T*          dieCfg,
                                                  QLIB_SECTIONS_CONF_TABLE_T* cfgSectionArray)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    if (NULL != cfgSectionArray)
    {
        if (W77Q_VAULT(qlibContext) != 0u)
        {
            QLIB_ASSERT_RET(0u == cfgSectionArray->sectionConfigTable[QLIB_SECTION_ID_VAULT]
                                      .sectionConfig.policy.plainAccessReadEnable,
                            QLIB_STATUS__INVALID_PARAMETER);
            QLIB_ASSERT_RET(0u == cfgSectionArray->sectionConfigTable[QLIB_SECTION_ID_VAULT]
                                      .sectionConfig.policy.plainAccessWriteEnable,
                            QLIB_STATUS__INVALID_PARAMETER);
            QLIB_ASSERT_RET(0u == cfgSectionArray->sectionConfigTable[QLIB_SECTION_ID_VAULT].sectionConfig.policy.authPlainAccess,
                            QLIB_STATUS__INVALID_PARAMETER);
            QLIB_ASSERT_RET(cfgSectionArray->sectionConfigTable[QLIB_SECTION_ID_VAULT].size ==
                                QLIB_VAULT_CFG_TO_SIZE(
                                    QLIB_VAULT_SIZE_TO_CFG(cfgSectionArray->sectionConfigTable[QLIB_SECTION_ID_VAULT].size)),
                            QLIB_STATUS__INVALID_PARAMETER);
        }
        else
        {
            QLIB_ASSERT_RET(cfgSectionArray->sectionConfigTable[QLIB_SECTION_ID_VAULT].size == 0u,
                            QLIB_STATUS__INVALID_PARAMETER);
        }
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Configure the device                                                                                */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SEC_ConfigDevice(qlibContext, flashCfg, dieCfg, cfgSectionArray));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Re-sync the device                                                                                  */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SEC_SyncState(qlibContext));

    return QLIB_STATUS__OK;
}

static QLIB_STATUS_T QLIB_GetDeviceConfigSingleDie_L(QLIB_CONTEXT_T*             qlibContext,
                                                     QLIB_FLASH_CONFIG_T*        flashCfg,
                                                     QLIB_DIE_CONFIG_T*          dieCfg,
                                                     QLIB_SECTIONS_CONF_TABLE_T* cfgSectionArray)
{
    GMC_T     gmc;
    DEVCFG_T  devCfg = 0;
    AWDTCFG_T awdtDefault;
    BOOL      quadEnabled;
    U32       val;
    U8        sectionId;
    U8        rstPAField = 0u;
    BOOL      gmcConfigured;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    QLIB_ASSERT_RET(((QLIB_INIT_DIE_ID == qlibContext->activeDie) && (NULL != flashCfg)) ||
                        ((QLIB_INIT_DIE_ID != qlibContext->activeDie) && (NULL == flashCfg)),
                    QLIB_STATUS__INVALID_PARAMETER);

    if (NULL != flashCfg)
    {
        (void)memset(flashCfg, 0, sizeof(QLIB_FLASH_CONFIG_T));
    }
    if (NULL != dieCfg)
    {
        (void)memset(dieCfg, 0, sizeof(QLIB_DIE_CONFIG_T));
    }
    if (NULL != cfgSectionArray)
    {
        (void)memset(cfgSectionArray, 0, sizeof(QLIB_SECTIONS_CONF_TABLE_T));
    }

    QLIB_STATUS_RET_CHECK(QLIB_SEC__get_GMC(qlibContext, gmc));
    if (QLIB_SEC_FLASH_IS_AFTER_FORMAT(qlibContext, gmc))
    {
        return QLIB_GetDefaultDieConfig_L(qlibContext, flashCfg, dieCfg, cfgSectionArray);
    }

    devCfg        = QLIB_REG_GMC_GET_DEVCFG(gmc);
    gmcConfigured = (READ_VAR_FIELD(devCfg, QLIB_REG_DEVCFG__RESERVED_1) > 0u) ? FALSE : TRUE;
    if (NULL != flashCfg)
    {
        awdtDefault = QLIB_REG_GMC_GET_AWDT_DFLT(gmc);
        // Set watchdogDefault values
        if (gmcConfigured == FALSE)
        {
            // watchdog was not configured yet - return default reset values
            flashCfg->watchdogDefault.enable        = FALSE;
            flashCfg->watchdogDefault.lfOscEn       = FALSE;
            flashCfg->watchdogDefault.swResetEn     = FALSE;
            flashCfg->watchdogDefault.authenticated = FALSE;
            flashCfg->watchdogDefault.sectionID     = 0xFu;
            flashCfg->watchdogDefault.threshold     = QLIB_AWDT_TH_12_DAYS;
            flashCfg->watchdogDefault.lock          = FALSE;
            flashCfg->watchdogDefault.oscRateHz     = ((U32)QLIB_AWDTCFG__OSC_RATE_KHZ_DEFAULT << 10);
            flashCfg->watchdogDefault.fallbackEn    = FALSE;
        }
        else
        {
            flashCfg->watchdogDefault.enable        = INT_TO_BOOLEAN(READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__AWDT_EN));
            flashCfg->watchdogDefault.lfOscEn       = INT_TO_BOOLEAN(READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__LFOSC_EN));
            flashCfg->watchdogDefault.swResetEn     = INT_TO_BOOLEAN(READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__SRST_EN));
            flashCfg->watchdogDefault.authenticated = INT_TO_BOOLEAN(READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__AUTH_WDT));
            flashCfg->watchdogDefault.sectionID     = (U32)READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__KID);
            flashCfg->watchdogDefault.threshold =
                (QLIB_AWDT_TH_T)MIN((U32)READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__TH), (U32)QLIB_AWDT_TH_12_DAYS);
            flashCfg->watchdogDefault.lock      = INT_TO_BOOLEAN(READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__LOCK));
            flashCfg->watchdogDefault.oscRateHz = ((U32)READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__OSC_RATE_KHZ) << 10);
            if (W77Q_AWDTCFG_OSC_RATE_FRAC(qlibContext) != 0u)
            {
                flashCfg->watchdogDefault.oscRateHz += ((U32)READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__OSC_RATE_FRAC) << 6);
            }
            flashCfg->watchdogDefault.fallbackEn = INT_TO_BOOLEAN(READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__FB_EN));
        }
        // Set devCfg values
        if (NULL != dieCfg)
        {
            if ((0u == W77Q_RST_RESP(qlibContext)) || (0u == READ_VAR_FIELD(devCfg, QLIB_REG_DEVCFG__RST_RESP_EN)) ||
                (gmcConfigured == FALSE))
            {
                (void)memset(dieCfg->resetResp.response1, 0, sizeof(dieCfg->resetResp.response1));
                (void)memset(dieCfg->resetResp.response2, 0, sizeof(dieCfg->resetResp.response2));
            }
            else
            {
                QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_RST_RESP(qlibContext, dieCfg->resetResp.response1));
            }
            if (gmcConfigured == FALSE)
            {
                dieCfg->safeFB                      = FALSE;
                dieCfg->speculCK                    = TRUE;
                dieCfg->nonSecureFormatEn           = TRUE;
                dieCfg->devcfgLock                  = FALSE;
                flashCfg->pinMux.dedicatedResetInEn = TRUE;
            }
            else
            {
                dieCfg->safeFB                      = INT_TO_BOOLEAN(READ_VAR_FIELD(devCfg, QLIB_REG_DEVCFG__FB_EN));
                dieCfg->speculCK                    = INT_TO_BOOLEAN(READ_VAR_FIELD(devCfg, QLIB_REG_DEVCFG__CK_SPECUL));
                dieCfg->nonSecureFormatEn           = INT_TO_BOOLEAN(READ_VAR_FIELD(devCfg, QLIB_REG_DEVCFG__FORMAT_EN));
                dieCfg->devcfgLock                  = (W77Q_DEVCFG_LOCK(qlibContext) != 0u)
                                                          ? INT_TO_BOOLEAN(READ_VAR_FIELD(devCfg, QLIB_REG_DEVCFG__CFG_LOCK))
                                                          : FALSE;
                flashCfg->pinMux.dedicatedResetInEn = INT_TO_BOOLEAN(READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__RST_IN_EN));
            }
        }

        QLIB_STATUS_RET_CHECK(QLIB_STD_GetQuadEnable(qlibContext, &quadEnabled));
        flashCfg->pinMux.io23Mux = QLIB_IO23_MODE__NONE;
        if (quadEnabled == TRUE)
        {
            if ((gmcConfigured == FALSE) || ((READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__RSTI_OVRD) == 0u) &&
                                             (READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__RSTO_EN) == 0u)))
            {
                flashCfg->pinMux.io23Mux = QLIB_IO23_MODE__QUAD;
            }
        }
        else
        { // quadEnable == FALSE
            if ((gmcConfigured == FALSE) || ((READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__RSTI_OVRD) == 0u) &&
                                             (READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__RSTO_EN) == 0u)))
            {
                flashCfg->pinMux.io23Mux = QLIB_IO23_MODE__LEGACY_WP_HOLD;
            }
            else
            {
                if (
                    (READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__RSTI_OVRD) == 1u) &&
                    (READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__RSTO_EN) == 1u) &&
                    (READ_VAR_FIELD(awdtDefault, QLIB_REG_AWDTCFG__RSTI_EN) == 1u))
                {
                    flashCfg->pinMux.io23Mux = QLIB_IO23_MODE__RESET_IN_OUT;
                }
            }
        }

        if (NULL != dieCfg)
        {
            dieCfg->bootFailReset = (W77Q_DEVCFG_BOOT_FAIL_RST(qlibContext) != 0u)
                                        ? INT_TO_BOOLEAN(READ_VAR_FIELD(devCfg, QLIB_REG_DEVCFG__BOOT_FAIL_RST))
                                        : FALSE;
        }

        if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
        {
            QLIB_STATUS_RET_CHECK(QLIB_STD_getPowerUpDummyCycles(qlibContext, &flashCfg->fastReadDummyCycles));
        }

        if (gmcConfigured == TRUE)
        {
            val = (U32)(READ_VAR_FIELD(devCfg, QLIB_REG_DEVCFG__SECT_SEL) + (U32)LOG2(QLIB_MIN_SECTION_SIZE));
            QLIB_ASSERT_RET(val >= (U32)QLIB_STD_ADDR_LEN__22_BIT && val < (U32)QLIB_STD_ADDR_LEN__LAST,
                            QLIB_STATUS__INVALID_PARAMETER);
            flashCfg->stdAddrSize.addrLen = (QLIB_STD_ADDR_LEN_T)val;
        }
        else
        {
            flashCfg->stdAddrSize.addrLen = QLIB_STD_ADDR_LEN__25_BIT;
        }

        QLIB_STATUS_RET_CHECK(QLIB_STD_GetPowerUpAddressMode(qlibContext, &flashCfg->stdAddrSize.addrMode));

        if (NULL != dieCfg)
        {
            dieCfg->rngPAEn = (W77Q_RNG_FEATURE(qlibContext) != 0u)
                                  ? INT_TO_BOOLEAN(READ_VAR_FIELD(devCfg, QLIB_REG_DEVCFG__RNG_PA_EN))
                                  : FALSE;
        }
    }
    if (NULL != dieCfg)
    {
        dieCfg->ctagModeMulti = ((Q2_DEVCFG_CTAG_MODE(qlibContext) != 0u) && (gmcConfigured == TRUE))
                                    ? INT_TO_BOOLEAN(READ_VAR_FIELD(devCfg, QLIB_REG_DEVCFG__CTAG_MODE))
                                    : FALSE;
    }

    rstPAField = (W77Q_RST_PA(qlibContext) != 0u)
                     ? ((gmcConfigured == TRUE) ? (U8)READ_VAR_FIELD(devCfg, QLIB_REG_DEVCFG__RST_PA) : 3u)
                     : 1u;

    if (NULL != cfgSectionArray)
    {
        for (sectionId = 0; sectionId < QLIB_NUM_OF_SECTIONS; sectionId++)
        {
            if ((W77Q_VAULT(qlibContext) == 0u) && (sectionId == QLIB_SECTION_ID_VAULT))
            {
                // Vault is disabled, skip it
                continue;
            }
            QLIB_STATUS_RET_CHECK(
                QLIB_GetSectionConfiguration(qlibContext,
                                             sectionId,
                                             &cfgSectionArray->sectionConfigTable[sectionId].baseAddr,
                                             &cfgSectionArray->sectionConfigTable[sectionId].size,
                                             &cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.policy,
                                             &cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.digest,
                                             &cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.crc,
                                             &cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.version));

            cfgSectionArray->sectionConfigTable[sectionId].resetPA = INT_TO_BOOLEAN(rstPAField & (1u << sectionId));
#if QLIB_NUM_OF_DIES > 1
            if (qlibContext->activeDie >= QLIB_GET_NUM_OF_DIES(qlibContext))
            {
                cfgSectionArray->sectionConfigTable[sectionId].resetPA = 0u;
            }
#endif
        }
    }

    return QLIB_STATUS__OK;
}

// Update default values according to Quartz 2 & 3 documentation
static QLIB_STATUS_T QLIB_GetDefaultDieConfig_L(QLIB_CONTEXT_T*             qlibContext,
                                                QLIB_FLASH_CONFIG_T*        flashCfg,
                                                QLIB_DIE_CONFIG_T*          dieCfg,
                                                QLIB_SECTIONS_CONF_TABLE_T* cfgSectionArray)
{
    if (NULL != flashCfg)
    {
        (void)memset(flashCfg, 0, sizeof(QLIB_FLASH_CONFIG_T));

        flashCfg->watchdogDefault.enable        = 0;
        flashCfg->watchdogDefault.lfOscEn       = 0;
        flashCfg->watchdogDefault.swResetEn     = 0;
        flashCfg->watchdogDefault.authenticated = 0;
        flashCfg->watchdogDefault.sectionID     = 0xf;
        flashCfg->watchdogDefault.threshold     = QLIB_AWDT_TH_12_DAYS;
        flashCfg->watchdogDefault.lock          = 0;
        flashCfg->watchdogDefault.oscRateHz     = ((U32)0x36u) << 10u;
        flashCfg->watchdogDefault.fallbackEn    = 0;
        flashCfg->pinMux.io23Mux =
            (W77Q_EXTENDED_CONFIG_REGISTER(qlibContext) != 0u) ? QLIB_IO23_MODE__QUAD : QLIB_IO23_MODE__LEGACY_WP_HOLD;
        flashCfg->pinMux.dedicatedResetInEn = (W77Q_DEDICATED_RESET_INPUT_ENABLE_DEFAULT(qlibContext) == 1u) ? TRUE : FALSE;
        flashCfg->stdAddrSize.addrLen       = QLIB_STD_ADDR_LEN__25_BIT;
        flashCfg->stdAddrSize.addrMode      = QLIB_STD_ADDR_MODE__3_BYTE;
        flashCfg->fastReadDummyCycles       = 8;
        flashCfg->dqsDisable                = FALSE;
    }

    if (NULL != dieCfg)
    {
        (void)memset(dieCfg, 0, sizeof(QLIB_DIE_CONFIG_T));

        // dieCfg->suid      // undefined in spec
        // dieCfg->resetResp // already set to all zeros
        dieCfg->safeFB            = 0;
        dieCfg->speculCK          = 1;
        dieCfg->nonSecureFormatEn = 1;
        dieCfg->rngPAEn           = 1;
        dieCfg->ctagModeMulti     = 0;
        dieCfg->devcfgLock        = 0;
        dieCfg->bootFailReset     = (W77Q_BOOT_FAIL_RESET(qlibContext) != 0u) ? 1 : 0;
    }

    if (NULL != cfgSectionArray)
    {
        U8 sectionId;

        (void)memset(cfgSectionArray, 0, sizeof(QLIB_SECTIONS_CONF_TABLE_T));

        for (sectionId = 0; sectionId < QLIB_NUM_OF_SECTIONS; sectionId++)
        {
            if ((W77Q_VAULT(qlibContext) == 0u) && (sectionId == QLIB_SECTION_ID_VAULT))
            {
                // Vault is disabled, skip it
                continue;
            }

            cfgSectionArray->sectionConfigTable[sectionId].baseAddr                                     = 0;
            cfgSectionArray->sectionConfigTable[sectionId].size                                         = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.policy.digestIntegrity         = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.policy.checksumIntegrity       = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.policy.writeProt               = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.policy.rollbackProt            = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.policy.plainAccessWriteEnable  = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.policy.plainAccessReadEnable   = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.policy.authPlainAccess         = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.policy.digestIntegrityOnAccess = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.policy.slog                    = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.crc                            = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.digest                         = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.version                        = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.postActions.swap               = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.postActions.reload             = 0;
            cfgSectionArray->sectionConfigTable[sectionId].sectionConfig.postActions.reset              = 0;
            cfgSectionArray->sectionConfigTable[sectionId].resetPA = (0u == sectionId) ? 1 : 0;
        }
        if (W77Q_VAULT(qlibContext) == 1u)
        {
            cfgSectionArray->sectionConfigTable[QLIB_SECTION_ID_VAULT].baseAddr = MAX_U32;
            cfgSectionArray->sectionConfigTable[QLIB_SECTION_ID_VAULT].size =
                QLIB_VAULT_CFG_TO_SIZE(QLIB_VAULT_128KB_RPMC_DISABLED);
        }
    }

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
* @brief       This routine waits till flash is ready and detects and sets the bus mode (SPI/QPI) and flash device ID
*
* @param       qlibContext   qlib context object
*
* @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_waitReadyAndInitBusMode_L(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_BUS_FORMAT_T currentFormat;

    // identify and set current bus mode
    QLIB_STATUS_RET_CHECK(QLIB_STD_WaitTillFlashIsReady(qlibContext, &currentFormat, &qlibContext->detectedDeviceID));

    // Set bus mode
    qlibContext->busInterface.busMode = QLIB_BUS_FORMAT_GET_MODE(currentFormat);
    qlibContext->busInterface.dtr     = QLIB_BUS_FORMAT_GET_DTR(currentFormat);
    QLIB_STATUS_RET_CHECK(QLIB_SetInterface(qlibContext, currentFormat));

    // Clear errors in SSR caused by AutoSense
    QLIB_STATUS_RET_CHECK(QLIB_SEC_ClearSSR(qlibContext));

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
* @brief       This routine loads the ACL from Section Security Policy Register (SSPR), affecting the 
*              plain access runtime privileges of the Section
*
* @param       qlibContext   qlib context object
* @param       sectionID     Section ID
* @param       condition     load ACL only if the section is with specific authentication or access. fail otherwise
*
* @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_PlainAccessGrant_L(QLIB_CONTEXT_T* qlibContext, U32 sectionID, QLIB_LOAD_ACLR_T condition)
{
    QLIB_POLICY_T policy = {0};
    U32           sectionSize;
    QLIB_STATUS_T ret;

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(QLIB_SECTION_ID_VAULT > sectionID, QLIB_STATUS__INVALID_PARAMETER);

    /********************************************************************************************************
     * check if the section is authenticated plain access
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_GetSectionConfiguration(qlibContext, sectionID, NULL, &sectionSize, &policy, NULL, NULL, NULL));
    QLIB_ASSERT_RET(sectionSize != 0u, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE); // section is disabled

    QLIB_ASSERT_RET((policy.plainAccessWriteEnable == 1u) || (((U8)condition & (U8)QLIB_LOAD_ACLR_PLAIN_WR) == 0u),
                    QLIB_STATUS__DEVICE_PRIVILEGE_ERR);
    QLIB_ASSERT_RET((policy.plainAccessReadEnable == 1u) || (((U8)condition & (U8)QLIB_LOAD_ACLR_PLAIN_RD) == 0u),
                    QLIB_STATUS__DEVICE_PRIVILEGE_ERR);

    if (policy.authPlainAccess == 1u)
    {
        QLIB_ASSERT_RET(((U8)condition & (U8)QLIB_LOAD_ACLR_NON_AUTH) == 0u, QLIB_STATUS__DEVICE_PRIVILEGE_ERR);
        ret = QLIB_SEC_AuthPlainAccess_Grant(qlibContext, sectionID);
    }
    else
    {
        ret = QLIB_SEC_EnablePlainAccess(qlibContext, sectionID);
    }

    /*---------------------------------------------------------------------------------------------*/
    /* Mark plain access is enabled                                                                */
    /*---------------------------------------------------------------------------------------------*/
    if (QLIB_STATUS__OK == ret || QLIB_STATUS__DEVICE_INTEGRITY_ERR == ret)
    {
        QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[sectionID].plainEnabled = QLIB_SECTION_PLAIN_EN_NO;
        if (policy.plainAccessWriteEnable == 1u)
        {
            QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[sectionID].plainEnabled |= QLIB_SECTION_PLAIN_EN_WR;
        }
        if (policy.plainAccessReadEnable == 1u && QLIB_STATUS__DEVICE_INTEGRITY_ERR != ret)
        {
            QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[sectionID].plainEnabled |= QLIB_SECTION_PLAIN_EN_RD;
        }
    }
    QLIB_STATUS_RET_CHECK(ret);
    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This function translates the flash HW version to qlib target as defined in @ref qlib_targets.h
 *
 * @param[in]   hwVer        Hardware version information
 * @param[out]  target       The detected flash target
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_GetTargetFlash_L(QLIB_HW_VER_T* hwVer, U32* target)
{
    if ((hwVer->sec.securityVersion & 0xF0u) == 0x20u)
    {
        // W77Q (16Mb to 128Mb) flash
        if (hwVer->sec.securityVersion == 0x20u)
        {
            // W77Q (16Mb to 128Mb) MCD (32Mb)
            if (hwVer->info.revision == QLIB_TARGET_REVISION_B || hwVer->info.revision == QLIB_TARGET_REVISION_C)
            {
                *target = w77q32jw_revB;
            }
            else
            {
                return QLIB_STATUS__NOT_SUPPORTED;
            }
        }
        else if ((hwVer->sec.securityVersion == 0x24u) || (hwVer->sec.securityVersion == 0x28u))
        {
            // W77Q (16Mb to 128Mb) HCD (64Mb/128Mb)
            if (hwVer->info.voltage == QLIB_TARGET_VOLTAGE_1_8V)
            {
                if (hwVer->info.revision == QLIB_TARGET_REVISION_A)
                {
                    *target = (hwVer->info.flashSize == QLIB_TARGET_SIZE_64Mb ? w77q64jw_revA : w77q128jw_revA);
                }
                else
                {
                    return QLIB_STATUS__NOT_SUPPORTED;
                }
            }
            else
            {
                // QLIB_TARGET_VOLTAGE_3_3V
                if (hwVer->info.revision == QLIB_TARGET_REVISION_A)
                {
                    *target = (hwVer->info.flashSize == QLIB_TARGET_SIZE_64Mb ? w77q64jv_revA : w77q128jv_revA);
                }
                else
                {
                    return QLIB_STATUS__NOT_SUPPORTED;
                }
            }
        }
        else
        {
            return QLIB_STATUS__NOT_SUPPORTED;
        }
    }
    else if ((hwVer->sec.securityVersion & 0xF0u) == 0x30u)
    {
        // W77Q/T (256Mb to 1Gb) chip
        QLIB_ASSERT_RET(hwVer->info.revision == QLIB_TARGET_REVISION_B, QLIB_STATUS__NOT_SUPPORTED);
        if (hwVer->sec.securityVersion == 0x30u)
        {
            // W77Q/T (256Mb to 1Gb) chip W77Q/T industrial
            if (hwVer->std.memoryType == 0x8Eu)
            {
                // W77T : Octal SPI secure flash
                *target = w77t25nw_Ind_revB;
            }
            else
            {
                // W77Q: Quad SPI secure flash
                *target = w77q25nw_Ind_revB;
            }
        }
        else if (hwVer->sec.securityVersion == 0x34u)
        {
            // W77Q/T (256Mb to 1Gb) chip W77Q/T automotive
            if (hwVer->std.memoryType == 0x8Eu)
            {
                // W77T : Octal SPI secure flash
                *target = w77t25nw_Auto_revB;
            }
            else
            {
                // W77Q: Quad SPI secure flash
                *target = w77q25nw_Auto_revB;
            }
        }
        else
        {
            return QLIB_STATUS__NOT_SUPPORTED;
        }
    }
    else
    {
        return QLIB_STATUS__NOT_SUPPORTED;
    }

    return QLIB_STATUS__OK;
}

static QLIB_STATUS_T QLIB_SetInterface_L(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T busFormat, BOOL configFlash)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(NULL != qlibContext, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(QLIB_DEVICE_INITIALIZED(qlibContext), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_ASSERT_RET(QLIB_BUS_FORMAT_GET_MODE(busFormat) > QLIB_BUS_MODE_INVALID, QLIB_STATUS__INVALID_PARAMETER);
#ifdef Q2_API
    QLIB_ASSERT_RET(QLIB_BUS_FORMAT_GET_MODE(busFormat) <= QLIB_BUS_MODE_4_4_4, QLIB_STATUS__INVALID_PARAMETER);
#else
    QLIB_ASSERT_RET(QLIB_BUS_FORMAT_GET_MODE(busFormat) <= QLIB_BUS_MODE_MAX, QLIB_STATUS__INVALID_PARAMETER);
#endif
    QLIB_ASSERT_RET((W77Q_SUPPORT_DUAL_SPI(qlibContext) != 0u) || (QLIB_BUS_FORMAT_GET_MODE(busFormat) != QLIB_BUS_MODE_1_1_2 &&
                                                                   QLIB_BUS_FORMAT_GET_MODE(busFormat) != QLIB_BUS_MODE_1_2_2),
                    QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET((W77Q_SUPPORT_OCTAL_SPI(qlibContext) != 0u) || (QLIB_BUS_FORMAT_GET_MODE(busFormat) != QLIB_BUS_MODE_1_8_8 &&
                                                                    QLIB_BUS_FORMAT_GET_MODE(busFormat) != QLIB_BUS_MODE_8_8_8),
                    QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET((W77Q_SUPPORT__1_1_4_SPI(qlibContext) != 0u) || (QLIB_BUS_FORMAT_GET_MODE(busFormat) != QLIB_BUS_MODE_1_1_4),
                    QLIB_STATUS__INVALID_PARAMETER);
#ifndef QLIB_SUPPORT_QPI
    QLIB_ASSERT_RET(QLIB_BUS_FORMAT_GET_MODE(busFormat) != QLIB_BUS_MODE_4_4_4, QLIB_STATUS__NOT_SUPPORTED);
#endif
#ifndef QLIB_SUPPORT_OPI
    QLIB_ASSERT_RET(QLIB_BUS_FORMAT_GET_MODE(busFormat) != QLIB_BUS_MODE_8_8_8, QLIB_STATUS__NOT_SUPPORTED);
#endif

    /*-----------------------------------------------------------------------------------------------------*/
    /* Change the bus interface                                                                            */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_SetInterface(qlibContext, busFormat, configFlash));
    QLIB_STATUS_RET_CHECK(QLIB_SEC_SetInterface(qlibContext, busFormat));

    return QLIB_STATUS__OK;
}

#if !defined EXCLUDE_Q2_4_BYTES_ADDRESS_MODE || !defined EXCLUDE_FAST_READ_DUMMY_CONFIG
/************************************************************************************************************
* @brief       This routine reads the SPI volatile parameters from flash and updates qlib context
*
* @param       qlibContext   qlib context object
*
* @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_SyncSPI_L(QLIB_CONTEXT_T* qlibContext)
{
    if (Q2_4_BYTES_ADDRESS_MODE(qlibContext) != 0u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_STD_GetAddressMode(qlibContext, &qlibContext->addrMode));
    }

    if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_STD_GetFastReadDummyCycles(qlibContext, &qlibContext->fastReadDummy));
    }

    return QLIB_STATUS__OK;
}
#endif
