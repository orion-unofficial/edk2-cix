/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_std.c
* @brief      This file contains QLIB standard flash interface
*
* ### project qlib
*
************************************************************************************************************/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib_std.h"

#ifdef QLIB_SUPPORT_QPI
#include "qlib_sec.h"
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                              DEFINITIONS                                                */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

#define AUTOSENSE_NUM_RETRIES 4

#define Q2_MAX_SPI_FREQUENCY_IN_MHz     (133)
#define Q2_MIN_JEDEC_ID_CYCLES          (7) // num cycles for read JEDEC command in QPI mode
#define Q2_MIN_POWER_UP_CYCLES          (2) // num cycles for release power down command in QPI mode
#define Q2_MIN_READY_TEST_CYCLES        (Q2_MIN_JEDEC_ID_CYCLES + Q2_MIN_POWER_UP_CYCLES)
#define Q2_MAX_FLASH_BOOT_TIME_MICROSEC (100)
#define Q2_MAX_FLASH_BOOT_CYCLES        (Q2_MAX_SPI_FREQUENCY_IN_MHz * Q2_MAX_FLASH_BOOT_TIME_MICROSEC)
#define Q2_READY_TEST_NUM_RETRIES       (Q2_MAX_FLASH_BOOT_CYCLES / Q2_MIN_READY_TEST_CYCLES)

#define Q3_MAX_SPI_FREQUENCY_IN_MHz (166)

#define Q3_MIN_JEDEC_ID_CYCLES   (11) // num cycles for read JEDEC command in DOPI mode
#define Q3_MIN_POWER_UP_CYCLES   (1)  // num cycles for release power down command in DOPI mode
#define Q3_MIN_READY_TEST_CYCLES (Q3_MIN_JEDEC_ID_CYCLES + Q3_MIN_POWER_UP_CYCLES)

#define Q3_MAX_FLASH_BOOT_TIME_MICROSEC (100)
#define Q3_MAX_FLASH_BOOT_CYCLES        (Q3_MAX_SPI_FREQUENCY_IN_MHz * Q3_MAX_FLASH_BOOT_TIME_MICROSEC)
#define Q3_READY_TEST_NUM_RETRIES       (Q3_MAX_FLASH_BOOT_CYCLES / Q3_MIN_READY_TEST_CYCLES)

#define READY_TEST_NUM_RETRIES MAX(Q2_READY_TEST_NUM_RETRIES, Q3_READY_TEST_NUM_RETRIES)

#define Q3_QPI_POWER_UP_GET_ID_CYCLES (12)
#define Q3_QPI_POWER_UP_NUM_RETRIES   (Q3_MAX_FLASH_BOOT_CYCLES / Q3_QPI_POWER_UP_GET_ID_CYCLES)
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                             LOCAL FUNCTIONS                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
static QLIB_STATUS_T QLIB_STD_GetStatus_L(QLIB_CONTEXT_T* qlibContext, STD_FLASH_STATUS_T* status);
static QLIB_STATUS_T QLIB_STD_execute_std_cmd_L(QLIB_CONTEXT_T* qlibContext,
                                                QLIB_BUS_MODE_T format,
                                                BOOL            dtr,
                                                BOOL            needWriteEnable,
                                                BOOL            waitWhileBusy,
                                                U8              cmd,
                                                const U32*      address,
                                                const U8*       writeData,
                                                U32             writeDataSize,
                                                U32             dummyCycles,
                                                U8*             readData,
                                                U32             readDataSize,
                                                QLIB_REG_SSR_T* ssr) __RAM_SECTION;

static QLIB_STATUS_T QLIB_STD_SetQuadEnableInCR_L(QLIB_CONTEXT_T* qlibContext, BOOL enable);
static QLIB_STATUS_T QLIB_STD_GetQuadEnableFromCR_L(QLIB_CONTEXT_T* qlibContext, BOOL* enabled);
static QLIB_STATUS_T QLIB_STD_SetQuadEnableInSR_L(QLIB_CONTEXT_T* qlibContext, BOOL enable);
static QLIB_STATUS_T QLIB_STD_GetQuadEnableFromSR_L(QLIB_CONTEXT_T* qlibContext, BOOL* enabled);

static QLIB_STATUS_T QLIB_STD_SetStatus_L(QLIB_CONTEXT_T*     qlibContext,
                                          STD_FLASH_STATUS_T  statusIn,
                                          STD_FLASH_STATUS_T* statusOut);
static QLIB_STATUS_T QLIB_STD_PageProgram_L(QLIB_CONTEXT_T* qlibContext,
                                            const U8*       input,
                                            U32             logicalAddr,
                                            U32             size,
                                            BOOL            blocking);
static U8            QLIB_STD_GetReadCMD_L(QLIB_CONTEXT_T* qlibContext, U32* dummyCycles, QLIB_BUS_MODE_T* format);
static U8            QLIB_STD_GetWriteCMD_L(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_MODE_T* format);
static U8 QLIB_STD_GetReadDummyCyclesCMD_L(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_MODE_T busMode, BOOL dtr, U32* dummyCycles);
#ifdef QLIB_SUPPORT_QPI
static QLIB_STATUS_T QLIB_STD_CheckWritePrivilege_L(QLIB_CONTEXT_T* qlibContext, U32 logicalAddr);
#endif // QLIB_SUPPORT_QPI

#if !defined QLIB_INIT_AFTER_FLASH_POWER_UP && !defined QLIB_INIT_BUS_FORMAT
static QLIB_STATUS_T QLIB_STD_AutoSense_L(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T* busFormat, U8* deviceId);
#endif
static QLIB_STATUS_T QLIB_STD_AutosenseIsBusModeActive_L(QLIB_CONTEXT_T*   qlibContext,
                                                         QLIB_BUS_FORMAT_T busFormat,
                                                         BOOL*             isActive,
                                                         U8*               deviceId);
static QLIB_STATUS_T QLIB_STD_set_default_CR_L(QLIB_CONTEXT_T* qlibContext, U32 addr, U8 cr);

static QLIB_STATUS_T QLIB_STD_PrepareForResetFlash_L(QLIB_CONTEXT_T* qlibContext, BOOL forceReset);
static QLIB_STATUS_T QLIB_STD_ResetFlash_L(QLIB_CONTEXT_T* qlibContext) __RAM_SECTION;
#if defined QLIB_SUPPORT_QPI || defined QLIB_SUPPORT_OPI
static QLIB_STATUS_T QLIB_STD_SwitchFlashBusMode_L(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T busFormat) __RAM_SECTION;
#endif
#if !defined EXCLUDE_W77Q_INTERRUPTS && !defined Q2_API
U8                                               interruptsToCR_L(QLIB_INTERRUPT_T* intTypes);
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_STD_InitLib(QLIB_CONTEXT_T* qlibContext)
{
    TOUCH(qlibContext);

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine executes standard command using TM interface
 *
 * @param[in]   qlibContext       pointer to qlib context
 * @param[in]   format            SPI transaction format
 * @param[in]   dtr               If TRUE, SPI transaction is using DTR mode
 * @param[in]   needWriteEnable   If TRUE, WriteEnable command is sent prior to the given command
 * @param[in]   waitWhileBusy     If TRUE, busy-wait polling is performed after exec. of command
 * @param[in]   cmd               Command value
 * @param[in]   address           Pointer to address value or NULL if no address available
 * @param[in]   writeData         Pointer to output data or NULL if no output data available
 * @param[in]   writeDataSize     Size of the output data
 * @param[in]   dummyCycles       Delay Cycles between output and input command phases
 * @param[out]  readData          Pointer to input data or NULL if no input data required
 * @param[in]   readDataSize      Size of the input data
 * @param[out]  ssr              Pointer to status register following the transaction
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_STD_execute_std_cmd_L(QLIB_CONTEXT_T* qlibContext,
                                                QLIB_BUS_MODE_T format,
                                                BOOL            dtr,
                                                BOOL            needWriteEnable,
                                                BOOL            waitWhileBusy,
                                                U8              cmd,
                                                const U32*      address,
                                                const U8*       writeData,
                                                U32             writeDataSize,
                                                U32             dummyCycles,
                                                U8*             readData,
                                                U32             readDataSize,
                                                QLIB_REG_SSR_T* ssr)
{
    U32 ssrTemp = 0;

    /*-----------------------------------------------------------------------------------------------------*/
    /* No commands allowed in powered-down state, except of SPI_FLASH_CMD__RELEASE_POWER_DOWN              */
    /*-----------------------------------------------------------------------------------------------------*/
    if ((QLIB_ACTIVE_DIE_STATE(qlibContext).isPoweredDown == 1u) && (cmd != (U8)SPI_FLASH_CMD__RELEASE_POWER_DOWN))
    {
        SET_VAR_FIELD_32(ssrTemp, QLIB_REG_SSR__IGNORE_ERR_S, 1u);
        SET_VAR_FIELD_32(ssrTemp, QLIB_REG_SSR__ERR, 1u);
        if (NULL != ssr)
        {
            ssr->asUint = ssrTemp;
        }
        return QLIB_STATUS__COMMAND_IGNORED;
    }

    QLIB_STATUS_RET_CHECK(QLIB_TM_Standard(qlibContext,
                                           QLIB_BUS_FORMAT(format, dtr),
                                           needWriteEnable,
                                           waitWhileBusy,
                                           cmd,
                                           address,
                                           writeData,
                                           writeDataSize,
                                           dummyCycles,
                                           readData,
                                           readDataSize,
                                           ssr));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_WaitTillFlashIsReady(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T* currentFormat, U8* deviceId)
{
    U32 i = 0;
#if defined QLIB_INIT_AFTER_FLASH_POWER_UP || defined QLIB_INIT_BUS_FORMAT
    BOOL            isAlive = FALSE;
    QLIB_BUS_MODE_T busMode;
#ifdef QLIB_INIT_BUS_FORMAT
    *currentFormat = (QLIB_BUS_FORMAT_T)QLIB_INIT_BUS_FORMAT;
#else
    // When flash powers-up, it is in SPI mode. For initialization stage use single SPI
    *currentFormat = QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE);
#endif
    busMode = QLIB_BUS_FORMAT_GET_MODE(*currentFormat);
    if ((QLIB_BUS_MODE_4_4_4 != busMode) && (QLIB_BUS_MODE_8_8_8 != busMode))
    {
        busMode = QLIB_BUS_MODE_1_1_1;
    }
    while ((isAlive == FALSE) && (i <= (U32)READY_TEST_NUM_RETRIES))
    {
        i++;
        QLIB_STATUS_RET_CHECK(QLIB_STD_AutosenseIsBusModeActive_L(qlibContext,
                                                                  QLIB_BUS_FORMAT(busMode,
                                                                                  (busMode == QLIB_BUS_MODE_8_8_8)
                                                                                      ? QLIB_BUS_FORMAT_GET_DTR(*currentFormat)
                                                                                      : FALSE),
                                                                  &isAlive,
                                                                  deviceId));
    }
    QLIB_ASSERT_RET(isAlive == TRUE, QLIB_STATUS__CONNECTIVITY_ERR);
#else
    QLIB_STATUS_T status;
    do
    {
        status = QLIB_STD_AutoSense_L(qlibContext, currentFormat, deviceId);
        i++;
    } while ((status == QLIB_STATUS__CONNECTIVITY_ERR) && (i <= (U32)READY_TEST_NUM_RETRIES));

    QLIB_STATUS_RET_CHECK(status);
#endif

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_SetInterface(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T busFormat, BOOL configFlash)
{
#if defined QLIB_SUPPORT_QPI || defined QLIB_SUPPORT_OPI
    if (configFlash == TRUE)
    {
        QLIB_STATUS_RET_CHECK(QLIB_STD_SwitchFlashBusMode_L(qlibContext, busFormat));
    }
#else
    TOUCH(configFlash);
#endif

    /*-----------------------------------------------------------------------------------------------------*/
    /* update rest of the globals                                                                          */
    /*-----------------------------------------------------------------------------------------------------*/
    qlibContext->busInterface.busMode = QLIB_BUS_FORMAT_GET_MODE(busFormat);
    qlibContext->busInterface.dtr     = QLIB_BUS_FORMAT_GET_DTR(busFormat);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_SetQuadEnable(QLIB_CONTEXT_T* qlibContext, BOOL enable)
{
    if (W77Q_EXTENDED_CONFIG_REGISTER(qlibContext) != 0u)
    {
        return QLIB_STD_SetQuadEnableInCR_L(qlibContext, enable);
    }
    else
    {
        return QLIB_STD_SetQuadEnableInSR_L(qlibContext, enable);
    }
}

QLIB_STATUS_T QLIB_STD_GetQuadEnable(QLIB_CONTEXT_T* qlibContext, BOOL* enabled)
{
    if (W77Q_EXTENDED_CONFIG_REGISTER(qlibContext) != 0u)
    {
        return QLIB_STD_GetQuadEnableFromCR_L(qlibContext, enabled);
    }
    else
    {
        return QLIB_STD_GetQuadEnableFromSR_L(qlibContext, enabled);
    }
}

QLIB_STATUS_T QLIB_STD_SetResetInEnable(QLIB_CONTEXT_T* qlibContext, BOOL enable)
{
    STD_FLASH_STATUS_T status;
    STD_FLASH_STATUS_T statusCheck;
    U32                resetInEn = (U32)((TRUE == enable) ? 1u : 0u);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Read the status registers                                                                           */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_GetStatus_L(qlibContext, &status));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set the reset in enable bit                                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    SET_VAR_FIELD_8(status.SR3.asUint, SPI_FLASH__STATUS_3_FIELD__HOLD_RST, resetInEn);
    QLIB_STATUS_RET_CHECK(QLIB_STD_SetStatus_L(qlibContext, status, &statusCheck));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Check that the command succeed                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(READ_VAR_FIELD((U32)statusCheck.SR3.asUint, SPI_FLASH__STATUS_3_FIELD__HOLD_RST) == resetInEn,
                    QLIB_STATUS__COMMAND_IGNORED);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_Read(QLIB_CONTEXT_T* qlibContext, U8* output, U32 logicalAddr, U32 size)
{
    U8              readCMD     = 0;
    U32             dummyCycles = 0;
    QLIB_BUS_MODE_T format      = QLIB_BUS_MODE_INVALID;
    U8              mode;
    U8*             writeData     = NULL;
    U32             writeDataSize = 0;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(output != NULL, QLIB_STATUS__INVALID_PARAMETER);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Get read command                                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    readCMD = QLIB_STD_GetReadCMD_L(qlibContext, &dummyCycles, &format);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform read                                                                                        */
    /*-----------------------------------------------------------------------------------------------------*/
    if (SPI_FLASH_CMD_FAST_READ__MODE_EXIST(qlibContext, readCMD))
    {
        mode          = SPI_FLASH_CMD_FAST_READ__MODE_BYTE;
        writeData     = &mode;
        writeDataSize = 1u;
    }
    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     format,
                                                     qlibContext->busInterface.dtr,
                                                     FALSE,
                                                     TRUE,
                                                     readCMD,
                                                     &logicalAddr,
                                                     writeData,
                                                     writeDataSize,
                                                     dummyCycles,
                                                     output,
                                                     size,
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_Write(QLIB_CONTEXT_T* qlibContext, const U8* input, U32 logicalAddr, U32 size)
{
    U32 size_tmp = 0;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(input != NULL, QLIB_STATUS__INVALID_PARAMETER);

    while (0u < size)
    {
        /*-------------------------------------------------------------------------------------------------*/
        /* Calculate the iteration size                                                                    */
        /*-------------------------------------------------------------------------------------------------*/
        size_tmp = FLASH_PAGE_SIZE - (logicalAddr % FLASH_PAGE_SIZE);
        if (size < size_tmp)
        {
            size_tmp = size;
        }

        /*-------------------------------------------------------------------------------------------------*/
        /* One page program                                                                                */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_STD_PageProgram_L(qlibContext, input, logicalAddr, size_tmp, TRUE));

        /*-------------------------------------------------------------------------------------------------*/
        /* Update pointers for next iteration                                                              */
        /*-------------------------------------------------------------------------------------------------*/
        size -= size_tmp;
        logicalAddr += size_tmp;
        input += size_tmp;
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_PerformErase(QLIB_CONTEXT_T* qlibContext, QLIB_ERASE_T eraseType, U32 logicalAddr, BOOL blocking)
{
    U8 cmd = 0;
#if QLIB_NUM_OF_DIES > 1
    QLIB_STATUS_T ret     = QLIB_STATUS__OK;
    U32           origDie = qlibContext->activeDie;
#endif

    /*-----------------------------------------------------------------------------------------------------*/
    /* Check aligned address & calculate command                                                           */
    /*-----------------------------------------------------------------------------------------------------*/
    switch (eraseType)
    {
        case QLIB_ERASE_SECTOR_4K:
            cmd = SPI_FLASH_CMD__ERASE_SECTOR;
            break;

        case QLIB_ERASE_BLOCK_32K:
            cmd = SPI_FLASH_CMD__ERASE_BLOCK_32;
            break;

        case QLIB_ERASE_BLOCK_64K:
            cmd = SPI_FLASH_CMD__ERASE_BLOCK_64;
            break;

        case QLIB_ERASE_CHIP:
            cmd = SPI_FLASH_CMD__ERASE_CHIP;
            break;

        default:
            cmd = 0; // Set cmd to 0 to indicate we have invalid parameter
            // It's a PC-Lint require to put a default and break in any switch case
            break;
    }

    if (cmd == 0u)
    {
        return QLIB_STATUS__INVALID_PARAMETER;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Send erase command (with write enable and wait while busy after)                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    if (QLIB_ERASE_CHIP == eraseType)
    {
        if (Q2_BYPASS_HW_ISSUE_265(qlibContext) != 0u)
        {
            //Chip erase cannot work after global unlock if WPS=1(enable individual lock feature,
            //and whole chip is locked after POR)
            STD_FLASH_STATUS_T status;
            QLIB_STATUS_RET_CHECK(QLIB_STD_GetStatus_L(qlibContext, &status));
            if (READ_VAR_FIELD(status.SR3.asUint, SPI_FLASH__STATUS_3_FIELD__WPS) == 1u)
            {
                QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__ERASE_SECT_PLAIN(qlibContext, 0));
            }
        }
        else
        {
            QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                             QLIB_STD_GET_BUS_MODE(qlibContext),
                                                             FALSE,
                                                             TRUE,
                                                             blocking,
                                                             cmd,
                                                             NULL,
                                                             NULL,
                                                             0,
                                                             0,
                                                             NULL,
                                                             0,
                                                             &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
#if QLIB_NUM_OF_DIES > 1
            do
            {
                // QLIB_STD_SetActiveDie is used to wait for all Dies to be ready
                U8 nextDie = (qlibContext->activeDie + 1) % QLIB_GET_NUM_OF_DIES(qlibContext);
                QLIB_STATUS_RET_CHECK_GOTO(QLIB_STD_SetActiveDie(qlibContext, nextDie, FALSE), ret, error);
            } while (qlibContext->activeDie != origDie);
#endif
        }
    }
    else
    {
        QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                         QLIB_STD_GET_BUS_MODE(qlibContext),
                                                         FALSE,
                                                         TRUE,
                                                         blocking,
                                                         cmd,
                                                         &logicalAddr,
                                                         NULL,
                                                         0,
                                                         0,
                                                         NULL,
                                                         0,
                                                         &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;

#if QLIB_NUM_OF_DIES > 1
error:
    (void)QLIB_STD_SetActiveDie(qlibContext, origDie, FALSE);
    return ret;
#endif
}

QLIB_STATUS_T QLIB_STD_Erase(QLIB_CONTEXT_T* qlibContext, U32 logicalAddr, U32 size)
{
    U32          eraseSize = 0;
    QLIB_ERASE_T eraseType = QLIB_ERASE_FIRST;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(0u == (logicalAddr % FLASH_SECTOR_SIZE), QLIB_STATUS__INVALID_DATA_ALIGNMENT);
    QLIB_ASSERT_RET(0u == (size % FLASH_SECTOR_SIZE), QLIB_STATUS__INVALID_DATA_ALIGNMENT);

#ifdef QLIB_SUPPORT_QPI
    if ((Q2_BYPASS_HW_ISSUE_60(qlibContext) != 0u) && (qlibContext->busInterface.busMode == QLIB_BUS_MODE_4_4_4))
    {
        QLIB_STATUS_RET_CHECK(QLIB_STD_CheckWritePrivilege_L(qlibContext, logicalAddr));
    }
#endif // QLIB_SUPPORT_QPI

    /*-----------------------------------------------------------------------------------------------------*/
    /* Start erasing with optimal command                                                                  */
    /*-----------------------------------------------------------------------------------------------------*/
    while (0u < size)
    {
        if (_64KB_ <= size && 0u == (logicalAddr % _64KB_))
        {
            eraseSize = _64KB_;
            eraseType = QLIB_ERASE_BLOCK_64K;
        }
        else if (_32KB_ <= size && 0u == (logicalAddr % _32KB_))
        {
            eraseSize = _32KB_;
            eraseType = QLIB_ERASE_BLOCK_32K;
        }
        else
        {
            eraseSize = FLASH_SECTOR_SIZE;
            eraseType = QLIB_ERASE_SECTOR_4K;
        }

        /*-------------------------------------------------------------------------------------------------*/
        /* Start erase                                                                                     */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_STD_PerformErase(qlibContext, eraseType, logicalAddr, TRUE));

        /*-------------------------------------------------------------------------------------------------*/
        /* erase success                                                                                   */
        /*-------------------------------------------------------------------------------------------------*/
        logicalAddr += eraseSize;
        size -= eraseSize;
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_EraseSuspend(QLIB_CONTEXT_T* qlibContext)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform flash suspend and wait for flash device to finish                                           */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     QLIB_STD_GET_BUS_MODE(qlibContext),
                                                     FALSE,
                                                     FALSE,
                                                     TRUE,
                                                     SPI_FLASH_CMD__SUSPEND,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     0,
                                                     NULL,
                                                     0,
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    if ((1u == READ_VAR_FIELD(QLIB_ACTIVE_DIE_STATE(qlibContext).ssr.asUint, QLIB_REG_SSR__SUSPEND_E)) ||
        (1u == READ_VAR_FIELD(QLIB_ACTIVE_DIE_STATE(qlibContext).ssr.asUint, QLIB_REG_SSR__SUSPEND_W)))
    {
        QLIB_ACTIVE_DIE_STATE(qlibContext).isSuspended = 1u;
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_EraseResume(QLIB_CONTEXT_T* qlibContext, BOOL blocking)
{
    // Check if there is an ignore error before the resume and then after the resume check the last error, and ignore the "ignore error".
    BOOL ignorErrorWasSet = (qlibContext->dieState[qlibContext->activeDie].ssr.asStruct.IGNORE_ERR_S == 1u) ? TRUE : FALSE;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform flash resume                                                                                */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     QLIB_STD_GET_BUS_MODE(qlibContext),
                                                     FALSE,
                                                     FALSE,
                                                     blocking,
                                                     SPI_FLASH_CMD__RESUME,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     0,
                                                     NULL,
                                                     0,
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext,
                                                            ignorErrorWasSet == TRUE ? (U32)SSR_MASK__IGNORE_IGNORE_ERR
                                                                                     : (U32)SSR_MASK__ALL_ERRORS));
    QLIB_ASSERT_RET(0u == READ_VAR_FIELD(QLIB_ACTIVE_DIE_STATE(qlibContext).ssr.asUint, QLIB_REG_SSR__SUSPEND_E),
                    QLIB_STATUS__COMMAND_IGNORED);
    QLIB_ASSERT_RET(0u == READ_VAR_FIELD(QLIB_ACTIVE_DIE_STATE(qlibContext).ssr.asUint, QLIB_REG_SSR__SUSPEND_W),
                    QLIB_STATUS__COMMAND_IGNORED);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_Power(QLIB_CONTEXT_T* qlibContext, QLIB_POWER_T power)
{
    U8            origDie = qlibContext->activeDie;
    U8            die     = qlibContext->activeDie;
    QLIB_STATUS_T ret     = QLIB_STATUS__OK;

    /********************************************************************************************************
     * Select power state
    ********************************************************************************************************/
    switch (power)
    {
        case QLIB_POWER_UP:
            do
            {
                U8  devId      = 0u;
                U32 numRetries = 0u;
                do
                {
#if QLIB_NUM_OF_DIES > 1
                    if (die != origDie)
                    {
                        QLIB_STATUS_RET_CHECK_GOTO(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                                              QLIB_STD_GET_BUS_MODE(qlibContext),
                                                                              FALSE,
                                                                              FALSE,
                                                                              FALSE,
                                                                              SPI_FLASH_CMD__DIE_SELECT,
                                                                              NULL,
                                                                              &die,
                                                                              1,
                                                                              0,
                                                                              NULL,
                                                                              0,
                                                                              NULL),
                                                   ret,
                                                   recover_orig_die);
                    }
#endif
                    QLIB_STATUS_RET_CHECK_GOTO(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                                          QLIB_STD_GET_BUS_MODE(qlibContext),
                                                                          FALSE,
                                                                          FALSE,
                                                                          FALSE,
                                                                          SPI_FLASH_CMD__RELEASE_POWER_DOWN,
                                                                          NULL,
                                                                          NULL,
                                                                          0,
                                                                          SPI_FLASH_DUMMY_CYCLES__RELEASE_POWER_DOWN_GET_ID(
                                                                              qlibContext),
                                                                          &devId,
                                                                          1,
                                                                          NULL),
                                               ret,
                                               recover_orig_die);
                    numRetries++;
                } while (((Q3_BYPASS_HW_ISSUE_QPI_POWER_DOWN_RESP_ERR(qlibContext) != 0u) &&
                          (QLIB_BUS_MODE_4_4_4 == qlibContext->busInterface.busMode))
                             ? ((devId != qlibContext->detectedDeviceID) && (numRetries < (U32)Q3_QPI_POWER_UP_NUM_RETRIES))
                             : (devId == 0u || devId == 0xFFu));
                QLIB_ASSERT_WITH_ERROR_GOTO(devId == qlibContext->detectedDeviceID,
                                            QLIB_STATUS__COMMAND_FAIL,
                                            ret,
                                            recover_orig_die);

                // clear all error bits in SSR but do not consider any bit as error.
                QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_SSR_UNSIGNED(qlibContext, NULL, 0));

                qlibContext->dieState[die].isPoweredDown = FALSE;
                die                                      = (die + 1u) % QLIB_GET_NUM_OF_DIES(qlibContext);
            } while (die != origDie);

            // recover initial die
#if QLIB_NUM_OF_DIES > 1
            if (QLIB_GET_NUM_OF_DIES(qlibContext) > 1u)
            {
                QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                                 QLIB_STD_GET_BUS_MODE(qlibContext),
                                                                 FALSE,
                                                                 FALSE,
                                                                 FALSE,
                                                                 SPI_FLASH_CMD__DIE_SELECT,
                                                                 NULL,
                                                                 &origDie,
                                                                 1,
                                                                 0,
                                                                 NULL,
                                                                 0,
                                                                 NULL));
            }
#endif
            break;

        case QLIB_POWER_DOWN:
            QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                             QLIB_STD_GET_BUS_MODE(qlibContext),
                                                             FALSE,
                                                             FALSE,
                                                             FALSE,
                                                             SPI_FLASH_CMD__POWER_DOWN,
                                                             NULL,
                                                             NULL,
                                                             0,
                                                             0,
                                                             NULL,
                                                             0,
                                                             NULL));

            for (die = 0; die < QLIB_NUM_OF_DIES; die++)
            {
                qlibContext->dieState[die].isPoweredDown = TRUE;
            }

            break;

        default:
            ret = QLIB_STATUS__INVALID_PARAMETER;
            break;
    }

    return ret;

recover_orig_die:
#if QLIB_NUM_OF_DIES > 1
    if (QLIB_GET_NUM_OF_DIES(qlibContext) > 1u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                         QLIB_STD_GET_BUS_MODE(qlibContext),
                                                         FALSE,
                                                         FALSE,
                                                         FALSE,
                                                         SPI_FLASH_CMD__DIE_SELECT,
                                                         NULL,
                                                         &origDie,
                                                         1,
                                                         0,
                                                         NULL,
                                                         0,
                                                         NULL));
    }
#endif
    return ret;
}

QLIB_STATUS_T QLIB_STD_ResetFlash(QLIB_CONTEXT_T* qlibContext, BOOL forceReset)
{
    /********************************************************************************************************
     * wait for flash to be ready for reset (this part can be run from flash)
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_STD_PrepareForResetFlash_L(qlibContext, forceReset));

    /********************************************************************************************************
     * Perform the reset flow (run from RAM)
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_STD_ResetFlash_L(qlibContext));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_GetHwVersion(QLIB_CONTEXT_T* qlibContext, QLIB_STD_HW_VER_T* hwVer)
{
    U8              jedecId[3];
    QLIB_BUS_MODE_T busMode     = QLIB_STD_GET_BUS_MODE(qlibContext);
    U32             dummyCycles = (busMode == QLIB_BUS_MODE_1_1_1 ? SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_SPI
#ifdef QLIB_SUPPORT_QPI
                       : busMode == QLIB_BUS_MODE_4_4_4 ? SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_QPI(qlibContext->detectedDeviceID)
#endif
                                                        : SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_QPI_OPI);

    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     busMode,
                                                     FALSE,
                                                     FALSE,
                                                     FALSE,
                                                     SPI_FLASH_CMD__READ_JEDEC,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     dummyCycles,
                                                     jedecId,
                                                     sizeof(jedecId),
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    hwVer->manufacturerID = jedecId[0];
    hwVer->memoryType     = jedecId[1];
    hwVer->capacity       = jedecId[2];

    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     QLIB_STD_GET_BUS_MODE(qlibContext),
                                                     FALSE,
                                                     FALSE,
                                                     FALSE,
                                                     SPI_FLASH_CMD__DEVICE_ID,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     SPI_FLASH_DUMMY_CYCLES__RELEASE_POWER_DOWN_GET_ID(qlibContext),
                                                     &(hwVer->deviceID),
                                                     (sizeof(hwVer->deviceID)),
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_GetId(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ID_T* id_p)
{
    QLIB_STATUS_T   ret = QLIB_STATUS__OK;
    U32             dummyCycles;
    QLIB_BUS_MODE_T busMode;
#ifdef Q2_API
    U8* pUniqueId = (U8*)&(id_p->uniqueID);
#else
    U8* pUniqueId = (U8*)(id_p->uniqueID);
#endif
#ifdef QLIB_SUPPORT_QPI
    BOOL exitQpi = FALSE;
    if ((W77Q_SUPPORT_UNIQUE_ID_QPI(qlibContext) == 0u) && (QLIB_BUS_MODE_4_4_4 == qlibContext->busInterface.busMode))
    {
        QLIB_STATUS_RET_CHECK(
            QLIB_SetInterface(qlibContext, QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, qlibContext->busInterface.dtr)));
        exitQpi = TRUE;
    }
#endif //QLIB_SUPPORT_QPI

    (void)memset(id_p, 0, sizeof(QLIB_STD_ID_T));
    busMode = QLIB_STD_GET_BUS_MODE(qlibContext);
    if (QLIB_BUS_MODE_1_1_1 == busMode)
    {
        if ((Q2_4_BYTES_ADDRESS_MODE(qlibContext) != 0u) && (qlibContext->addrMode == QLIB_STD_ADDR_MODE__4_BYTE))
        {
            dummyCycles = (U32)SPI_FLASH_DUMMY_CYCLES__UNIQUE_ID__SPI_ADDRESS_MODE_4B;
        }
        else
        {
            dummyCycles = (U32)SPI_FLASH_DUMMY_CYCLES__UNIQUE_ID__SPI_ADDRESS_MODE_3B;
        }
    }
    else
    {
        dummyCycles = (U32)SPI_FLASH_DUMMY_CYCLES__UNIQUE_ID__QPI_OPI;
    }

    QLIB_STATUS_RET_CHECK_GOTO(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                          busMode,
                                                          FALSE,
                                                          FALSE,
                                                          FALSE,
                                                          SPI_FLASH_CMD__READ_UNIQUE_ID,
                                                          NULL,
                                                          NULL,
                                                          0,
                                                          dummyCycles,
                                                          pUniqueId,
                                                          W77Q_EXTENDED_UNIQUE_ID_16B(qlibContext) != 0u ? (U32)(16) : (U32)(8),
                                                          &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr),
                               ret,
                               error);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS), ret, error);

error:
#ifdef QLIB_SUPPORT_QPI
    if (exitQpi == TRUE)
    {
        QLIB_STATUS_RET_CHECK(
            QLIB_SetInterface(qlibContext, QLIB_BUS_FORMAT(QLIB_BUS_MODE_4_4_4, qlibContext->busInterface.dtr)));
    }
#endif
    return ret;
}

QLIB_STATUS_T QLIB_STD_SetActiveDie(QLIB_CONTEXT_T* qlibContext, U8 die, BOOL clearErrors)
{
    U8              origDie = qlibContext->activeDie;
    QLIB_REG_ESSR_T essr;
    QLIB_STATUS_T   ret = QLIB_STATUS__OK;
    U32             dataOutSize;
    U8              dataOut[2];
    QLIB_REG_SSR_T  ssr;

    QLIB_DATA_EXTENSION_SET_DATA_OUT(qlibContext, dataOut, die);
    dataOutSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);

    if (die == qlibContext->activeDie)
    {
        return QLIB_STATUS__OK;
    }

    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     QLIB_STD_GET_BUS_MODE(qlibContext),
                                                     FALSE,
                                                     FALSE,
                                                     TRUE,
                                                     SPI_FLASH_CMD__DIE_SELECT,
                                                     NULL,
                                                     dataOut,
                                                     dataOutSize,
                                                     0,
                                                     NULL,
                                                     0,
                                                     &ssr));

    qlibContext->activeDie = die; // assume command succeeded - set new die so SSR is set on correct die in context

    if ((1u == READ_VAR_FIELD(ssr.asUint, QLIB_REG_SSR__SUSPEND_E)) ||
        (1u == READ_VAR_FIELD(ssr.asUint, QLIB_REG_SSR__SUSPEND_W)))
    {
        QLIB_ACTIVE_DIE_STATE(qlibContext).isSuspended = 1u;
    }
    else
    {
        if (QLIB_ACTIVE_DIE_STATE(qlibContext).isPoweredDown == 0u)
        {
            if (TRUE == clearErrors)
            {
                QLIB_STATUS_RET_CHECK(QLIB_SEC_ClearSSR(qlibContext));
            }
            QLIB_STATUS_RET_CHECK_GOTO(QLIB_CMD_PROC__get_ESSR_UNSIGNED(qlibContext, &essr), ret, error);
            qlibContext->activeDie = (U8)READ_VAR_FIELD(essr.asUint64, QLIB_REG_ESSR__DIE_ID);
            QLIB_ASSERT_RET(qlibContext->activeDie == die, QLIB_STATUS__COMMAND_FAIL);
        }
        QLIB_ACTIVE_DIE_STATE(qlibContext).isSuspended = 0u;
    }

    return QLIB_STATUS__OK;

error:
    // since we do not know which of the dies is the active one.
    qlibContext->dieState[origDie].mcInSync = FALSE;
    qlibContext->dieState[die].mcInSync     = FALSE;

    return ret;
}

QLIB_STATUS_T QLIB_STD_SetAddressMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T addrMode)
{
    QLIB_STD_ADDR_MODE_T testAddrMode;
    QLIB_STD_ADDR_MODE_T origAddrMode;
    QLIB_STATUS_T        ret;

    if (QLIB_STD_ADDR_MODE__4_BYTE == addrMode)
    {
        // 4-Byte Address Mode
        QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                         QLIB_STD_GET_BUS_MODE(qlibContext),
                                                         FALSE,
                                                         FALSE,
                                                         FALSE,
                                                         SPI_FLASH_CMD__4_BYTE_ADDRESS_MODE_ENTER,
                                                         NULL,
                                                         NULL,
                                                         0,
                                                         0,
                                                         NULL,
                                                         0,
                                                         &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
    }
    else
    {
        // 3-Byte Address Mode
        QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                         QLIB_STD_GET_BUS_MODE(qlibContext),
                                                         FALSE,
                                                         FALSE,
                                                         FALSE,
                                                         SPI_FLASH_CMD__4_BYTE_ADDRESS_MODE_EXIT,
                                                         NULL,
                                                         NULL,
                                                         0,
                                                         0,
                                                         NULL,
                                                         0,
                                                         &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
    }

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    /********************************************************************************************************
     * Update context with new address mode since next command uses the address
    ********************************************************************************************************/
    origAddrMode = qlibContext->addrMode;
    if ((QLIB_STD_ADDR_MODE__4_BYTE == addrMode) || (qlibContext->busInterface.busMode != QLIB_BUS_MODE_8_8_8))
    {
        // OPI mode force 32b address mode
        qlibContext->addrMode = addrMode;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Verify success                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    if (W77Q_EXTENDED_CONFIG_REGISTER(qlibContext) != 0u)
    {
        U8  configReg;
        U32 addr = CONFIG_REG_ADDR__ADDRESS_MODE;
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_CMD_PROC__get_CR(qlibContext, addr, &configReg), ret, error);

        testAddrMode = (READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_5_FIELD__AM) == 0u)
                           ? QLIB_STD_ADDR_MODE__4_BYTE
                           : QLIB_STD_ADDR_MODE__3_BYTE;
    }
    else
    {
        U8 statusReg3;
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                              QLIB_STD_GET_BUS_MODE(qlibContext),
                                                              FALSE,
                                                              FALSE,
                                                              FALSE,
                                                              SPI_FLASH_CMD__READ_STATUS_REGISTER_3,
                                                              NULL,
                                                              NULL,
                                                              0,
                                                              0,
                                                              &statusReg3,
                                                              1,
                                                              &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr),
                                   ret,
                                   error);
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
        testAddrMode = (READ_VAR_FIELD(statusReg3, SPI_FLASH__STATUS_3_FIELD__ADS) == 1u) ? QLIB_STD_ADDR_MODE__4_BYTE
                                                                                          : QLIB_STD_ADDR_MODE__3_BYTE;
    }
    QLIB_ASSERT_WITH_ERROR_GOTO(testAddrMode == addrMode, QLIB_STATUS__COMMAND_IGNORED, ret, error);

    return QLIB_STATUS__OK;
error:
    qlibContext->addrMode = origAddrMode;
    return ret;
}

QLIB_STATUS_T QLIB_STD_SetPowerUpAddressMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T addrMode)
{
    if (W77Q_EXTENDED_CONFIG_REGISTER(qlibContext) == 0u)
    {
        STD_FLASH_STATUS_T status;
        STD_FLASH_STATUS_T statusCheck;
        U8                 addrModeBit = ((QLIB_STD_ADDR_MODE__4_BYTE == addrMode) ? 1u : 0u);

        /*-----------------------------------------------------------------------------------------------------*/
        /* Read the status registers                                                                           */
        /*-----------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_STD_GetStatus_L(qlibContext, &status));

        /*-----------------------------------------------------------------------------------------------------*/
        /* Set the ADP bit                                                                                     */
        /*-----------------------------------------------------------------------------------------------------*/
        SET_VAR_FIELD_8(status.SR3.asUint, SPI_FLASH__STATUS_3_FIELD__ADP, addrModeBit);
        QLIB_STATUS_RET_CHECK(QLIB_STD_SetStatus_L(qlibContext, status, &statusCheck));

        /*-----------------------------------------------------------------------------------------------------*/
        /* Check that the command succeed                                                                      */
        /*-----------------------------------------------------------------------------------------------------*/
        QLIB_ASSERT_RET(READ_VAR_FIELD(statusCheck.SR3.asUint, SPI_FLASH__STATUS_3_FIELD__ADP) == addrModeBit,
                        QLIB_STATUS__COMMAND_IGNORED);
    }
    else
    {
        U8 configReg;
        U8 addrModeBit = ((QLIB_STD_ADDR_MODE__4_BYTE == addrMode) ? 0u : 1u);

        /****************************************************************************************************
         * Read the configuration register
        ****************************************************************************************************/
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_default_CR(qlibContext, CONFIG_REG_ADDR__ADDRESS_MODE, &configReg));

        /****************************************************************************************************
         * Set the AM bit
        ****************************************************************************************************/
        SET_VAR_FIELD_8(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_5_FIELD__AM, addrModeBit);
        QLIB_STATUS_RET_CHECK(QLIB_STD_set_default_CR_L(qlibContext, CONFIG_REG_ADDR__ADDRESS_MODE, configReg));
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_GetAddressMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T* addrMode)
{
    if (W77Q_FLAG_REGISTER(qlibContext) == 0u)
    {
        U8 statusReg3;
        QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                         QLIB_STD_GET_BUS_MODE(qlibContext),
                                                         FALSE,
                                                         FALSE,
                                                         FALSE,
                                                         SPI_FLASH_CMD__READ_STATUS_REGISTER_3,
                                                         NULL,
                                                         NULL,
                                                         0,
                                                         0,
                                                         &statusReg3,
                                                         1,
                                                         NULL));
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
        *addrMode = (READ_VAR_FIELD(statusReg3, SPI_FLASH__STATUS_3_FIELD__ADS) != 0u) ? QLIB_STD_ADDR_MODE__4_BYTE
                                                                                       : QLIB_STD_ADDR_MODE__3_BYTE;
    }
    else
    {
        STD_FLASH_FLAGS_T flagReg;
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_FR(qlibContext, &flagReg));
        *addrMode = (READ_VAR_FIELD(flagReg.asUint, SPI_FLASH__FLAG_FIELD__AMF) != 0u) ? QLIB_STD_ADDR_MODE__4_BYTE
                                                                                       : QLIB_STD_ADDR_MODE__3_BYTE;
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_GetPowerUpAddressMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T* addrMode)
{
    if (W77Q_EXTENDED_CONFIG_REGISTER(qlibContext) == 0u)
    {
        U8 statusReg3;
        QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                         QLIB_STD_GET_BUS_MODE(qlibContext),
                                                         FALSE,
                                                         FALSE,
                                                         FALSE,
                                                         SPI_FLASH_CMD__READ_STATUS_REGISTER_3,
                                                         NULL,
                                                         NULL,
                                                         0,
                                                         0,
                                                         &statusReg3,
                                                         1,
                                                         NULL));
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
        *addrMode = (READ_VAR_FIELD(statusReg3, SPI_FLASH__STATUS_3_FIELD__ADP) == 1u) ? QLIB_STD_ADDR_MODE__4_BYTE
                                                                                       : QLIB_STD_ADDR_MODE__3_BYTE;
    }
    else
    {
        U8 configReg;
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_default_CR(qlibContext, CONFIG_REG_ADDR__ADDRESS_MODE, &configReg));
        *addrMode = READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_5_FIELD__AM) == 0u ? QLIB_STD_ADDR_MODE__4_BYTE
                                                                                                   : QLIB_STD_ADDR_MODE__3_BYTE;
    }
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_getPowerUpDummyCycles(QLIB_CONTEXT_T* qlibContext, U8* dummy)
{
    U8 configReg = 0;
    /*-----------------------------------------------------------------------------------------------------*/
    /* Read the configuration register                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_default_CR(qlibContext, CONFIG_REG_ADDR__DUMMY, &configReg));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set the dummy bit                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    *dummy =
        (configReg == 0xFFu ? SPI_FLASH__EXTENDED_CONFIGURATION_DEFAULT_DUMMY_CONFIG // CR was not set yet, assume reset value
                            : (U8)READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_1_FIELD__DUMMY));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_GetFastReadDummyCycles(QLIB_CONTEXT_T* qlibContext, U8* dummyCycles)
{
    U8 configReg;

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_CR(qlibContext, CONFIG_REG_ADDR__DUMMY, &configReg));
    if (configReg == 0xFFu)
    {
        // CR was not set yet, assume reset value
        *dummyCycles = SPI_FLASH__EXTENDED_CONFIGURATION_DEFAULT_DUMMY_CONFIG;
    }
    else
    {
        *dummyCycles = (U8)READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_1_FIELD__DUMMY);
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_SetPowerUpFastReadDummyCycles(QLIB_CONTEXT_T* qlibContext, U8 dummyCycles)
{
    U8 configReg;
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_default_CR(qlibContext, CONFIG_REG_ADDR__DUMMY, &configReg));
    SET_VAR_FIELD_8(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_1_FIELD__DUMMY, dummyCycles);
    return QLIB_STD_set_default_CR_L(qlibContext, CONFIG_REG_ADDR__DUMMY, configReg);
}

QLIB_STATUS_T QLIB_STD_SetFastReadDummyCycles(QLIB_CONTEXT_T* qlibContext, U8 dummyCycles)
{
    U8 configReg;
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_CR(qlibContext, CONFIG_REG_ADDR__DUMMY, &configReg));
    SET_VAR_FIELD_8(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_1_FIELD__DUMMY, dummyCycles);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__set_CR(qlibContext, CONFIG_REG_ADDR__DUMMY, configReg));

    qlibContext->fastReadDummy = dummyCycles;
    return QLIB_STATUS__OK;
}

#if !defined EXCLUDE_W77Q_ECC && !defined Q2_API
QLIB_STATUS_T QLIB_STD_GetEccStatus(QLIB_CONTEXT_T* qlibContext, QLIB_ECC_STATUS_T* eccStatus, QLIB_ADVANCED_ECC_T* advancedEcc)
{
    STD_FLASH_ECC_STATUS_T eccReg;
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_ECCR(qlibContext, &eccReg));
    eccStatus->singleErr    = INT_TO_BOOLEAN(READ_VAR_FIELD(eccReg.asUint, SPI_FLASH__ECC_STATUS_FIELD__SEC));
    eccStatus->doubleErr    = INT_TO_BOOLEAN(READ_VAR_FIELD(eccReg.asUint, SPI_FLASH__ECC_STATUS_FIELD__DED));
    eccStatus->multipleProg = INT_TO_BOOLEAN(READ_VAR_FIELD(eccReg.asUint, SPI_FLASH__ECC_STATUS_FIELD__MULT_PROG));
    eccStatus->erasedLine   = INT_TO_BOOLEAN(READ_VAR_FIELD(eccReg.asUint, SPI_FLASH__ECC_STATUS_FIELD__ERASED));
    eccStatus->eccEnabled   = INT_TO_BOOLEAN(READ_VAR_FIELD(eccReg.asUint, SPI_FLASH__ECC_STATUS_FIELD__ECC_EN));
    eccStatus->eccOff       = INT_TO_BOOLEAN(READ_VAR_FIELD(eccReg.asUint, SPI_FLASH__ECC_STATUS_FIELD__ECCO));

    if (advancedEcc != NULL)
    {
        STD_FLASH_ADVANCED_ECC_T advancedEccReg;
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_AECCR(qlibContext, &advancedEccReg));
        advancedEcc->sacvf = INT_TO_BOOLEAN(READ_VAR_FIELD(advancedEccReg.AECCR0.asUint, SPI_FLASH__ADVANCED_ECC_0_FIELD__SACVF));
        if (advancedEcc->sacvf == TRUE)
        {
            advancedEcc->sra = (U32)(READ_VAR_FIELD(advancedEccReg.AECCR0.asUint, SPI_FLASH__ADVANCED_ECC_0_FIELD__SRA3) << 24u) |
                               (U32)(READ_VAR_FIELD(advancedEccReg.AECCR1.asUint, SPI_FLASH__ADVANCED_ECC_1_FIELD__SRA2) << 16u) |
                               (U32)(READ_VAR_FIELD(advancedEccReg.AECCR2.asUint, SPI_FLASH__ADVANCED_ECC_2_FIELD__SRA1) << 8u) |
                               (U32)(READ_VAR_FIELD(advancedEccReg.AECCR3.asUint, SPI_FLASH__ADVANCED_ECC_3_FIELD__SRA0) << 4u);
        }
        else
        {
            advancedEcc->sra = 0u;
        }
        advancedEcc->sc    = (U8)READ_VAR_FIELD(advancedEccReg.AECCR3.asUint, SPI_FLASH__ADVANCED_ECC_3_FIELD__SC);
        advancedEcc->dacvf = INT_TO_BOOLEAN(READ_VAR_FIELD(advancedEccReg.AECCR4.asUint, SPI_FLASH__ADVANCED_ECC_4_FIELD__DACVF));
        if (advancedEcc->dacvf == TRUE)
        {
            advancedEcc->dra = (U32)(READ_VAR_FIELD(advancedEccReg.AECCR4.asUint, SPI_FLASH__ADVANCED_ECC_4_FIELD__DRA3) << 24u) |
                               (U32)(READ_VAR_FIELD(advancedEccReg.AECCR5.asUint, SPI_FLASH__ADVANCED_ECC_5_FIELD__DRA2) << 16u) |
                               (U32)(READ_VAR_FIELD(advancedEccReg.AECCR6.asUint, SPI_FLASH__ADVANCED_ECC_6_FIELD__DRA1) << 8u) |
                               (U32)(READ_VAR_FIELD(advancedEccReg.AECCR7.asUint, SPI_FLASH__ADVANCED_ECC_7_FIELD__DRA0) << 4u);
        }
        else
        {
            advancedEcc->dra = 0u;
        }
        advancedEcc->dc = (U8)READ_VAR_FIELD(advancedEccReg.AECCR7.asUint, SPI_FLASH__ADVANCED_ECC_7_FIELD__DC);
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_SetEccEnable(QLIB_CONTEXT_T* qlibContext, BOOL enable)
{
    STD_FLASH_ECC_STATUS_T eccReg;

    /********************************************************************************************************
     * Set the ecc enable bit in the ECC status register
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_ECCR(qlibContext, &eccReg));
    SET_VAR_FIELD_8(eccReg.asUint, SPI_FLASH__ECC_STATUS_FIELD__ECC_EN, BOOLEAN_TO_INT(enable));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__set_ECCR(qlibContext, eccReg));

    return QLIB_STATUS__OK;
}
#endif
#ifndef Q2_API
QLIB_STATUS_T QLIB_STD_getPowerUpDqsDisable(QLIB_CONTEXT_T* qlibContext, BOOL* dqsDisable)
{
    if (W77Q_EXTENDED_CONFIG_REGISTER(qlibContext) != 0u)
    {
        U8 configReg = 0;
        /*-----------------------------------------------------------------------------------------------------*/
        /* Read the configuration register                                                                     */
        /*-----------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_default_CR(qlibContext, CONFIG_REG_ADDR__BUSMOD, &configReg));

        /*-----------------------------------------------------------------------------------------------------*/
        /* Set the DQS Enable bit                                                                              */
        /*-----------------------------------------------------------------------------------------------------*/
        *dqsDisable = (0u != (U8)READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_0_FIELD__DQS_EN)) ? FALSE : TRUE;
    }
    else
    {
        *dqsDisable = FALSE;
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_SetPowerUpDqsDisable(QLIB_CONTEXT_T* qlibContext, BOOL dqsDisable)
{
    if (W77Q_EXTENDED_CONFIG_REGISTER(qlibContext) != 0u)
    {
        U8 configReg;
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_default_CR(qlibContext, CONFIG_REG_ADDR__BUSMOD, &configReg));
        SET_VAR_FIELD_8(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_0_FIELD__DQS_EN, (FALSE == dqsDisable) ? 1u : 0u);
        return QLIB_STD_set_default_CR_L(qlibContext, CONFIG_REG_ADDR__BUSMOD, configReg);
    }
    else
    {
        return QLIB_STATUS__NOT_SUPPORTED;
    }
}
#endif


#if !defined EXCLUDE_W77Q_INTERRUPTS && !defined Q2_API
QLIB_STATUS_T QLIB_STD_ConfigInterrupts(QLIB_CONTEXT_T* qlibContext, QLIB_INTERRUPT_T* intTypes, BOOL enable)
{
    U8 configReg;
    U8 interruptMask = interruptsToCR_L(intTypes);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_CR(qlibContext, CONFIG_REG_ADDR__INTERRUPT_MASK, &configReg));

    if (enable == TRUE)
    {
        configReg = configReg & (~(interruptMask)); // clear interrupt mask bits
    }
    else
    {
        configReg = (configReg | interruptMask); // set interrupt mask bits
    }

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__set_CR(qlibContext, CONFIG_REG_ADDR__INTERRUPT_MASK, configReg));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_ClearInterrupts(QLIB_CONTEXT_T* qlibContext, QLIB_INTERRUPT_T* intTypes)
{
    U8 interruptMask = interruptsToCR_L(intTypes);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__set_CR(qlibContext, CONFIG_REG_ADDR__INTERRUPT_EVENTS, interruptMask));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_STD_ReadInterruptState(QLIB_CONTEXT_T* qlibContext, QLIB_INTERRUPT_T* intState)
{
    U8 configReg = 0u;
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_CR(qlibContext, CONFIG_REG_ADDR__INTERRUPT_EVENTS, &configReg));
    intState->securityViolationInt = (U32)READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__SECURE_IMASK);
    intState->watchdogTimerInt     = (U32)READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__AWDT_IMASK);
    intState->spiIntegrityInt    = (U32)READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__SPI_INTG_IMASK);
    intState->eccSingleErrInt    = (U32)READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__SEC_IMASK);
    intState->eccDoubleErrInt    = (U32)READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__DED_IMASK);
    intState->eccMultipleProgInt = (U32)READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__MULTP_IMASK);

    return QLIB_STATUS__OK;
}
#endif

QLIB_STATUS_T QLIB_STD_GetConfiguredAddrMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T* addrModeConfig)
{
    if ((QLIB_BUS_MODE_8_8_8 == QLIB_STD_GET_BUS_MODE(qlibContext)) && (QLIB_STD_ADDR_MODE__4_BYTE == qlibContext->addrMode))
    {
        U8 configReg;
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_CR(qlibContext, CONFIG_REG_ADDR__ADDRESS_MODE, &configReg));
        *addrModeConfig = (READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_5_FIELD__AM) == 0u)
                              ? QLIB_STD_ADDR_MODE__4_BYTE
                              : QLIB_STD_ADDR_MODE__3_BYTE;
    }
    else
    {
        *addrModeConfig = qlibContext->addrMode;
    }
    return QLIB_STATUS__OK;
}

#if QLIB_NUM_OF_DIES > 1
QLIB_STATUS_T QLIB_STD_WaitForDiesAfterReset(QLIB_CONTEXT_T* qlibContext, U32* fallbackStatus)
{
    U8              dieId;
    U8              nextDie;
    BOOL            dieIsUp;
    QLIB_REG_ESSR_T essr;

    if (fallbackStatus != NULL)
    {
        *fallbackStatus = 0;
    }
    for (dieId = 0; dieId < QLIB_GET_NUM_OF_DIES(qlibContext); dieId++)
    {
        nextDie = (dieId + 1) % QLIB_GET_NUM_OF_DIES(qlibContext);
        dieIsUp = FALSE;

        while (dieIsUp == FALSE)
        {
            QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                             QLIB_STD_GET_BUS_MODE(qlibContext),
                                                             FALSE,
                                                             FALSE,
                                                             TRUE,
                                                             SPI_FLASH_CMD__DIE_SELECT,
                                                             NULL,
                                                             &nextDie,
                                                             1,
                                                             0,
                                                             NULL,
                                                             0,
                                                             NULL));

            qlibContext->activeDie = nextDie;
            if (QLIB_CMD_PROC__get_ESSR_UNSIGNED(qlibContext, &essr) != QLIB_STATUS__OK)
            {
                continue;
            }
            qlibContext->activeDie = READ_VAR_FIELD(essr.asUint64, QLIB_REG_ESSR__DIE_ID);
            if (qlibContext->activeDie == nextDie)
            {
                dieIsUp = TRUE;
                if (fallbackStatus != NULL)
                {
                    *fallbackStatus |=
                        ((U32)READ_VAR_FIELD(QLIB_GET_LAST_SEC_STATUS_FIELD(qlibContext), QLIB_REG_SSR__FB_REMAP) << nextDie);
                }
            }
        }
    }
    return QLIB_STATUS__OK;
}
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                             LOCAL FUNCTIONS                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This routine sets the quad enable bit in the extended configuration register
 *
 * @param       qlibContext   qlib context object
 * @param[in]   enable        TRUE for enable, FALSE for disable
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_STD_SetQuadEnableInCR_L(QLIB_CONTEXT_T* qlibContext, BOOL enable)
{
    U8  configReg;
    U32 quadEnableBit = ((TRUE == enable) ? 1u : 0u);

    /********************************************************************************************************
     * Set the quad enable bit in the volatile CR
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_CR(qlibContext, CONFIG_REG_ADDR__QUAD_ENABLE, &configReg));
    SET_VAR_FIELD_8(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_10_FIELD__QE, quadEnableBit);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__set_CR(qlibContext, CONFIG_REG_ADDR__QUAD_ENABLE, configReg));

    /********************************************************************************************************
     * Set the quad enable bit in the non volatile default CR
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_default_CR(qlibContext, CONFIG_REG_ADDR__QUAD_ENABLE, &configReg));
    SET_VAR_FIELD_8(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_10_FIELD__QE, quadEnableBit);
    QLIB_STATUS_RET_CHECK(QLIB_STD_set_default_CR_L(qlibContext, CONFIG_REG_ADDR__QUAD_ENABLE, configReg));

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine retrieves the quad enable value from the extended configuration register
 *
 * @param       qlibContext   qlib context object
 * @param[out]  enabled       TRUE if enabled, FALSE if disabled
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_STD_GetQuadEnableFromCR_L(QLIB_CONTEXT_T* qlibContext, BOOL* enabled)
{
    U8 configReg;

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_CR(qlibContext, CONFIG_REG_ADDR__QUAD_ENABLE, &configReg));
    *enabled = (READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_10_FIELD__QE) == 1u) ? TRUE : FALSE;

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine sets the quad enable bit in the status register
 *
 * @param       qlibContext   qlib context object
 * @param[in]   enable        TRUE for enable, FALSE for disable
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_STD_SetQuadEnableInSR_L(QLIB_CONTEXT_T* qlibContext, BOOL enable)
{
    STD_FLASH_STATUS_T status;
    STD_FLASH_STATUS_T statusCheck;
    U32                quadEnableBit = ((TRUE == enable) ? 1u : 0u);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Read the status registers                                                                           */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_GetStatus_L(qlibContext, &status));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set the quad enable bit                                                                             */
    /*-----------------------------------------------------------------------------------------------------*/
    SET_VAR_FIELD_8(status.SR2.asUint, SPI_FLASH__STATUS_2_FIELD__QE, quadEnableBit);
    QLIB_STATUS_RET_CHECK(QLIB_STD_SetStatus_L(qlibContext, status, &statusCheck));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Check that the command succeed                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(READ_VAR_FIELD((U32)statusCheck.SR2.asUint, SPI_FLASH__STATUS_2_FIELD__QE) == quadEnableBit,
                    QLIB_STATUS__COMMAND_IGNORED);

    return QLIB_STATUS__OK;
}
/************************************************************************************************************
 * @brief       This routine retrieves the quad enable value from the status register
 *
 * @param       qlibContext   qlib context object
 * @param[out]  enabled       TRUE if enabled, FALSE if disabled
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_STD_GetQuadEnableFromSR_L(QLIB_CONTEXT_T* qlibContext, BOOL* enabled)
{
    U8 statusReg2;
    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     QLIB_STD_GET_BUS_MODE(qlibContext),
                                                     FALSE,
                                                     FALSE,
                                                     FALSE,
                                                     SPI_FLASH_CMD__READ_STATUS_REGISTER_2,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     0,
                                                     &statusReg2,
                                                     1,
                                                     NULL));
    *enabled = (statusReg2 == 0xFFu) ? FALSE : INT_TO_BOOLEAN(READ_VAR_FIELD((U32)statusReg2, SPI_FLASH__STATUS_2_FIELD__QE));

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine returns the Flash status registers
 *
 * @param       qlibContext   qlib context object
 * @param[out]  status        Flash status
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_STD_GetStatus_L(QLIB_CONTEXT_T* qlibContext, STD_FLASH_STATUS_T* status)
{
    QLIB_BUS_MODE_T busMode     = QLIB_STD_GET_BUS_MODE(qlibContext);
    U32             dummyCycles = (W77Q_READ_SR_DUMMY(qlibContext) == 0u)
                                      ? 0u
                                      : (busMode == QLIB_BUS_MODE_1_1_1 ? SPI_FLASH_DUMMY_CYCLES__READ_STATUS_REGISTER_SPI
                                                                        : SPI_FLASH_DUMMY_CYCLES__READ_STATUS_REGISTER_QPI_OPI);
    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform read status register 1                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     busMode,
                                                     FALSE,
                                                     FALSE,
                                                     FALSE,
                                                     SPI_FLASH_CMD__READ_STATUS_REGISTER_1,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     dummyCycles,
                                                     &status->SR1.asUint,
                                                     1,
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform read status register 2                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     busMode,
                                                     FALSE,
                                                     FALSE,
                                                     FALSE,
                                                     SPI_FLASH_CMD__READ_STATUS_REGISTER_2,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     0,
                                                     &status->SR2.asUint,
                                                     1,
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform read status register 3                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     busMode,
                                                     FALSE,
                                                     FALSE,
                                                     FALSE,
                                                     SPI_FLASH_CMD__READ_STATUS_REGISTER_3,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     0,
                                                     &status->SR3.asUint,
                                                     1,
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine performs blocking / non-blocking page program
 *
 * @param       qlibContext   qlib context object
 * @param[in]   input         buffer of data to write
 * @param[in]   logicalAddr   logical flash address
 * @param[in]   size          data size to write (page-size bytes max)
 * @param[in]   blocking      if TRUE, this function is blocking till the program is finish
 *
 * @return      0 in no error occurred, or QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_STD_PageProgram_L(QLIB_CONTEXT_T* qlibContext,
                                            const U8*       input,
                                            U32             logicalAddr,
                                            U32             size,
                                            BOOL            blocking)
{
    U8              writeCMD = 0;
    QLIB_BUS_MODE_T format   = QLIB_BUS_MODE_INVALID;
#ifdef QLIB_SUPPORT_QPI
#define BYPASS_MIN_WRITE_SIZE 16u
    U8 buffer[BYPASS_MIN_WRITE_SIZE];
#endif // QLIB_SUPPORT_QPI

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(0u < size, QLIB_STATUS__PARAMETER_OUT_OF_RANGE);
    QLIB_ASSERT_RET(size <= FLASH_PAGE_SIZE, QLIB_STATUS__INVALID_DATA_SIZE);
    QLIB_ASSERT_RET(((logicalAddr % FLASH_PAGE_SIZE) + size) <= FLASH_PAGE_SIZE, QLIB_STATUS__PARAMETER_OUT_OF_RANGE);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Get write command                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    writeCMD = QLIB_STD_GetWriteCMD_L(qlibContext, &format);
    // TODO - make this calculation in Config time and store the command in the globals

#ifdef QLIB_SUPPORT_QPI

    if ((Q2_BYPASS_HW_ISSUE_60(qlibContext) != 0u) && format == QLIB_BUS_MODE_4_4_4 && writeCMD == SPI_FLASH_CMD__PAGE_PROGRAM &&
        size < BYPASS_MIN_WRITE_SIZE)
    {
        (void)memset(buffer, 0xFF, BYPASS_MIN_WRITE_SIZE);

        if ((logicalAddr % FLASH_PAGE_SIZE) >= (BYPASS_MIN_WRITE_SIZE - size))
        {
            //read extra bytes b4 the beginning instead of the end
            logicalAddr = logicalAddr - (BYPASS_MIN_WRITE_SIZE - size);
            (void)memcpy(buffer + (BYPASS_MIN_WRITE_SIZE - size), input, size);
        }
        else
        {
            (void)memcpy(buffer, input, size);
        }

        input = buffer;
        size  = BYPASS_MIN_WRITE_SIZE;
    }
#endif // QLIB_SUPPORT_QPI

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform write                                                                                       */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     format,
                                                     FALSE,
                                                     TRUE,
                                                     blocking,
                                                     writeCMD,
                                                     &logicalAddr,
                                                     input,
                                                     size,
                                                     0,
                                                     NULL,
                                                     0,
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

#if defined QLIB_SUPPORT_QPI || defined QLIB_SUPPORT_OPI
/************************************************************************************************************
 * @brief       This routine switch the flash device between SPI, QPI, OPI and DOPI modes
 *
 * @param       qlibContext   qlib context object
 * @param[in]   busFormat     bus format
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_STD_SwitchFlashBusMode_L(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T busFormat)
{
    QLIB_BUS_MODE_T format        = QLIB_BUS_FORMAT_GET_MODE(busFormat);
    QLIB_BUS_MODE_T formatCurrent = qlibContext->busInterface.busMode;
#ifdef QLIB_SUPPORT_OPI
    BOOL dtr = QLIB_BUS_FORMAT_GET_DTR(busFormat);
    U8   mode[2];
    BOOL switchFromOPI = ((format != QLIB_BUS_MODE_8_8_8) && (formatCurrent == QLIB_BUS_MODE_8_8_8)) ? TRUE : FALSE;
    QLIB_STD_ADDR_MODE_T addrModeConfig = QLIB_STD_ADDR_MODE__3_BYTE;
#endif
    U8  cmd   = SPI_FLASH_CMD__NONE;
    U8* pMode = NULL;

#ifdef QLIB_SUPPORT_OPI
    if (switchFromOPI == TRUE)
    {
        U8 configReg = 0;
        /****************************************************************************************************
         * OPI enforce 4 byte addr mode. On exit OPI, flash returns to configured address mode
        ****************************************************************************************************/
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_CR(qlibContext, CONFIG_REG_ADDR__ADDRESS_MODE, &configReg));

        addrModeConfig = (READ_VAR_FIELD(configReg, SPI_FLASH__EXTENDED_CONFIGURATION_5_FIELD__AM) == 0u)
                             ? QLIB_STD_ADDR_MODE__4_BYTE
                             : QLIB_STD_ADDR_MODE__3_BYTE;

        qlibContext->addrMode = addrModeConfig;
    }
    else if (format == QLIB_BUS_MODE_8_8_8)
    {
        qlibContext->addrMode = QLIB_STD_ADDR_MODE__4_BYTE;
    }
    else
    {
        // dummy else for pc-lint
    }
#endif

    switch (format)
    {
#ifdef QLIB_SUPPORT_QPI
        case QLIB_BUS_MODE_4_4_4:
            if (QLIB_BUS_MODE_4_4_4 != formatCurrent)
            {
                cmd = SPI_FLASH_CMD__ENTER_QPI;
            }
            break;
#endif
#ifdef QLIB_SUPPORT_OPI
        case QLIB_BUS_MODE_8_8_8:
            if ((QLIB_BUS_MODE_8_8_8 != formatCurrent) || (dtr != qlibContext->busInterface.dtr))
            {
                cmd = SPI_FLASH_CMD__ENTER_OPI;
                if (dtr == TRUE)
                {
                    pMode = mode;
                    QLIB_DATA_EXTENSION_SET_DATA_OUT(qlibContext, mode, SPI_FLASH_CMD__ENTER_DOPI_MODE);
                }
            }
            break;
#endif
        default:
            // SPI
            if (QLIB_BUS_MODE_8_8_8 == formatCurrent || QLIB_BUS_MODE_4_4_4 == formatCurrent)
            {
                cmd = SPI_FLASH_CMD__ENTER_SPI;
            }
            break;
    }

    if (cmd != SPI_FLASH_CMD__NONE)
    {
        /****************************************************************************************************
         * Enter / Exit QPI / OPI
        ****************************************************************************************************/
        QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                         QLIB_STD_GET_BUS_MODE(qlibContext),
                                                         FALSE,
                                                         FALSE,
                                                         FALSE,
                                                         cmd,
                                                         NULL,
                                                         pMode,
                                                         pMode == NULL ? 0u : QLIB_DATA_EXTENSION_SIZE(qlibContext),
                                                         0,
                                                         NULL,
                                                         0,
                                                         NULL));
#if (QLIB_QPI_READ_DUMMY_CYCLES != Q2_DEFAULT_QPI_READ_DUMMY_CYCLES)
        if ((Q2_QPI_SET_READ_PARAMS(qlibContext) != 0u) && (cmd == SPI_FLASH_CMD__ENTER_QPI))
        {
            /*---------------------------------------------------------------------------------------------*/
            /* for W77Q (16Mb to 128Mb),                                                                   */
            /* set dummy cycles for QPI read commands with Set Read Parameters command.                    */
            /* In W77Q/T (256Mb to 1Gb) chip this is set in the CR register                                */
            /*---------------------------------------------------------------------------------------------*/

            U8 readParams = SPI_FLASH_SET_READ_PARAMETERS;
            QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                             QLIB_BUS_MODE_4_4_4,
                                                             FALSE,
                                                             FALSE,
                                                             FALSE,
                                                             SPI_FLASH_CMD__SET_READ_PARAMETERS,
                                                             NULL,
                                                             &readParams,
                                                             1,
                                                             0,
                                                             NULL,
                                                             0,
                                                             NULL));
        }
#endif
    }
    return QLIB_STATUS__OK;
}
#endif // QLIB_SUPPORT_QPI || QLIB_SUPPORT_OPI

static U8 QLIB_STD_GetReadCMD_L(QLIB_CONTEXT_T* qlibContext, U32* dummyCycles, QLIB_BUS_MODE_T* format)
{
    *format = qlibContext->busInterface.busMode;
    if ((W77Q_SUPPORT__1_1_4_SPI(qlibContext) != 0u) || (W77Q_SUPPORT_DUAL_SPI(qlibContext) != 0u))
    {
        if (TRUE == qlibContext->busInterface.dtr)
        {
            if (QLIB_BUS_MODE_1_1_4 == qlibContext->busInterface.busMode
#ifdef QLIB_SUPPORT_DUAL_SPI
                || QLIB_BUS_MODE_1_1_2 == qlibContext->busInterface.busMode
#endif //QLIB_SUPPORT_DUAL_SPI
            )
            {
                *format = QLIB_BUS_MODE_1_1_1;
            }
        }
    }

    return QLIB_STD_GetReadDummyCyclesCMD_L(qlibContext, *format, qlibContext->busInterface.dtr, dummyCycles);
}

/************************************************************************************************************
 * @brief       This routine calculates Write Command
 *
 * @param       qlibContext    internal context object
 * @param[out]  format         Interface format to use for performing the command
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static U8 QLIB_STD_GetWriteCMD_L(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_MODE_T* format)
{
    U8 cmd = 0;

    switch (qlibContext->busInterface.busMode)
    {
        case QLIB_BUS_MODE_1_1_4:
        case QLIB_BUS_MODE_1_4_4:
            if (W77Q_SUPPORT__1_1_4_SPI(qlibContext) != 0u)
            {
                *format = QLIB_BUS_MODE_1_1_4;
                cmd     = SPI_FLASH_CMD__PAGE_PROGRAM_1_1_4;
            }
            else
            {
                *format = QLIB_BUS_MODE_1_4_4;
                cmd     = SPI_FLASH_CMD__PAGE_PROGRAM_1_4_4;
            }
            break;
#ifdef QLIB_SUPPORT_QPI
        case QLIB_BUS_MODE_4_4_4:
            *format = QLIB_BUS_MODE_4_4_4;
            cmd     = SPI_FLASH_CMD__PAGE_PROGRAM;
            break;
#endif
        case QLIB_BUS_MODE_1_8_8:
            *format = QLIB_BUS_MODE_1_8_8;
            cmd     = SPI_FLASH_CMD__PAGE_PROGRAM_1_8_8;
            break;
#ifdef QLIB_SUPPORT_OPI
        case QLIB_BUS_MODE_8_8_8:
            *format = QLIB_BUS_MODE_8_8_8;
            cmd     = SPI_FLASH_CMD__PAGE_PROGRAM_8_8_8;
            break;
#endif
        default:
            *format = QLIB_BUS_MODE_1_1_1;
            cmd     = SPI_FLASH_CMD__PAGE_PROGRAM;
            break;
    }
    return cmd;
}

static QLIB_STATUS_T QLIB_STD_SetStatus_L(QLIB_CONTEXT_T* qlibContext, STD_FLASH_STATUS_T statusIn, STD_FLASH_STATUS_T* statusOut)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Write status register                                                                               */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     QLIB_STD_GET_BUS_MODE(qlibContext),
                                                     FALSE,
                                                     TRUE,
                                                     TRUE,
                                                     SPI_FLASH_CMD__WRITE_STATUS_REGISTER_1,
                                                     NULL,
                                                     &statusIn.SR1.asUint,
                                                     1,
                                                     0,
                                                     NULL,
                                                     0,
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     QLIB_STD_GET_BUS_MODE(qlibContext),
                                                     FALSE,
                                                     TRUE,
                                                     TRUE,
                                                     SPI_FLASH_CMD__WRITE_STATUS_REGISTER_2,
                                                     NULL,
                                                     &statusIn.SR2.asUint,
                                                     1,
                                                     0,
                                                     NULL,
                                                     0,
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     QLIB_STD_GET_BUS_MODE(qlibContext),
                                                     FALSE,
                                                     TRUE,
                                                     TRUE,
                                                     SPI_FLASH_CMD__WRITE_STATUS_REGISTER_3,
                                                     NULL,
                                                     &statusIn.SR3.asUint,
                                                     1,
                                                     0,
                                                     NULL,
                                                     0,
                                                     &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Read back status register                                                                           */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_STD_GetStatus_L(qlibContext, statusOut));

    return QLIB_STATUS__OK;
}

static U8 QLIB_STD_GetReadDummyCyclesCMD_L(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_MODE_T busMode, BOOL dtr, U32* dummyCycles)
{
    U8 cmd = 0;
    if (FALSE == dtr)
    {
        switch (busMode)
        {
            case QLIB_BUS_MODE_1_1_1:
                if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
                {
                    *dummyCycles = (U32)(qlibContext->fastReadDummy);
                }
                else
                {
                    *dummyCycles = SPI_FLASH_DUMMY_CYCLES__FAST_READ__1_1_X;
                }
                cmd = SPI_FLASH_CMD__READ_FAST__1_1_1;
                break;
#ifdef QLIB_SUPPORT_DUAL_SPI
            case QLIB_BUS_MODE_1_1_2:
                *dummyCycles = SPI_FLASH_DUMMY_CYCLES__FAST_READ__1_1_X;
                cmd          = SPI_FLASH_CMD__READ_FAST__1_1_2;
                break;

            case QLIB_BUS_MODE_1_2_2:
                *dummyCycles = SPI_FLASH_DUMMY_CYCLES__FAST_READ__1_2_2;
                cmd          = SPI_FLASH_CMD__READ_FAST__1_2_2;
                break;
#endif
            case QLIB_BUS_MODE_1_1_4:
                *dummyCycles = SPI_FLASH_DUMMY_CYCLES__FAST_READ__1_1_X;
                cmd          = SPI_FLASH_CMD__READ_FAST__1_1_4;
                break;

            case QLIB_BUS_MODE_1_4_4:
                if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
                {
                    *dummyCycles = (U32)(qlibContext->fastReadDummy);
                }
                else
                {
                    *dummyCycles = SPI_FLASH_DUMMY_CYCLES__FAST_READ__1_4_4;
                }
                cmd = SPI_FLASH_CMD__READ_FAST__1_4_4;
                break;
            case QLIB_BUS_MODE_1_8_8:
                *dummyCycles = (U32)(qlibContext->fastReadDummy);
                cmd          = SPI_FLASH_CMD__READ_FAST__1_8_8;
                break;
#ifdef QLIB_SUPPORT_QPI
            case QLIB_BUS_MODE_4_4_4:
                if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
                {
                    *dummyCycles = (U32)(qlibContext->fastReadDummy);
                }
                else
                {
                    *dummyCycles =
                        SPI_FLASH_DUMMY_CYCLES__FAST_READ__4_4_4; ///< 2 clocks default (configurable with 0xC0 command)
                }

                cmd = SPI_FLASH_CMD__READ_FAST__4_4_4; ///< QPI
                break;
#endif
#ifdef QLIB_SUPPORT_OPI
            case QLIB_BUS_MODE_8_8_8:
                *dummyCycles = (U32)(qlibContext->fastReadDummy);
                cmd          = SPI_FLASH_CMD__READ_FAST__8_8_8; ///< OPI
                break;

#endif
            default:
                cmd = SPI_FLASH_CMD__READ_DATA__1_1_1;
                break;
        }
    }
    else
    {
        switch (busMode)
        {
            case QLIB_BUS_MODE_1_1_1:
#ifdef QLIB_SUPPORT_DUAL_SPI
            case QLIB_BUS_MODE_1_1_2: // dtr is not support 1_1_2 format, fall back to single
#endif
                if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
                {
                    *dummyCycles = (U32)(qlibContext->fastReadDummy);
                }
                else
                {
                    *dummyCycles = SPI_FLASH_DUMMY_CYCLES__FAST_READ_DTR__1_1_1;
                }
                cmd = SPI_FLASH_CMD__READ_FAST_DTR__1_1_1;
                break;
#ifdef QLIB_SUPPORT_DUAL_SPI
            case QLIB_BUS_MODE_1_2_2:
#endif
            case QLIB_BUS_MODE_1_1_4: // dtr is not support 1_1_4 format, fall back to single
                *dummyCycles = SPI_FLASH_DUMMY_CYCLES__FAST_READ_DTR__1_2_2;
                cmd          = SPI_FLASH_CMD__READ_FAST_DTR__1_2_2;
                break;
            case QLIB_BUS_MODE_1_4_4:
                if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
                {
                    *dummyCycles = (U32)(qlibContext->fastReadDummy);
                }
                else
                {
                    *dummyCycles = SPI_FLASH_DUMMY_CYCLES__FAST_READ_DTR__1_4_4;
                }
                cmd = SPI_FLASH_CMD__READ_FAST_DTR__1_4_4;
                break;
            case QLIB_BUS_MODE_1_8_8:
                *dummyCycles = (U32)(qlibContext->fastReadDummy);
                cmd          = SPI_FLASH_CMD__READ_FAST_DTR__1_8_8;
                break;
#ifdef QLIB_SUPPORT_QPI
            case QLIB_BUS_MODE_4_4_4:
                if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
                {
                    *dummyCycles = (U32)(qlibContext->fastReadDummy);
                }
                else
                {
                    *dummyCycles =
                        SPI_FLASH_DUMMY_CYCLES__FAST_READ_DTR__4_4_4; ///< 2 clocks default (configurable with 0xC0 command)
                }
                cmd = SPI_FLASH_CMD__READ_FAST_DTR__4_4_4; ///< QPI
                break;
#endif
#ifdef QLIB_SUPPORT_OPI
            case QLIB_BUS_MODE_8_8_8:
                *dummyCycles = (U32)(qlibContext->fastReadDummy);
                cmd          = SPI_FLASH_CMD__READ_FAST_DTR__8_8_8;
                break;
#endif
            default:
                cmd = SPI_FLASH_CMD__READ_DATA__1_1_1;
                break;
        }
    }
    return cmd;
}

#ifdef QLIB_SUPPORT_QPI
static QLIB_STATUS_T QLIB_STD_CheckWritePrivilege_L(QLIB_CONTEXT_T* qlibContext, U32 logicalAddr)
{
    QLIB_POLICY_T policy;
    U32           sectionSize;
    U32           sectionID = _QLIB_SECTION_FROM_LOGICAL_ADDRESS(logicalAddr, qlibContext->addrSize);
    ACLR_T        aclr;
    U32           sectionWriteLock;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Get section ID in case of fallback                                                                  */
    /*-----------------------------------------------------------------------------------------------------*/
    sectionID = QLIB_FALLBACK_SECTION(qlibContext, sectionID);

    QLIB_STATUS_RET_CHECK(
        QLIB_SEC_GetSectionConfiguration(qlibContext, sectionID, NULL, &sectionSize, &policy, NULL, NULL, NULL));
    QLIB_ASSERT_RET(sectionSize > 0u, QLIB_STATUS__INVALID_PARAMETER);

    QLIB_ASSERT_RET(policy.plainAccessWriteEnable == 1u, QLIB_STATUS__DEVICE_PRIVILEGE_ERR);
    QLIB_ASSERT_RET(policy.writeProt != 1u, QLIB_STATUS__DEVICE_PRIVILEGE_ERR);
    QLIB_ASSERT_RET((policy.authPlainAccess != 1u) || ((QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[sectionID].plainEnabled &
                                                        QLIB_SECTION_PLAIN_EN_WR) != 0u),
                    QLIB_STATUS__DEVICE_PRIVILEGE_ERR);
    if (policy.rollbackProt == 1u)
    {
        QLIB_ASSERT_RET(_QLIB_OFFSET_FROM_LOGICAL_ADDRESS(logicalAddr, qlibContext->addrSize) >= (sectionSize / 2u),
                        QLIB_STATUS__DEVICE_PRIVILEGE_ERR);
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Check ACLR write lock                                                                               */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_ACLR(qlibContext, &aclr));
    sectionWriteLock = (U32)READ_VAR_BIT(READ_VAR_FIELD(aclr, QLIB_REG_ACLR__WR_LOCK), sectionID);
    QLIB_ASSERT_RET(sectionWriteLock != 1u, QLIB_STATUS__DEVICE_PRIVILEGE_ERR);

    return QLIB_STATUS__OK;
}
#endif // QLIB_SUPPORT_QPI

/************************************************************************************************************
 * @brief       This routine exits power down mode, check if the flash is run in SPI/QPI/OPI mode,
 *              and make sure flash is not busy and there is no connectivity issues.
 *
 * @param       qlibContext    qlib context
 * @param[out]  busFormat      detected bus format
 * @param[out]  deviceId       the detected device Id
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
#if !defined QLIB_INIT_AFTER_FLASH_POWER_UP && !defined QLIB_INIT_BUS_FORMAT
static QLIB_STATUS_T QLIB_STD_AutoSense_L(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T* busFormat, U8* deviceId)
{
    BOOL              busModeActive  = FALSE;
    U32               i              = 0;
    QLIB_BUS_FORMAT_T busFormatArr[] = {QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE)
#ifdef QLIB_SUPPORT_QPI
                                            ,
                                        QLIB_BUS_FORMAT(QLIB_BUS_MODE_4_4_4, FALSE)
#endif
#ifdef QLIB_SUPPORT_OPI
                                            ,
                                        QLIB_BUS_FORMAT(QLIB_BUS_MODE_8_8_8, FALSE),
                                        QLIB_BUS_FORMAT(QLIB_BUS_MODE_8_8_8, TRUE)
#endif
    };

    for (i = 0; i < sizeof(busFormatArr) / sizeof(QLIB_BUS_FORMAT_T); i++)
    {
        QLIB_STATUS_RET_CHECK(QLIB_STD_AutosenseIsBusModeActive_L(qlibContext, busFormatArr[i], &busModeActive, deviceId));
        if (busModeActive == TRUE)
        {
            *busFormat = busFormatArr[i];
            return QLIB_STATUS__OK;
        }
    }

    return QLIB_STATUS__CONNECTIVITY_ERR;
}
#endif

/************************************************************************************************************
 * @brief       This routine checks whether a bus format is supported and all its lines connected
 *
 * @param       qlibContext   qlib context object
 * @param       busFormat     bus format
 * @param       isActive      (OUT) TRUE if the bus format is supported. FALSE otherwise
 * @param       deviceId      (OUT) The detected flash device Id
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_STD_AutosenseIsBusModeActive_L(QLIB_CONTEXT_T*   qlibContext,
                                                         QLIB_BUS_FORMAT_T busFormat,
                                                         BOOL*             isActive,
                                                         U8*               deviceId)
{
    U8              id[3];
    U32             dummy   = 0;
    U32             i       = 0;
    QLIB_BUS_MODE_T busMode = QLIB_BUS_FORMAT_GET_MODE(busFormat);
    BOOL            dtr     = QLIB_BUS_FORMAT_GET_DTR(busFormat);
    QLIB_STATUS_T   ret     = QLIB_STATUS__OK;
    *isActive               = FALSE;

    switch (busMode)
    {
        case QLIB_BUS_MODE_1_1_1:
            QLIB_ASSERT_RET(dtr == FALSE, QLIB_STATUS__NOT_SUPPORTED);
            dummy = SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_SPI;
            break;
#ifdef QLIB_SUPPORT_QPI
        case QLIB_BUS_MODE_4_4_4:
            QLIB_ASSERT_RET(dtr == FALSE, QLIB_STATUS__NOT_SUPPORTED);
            dummy = W77Q_READ_JEDEC_QPI_DUMMY(qlibContext) != 0u ? SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_QPI_OPI : 0u;
            break;
#endif // QLIB_SUPPORT_QPI
#ifdef QLIB_SUPPORT_OPI
        case QLIB_BUS_MODE_8_8_8:
            dummy = SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_QPI_OPI;
            break;
#endif // QLIB_SUPPORT_OPI
        default:
            ret = QLIB_STATUS__NOT_SUPPORTED;
            break;
    }

    QLIB_STATUS_RET_CHECK(ret);
#ifndef QLIB_INIT_AFTER_FLASH_POWER_UP
    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     busMode,
                                                     dtr,
                                                     FALSE,
                                                     FALSE,
                                                     SPI_FLASH_CMD__RELEASE_POWER_DOWN,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     0,
                                                     NULL,
                                                     0,
                                                     NULL));
#endif

    // verify a few times to make sure we did not get the correct manufacturer ID by accident
    while (i < (U8)AUTOSENSE_NUM_RETRIES)
    {
        id[0] = 0;

        (void)QLIB_STD_execute_std_cmd_L(qlibContext,
                                         busMode,
                                         dtr,
                                         FALSE,
                                         FALSE,
                                         SPI_FLASH_CMD__READ_JEDEC,
                                         NULL,
                                         NULL,
                                         0,
                                         dummy,
                                         id,
                                         3,
                                         NULL);

        if ((U8)STD_FLASH_MANUFACTURER__WINBOND_SERIAL_FLASH != id[0])
        {
#if defined QLIB_SUPPORT_QPI && !defined QLIB_KNOWN_READ_JEDEC_QPI_DUMMY
            if (busMode == QLIB_BUS_MODE_4_4_4 && dummy == 0u)
            {
                dummy = SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_QPI_OPI;
                i     = 0;
                continue;
            }
            else
#endif
            {
                *isActive = FALSE;
                return QLIB_STATUS__OK;
            }
        }
        i++;
    }

    *isActive = TRUE;
    *deviceId = id[2] - 1u;
    return QLIB_STATUS__OK;
}


static QLIB_STATUS_T QLIB_STD_set_default_CR_L(QLIB_CONTEXT_T* qlibContext, U32 addr, U8 cr)
{
#if QLIB_NUM_OF_DIES > 1
    U32 origDie = qlibContext->activeDie;
#endif
    QLIB_STATUS_T     ret = QLIB_STATUS__OK;
    STD_FLASH_FLAGS_T fr;

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_FR(qlibContext, &fr));
    QLIB_ASSERT_RET(qlibContext->addrMode ==
                        ((fr.asStruct.AMF == 0x1u) ? QLIB_STD_ADDR_MODE__4_BYTE : QLIB_STD_ADDR_MODE__3_BYTE),
                    QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__set_default_CR(qlibContext, addr, cr));

#if QLIB_NUM_OF_DIES > 1
    // wait for all dies to finish processing the command
    do
    {
        // QLIB_STD_SetActiveDie both switch to new die and waits for it to be ready
        U8 nextDie = (qlibContext->activeDie + 1) % QLIB_GET_NUM_OF_DIES(qlibContext);
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_STD_SetActiveDie(qlibContext, nextDie, FALSE), ret, error);
    } while (qlibContext->activeDie != origDie);

    return QLIB_STATUS__OK;
error:
    (void)QLIB_STD_SetActiveDie(qlibContext, origDie, FALSE);
#endif
    return ret;
}

/************************************************************************************************************
 * @brief       This routine makes sure the flash is prepared for reset
 *
 * @param       qlibContext   qlib context object
 * @param       forceReset    if TRUE, the reset operation force the reset, this may cause a
 *                            a corrupted data.
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_STD_PrepareForResetFlash_L(QLIB_CONTEXT_T* qlibContext, BOOL forceReset)
{
    QLIB_STATUS_T ret = QLIB_STATUS__OK;
#if QLIB_NUM_OF_DIES > 1
    U8 startDie = qlibContext->activeDie;
#endif

    if (TRUE == forceReset)
    {
        /****************************************************************************************************
         * Release from power down in case of power down (no error checking is needed)
        ****************************************************************************************************/
        (void)QLIB_STD_Power(qlibContext, QLIB_POWER_UP);
    }
    else
    {
        /****************************************************************************************************
         * Release from power down in case of power down (no error checking is needed)
        ****************************************************************************************************/
        (void)QLIB_STD_Power(qlibContext, QLIB_POWER_UP);
#if QLIB_NUM_OF_DIES > 1
        U8 i;
        for (i = 1; i <= QLIB_GET_NUM_OF_DIES(qlibContext); i++)
        {
            U8 dieNum = ((startDie + i) % QLIB_GET_NUM_OF_DIES(qlibContext));
#endif
            /************************************************************************************************
             * check that the device is not in a critical state (avoid corrupted data)
            ************************************************************************************************/
            if (W77Q_FLAG_REGISTER(qlibContext) != 0u)
            {
                STD_FLASH_FLAGS_T fr;
                do
                {
                    QLIB_STATUS_RET_CHECK_GOTO(QLIB_CMD_PROC__get_FR(qlibContext, &fr), ret, error);
                } while (1u != READ_VAR_FIELD(fr.asUint, SPI_FLASH__FLAG_FIELD__RF) ||
                         0u != READ_VAR_FIELD(fr.asUint, SPI_FLASH__FLAG_FIELD__ESF) ||
                         0u != READ_VAR_FIELD(fr.asUint, SPI_FLASH__FLAG_FIELD__PSF));
            }
            else
            {
                STD_FLASH_STATUS_T status;
                do
                {
                    QLIB_STATUS_RET_CHECK(QLIB_STD_GetStatus_L(qlibContext, &status));
                } while (0u != READ_VAR_FIELD((U32)status.SR1.asUint, SPI_FLASH__STATUS_1_FIELD__BUSY) ||
                         0u != READ_VAR_FIELD((U32)status.SR2.asUint, SPI_FLASH__STATUS_2_FIELD__SUS));
            }
#if QLIB_NUM_OF_DIES > 1
            QLIB_STATUS_RET_CHECK_GOTO(QLIB_STD_SetActiveDie(qlibContext, dieNum, TRUE), ret, error);
        }
#endif
    }

    goto exit;

error:
#if QLIB_NUM_OF_DIES > 1
    QLIB_STD_SetActiveDie(qlibContext, startDie, FALSE);
#endif
exit:
    return ret;
}

/************************************************************************************************************
 * @brief       This routine resets the Flash
 *
 * @param       qlibContext   qlib context object
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_STD_ResetFlash_L(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_BUS_MODE_T preResetFormat = QLIB_STD_GET_BUS_MODE(qlibContext);
#ifndef EXCLUDE_Q2_4_BYTES_ADDRESS_MODE
    QLIB_STD_ADDR_MODE_T preResetAddrModeConfig = QLIB_STD_ADDR_MODE__3_BYTE;
#endif
    QLIB_REG_SSR_T ssr;
    QLIB_STATUS_T  status;
    INTERRUPTS_VAR_DECLARE(ints);
#ifndef EXCLUDE_Q2_4_BYTES_ADDRESS_MODE
    if (Q2_4_BYTES_ADDRESS_MODE(qlibContext) != 0u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_STD_GetConfiguredAddrMode(qlibContext, &preResetAddrModeConfig));
    }
#endif
    /********************************************************************************************************
     * Perform the reset flow ("reset enable" & "reset device") without interruptions
    ********************************************************************************************************/
    INTERRUPTS_SAVE_DISABLE(ints);

    QLIB_STATUS_RET_CHECK(QLIB_STD_execute_std_cmd_L(qlibContext,
                                                     preResetFormat,
                                                     FALSE,
                                                     FALSE,
                                                     FALSE,
                                                     SPI_FLASH_CMD__RESET_ENABLE,
                                                     NULL,
                                                     NULL,
                                                     0,
                                                     0,
                                                     NULL,
                                                     0,
                                                     NULL));

#ifdef QLIB_SUPPORT_QPI
    if (QLIB_BUS_MODE_4_4_4 == preResetFormat)
    {
        // Set the bus mode to quad because the reset command exits the QPI mode
        qlibContext->busInterface.busMode          = QLIB_BUS_MODE_1_4_4;
        qlibContext->busInterface.secureCmdsFormat = QLIB_BUS_MODE_1_1_1; // set single to be agnostic to CTAG mode
        QLIB_SEC_configOPS(qlibContext);
    }
#endif // QLIB_SUPPORT_QPI
#if defined QLIB_SUPPORT_OPI
    if (QLIB_BUS_MODE_8_8_8 == preResetFormat)
    {
        // Set the bus mode to octal because the reset command exits the OPI mode
        qlibContext->busInterface.busMode          = QLIB_BUS_MODE_1_8_8;
        qlibContext->busInterface.secureCmdsFormat = QLIB_BUS_MODE_1_8_8;
    }
#endif // QLIB_SUPPORT_OPI

    QLIB_STATUS_RET_CHECK(
        QLIB_STD_execute_std_cmd_L(qlibContext,
                                   preResetFormat,
                                   (preResetFormat == QLIB_BUS_MODE_8_8_8) && (qlibContext->busInterface.dtr == TRUE) ? TRUE
                                                                                                                      : FALSE,
                                   FALSE,
                                   TRUE,
                                   SPI_FLASH_CMD__RESET_DEVICE,
                                   NULL,
                                   NULL,
                                   0,
                                   0,
                                   NULL,
                                   0,
                                   NULL));

    INTERRUPTS_RESTORE(ints);

    // Wait while secure module is not-ready. It will also clear errors on SSR
    do
    {
        status = QLIB_CMD_PROC__get_SSR_UNSIGNED(qlibContext, &ssr, SSR_MASK__ALL_ERRORS);
    } while (QLIB_STATUS__OK != status || (READ_VAR_FIELD(ssr.asUint, QLIB_REG_SSR__BUSY) == 1u));
#if QLIB_NUM_OF_DIES > 1
    if (QLIB_GET_NUM_OF_DIES(qlibContext) > 1u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_STD_WaitForDiesAfterReset(qlibContext, NULL));
    }
#endif

#ifdef QLIB_SUPPORT_QPI
    /********************************************************************************************************
     * Reset will automatic exits QPI mode, enter back if was in QPI before reset
    ********************************************************************************************************/
    if (QLIB_BUS_MODE_4_4_4 == preResetFormat)
    {
        // Set the bus mode back to QPI because the reset command exits the QPI mode
        QLIB_STATUS_RET_CHECK(
            QLIB_STD_SwitchFlashBusMode_L(qlibContext, QLIB_BUS_FORMAT(QLIB_BUS_MODE_4_4_4, qlibContext->busInterface.dtr)));

        qlibContext->busInterface.busMode          = QLIB_BUS_MODE_4_4_4;
        qlibContext->busInterface.secureCmdsFormat = QLIB_BUS_MODE_4_4_4;
        QLIB_SEC_configOPS(qlibContext);
    }
#endif // QLIB_SUPPORT_QPI
#ifdef QLIB_SUPPORT_OPI
    /********************************************************************************************************
     * Reset will automatic exits OPI mode, enter back if was in OPI before reset
    ********************************************************************************************************/
    if (QLIB_BUS_MODE_8_8_8 == preResetFormat)
    {
        QLIB_STATUS_RET_CHECK(
            QLIB_STD_SwitchFlashBusMode_L(qlibContext, QLIB_BUS_FORMAT(QLIB_BUS_MODE_8_8_8, qlibContext->busInterface.dtr)));

        qlibContext->busInterface.busMode          = QLIB_BUS_MODE_8_8_8;
        qlibContext->busInterface.secureCmdsFormat = QLIB_BUS_MODE_8_8_8;
    }
#endif

    /********************************************************************************************************
     * Reset loads default address mode and fast read dummy cycles - set back to pre-reset values
    ********************************************************************************************************/
#ifndef EXCLUDE_Q2_4_BYTES_ADDRESS_MODE
    if (Q2_4_BYTES_ADDRESS_MODE(qlibContext) != 0u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_STD_SetAddressMode(qlibContext, preResetAddrModeConfig));
    }
#endif
    if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_STD_SetFastReadDummyCycles(qlibContext, qlibContext->fastReadDummy));
    }

    /********************************************************************************************************
     * Read SSR and check for errors (the above commands are executed without reading SSR)
    ********************************************************************************************************/
    // QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_SSR(qlibContext, NULL, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

#if !defined EXCLUDE_W77Q_INTERRUPTS && !defined Q2_API
/************************************************************************************************************
 * @brief       This routine gets interrupt types and convers it to the configuration register interrupt mask
 *
 * @param       intTypes   interrupt types to be set in mask
 *
 * @return      extended CR interrupt byte register (either byte 12 or 13 of CR)
************************************************************************************************************/
U8 interruptsToCR_L(QLIB_INTERRUPT_T* intTypes)
{
    U8 interruptMask = 0;

    if (intTypes->securityViolationInt == 1u)
    {
        SET_VAR_FIELD_8(interruptMask, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__SECURE_IMASK, 1u);
    }
    if (intTypes->watchdogTimerInt == 1u)
    {
        SET_VAR_FIELD_8(interruptMask, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__AWDT_IMASK, 1u);
    }
    if (intTypes->spiIntegrityInt == 1u)
    {
        SET_VAR_FIELD_8(interruptMask, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__SPI_INTG_IMASK, 1u);
    }
    if (intTypes->eccSingleErrInt == 1u)
    {
        SET_VAR_FIELD_8(interruptMask, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__SEC_IMASK, 1u);
    }
    if (intTypes->eccDoubleErrInt == 1u)
    {
        SET_VAR_FIELD_8(interruptMask, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__DED_IMASK, 1u);
    }
    if (intTypes->eccMultipleProgInt == 1u)
    {
        SET_VAR_FIELD_8(interruptMask, SPI_FLASH__EXTENDED_CONFIGURATION_12_13_FIELD__MULTP_IMASK, 1u);
    }
    return interruptMask;
}
#endif
