/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_tm.c
* @brief      This file contains QLIB Transaction Manager (TM) implementation
*
* ### project qlib
*
************************************************************************************************************/
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib_tm.h"
#include <trace.h>
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                              DEFINITIONS                                                */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_EXTENDED_ADDRESS_INIT_VAL 0x00

#ifdef QLIB_SUPPORT_OPI
#define QLIB_DTR_TO_DTR_FLAGS(qlibContext, cmdFormat, cmdDtr)                                                    \
    ((((TRUE == (qlibContext)->busInterface.dtr) || (TRUE == (cmdDtr))) && (QLIB_BUS_MODE_8_8_8 == (cmdFormat))) \
         ? QLIB_DTR__ALL                                                                                         \
         : ((cmdDtr) == FALSE ? QLIB_DTR__NO_DTR : QLIB_DTR__ADDR_DATA))
#else
#define QLIB_DTR_TO_DTR_FLAGS(qlibContext, cmdFormat, cmdDtr) ((cmdDtr) == FALSE ? QLIB_DTR__NO_DTR : QLIB_DTR__ADDR_DATA)
#endif

#ifdef QLIB_SUPPORT_XIP
// Can not use lib memcpy in TM as it is not mapped to RAM
#define QLIB_TM_MEMCPY(dst, src, size)                       \
    {                                                        \
        U32 __cnt;                                           \
        for (__cnt = 0; __cnt < (size); __cnt++)             \
        {                                                    \
            ((U8*)(dst))[__cnt] = ((const U8*)(src))[__cnt]; \
        }                                                    \
    }
#else
#define QLIB_TM_MEMCPY(dst, src, size) (void)memcpy((dst), (src), (size))
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                       LOCAL FUNCTION DECLARATIONS                                       */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
static _INLINE_ QLIB_STATUS_T QLIB_TM__OP0_get_ssr_L(QLIB_CONTEXT_T* qlibContext, QLIB_REG_SSR_T* ssr);
static _INLINE_ QLIB_STATUS_T QLIB_TM__OP1_write_ibuf_L(QLIB_CONTEXT_T* qlibContext, U32 ctag, const U32* buf, U32 size);
static _INLINE_ QLIB_STATUS_T QLIB_TM__OP2_read_obuf_L(QLIB_CONTEXT_T* qlibContext, U32* buf, U32 size);
static QLIB_STATUS_T          QLIB_TM_WriteEnable_L(QLIB_CONTEXT_T* qlibContext) __RAM_SECTION;
static QLIB_STATUS_T          QLIB_TM_WaitWhileBusySec_L(QLIB_CONTEXT_T* qlibContext,
                                                         BOOL            lastCmdWasOP1,
                                                         BOOL            waitAfterFlashReset,
                                                         QLIB_REG_SSR_T* userSsr) __RAM_SECTION;

static QLIB_STATUS_T QLIB_TM_IsWinbondDevice_L(QLIB_CONTEXT_T* qlibContext, BOOL* isWinbond) __RAM_SECTION;
static QLIB_STATUS_T QLIB_TM_WaitWhileBusyStd_L(QLIB_CONTEXT_T* qlibContext, BOOL waitAfterFlashReset) __RAM_SECTION;
static void QLIB_TM_GetAddressParams_L(QLIB_CONTEXT_T* qlibContext, U8 cmd, U8 addressMsb, U32* addrSize, BOOL* setExtended)
    __RAM_SECTION;
static QLIB_STATUS_T QLIB_TM_SetExtendedAddress_L(QLIB_CONTEXT_T* qlibContext, U8 extAddrByte) __RAM_SECTION;
#if defined QLIB_SUPPORT_QPI || defined QLIB_SUPPORT_OPI
static QLIB_STATUS_T QLIB_TM_SwitchSPIBusMode_L(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_MODE_T format, BOOL dtr) __RAM_SECTION;
#endif

static QLIB_STATUS_T QLIB_TM_GetSR1_L(QLIB_CONTEXT_T* qlibContext, U8* pSR1) __RAM_SECTION;
#if QLIB_NUM_OF_DIES > 1
static QLIB_STATUS_T QLIB_TM_SetActiveDie_L(QLIB_CONTEXT_T* qlibContext) __RAM_SECTION;
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_TM_Init(QLIB_CONTEXT_T* qlibContext)
{
    qlibContext->busInterface.dtr              = FALSE;
    qlibContext->busInterface.busMode          = QLIB_BUS_MODE_INVALID;
    qlibContext->busInterface.secureCmdsFormat = QLIB_BUS_MODE_INVALID;
    qlibContext->busInterface.busIsLocked      = FALSE;

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_TM_Connect(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T     ret          = QLIB_STATUS__OK;
    QLIB_INTERFACE_T* busInterface = &qlibContext->busInterface;
    INTERRUPTS_VAR_DECLARE(ints);

    INTERRUPTS_SAVE_DISABLE(ints);

    if (TRUE == busInterface->busIsLocked)
    {
        ret = QLIB_STATUS__DEVICE_BUSY;
    }
    else
    {
        busInterface->busIsLocked = TRUE;
    }

    INTERRUPTS_RESTORE(ints);

    return ret;
}

QLIB_STATUS_T QLIB_TM_Disconnect(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T     ret          = QLIB_STATUS__OK;
    QLIB_INTERFACE_T* busInterface = &qlibContext->busInterface;
    INTERRUPTS_VAR_DECLARE(ints);

    INTERRUPTS_SAVE_DISABLE(ints);

    if (FALSE == busInterface->busIsLocked)
    {
        ret = QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE;
    }
    else
    {
        busInterface->busIsLocked = FALSE;
    }

    INTERRUPTS_RESTORE(ints);

    return ret;
}

QLIB_STATUS_T QLIB_TM_Standard(QLIB_CONTEXT_T*   qlibContext,
                               QLIB_BUS_FORMAT_T busFormat,
                               BOOL              needWriteEnable,
                               BOOL              waitWhileBusy,
                               U8                cmd,
                               const U32*        address,
                               const U8*         writeData,
                               U32               writeDataSize,
                               U32               dummyCycles,
                               U8*               readData,
                               U32               readDataSize,
                               QLIB_REG_SSR_T*   ssr)
{
    QLIB_STATUS_T   ret             = QLIB_STATUS__OK;
    U32             addr            = 0;
    U32             addrSize        = 0;
    QLIB_BUS_MODE_T mode            = QLIB_BUS_FORMAT_GET_MODE(busFormat);
    U32             dtr             = QLIB_DTR_TO_DTR_FLAGS(qlibContext, mode, QLIB_BUS_FORMAT_GET_DTR(busFormat));
    U8              addressMsb      = 0;
    BOOL            setExtendedAddr = FALSE;
    U32             cmdSize         = QLIB_CMD_EXTENSION_SIZE(qlibContext);

    INTERRUPTS_VAR_DECLARE(ints);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    if (FALSE == qlibContext->busInterface.busIsLocked)
    {
        return QLIB_STATUS__NOT_CONNECTED;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Handle address                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    if (address != NULL)
    {
        addr       = *address;
        addressMsb = (U8)((addr >> 24) & 0xFFu);
        QLIB_TM_GetAddressParams_L(qlibContext, cmd, addressMsb, &addrSize, &setExtendedAddr);
    }
    else
    {
        addr     = 0;
        addrSize = 0;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Start atomic transaction                                                                            */
    /*-----------------------------------------------------------------------------------------------------*/
    INTERRUPTS_SAVE_DISABLE(ints);
    PLATFORM_XIP_DISABLE();

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set extended address bits                                                                           */
    /*-----------------------------------------------------------------------------------------------------*/
    if (setExtendedAddr == TRUE)
    {
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_SetExtendedAddress_L(qlibContext, addressMsb), ret, error);
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform write enable if needed                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    if (TRUE == needWriteEnable)
    {
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_WriteEnable_L(qlibContext), ret, error);
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform transaction                                                                                 */
    /*-----------------------------------------------------------------------------------------------------*/
    if (cmd != (U8)SPI_FLASH_CMD__NONE)
    {
        U8 cmdBuf[2u + 4u + FLASH_PAGE_SIZE]; // command + address + max data size
        QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, cmdBuf, cmd);

        if (addrSize > 0u)
        {
            cmdBuf[cmdSize]      = BYTE(addr, addrSize - 1u);
            cmdBuf[cmdSize + 1u] = BYTE(addr, addrSize - 2u);
            cmdBuf[cmdSize + 2u] = BYTE(addr, addrSize - 3u);
            /*lint -save -e774 */
            if (addrSize == 4u)
            {
                cmdBuf[cmdSize + 3u] = BYTE(addr, 0);
            }
        }

        // check that the data size is not bigger than the cmdBuf max data size allocated
        QLIB_ASSERT_RET((writeDataSize <= FLASH_PAGE_SIZE), QLIB_STATUS__INVALID_DATA_SIZE);

        if (writeDataSize > 0u)
        {
            QLIB_TM_MEMCPY(&cmdBuf[cmdSize + addrSize], writeData, writeDataSize);
        }

        QLIB_ASSERT_WITH_ERROR_GOTO((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                                   mode,
                                                                   dtr,
                                                                   cmdBuf,
                                                                   cmdSize,
                                                                   addrSize,
                                                                   writeDataSize,
                                                                   dummyCycles,
                                                                   readData,
                                                                   readDataSize) == 0),
                                    QLIB_STATUS__HARDWARE_FAILURE,
                                    ret,
                                    error);
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform wait while busy                                                                             */
    /*-----------------------------------------------------------------------------------------------------*/
    if (TRUE == waitWhileBusy || NULL != ssr)
    {
        if (Q2_BYPASS_HW_ISSUE_294(qlibContext) != 0u)
        {
            if ((U8)SPI_FLASH_CMD__PAGE_PROGRAM_1_1_4 == cmd)
            {
                QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_WaitWhileBusyStd_L(qlibContext, FALSE), ret, error);
            }
        }
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_WaitWhileBusySec_L(qlibContext,
                                                              FALSE,
                                                              (U8)SPI_FLASH_CMD__RESET_DEVICE == cmd ? TRUE : FALSE,
                                                              ssr),
                                   ret,
                                   error);
    }

#ifndef QLIB_NO_DIRECT_FLASH_ACCESS
    /*-----------------------------------------------------------------------------------------------------*/
    /* Restore extended address                                                                            */
    /*-----------------------------------------------------------------------------------------------------*/
    if (setExtendedAddr == TRUE)
    {
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_SetExtendedAddress_L(qlibContext, QLIB_EXTENDED_ADDRESS_INIT_VAL), ret, error);
    }
#endif
    /*-----------------------------------------------------------------------------------------------------*/
    /* End atomic transaction                                                                              */
    /*-----------------------------------------------------------------------------------------------------*/
    PLATFORM_XIP_ENABLE();
    INTERRUPTS_RESTORE(ints);

    return QLIB_STATUS__OK;

error:
#ifndef QLIB_NO_DIRECT_FLASH_ACCESS
    if (qlibContext->extendedAddr != (U8)QLIB_EXTENDED_ADDRESS_INIT_VAL)
    {
        (void)QLIB_TM_SetExtendedAddress_L(qlibContext, QLIB_EXTENDED_ADDRESS_INIT_VAL);
    }
#endif

    PLATFORM_XIP_ENABLE();
    INTERRUPTS_RESTORE(ints);

    return ret;
}

QLIB_STATUS_T QLIB_TM_Secure(QLIB_CONTEXT_T* qlibContext,
                             U32             ctag,
                             const U32*      writeData,
                             U32             writeDataSize,
                             U32*            readData,
                             U32             readDataSize,
                             QLIB_REG_SSR_T* ssr)
{
    QLIB_STATUS_T     ret          = QLIB_STATUS__OK;
    QLIB_INTERFACE_T* busInterface = &(qlibContext->busInterface);
#if defined QLIB_SUPPORT_QPI || defined QLIB_SUPPORT_OPI
    BOOL            switchToSPI = FALSE;
    QLIB_BUS_MODE_T origBusMode = busInterface->secureCmdsFormat;
#endif
    BOOL exitQuad     = FALSE;
    BOOL triggerReset = FALSE;
    U8   cmd          = QLIB_CMD_PROC__CTAG_GET_CMD(ctag);

    INTERRUPTS_VAR_DECLARE(ints);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    if (FALSE == busInterface->busIsLocked)
    {
        return QLIB_STATUS__NOT_CONNECTED;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Start atomic transaction                                                                            */
    /*-----------------------------------------------------------------------------------------------------*/
    INTERRUPTS_SAVE_DISABLE(ints);
    PLATFORM_XIP_DISABLE();

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform write phase                                                                                 */
    /*-----------------------------------------------------------------------------------------------------*/
    if (ctag != 0u)
    {
        triggerReset = ((qlibContext->multiTransactionCmd == 0u) &&
                                (((U8)QLIB_CMD_SEC_AWDT_EXPIRE == cmd) ||
#ifndef EXCLUDE_LMS
                                 (((U8)QLIB_CMD_SEC_LMS_EXEC == cmd) && (Q3_BYPASS_HW_ISSUE_282(qlibContext) == 0u) &&
                                  ((ctag & QLIB_TM_CTAG_SCR_NEED_RESET_MASK) != 0u)) ||
#endif
                                 (((U8)QLIB_CMD_SEC_SFORMAT == cmd || (U8)QLIB_CMD_SEC_FORMAT == cmd) &&
                                  (W77Q_FORMAT_MODE(qlibContext) != 0u) &&
                                  (READ_VAR_FIELD(BYTE(ctag, 1), QLIB_SEC_CMD_FORMAT_MODE_FIELD_RESET) != 0u)) ||
                                 (((U8)QLIB_CMD_SEC_SET_SCR == cmd || (U8)QLIB_CMD_SEC_SET_SCR_SWAP == cmd) &&
                                  (W77Q_SET_SCR_MODE(qlibContext) != 0u) &&
                                  (READ_VAR_FIELD(BYTE(ctag, 2), QLIB_SEC_CMD_SET_SCR_MODE_FIELD_RESET) != 0u)))
                            ? TRUE
                            : FALSE);

#ifdef QLIB_SUPPORT_QPI
        if (QLIB_BUS_MODE_4_4_4 == origBusMode)
        {
            if ((triggerReset == TRUE) || (Q2_BYPASS_HW_ISSUE_23(qlibContext) != 0u) ||
                (Q2_SEC_INST_SUPPORTED_IN_QPI(qlibContext->detectedDeviceID) == FALSE) ||

                ((Q2_BYPASS_HW_ISSUE_262(qlibContext) != 0u) && (cmd >= 0xE0u
                                                                 )) ||
                ((Q2_SEC_HW_VER_INST_SUPPORTED_IN_QPI(qlibContext->detectedDeviceID) == FALSE) &&
                 (cmd == (U8)QLIB_CMD_SEC_GET_VERSION))

            )
            {
                switchToSPI = TRUE;
            }
        }
#endif
#ifdef QLIB_SUPPORT_OPI
        if (QLIB_BUS_MODE_8_8_8 == origBusMode)
        {
            if (triggerReset == TRUE)
            {
                switchToSPI = TRUE;
            }
        }
#endif
#if defined QLIB_SUPPORT_QPI || defined QLIB_SUPPORT_OPI
        if (TRUE == switchToSPI)
        {
            QLIB_BUS_MODE_T newBusMode = (QLIB_BUS_MODE_4_4_4 == origBusMode) ? QLIB_BUS_MODE_1_4_4 : QLIB_BUS_MODE_1_8_8;
            QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_SwitchSPIBusMode_L(qlibContext, newBusMode, FALSE), ret, exit);

            // After exit QPI/OPI temporarily set the format to quad/octal so the follow transactions execute
            busInterface->busMode = newBusMode;
            if ((Q2_DEVCFG_CTAG_MODE(qlibContext) != 0u) ||
                (Q2_SEC_INST_BYPASS_CTAG_MODE(qlibContext->detectedDeviceID) == TRUE) || (cmd == (U8)QLIB_CMD_SEC_GET_VERSION))
            {
                // set to single SPI to suit both CTAG modes
                busInterface->secureCmdsFormat = QLIB_BUS_MODE_1_1_1;
                W77Q_SEC_INST__SET_FORMAT(busInterface->op0, QLIB_BUS_MODE_1_1_1);
                W77Q_SEC_INST__SET_FORMAT(busInterface->op1, QLIB_BUS_MODE_1_1_1);
                W77Q_SEC_INST__SET_FORMAT(busInterface->op2, QLIB_BUS_MODE_1_1_1);
            }
            else
            {
                busInterface->secureCmdsFormat = (newBusMode == QLIB_BUS_MODE_1_4_4 &&
                                                  ((W77Q_SUPPORT__1_1_4_SPI(qlibContext) != 0u) ||
                                                   (Q2_SEC_INST_SUPPORTED_IN_1_1_4(qlibContext->detectedDeviceID) == TRUE)))
                                                     ? QLIB_BUS_MODE_1_1_4
                                                     : newBusMode;
            }
        }
#endif

        if (Q2_BYPASS_HW_ISSUE_262(qlibContext) != 0u)
        {
            if (cmd >= 0xE0u)
            {
                // OP1 does not support 1-4-4 mode
                if (QLIB_BUS_MODE_1_4_4 == busInterface->secureCmdsFormat)
                {
                    exitQuad                       = TRUE;
                    busInterface->secureCmdsFormat = QLIB_BUS_MODE_1_1_1;
                    W77Q_SEC_INST__SET_FORMAT(busInterface->op0, QLIB_BUS_MODE_1_1_1);
                    W77Q_SEC_INST__SET_FORMAT(busInterface->op1, QLIB_BUS_MODE_1_1_1);
                }
            }
        }

        QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM__OP1_write_ibuf_L(qlibContext, ctag, writeData, writeDataSize), ret, exit);
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Wait while busy                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    if (ssr != NULL)
    {
        if ((Q2_BYPASS_HW_ISSUE_294(qlibContext) != 0u) && ((U8)QLIB_CMD_SEC_SAWR == cmd) &&
            (QLIB_BUS_MODE_1_1_4 == qlibContext->busInterface.secureCmdsFormat ||
             QLIB_BUS_MODE_1_4_4 == qlibContext->busInterface.secureCmdsFormat))
        {
            QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_WaitWhileBusyStd_L(qlibContext, FALSE), ret, exit);
        }

        QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_WaitWhileBusySec_L(qlibContext, TRUE, triggerReset, ssr), ret, exit);
        if (((Q2_BYPASS_HW_ISSUE_96(qlibContext) != 0u) && (cmd == (U32)QLIB_CMD_SEC_SAWR)) ||
            ((Q2_BYPASS_HW_ISSUE_98(qlibContext) != 0u) && (cmd == (U32)QLIB_CMD_SEC_CALC_SIG)) ||
            ((Q2_OPEN_CLOSE_SESSION_2ND_GET_SSR(qlibContext) != 0u) &&
             ((cmd == (U8)QLIB_CMD_SEC_SESSION_OPEN) || (cmd == (U8)QLIB_CMD_SEC_SESSION_CLOSE))) ||
            ((Q3_BYPASS_HW_ISSUE_OCTAL_SESSION_OPEN_SECOND_SSR(qlibContext) != 0u) && (cmd == (U8)QLIB_CMD_SEC_SESSION_OPEN)))
        {
            QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_WaitWhileBusySec_L(qlibContext, TRUE, FALSE, ssr), ret, exit);
        }
    }
    if ((Q2_BYPASS_HW_ISSUE_262(qlibContext) != 0u) && exitQuad == TRUE)
    {
        // Set the original format back
        busInterface->secureCmdsFormat = QLIB_BUS_MODE_1_4_4;
        W77Q_SEC_INST__SET_FORMAT(busInterface->op0, QLIB_BUS_MODE_1_4_4);
        W77Q_SEC_INST__SET_FORMAT(busInterface->op1, QLIB_BUS_MODE_1_4_4);
    }
    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform read phase if needed                                                                        */
    /*-----------------------------------------------------------------------------------------------------*/
    if (0u != readDataSize)
    {
        if ((ssr != NULL) &&
            (0u == READ_VAR_FIELD(ssr->asUint, QLIB_REG_SSR__RESP_READY))
        )
        {
            if ((Q2_BYPASS_HW_ISSUE_98(qlibContext) != 0u) && (cmd == (U32)QLIB_CMD_SEC_CALC_SIG))
            {
                QLIB_REG_SSR_T tempSSR = *ssr;

                while ((READ_VAR_FIELD(tempSSR.asUint, QLIB_REG_SSR__ERR) == 0u) &&
                       (READ_VAR_FIELD(tempSSR.asUint, QLIB_REG_SSR__RESP_READY) == 0u))

                {
                    QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM__OP0_get_ssr_L(qlibContext, &tempSSR), ret, exit);
                }

                SET_VAR_FIELD_32(ssr->asUint, QLIB_REG_SSR__RESP_READY, READ_VAR_FIELD(tempSSR.asUint, QLIB_REG_SSR__RESP_READY));
                SET_VAR_FIELD_32(ssr->asUint, QLIB_REG_SSR__ERR, READ_VAR_FIELD(tempSSR.asUint, QLIB_REG_SSR__ERR));

                if (READ_VAR_FIELD(ssr->asUint, QLIB_REG_SSR__ERR) == 1u)
                {
                    goto exit;
                }
            }
            else
            {
                /*-----------------------------------------------------------------------------------------*/
                /* Emulate HW error                                                                        */
                /*-----------------------------------------------------------------------------------------*/
                SET_VAR_FIELD_32(ssr->asUint, QLIB_REG_SSR__ERR, 1u);
                goto exit;
            }
        }
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM__OP2_read_obuf_L(qlibContext, readData, readDataSize), ret, exit);
    }

    if (qlibContext->multiTransactionCmd == 0u) //if TRUE this comes from Read/Write commands
    {
        // Check if need post command operations
        if (W77Q_SET_SCR_MODE(qlibContext) == 0u && (cmd == (U8)QLIB_CMD_SEC_SET_SCR || cmd == (U8)QLIB_CMD_SEC_SET_SCR_SWAP))
        {
            if (NULL != ssr && 0u == READ_VAR_FIELD(ssr->asUint, QLIB_REG_SSR__ERR))
            {
                U8 mode = BYTE(ctag, 2);
                if (1u == READ_VAR_FIELD(mode, QLIB_SEC_CMD_SET_SCR_MODE_FIELD_RESET))
                {
                    CORE_RESET();
                }
                if (1u == READ_VAR_FIELD(mode, QLIB_SEC_CMD_SET_SCR_MODE_FIELD_RELOAD))
                {
                    ctag = MAKE_32_BIT(QLIB_CMD_SEC_INIT_SECTION_PA, BYTE(ctag, 1), 0, 0);
                    QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM__OP1_write_ibuf_L(qlibContext, ctag, writeData, writeDataSize), ret, exit);
                    QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_WaitWhileBusySec_L(qlibContext, TRUE, FALSE, ssr), ret, exit);
                }
            }
        }
        if ((Q3_BYPASS_HW_ISSUE_282(qlibContext) != 0u) && ((U8)QLIB_CMD_SEC_LMS_EXEC == cmd))
        {
            if (NULL != ssr && 0u == READ_VAR_FIELD(ssr->asUint, QLIB_REG_SSR__ERR))
            {
                if ((ctag & QLIB_TM_CTAG_SCR_NEED_RESET_MASK) != 0u)
                {
                    CORE_RESET();
                }
                if ((ctag & QLIB_TM_CTAG_SCR_NEED_GRANT_PA_MASK) != 0u)
                {
                    U8  sectionId      = QLIB_KEY_MNGR__GET_KEY_SECTION(BYTE(ctag, 1));
                    U32 plainGrantCtag = MAKE_32_BIT((U8)QLIB_CMD_SEC_PA_GRANT_PLAIN, sectionId, 0u, 0u);
                    QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM__OP1_write_ibuf_L(qlibContext, plainGrantCtag, NULL, 0), ret, exit);
                    QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_WaitWhileBusySec_L(qlibContext, TRUE, FALSE, ssr), ret, exit);
                }
            }
        }
    }

#if defined QLIB_SUPPORT_QPI || defined QLIB_SUPPORT_OPI
    if (switchToSPI == TRUE)
    {
        // Enter back to original mode
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_TM_SwitchSPIBusMode_L(qlibContext, origBusMode, busInterface->dtr), ret, exit);

        // After entering back to QPI set the format back to QPI
        busInterface->secureCmdsFormat = origBusMode;
        busInterface->busMode          = origBusMode;
        W77Q_SEC_INST__SET_FORMAT(busInterface->op0, origBusMode);
        W77Q_SEC_INST__SET_FORMAT(busInterface->op1, origBusMode);
        W77Q_SEC_INST__SET_FORMAT(busInterface->op2, origBusMode);
    }
#endif // QLIB_SUPPORT_QPI

exit:

    /*-----------------------------------------------------------------------------------------------------*/
    /* End atomic transaction                                                                              */
    /*-----------------------------------------------------------------------------------------------------*/
    PLATFORM_XIP_ENABLE();
    INTERRUPTS_RESTORE(ints);
    return ret;
}

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                             LOCAL FUNCTIONS                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This routine retrieve SSR (Secure Status Register)
 *
 * @param[in]   qlibContext    Context
 * @param[out]  ssr            SSR (Secure Status Register) value (32bit)
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static _INLINE_ QLIB_STATUS_T QLIB_TM__OP0_get_ssr_L(QLIB_CONTEXT_T* qlibContext, QLIB_REG_SSR_T* ssr)
{
    U8              op0[2];
    QLIB_BUS_MODE_T format      = qlibContext->busInterface.secureCmdsFormat;
    U8              cmd         = qlibContext->busInterface.op0;
    U32             cmdSize     = QLIB_CMD_EXTENSION_SIZE(qlibContext);
    BOOL            dtr         = (Q2_OP0_OP2_DTR_FEATURE(qlibContext) != 0u) ? qlibContext->busInterface.dtr : FALSE;
    U32             dummyCycles = (U32)W77Q_SEC_INST_DUMMY_CYCLES__OP0(qlibContext, dtr, qlibContext->detectedDeviceID);
#ifdef QLIB_SUPPORT_DUAL_SPI
    /*-----------------------------------------------------------------------------------------------------*/
    /* OP0 doesn't support DUAL                                                                            */
    /*-----------------------------------------------------------------------------------------------------*/
    if ((format == QLIB_BUS_MODE_1_1_2) || (format == QLIB_BUS_MODE_1_2_2))
    {
        format = QLIB_BUS_MODE_1_1_1;
        cmd    = W77Q_SEC_INST__MAKE(W77Q_SEC_INST__OP0, QLIB_BUS_MODE_1_1_1, BOOLEAN_TO_INT(dtr));
    }
#endif
    QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, op0, cmd);
    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform GET_SSR command                                                                             */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                   format,
                                                   QLIB_DTR_TO_DTR_FLAGS(qlibContext, format, dtr),
                                                   op0,
                                                   cmdSize,
                                                   0,
                                                   0,
                                                   dummyCycles,
                                                   (U8*)&(ssr->asUint),
                                                   sizeof(U32)) == 0),
                    QLIB_STATUS__HARDWARE_FAILURE);
    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine write secure command to chip input buffer
 *
 * @param[in]   qlibContext    Context
 * @param[in]   ctag           Secure Command CTAG value
 * @param[in]   buf            data buffer
 * @param       size           data buffer size (in bytes)
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static _INLINE_ QLIB_STATUS_T QLIB_TM__OP1_write_ibuf_L(QLIB_CONTEXT_T* qlibContext, U32 ctag, const U32* buf, U32 size)
{
    U32             cmdSize;
    U32             ctagSize;
    QLIB_BUS_MODE_T secFormat;
    U8              op1;
    QLIB_STATUS_T   ret;
    U8              dataOutStream[sizeof(U16) + (U8)W77Q_CTAG_SIZE_BYTE + W77Q_MAX_IBUF_SIZE_BYTE]; // op1 + ctag + data
    const U32*      pData;

#if defined(QLIB_MAX_SPI_OUTPUT_SIZE)
    U8              splitIbufCmd[2];
    QLIB_BUS_MODE_T stdFormat;
    U32             maxSpiOutputSize;
#if defined(QLIB_SPI_OPTIMIZATION_ENABLED) && (QLIB_MAX_SPI_OUTPUT_SIZE < W77Q_CTAG_SIZE_BYTE + QLIB_SEC_WRITE_PAGE_SIZE_BYTE + 8)
    // when there is optimization, we do not allow write buffer split (max size includes ctag + write_page + sig)
#error When SPI optimization is defined, we do not allow write buffer split
#endif
    U32  totalSize;
    BOOL split;
#if QLIB_MAX_SPI_OUTPUT_SIZE < 4
#error in W77Q (16Mb to 128Mb) HCD QLIB_MAX_SPI_OUTPUT_SIZE should be at least 4 bytes, on W77Q/T (256Mb to 1Gb) it should contain the ctag and 32 bit aligned data
#endif

    maxSpiOutputSize = (U32)QLIB_MAX_SPI_OUTPUT_SIZE;
#endif

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(size <= W77Q_MAX_IBUF_SIZE_BYTE, QLIB_STATUS__INVALID_DATA_SIZE);
#if defined(QLIB_MAX_SPI_OUTPUT_SIZE)
    // sending CTAG + DATA on SPI
    QLIB_ASSERT_RET((Q2_SPLIT_IBUF_FEATURE(qlibContext) != 0u) || (0u == maxSpiOutputSize) ||
                        ((U32)W77Q_CTAG_SIZE_BYTE + size <= maxSpiOutputSize),
                    QLIB_STATUS__INVALID_DATA_SIZE);
    totalSize = size;
    split     = FALSE;
#endif

    ret       = QLIB_STATUS__OK;
    pData     = buf;
    ctagSize  = W77Q_CTAG_SIZE_BYTE;
    cmdSize   = QLIB_CMD_EXTENSION_SIZE(qlibContext);
    secFormat = qlibContext->busInterface.secureCmdsFormat;
#ifdef QLIB_SUPPORT_DUAL_SPI
    /*-----------------------------------------------------------------------------------------------------*/
    /* The bellow code block bypasses OP1 specific limitations, since op1 doesn't support dual             */
    /*-----------------------------------------------------------------------------------------------------*/
    if ((secFormat == QLIB_BUS_MODE_1_1_2) || (secFormat == QLIB_BUS_MODE_1_2_2))
    {
        secFormat = QLIB_BUS_MODE_1_1_1; // Bus-mode: single
        op1       = 0xA1;                // OP code:  single
    }
    else
#endif
    {
        op1 = qlibContext->busInterface.op1;
    }
    QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, dataOutStream, op1);
    dataOutStream[cmdSize]      = BYTE(ctag, 0);
    dataOutStream[cmdSize + 1u] = BYTE(ctag, 1);
    dataOutStream[cmdSize + 2u] = BYTE(ctag, 2);
    dataOutStream[cmdSize + 3u] = BYTE(ctag, 3);

#if defined(QLIB_MAX_SPI_OUTPUT_SIZE)
    // sending CTAG + DATA on SPI
    if ((Q2_SPLIT_IBUF_FEATURE(qlibContext) != 0u) && (0u != maxSpiOutputSize) && (W77Q_CTAG_SIZE_BYTE + size > maxSpiOutputSize))
    {
        stdFormat = QLIB_STD_GET_BUS_MODE(qlibContext);
        QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, splitIbufCmd, W77Q_SEC_INST__WR_IBUF_START);
        QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                       stdFormat,
                                                       QLIB_DTR_TO_DTR_FLAGS(qlibContext, stdFormat, FALSE),
                                                       splitIbufCmd,
                                                       cmdSize,
                                                       0,
                                                       0,
                                                       0,
                                                       NULL,
                                                       0) == 0),
                        QLIB_STATUS__HARDWARE_FAILURE);

        split = TRUE;
        // Each OP1 instruction must be terminated on a 32-bit word boundary
        if (0u != maxSpiOutputSize)
        {
            size = ROUND_DOWN(maxSpiOutputSize - W77Q_CTAG_SIZE_BYTE, 4u);
        }
    }
    do
    {
#endif
        if ((NULL != pData) && (size > 0u))
        {
            QLIB_TM_MEMCPY(((void*)&dataOutStream[cmdSize + ctagSize]), ((const void*)pData), size);
        }


        /*-------------------------------------------------------------------------------------------------*/
        /* Perform Write IBUF command                                                                      */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_ASSERT_WITH_ERROR_GOTO((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                                   secFormat,
                                                                   QLIB_DTR_TO_DTR_FLAGS(qlibContext, secFormat, FALSE),
                                                                   dataOutStream,
                                                                   cmdSize,
                                                                   ctagSize,
                                                                   size,
                                                                   0,
                                                                   NULL,
                                                                   0) == 0),
                                    QLIB_STATUS__HARDWARE_FAILURE,
                                    ret,
                                    exit);

#if defined(QLIB_MAX_SPI_OUTPUT_SIZE)
        totalSize -= size;
        if (W77Q_SPLIT_IBUF_CTAG_ONLY_FIRST(qlibContext) != 0u)
        {
            ctagSize = 0;
        }
        else
        {
            if ((pData == buf) && (totalSize > 0u))
            {
                // SW should set ctag of next commands to 0 for future compatibility
                dataOutStream[cmdSize]      = 0;
                dataOutStream[cmdSize + 1u] = 0;
                dataOutStream[cmdSize + 2u] = 0;
                dataOutStream[cmdSize + 3u] = 0;
            }
        }

        pData += (size / (U32)sizeof(U32));
        size = ((0u != maxSpiOutputSize) && (maxSpiOutputSize - ctagSize < totalSize))
                   ? ROUND_DOWN(maxSpiOutputSize - ctagSize, 4u)
                   : totalSize;
    } while (size > 0u);
#endif

exit:
#if defined(QLIB_MAX_SPI_OUTPUT_SIZE)
    if ((Q2_SPLIT_IBUF_FEATURE(qlibContext) != 0u) && (split == TRUE) && (0u != maxSpiOutputSize))
    {
        QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, splitIbufCmd, W77Q_SEC_INST__WR_IBUF_END);

        stdFormat = QLIB_STD_GET_BUS_MODE(qlibContext);
        QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                       stdFormat,
                                                       QLIB_DTR_TO_DTR_FLAGS(qlibContext, stdFormat, FALSE),
                                                       splitIbufCmd,
                                                       cmdSize,
                                                       0,
                                                       0,
                                                       0,
                                                       NULL,
                                                       0) == 0),
                        QLIB_STATUS__HARDWARE_FAILURE);
    }
#endif
    return ret;
}

/************************************************************************************************************
 * @brief       This routine reads secure command response from the chip output buffer
 *
 * @param[in]   qlibContext    Context
 * @param[out]  buf            data buffer
 * @param[in]   size           data buffer size (in bytes)
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static _INLINE_ QLIB_STATUS_T QLIB_TM__OP2_read_obuf_L(QLIB_CONTEXT_T* qlibContext, U32* buf, U32 size)
{
#if defined(QLIB_MAX_SPI_INPUT_SIZE)
    U8* tempBuf;
#endif // QLIB_MAX_SPI_INPUT_SIZE
    U32 cmdSize = QLIB_CMD_EXTENSION_SIZE(qlibContext);
#if !defined(QLIB_MAX_SPI_INPUT_SIZE)
    int status;
#endif
    U8 cmd[2];
    QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, cmd, qlibContext->busInterface.op2);

#if !defined(QLIB_MAX_SPI_INPUT_SIZE)
    {
        status = PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                               qlibContext->busInterface.secureCmdsFormat,
                                               QLIB_DTR_TO_DTR_FLAGS(qlibContext,
                                                                     qlibContext->busInterface.secureCmdsFormat,
                                                                     (Q2_OP0_OP2_DTR_FEATURE(qlibContext) != 0u)
                                                                         ? qlibContext->busInterface.dtr
                                                                         : FALSE),
                                               cmd,
                                               cmdSize,
                                               0,
                                               0,
                                               W77Q_SEC_INST_DUMMY_CYCLES__OP2,
                                               (U8*)buf,
                                               size);
        if (status != 0)
        {
            return QLIB_STATUS__COMMUNICATION_ERR;
        }
        else
        {
            return QLIB_STATUS__OK;
        }
    }
#endif
#if defined(QLIB_MAX_SPI_INPUT_SIZE)
#if defined(QLIB_SPI_OPTIMIZATION_ENABLED) && (QLIB_MAX_SPI_INPUT_SIZE < W77Q_CTAG_SIZE_BYTE + QLIB_SEC_READ_PAGE_SIZE_BYTE + 8)
    // when there is optimization, we do not allow split of read buffer (max size includes ctag + write_page + sig)
#error When SPI optimization is defined, we do not allow split of read buffer
#endif
    {
        /*-----------------------------------------------------------------------------------------------------*/
        /* Perform multiple small Read OBUF command for the case that SPI bus not supporting big transactions  */
        /*-----------------------------------------------------------------------------------------------------*/
        tempBuf = (U8*)buf;

        while (0u != size)
        {
            U32 transactionSize = MIN(size, ROUND_DOWN((U32)QLIB_MAX_SPI_INPUT_SIZE, 4u));

            QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                           qlibContext->busInterface.secureCmdsFormat,
                                                           QLIB_DTR_TO_DTR_FLAGS(qlibContext,
                                                                                 qlibContext->busInterface.secureCmdsFormat,
                                                                                 (Q2_OP0_OP2_DTR_FEATURE(qlibContext) != 0u)
                                                                                     ? qlibContext->busInterface.dtr
                                                                                     : FALSE),
                                                           cmd,
                                                           cmdSize,
                                                           0,
                                                           0,
                                                           W77Q_SEC_INST_DUMMY_CYCLES__OP2,
                                                           tempBuf,
                                                           transactionSize) == 0),
                            QLIB_STATUS__HARDWARE_FAILURE);

            // prepare next iteration
            tempBuf += transactionSize;
            size -= transactionSize;
        }
    }
    return QLIB_STATUS__OK;
#endif // QLIB_MAX_SPI_INPUT_SIZE
}

/************************************************************************************************************
 * @brief       This routine waits till Flash becomes not busy, and optionally returns the Flash
 *              secure status
 *
 * @param[in]   qlibContext         Pointer to the qlib context
 * @param[in]   lastCmdWasOP1       whether the last executed command was OP1
 * @param[in]   waitAfterFlashReset wait till flash finish reset before getting its secure status
 * @param[out]  userSsr             Output SSR value (can be NULL)
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_TM_WaitWhileBusySec_L(QLIB_CONTEXT_T* qlibContext,
                                                BOOL            lastCmdWasOP1,
                                                BOOL            waitAfterFlashReset,
                                                QLIB_REG_SSR_T* userSsr)
{
    QLIB_REG_SSR_T  ssr   = {0};
    QLIB_REG_SSR_T* ssr_p = (userSsr != NULL) ? userSsr : &ssr;

#ifdef QLIB_SUPPORT_QPI
    BOOL exitQpi = FALSE;

    /*-----------------------------------------------------------------------------------------------------*/
    /* OP0 doesn't support QPI, exit QPI for op0                                                           */
    /*-----------------------------------------------------------------------------------------------------*/
    if (QLIB_BUS_MODE_4_4_4 == qlibContext->busInterface.secureCmdsFormat &&
        ((Q2_BYPASS_HW_ISSUE_23(qlibContext) != 0u) || Q2_SEC_INST_SUPPORTED_IN_QPI(qlibContext->detectedDeviceID) == FALSE))
    {
        U8 sr1;
        do
        {
            QLIB_STATUS_RET_CHECK(QLIB_TM_GetSR1_L(qlibContext, &sr1));
        } while (1u == READ_VAR_FIELD(sr1, SPI_FLASH__STATUS_1_FIELD__BUSY));

        exitQpi = TRUE;
        QLIB_STATUS_RET_CHECK(QLIB_TM_SwitchSPIBusMode_L(qlibContext, QLIB_BUS_MODE_1_1_1, FALSE));

        // After exit QPI temporarily set the format to quad so the follow transactions execute in quad
        qlibContext->busInterface.secureCmdsFormat = QLIB_BUS_MODE_1_1_4;
        qlibContext->busInterface.busMode          = QLIB_BUS_MODE_1_1_1;
    }
#endif // QLIB_SUPPORT_QPI
    if (waitAfterFlashReset == TRUE)
    {
        /*-------------------------------------------------------------------------------------------------*/
        /* Reset takes up to 35us to complete. Test flash is up using standard commands (so SSR error bits */
        /* of previous command will not be cleared)                                                        */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_TM_WaitWhileBusyStd_L(qlibContext, TRUE));
    }
    /*-----------------------------------------------------------------------------------------------------*/
    /* Wait while busy by polling SSR                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    do
    {

        /*-------------------------------------------------------------------------------------------------*/
        /* Read next SSR                                                                                   */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_TM__OP0_get_ssr_L(qlibContext, ssr_p));

        /*-------------------------------------------------------------------------------------------------*/
        /* Check if flash is alive                                                                         */
        /*-------------------------------------------------------------------------------------------------*/
        if (ssr_p->asUint == MAX_U32 || ssr_p->asUint == 0u)
        {
#ifdef QLIB_SUPPORT_QPI
            // in case of power-down mode all commands are ignored and we are still in QPI
            if (exitQpi == TRUE)
            {
                qlibContext->busInterface.secureCmdsFormat = QLIB_BUS_MODE_4_4_4;
            }
#endif // QLIB_SUPPORT_QPI
            return QLIB_STATUS__CONNECTIVITY_ERR;
        }

        /*-------------------------------------------------------------------------------------------------*/
        /* if response is there we do not keep on waiting till busy goes off, We can use op2 right away    */
        /* SSR__RESP_READY_BIT is cleared by OP1 command                                                   */
        /*-------------------------------------------------------------------------------------------------*/
        if (((ssr_p->asUint & SSR__RESP_READY_BIT) != 0u) && (lastCmdWasOP1 == TRUE))
        {
            ssr_p->asUint &= (U32)(~SSR__BUSY_BIT);
        }
    } while ((ssr_p->asUint & SSR__BUSY_BIT) != 0u);
    if (waitAfterFlashReset == TRUE)
    {
        /*-------------------------------------------------------------------------------------------------*/
        /* clear ignore bit, since it might be set by standard commands while flash did not finish reset.  */
        /* Note: if the command that triggered the reset was ignored, we would not have indication for it  */
        /*-------------------------------------------------------------------------------------------------*/
        ssr_p->asUint &= (U32)(~(SSR__IGNORE_BIT | SSR__ERR_BIT));
    }

#ifdef QLIB_SUPPORT_QPI
    /*-----------------------------------------------------------------------------------------------------*/
    /* OP0 doesn't support QPI, if we exit QPI for op0 now need to enter back                              */
    /*-----------------------------------------------------------------------------------------------------*/
    if (exitQpi == TRUE)
    {
        QLIB_STATUS_RET_CHECK(QLIB_TM_SwitchSPIBusMode_L(qlibContext, QLIB_BUS_MODE_4_4_4, FALSE));

        // After entering back to QPI set the format back to quad
        qlibContext->busInterface.secureCmdsFormat = QLIB_BUS_MODE_4_4_4;
        qlibContext->busInterface.busMode          = QLIB_BUS_MODE_4_4_4;
    }
#endif // QLIB_SUPPORT_QPI
    return QLIB_STATUS__OK;
}

/************************************************************************************************************
* @brief       This routine read the JEDEC ID and check if is Winbond device
*
* @param[in]   qlibContext    pointer to qlib context
* @param[out]  isWinbond      TRUE if the flash is winbond flash
*
* @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_TM_IsWinbondDevice_L(QLIB_CONTEXT_T* qlibContext, BOOL* isWinbond)
{
    U32             cmdSize = QLIB_CMD_EXTENSION_SIZE(qlibContext);
    U8              cmd[2];
    U8              jedecID[1];
    QLIB_BUS_MODE_T busMode = QLIB_STD_GET_BUS_MODE(qlibContext);
    U32             dummyCycles =
        (busMode == QLIB_BUS_MODE_1_1_1 ? SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_SPI
                                        : ((QLIB_BUS_MODE_4_4_4 == busMode && W77Q_READ_JEDEC_QPI_DUMMY(qlibContext) == 0u)
                                               ? 0u
                                               : SPI_FLASH_DUMMY_CYCLES__READ_JEDEC_QPI_OPI));
    QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, cmd, SPI_FLASH_CMD__READ_JEDEC);

    *isWinbond = FALSE;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Read the JEDEC from flash device                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/

    QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                   busMode,
                                                   QLIB_DTR_TO_DTR_FLAGS(qlibContext, busMode, FALSE),
                                                   cmd,
                                                   cmdSize,
                                                   0,
                                                   0,
                                                   dummyCycles,
                                                   jedecID,
                                                   sizeof(jedecID)) == 0),
                    QLIB_STATUS__HARDWARE_FAILURE);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Check if it is winbond manufacturer id                                                              */
    /*-----------------------------------------------------------------------------------------------------*/
    if ((U8)STD_FLASH_MANUFACTURER__WINBOND_SERIAL_FLASH == jedecID[0])
    {
        *isWinbond = TRUE;
    }

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
* @brief       This routine waits till Flash becomes not busy using standard command,
*
* @param[in]   qlibContext         Pointer to the qlib context
* @param[in]   waitAfterFlashReset Wait while flash is not alive
*
* @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_TM_WaitWhileBusyStd_L(QLIB_CONTEXT_T* qlibContext, BOOL waitAfterFlashReset)
{
    BOOL isWinbond = TRUE;
    U8   sr1;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Wait while busy by polling status register                                                          */
    /*-----------------------------------------------------------------------------------------------------*/
    do
    {
#if QLIB_NUM_OF_DIES > 1
        if ((waitAfterFlashReset == TRUE) && (qlibContext->activeDie != QLIB_INIT_DIE_ID))
        {
            QLIB_STATUS_RET_CHECK(QLIB_TM_SetActiveDie_L(qlibContext));
        }
#endif
        QLIB_STATUS_RET_CHECK(QLIB_TM_GetSR1_L(qlibContext, &sr1));
    } while (0u != READ_VAR_FIELD((U32)sr1, SPI_FLASH__STATUS_1_FIELD__BUSY));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Check if flash is alive. This is relevant only for standard flash, or after flash reset             */
    /*-----------------------------------------------------------------------------------------------------*/
    if (TRUE == waitAfterFlashReset)
    {
        do
        {
#if QLIB_NUM_OF_DIES > 1
            if (qlibContext->activeDie != QLIB_INIT_DIE_ID)
            {
                QLIB_STATUS_RET_CHECK(QLIB_TM_SetActiveDie_L(qlibContext));
            }
#endif
            QLIB_STATUS_RET_CHECK(QLIB_TM_IsWinbondDevice_L(qlibContext, &isWinbond));
        } while (FALSE == isWinbond);
    }

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine sends standard write enable command, and optionally returns the Flash
 *              status
 *
 * @param[in]   qlibContext   pointer to qlib context
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_TM_WriteEnable_L(QLIB_CONTEXT_T* qlibContext)
{
    U8              sr1;
    QLIB_BUS_MODE_T stdFormat = QLIB_STD_GET_BUS_MODE(qlibContext);
    U32             cmdSize   = QLIB_CMD_EXTENSION_SIZE(qlibContext);
    U8              cmd[2];
    QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, cmd, SPI_FLASH_CMD__WRITE_ENABLE);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform Write Enable                                                                                */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                   stdFormat,
                                                   QLIB_DTR_TO_DTR_FLAGS(qlibContext, stdFormat, FALSE),
                                                   cmd,
                                                   cmdSize,
                                                   0,
                                                   0,
                                                   0,
                                                   NULL,
                                                   0) == 0),
                    QLIB_STATUS__HARDWARE_FAILURE);

    /*-----------------------------------------------------------------------------------------------------*/
    /* wait for the command to finish                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_TM_WaitWhileBusySec_L(qlibContext, FALSE, FALSE, NULL));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Check that write is enabled                                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_TM_GetSR1_L(qlibContext, &sr1));

    if (0u == READ_VAR_FIELD((U32)sr1, SPI_FLASH__STATUS_1_FIELD__WEL))
    {
        return QLIB_STATUS__COMMAND_FAIL;
    }

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine returns the command address size and whether it uses extended bits
 *
 * @param[in]   qlibContext   Pointer to qlib context
 * @param[in]   cmd           Command value
 * @param[in]   addressMsb    address[24:31]
 * @param[out]  addrSize      address size
 * @param[out]  setExtended   whether extended address register should be set
 *
 ***********************************************************************************************************/
static void QLIB_TM_GetAddressParams_L(QLIB_CONTEXT_T* qlibContext, U8 cmd, U8 addressMsb, U32* addrSize, BOOL* setExtended)
{
    *setExtended = FALSE;
    if (Q2_4_BYTES_ADDRESS_MODE(qlibContext) != 0u)
    {
        if ((W77Q_CMD_MANUFACTURER_AND_DEVICE_ID(qlibContext) != 0u) && (cmd == SPI_FLASH_CMD__MANUFACTURER_AND_DEVICE_ID))
        {
            *addrSize = 3;
        }
        else if ((qlibContext->addrMode == QLIB_STD_ADDR_MODE__4_BYTE) || (cmd == (U8)SPI_FLASH_CMD__READ_FAST_DTR__1_8_8))
        {
            *addrSize = 4;
        }
        else
        {
            *addrSize    = 3;
            *setExtended = (addressMsb != qlibContext->extendedAddr ? TRUE : FALSE);
        }
    }
    else
    {
        (void)cmd;
        *addrSize    = 3;
        *setExtended = (addressMsb != qlibContext->extendedAddr ? TRUE : FALSE);
    }
}

/************************************************************************************************************
 * @brief       This routine sets extended address register
 *
 * @param[in]   qlibContext   pointer to qlib context
 * @param[in]   extAddrByte   address MSB to set in extended address register
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_TM_SetExtendedAddress_L(QLIB_CONTEXT_T* qlibContext, U8 extAddrByte)
{
    U8              testExtAddr;
    U32             cmdSize;
    U8              cmdBuf[3];
    QLIB_BUS_MODE_T busMode = QLIB_STD_GET_BUS_MODE(qlibContext);

    cmdSize = QLIB_CMD_EXTENSION_SIZE(qlibContext);

    QLIB_STATUS_RET_CHECK(QLIB_TM_WriteEnable_L(qlibContext));

    QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, cmdBuf, SPI_FLASH_CMD__WRITE_EAR);
    cmdBuf[cmdSize] = extAddrByte;

    QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                   busMode,
                                                   QLIB_DTR_TO_DTR_FLAGS(qlibContext, busMode, FALSE),
                                                   cmdBuf,
                                                   cmdSize,
                                                   0,
                                                   sizeof(extAddrByte),
                                                   0,
                                                   NULL,
                                                   0) == 0),
                    QLIB_STATUS__HARDWARE_FAILURE);

    /*-----------------------------------------------------------------------------------------------------*/
    /* wait for the command to finish                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_TM_WaitWhileBusySec_L(qlibContext, FALSE, FALSE, NULL));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Read extended address register to verify success                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    if (W77Q_EXTENDED_ADDR_REG(qlibContext) != 0u)
    {
        U32 dataInSize;
        U8  dataIn[2];
        dataInSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);
        QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, cmdBuf, SPI_FLASH_CMD__READ_EAR);

        QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                       busMode,
                                                       QLIB_DTR_TO_DTR_FLAGS(qlibContext, busMode, FALSE),
                                                       cmdBuf,
                                                       cmdSize,
                                                       0,
                                                       0,
                                                       (W77Q_READ_EXTENDED_ADDR_DUMMY(qlibContext) == 0u
                                                            ? 0u
                                                            : (busMode == QLIB_BUS_MODE_1_1_1
                                                                   ? SPI_FLASH_DUMMY_CYCLES__READ_EAR_SPI
                                                                   : SPI_FLASH_DUMMY_CYCLES__READ_EAR_QPI_OPI)),
                                                       dataIn,
                                                       dataInSize) == 0),
                        QLIB_STATUS__HARDWARE_FAILURE);
        testExtAddr = dataIn[0];
        QLIB_ASSERT_RET((1u == dataInSize) || QLIB_DATA_EXTENSION_VALID(qlibContext, dataIn), QLIB_STATUS__COMMUNICATION_ERR);
    }
    else
    {
        STD_FLASH_STATUS_T status;
        cmdBuf[0] = SPI_FLASH_CMD__READ_STATUS_REGISTER_3;
        QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                       busMode,
                                                       QLIB_DTR_TO_DTR_FLAGS(qlibContext, busMode, FALSE),
                                                       cmdBuf,
                                                       cmdSize,
                                                       0,
                                                       0,
                                                       0,
                                                       &(status.SR3.asUint),
                                                       1) == 0),
                        QLIB_STATUS__HARDWARE_FAILURE);
        testExtAddr = (U8)READ_VAR_FIELD((U32)status.SR3.asUint, SPI_FLASH__STATUS_3_FIELD__A24);
    }

    QLIB_ASSERT_RET(extAddrByte == testExtAddr, QLIB_STATUS__COMMAND_FAIL);
    qlibContext->extendedAddr = extAddrByte;

    return QLIB_STATUS__OK;
}

#if defined QLIB_SUPPORT_QPI || defined QLIB_SUPPORT_OPI
/************************************************************************************************************
 * @brief       This routine switch from/to SPI mode
 *
 * @param[in]   qlibContext   pointer to qlib context
 * @param[in]   format        SPI bus mode
 * @param[in]   dtr           SPI bus DTR (relevant only for OPI mode)
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_TM_SwitchSPIBusMode_L(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_MODE_T format, BOOL dtr)
{
    U32 cmdSize = QLIB_CMD_EXTENSION_SIZE(qlibContext);

    U8  cmdBuf[2]; // cmd byte and cmd extension byte (if in DOPI) or mode byte (if switch to DOPI)
    U32 modeSize = 0;

#ifndef QLIB_SUPPORT_OPI
    (void)dtr;
#endif
    switch (format)
    {
#ifdef QLIB_SUPPORT_QPI
        case QLIB_BUS_MODE_4_4_4:
            cmdBuf[0] = SPI_FLASH_CMD__ENTER_QPI;
            break;
#endif
#ifdef QLIB_SUPPORT_OPI
        case QLIB_BUS_MODE_8_8_8:
            cmdBuf[0] = SPI_FLASH_CMD__ENTER_OPI;
            if (dtr == TRUE)
            {
                cmdBuf[1] = SPI_FLASH_CMD__ENTER_DOPI_MODE;
                modeSize  = 1;
            }
            break;
#endif
        default:
            cmdBuf[0] = SPI_FLASH_CMD__ENTER_SPI;
            break;
    }
#ifdef QLIB_SUPPORT_OPI
    if (cmdSize > 1u)
    {
        cmdBuf[1] = cmdBuf[0];
    }
#endif
    QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                   QLIB_STD_GET_BUS_MODE(qlibContext),
                                                   QLIB_DTR_TO_DTR_FLAGS(qlibContext, QLIB_STD_GET_BUS_MODE(qlibContext), FALSE),
                                                   cmdBuf,
                                                   cmdSize,
                                                   0,
                                                   modeSize,
                                                   0,
                                                   NULL,
                                                   0) == 0),
                    QLIB_STATUS__HARDWARE_FAILURE);

#if (QLIB_QPI_READ_DUMMY_CYCLES != Q2_DEFAULT_QPI_READ_DUMMY_CYCLES)
    if ((Q2_QPI_SET_READ_PARAMS(qlibContext) != 0u) && (cmdBuf[0] == SPI_FLASH_CMD__ENTER_QPI))
    {
        // exit QPI clears the read dummy cycles back to default value. After entering back to QPI set it to correct value
        cmdBuf[0] = SPI_FLASH_CMD__SET_READ_PARAMETERS;
        cmdBuf[1] = SPI_FLASH_SET_READ_PARAMETERS;
        QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                       QLIB_BUS_MODE_4_4_4,
                                                       QLIB_DTR__NO_DTR,
                                                       cmdBuf,
                                                       1,
                                                       0,
                                                       1,
                                                       0,
                                                       NULL,
                                                       0) == 0),
                        QLIB_STATUS__HARDWARE_FAILURE);
    }
#endif

    return QLIB_STATUS__OK;
}
#endif

static QLIB_STATUS_T QLIB_TM_GetSR1_L(QLIB_CONTEXT_T* qlibContext, U8* pSR1)
{
    int             status;
    U32             cmdSize;
    U32             dataInSize;
    U8              dataIn[2];
    U8              cmd[2];
    QLIB_BUS_MODE_T busMode     = QLIB_STD_GET_BUS_MODE(qlibContext);
    U32             dummyCycles = (W77Q_READ_SR_DUMMY(qlibContext) == 0u)
                                      ? 0u
                                      : (busMode == QLIB_BUS_MODE_1_1_1 ? SPI_FLASH_DUMMY_CYCLES__READ_STATUS_REGISTER_SPI
                                                                        : SPI_FLASH_DUMMY_CYCLES__READ_STATUS_REGISTER_QPI_OPI);

    QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, cmd, SPI_FLASH_CMD__READ_STATUS_REGISTER_1);

    cmdSize    = QLIB_CMD_EXTENSION_SIZE(qlibContext);
    dataInSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);

    status = PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                           busMode,
                                           QLIB_DTR_TO_DTR_FLAGS(qlibContext, busMode, FALSE),
                                           cmd,
                                           cmdSize,
                                           0,
                                           0,
                                           dummyCycles,
                                           dataIn,
                                           dataInSize);
    *pSR1  = dataIn[0];
    QLIB_ASSERT_RET(0 == status, QLIB_STATUS__COMMUNICATION_ERR);
    QLIB_ASSERT_RET((1u == dataInSize) || QLIB_DATA_EXTENSION_VALID(qlibContext, dataIn), QLIB_STATUS__COMMUNICATION_ERR);
    return QLIB_STATUS__OK;
}

#if QLIB_NUM_OF_DIES > 1
static QLIB_STATUS_T QLIB_TM_SetActiveDie_L(QLIB_CONTEXT_T* qlibContext)
{
    U8 dieSelCmd[2] = {SPI_FLASH_CMD__DIE_SELECT, qlibContext->activeDie};
    QLIB_ASSERT_RET((PLAT_SPI_WriteReadTransaction(qlibContext->userData,
                                                   QLIB_STD_GET_BUS_MODE(qlibContext),
                                                   QLIB_DTR_TO_DTR_FLAGS(qlibContext, QLIB_STD_GET_BUS_MODE(qlibContext), FALSE),
                                                   dieSelCmd,
                                                   1,
                                                   0,
                                                   1,
                                                   0,
                                                   NULL,
                                                   0) == 0),
                    QLIB_STATUS__HARDWARE_FAILURE);
    return QLIB_STATUS__OK;
}
#endif
