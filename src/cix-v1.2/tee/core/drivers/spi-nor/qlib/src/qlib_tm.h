/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_tm.h
* @brief      This file contains QLIB Transaction Manager (TM) interface
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_TM_H__
#define __QLIB_TM_H__

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
/*                                              DEFINITIONS                                                */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_TM_CTAG_SCR_NEED_RESET_MASK    (0x01000000u)
#define QLIB_TM_CTAG_SCR_NEED_GRANT_PA_MASK (0x02000000u)

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This routine initializes the transaction layer
 *
 * @param[in,out]   qlibContext   qlib context object
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_TM_Init(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This routine initiate exclusive bus communication to the flash by locking the
 *              communication bus, if fails return `QLIB_STATUS__DEVICE_BUSY`.
 *
 * @param[in,out]   qlibContext   qlib context object
 *
 * @return      0 if success, QLIB_STATUS__DEVICE_BUSY otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_TM_Connect(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This routine release any exclusive bus communication.
 *
 * @param[in,out]   qlibContext   qlib context object
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_TM_Disconnect(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This function performs a standard Flash command flow
 *
 * @param[in]   qlibContext       pointer to qlib context
 * @param[in]   busFormat         SPI transaction format, consists of bus mode and dtr
 * @param[in]   needWriteEnable   If TRUE, WriteEnable command is sent prior to the given command
 * @param[in]   waitWhileBusy     If TRUE, busy-wait polling is performed after exec. of command
 * @param[in]   cmd               Command value
 * @param[in]   address           Pointer to address value or NULL if no address available
 * @param[in]   writeData         Pointer to output data or NULL if no output data available
 * @param[in]   writeDataSize     Size of the output data
 * @param[in]   dummyCycles       Delay Cycles between output and input command phases
 * @param[out]  readData          Pointer to input data or NULL if no input data required
 * @param[in]   readDataSize      Size of the input data
 * @param[out]  ssr               Pointer to status register following the transaction
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
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
                               QLIB_REG_SSR_T*   ssr) __RAM_SECTION;

/************************************************************************************************************
 * @brief       This function performs a secure command flow
 *              This function verifies that session is open before sending the command
 *
 * @param[in]   qlibContext          pointer to the sec qlib context
 * @param[in]   ctag                 Secure Command CTAG value
 * @param[in]   writeData            Pointer to output data.
 * @param[in]   writeDataSize        Size of the output data
 * @param[out]  readData             Pointer to input data or NULL if no input data required
 * @param[in]   readDataSize         Size of the input data
 * @param[out]  ssr                  Pointer to status register following the transaction
 *
 * @return      0 if no error occurred, Q2_STATUS_(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_TM_Secure(QLIB_CONTEXT_T* qlibContext,
                             U32             ctag,
                             const U32*      writeData,
                             U32             writeDataSize,
                             U32*            readData,
                             U32             readDataSize,
                             QLIB_REG_SSR_T* ssr) __RAM_SECTION;

#ifdef __cplusplus
}
#endif

#endif // __QLIB_TM_H__
