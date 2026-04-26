/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_std.h
* @brief      This file contains QLIB standard flash interface
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_STD_H__
#define __QLIB_STD_H__

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                               DEFINITIONS                                               */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

#define QLIB_STD_GET_BUS_MODE(context)                                                \
    ((QLIB_BUS_MODE_4_4_4 == (context)->busInterface.busMode)   ? QLIB_BUS_MODE_4_4_4 \
     : (QLIB_BUS_MODE_8_8_8 == (context)->busInterface.busMode) ? QLIB_BUS_MODE_8_8_8 \
                                                                : QLIB_BUS_MODE_1_1_1)

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 MACROS                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define _QLIB_MAKE_LOGICAL_ADDRESS(dieId, secId, offset, addrSize) \
    ((((U32)(dieId)) << (((U32)(addrSize)) + 3u)) | (((U32)(secId)) << ((U32)(addrSize))) | ((U32)(offset)))
#define _QLIB_MAX_LEGACY_OFFSET_BY_ADDR_SIZE(addrSize) ((U32)0x1 << (addrSize))
#define _QLIB_MAX_LEGACY_OFFSET(context)               _QLIB_MAX_LEGACY_OFFSET_BY_ADDR_SIZE((context)->addrSize)
#define _QLIB_OFFSET_FROM_LOGICAL_ADDRESS(logicalAddr, addrSize) \
    ((_QLIB_MAX_LEGACY_OFFSET_BY_ADDR_SIZE(addrSize) - 1u) & (logicalAddr))
#define _QLIB_SECTION_FROM_LOGICAL_ADDRESS(logicalAddr, addrSize) (((logicalAddr) >> (addrSize)) & 0x07u)
#define _QLIB_DIE_FROM_LOGICAL_ADDRESS(logicalAddr, addrSize)     ((logicalAddr) >> ((addrSize) + 3u))
#define QLIB_RPMC_VALID_CNT_ADDR(context, cntAddr)                                                    \
    (((QLIB_VAULT_DISABLED_RPMC_8_COUNTERS == (context)->dieState[(context)->activeDie].vaultSize) && \
      ((cntAddr) < QLIB_NUM_OF_RPMC_MAX)) ||                                                          \
     ((QLIB_VAULT_64KB_RPMC_4_COUNTERS == (context)->dieState[(context)->activeDie].vaultSize) &&     \
      ((cntAddr) < QLIB_NUM_OF_RPMC_HALF)))

/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This routine initializes the context of the STD module
 *              This function should be called once at the beginning of every host (local or remote)
 *
 * @param       qlibContext       internal context object
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_InitLib(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
* @brief       This routine checks that flash is ready (powered up and not busy) and returns operational bus mode
*              and the flash device Id
* @param       qlibContext   qlib context object
* @param[out]  currentFormat qlib bus format
* @param[out]  deviceId      Detected device ID
*
* @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_WaitTillFlashIsReady(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T* currentFormat, U8* deviceId);

/************************************************************************************************************
 * @brief       This routine configures Flash interface configuration. If upgrade from single -> quad
 *              detected, quad hw is enabled. If detected quad -> single, hw quad is NOT disabled since to
 *              save transaction.
 *
 * @param       qlibContext   qlib context object
 * @param[in]   busFormat     SPI bus format
 * @param[in]   configFlash   true if the bus interface should be set in flash. false only updates the QLIB context
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_SetInterface(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T busFormat, BOOL configFlash) __RAM_SECTION;

/************************************************************************************************************
 * @brief           This function sets non-volatile QE (QuadEnable) bit value
 *
 * @param[out]      qlibContext     qlib context object
 * @param[in]       enable          QE value
 *
 * @return          0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_SetQuadEnable(QLIB_CONTEXT_T* qlibContext, BOOL enable);

/************************************************************************************************************
 * @brief           This function returns if QuadEnable or not
 *
 * @param[out]      qlibContext     qlib context object
 * @param[out]      enabled         QE value
 *
 * @return          0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_GetQuadEnable(QLIB_CONTEXT_T* qlibContext, BOOL* enabled);

/************************************************************************************************************
 * @brief           This function sets SR3.HOLD/RST bit value
 *
 * @param[out]      qlibContext     qlib context object
 * @param[in]       enable          HOLD/RST value
 *
 * @return          0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_SetResetInEnable(QLIB_CONTEXT_T* qlibContext, BOOL enable);

/************************************************************************************************************
 * @brief       This routine performs legacy Flash read command
 *
 * @param       qlibContext   qlib context object
 * @param[out]  output        Output buffer for read data
 * @param[in]   logicalAddr   logical flash address
 * @param[in]   size          Number of bytes to read from Flash
 *
 * @return      0 in no error occurred, or QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_Read(QLIB_CONTEXT_T* qlibContext, U8* output, U32 logicalAddr, U32 size);

/************************************************************************************************************
 * @brief       This routine performs STD Flash write command
 *
 * @param       qlibContext   qlib context object
 * @param[in]   input         Data for writing
 * @param[in]   logicalAddr   logical flash address
 * @param[in]   size          Number of bytes to write
 *
 * @return      0 in no error occurred, or QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_Write(QLIB_CONTEXT_T* qlibContext, const U8* input, U32 logicalAddr, U32 size);

/************************************************************************************************************
 * @brief       This routine performs blocking / non-blocking Flash erase (sector/block/chip)
 *
 * @param       qlibContext   qlib context object
 * @param[in]   eraseType     type of erase (sector/block/chip)
 * @param[in]   logicalAddr   logical flash address
 * @param[in]   blocking      if true, this function is blocking till the erase is finish
 *
 * @return      0 on success, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_PerformErase(QLIB_CONTEXT_T* qlibContext, QLIB_ERASE_T eraseType, U32 logicalAddr, BOOL blocking);

/************************************************************************************************************
 * @brief       This routine performs STD Flash erase command (sector/block)
 *
 * @param       qlibContext   qlib context object
 * @param[in]   logicalAddr   logical flash address
 * @param[in]   size          number of Bytes to erase
 *
 * @return      0 in no error occurred, or QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_Erase(QLIB_CONTEXT_T* qlibContext, U32 logicalAddr, U32 size);

/************************************************************************************************************
 * @brief       This routine suspends the on-going erase operation
 *
 * @param       qlibContext   qlib context object
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_EraseSuspend(QLIB_CONTEXT_T* qlibContext) __RAM_SECTION;

/************************************************************************************************************
 * @brief       This routine resumes the suspended erase operation
 *
 * @param       qlibContext   qlib context object
 * @param       blocking      if TRUE, this function will block execution till the suspended operation end
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_EraseResume(QLIB_CONTEXT_T* qlibContext, BOOL blocking) __RAM_SECTION;

/************************************************************************************************************
 * @brief       This routine changes Flash power state
 *
 * @param       qlibContext   internal context object
 * @param       power         the require new power state
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_Power(QLIB_CONTEXT_T* qlibContext, QLIB_POWER_T power) __RAM_SECTION;

/************************************************************************************************************
 * @brief       This routine resets the Flash, forceReset must be true if the reset is called before
 *              the module has initiated
 *
 * @param       qlibContext   qlib context object
 * @param       forceReset    if TRUE, the reset operation force the reset, this may cause a
 *                            a corrupted data.
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_ResetFlash(QLIB_CONTEXT_T* qlibContext, BOOL forceReset) __RAM_SECTION;

/************************************************************************************************************
 * @brief       This routine return the Hardware info of the device (manufacturer, device, memory type, capacity)
 *
 * @param       qlibContext   qlib context object
 * @param[out]  hwVer        (OUT) pointer to struct contains the HW version
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_GetHwVersion(QLIB_CONTEXT_T* qlibContext, QLIB_STD_HW_VER_T* hwVer);

/************************************************************************************************************
 * @brief       This routine return the Unique IDs of the flash
 *
 * @param       qlibContext   qlib context object
 * @param[out]  id_p          (OUT) pointer to struct contains the Unique IDs of flash
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_GetId(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ID_T* id_p);

/************************************************************************************************************
 * @brief       This function sets flash active die
 *
 * @param       qlibContext     QLIB state object
 * @param       die             Flash die number to set
 * @param       clearErrors     Clear previous errors on selected die
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_SetActiveDie(QLIB_CONTEXT_T* qlibContext, U8 die, BOOL clearErrors);

/************************************************************************************************************
* @brief       This function sets flash address mode, either 4 bytes or 3 bytes
*
* @param       qlibContext     QLIB state object
* @param       addrMode        3 bytes or 4 bytes address
*
* @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_SetAddressMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T addrMode) __RAM_SECTION;

/************************************************************************************************************
* @brief       This function This function updates non-volatile ADP to set Power-Up Address Mode
*
* @param       qlibContext     QLIB state object
* @param       addrMode        3 bytes or 4 bytes address
*
* @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_SetPowerUpAddressMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T addrMode);

/************************************************************************************************************
* @brief       This function gets flash address mode
*
* @param       qlibContext     QLIB state object
* @param       addrMode       (OUT) The current address mode (either 3 or 4 bytes)
*
* @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_GetAddressMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T* addrMode);

/************************************************************************************************************
 * @brief       This function gets flash non-volatile default address mode
 *
 * @param       qlibContext     QLIB state object
 * @param       addrMode       (OUT) The power up address mode (either 3 or 4 bytes)
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_GetPowerUpAddressMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T* addrMode);

/************************************************************************************************************
* @brief       This function get the number of dummy cycles for all Fast-Read instructions
*
* @param       qlibContext     QLIB state object
* @param       dummy           dummy cycles
*
* @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_getPowerUpDummyCycles(QLIB_CONTEXT_T* qlibContext, U8* dummy);

/************************************************************************************************************
 * @brief       This function gets number of configured dummy cycles for fast read commands
 *
 * @param       qlibContext     QLIB state object
 * @param       dummyCycles     (OUT) The number of dummy cycles for fast read commands
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_GetFastReadDummyCycles(QLIB_CONTEXT_T* qlibContext, U8* dummyCycles);

/************************************************************************************************************
 * @brief       This function sets the default number of configured dummy cycles for fast read commands
 *
 * @param       qlibContext     QLIB state object
 * @param       dummyCycles     (IN) The number of dummy cycles for fast read commands
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_SetPowerUpFastReadDummyCycles(QLIB_CONTEXT_T* qlibContext, U8 dummyCycles);

/************************************************************************************************************
 * @brief       This function sets the number of dummy cycles for fast read commands in CR volatile register
 *
 * @param       qlibContext     QLIB state object
 * @param       dummyCycles     (IN) The number of dummy cycles for fast read commands
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_SetFastReadDummyCycles(QLIB_CONTEXT_T* qlibContext, U8 dummyCycles) __RAM_SECTION;

#ifndef Q2_API
/************************************************************************************************************
* @brief       This function gets the default value of the DQS enable bit in CR
*
* @param       qlibContext     QLIB state object
* @param       dqsDisable      Is DQS disabled or not
*
* @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_getPowerUpDqsDisable(QLIB_CONTEXT_T* qlibContext, BOOL* dqsDisable);

/************************************************************************************************************
 * @brief       This function sets the default value of the DQS enable bit in CR
 *
 * @param       qlibContext     QLIB state object
 * @param       dqsDisable      (IN) TRUE to disable DQS, FALSE to enable
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_SetPowerUpDqsDisable(QLIB_CONTEXT_T* qlibContext, BOOL dqsDisable);

#ifndef EXCLUDE_W77Q_ECC
/************************************************************************************************************
 * @brief       This function reads ECC status registers
 *
 * @param  qlibContext         QLIB state object
 * @param  eccStatus           (OUT) ECC status
 * @param  advancedEcc         (OUT) Advanced ECC (can be NULL)
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_GetEccStatus(QLIB_CONTEXT_T* qlibContext, QLIB_ECC_STATUS_T* eccStatus, QLIB_ADVANCED_ECC_T* advancedEcc);

/************************************************************************************************************
 * @brief       This routine enables or disables the ECC
 *
 * @param       qlibContext   qlib context object
 * @param       enable        TRUE to enable ECC, FALSE to disable.
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_SetEccEnable(QLIB_CONTEXT_T* qlibContext, BOOL enable);
#endif
#endif // Q2_API


#if !defined EXCLUDE_W77Q_INTERRUPTS && !defined Q2_API
/************************************************************************************************************
 * @brief       This function enables or disables interrupts from flash
 *
 * @param[in,out]  qlibContext       [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]      intTypes          Interrupt types to configure
 * @param[in]      enable            TRUE to enable the interrupts set in intTypes, FALSE to disable
 *
 * Note that interrupt configuration for interrupts which are not set in @p intTypes will remain unchanged
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_ConfigInterrupts(QLIB_CONTEXT_T* qlibContext, QLIB_INTERRUPT_T* intTypes, BOOL enable);

/************************************************************************************************************
 * @brief       This function clears the interrupt events in configuration register
 *
 * @param[in,out]  qlibContext       [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]      intTypes          Interrupt types to clear
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_ClearInterrupts(QLIB_CONTEXT_T* qlibContext, QLIB_INTERRUPT_T* intTypes);

/************************************************************************************************************
 * @brief       This function Reads the interrupt state and return whether the interrupt was triggered
 *
 * @param[in,out]  qlibContext       [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[out]     intState          Interrupt state : 1 if interrupt was triggered, 0 otherwise
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_ReadInterruptState(QLIB_CONTEXT_T* qlibContext, QLIB_INTERRUPT_T* intState);
#endif

/************************************************************************************************************
 * @brief       This routine gets the address mode configuration as defined in the AM bit, which can be
 *              different from the current address mode in OPI bus mode
 *
 * @param       qlibContext    qlib context object
 * @param[out]  addrModeConfig configured address mode
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_STD_GetConfiguredAddrMode(QLIB_CONTEXT_T* qlibContext, QLIB_STD_ADDR_MODE_T* addrModeConfig);

#if QLIB_NUM_OF_DIES > 1
/************************************************************************************************************
 * @brief       This routine waits for all dies to be up after flash reset.
 *              Choose the next die in loop until flash is up to receive the die select command
 *
 * @param       qlibContext   qlib context object
 * @param       fallbackStatus if not NULL, return the fallback status
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T
QLIB_STD_WaitForDiesAfterReset(QLIB_CONTEXT_T* qlibContext, U32* fallbackStatus) __RAM_SECTION;
#endif

#ifdef __cplusplus
}
#endif

#endif // __QLIB_STD_H__
