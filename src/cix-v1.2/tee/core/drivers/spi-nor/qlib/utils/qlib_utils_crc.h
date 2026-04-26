/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2021 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_utils_crc.h
* @brief      This file contains CRC utility functions
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_UTILS_CRC_H__
#define __QLIB_UTILS_CRC_H__

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
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This function calculates the CRC of a given data
 *
 * @param[in]   buf       data buffer
 * @param[in]   size      data buffer size
 * @param[in]   padValue  4 bytes padding after data value
 * @param[in]   padSize   padding after data size
 * @param[out]  crc       CRC value
 *
 * @return
 * QLIB_STATUS__OK = 0              - no error occurred\n
 * QLIB_STATUS__INVALID_PARAMETER   - @p buf or @p crc is NULL\n
 * QLIB_STATUS__INVALID_PARAMETER   - @p size or @p padSize are not multiply of 4\n
 * QLIB_STATUS__(ERROR)             - Other error
************************************************************************************************************/
QLIB_STATUS_T QLIB_UTILS_CalcCRCWithPadding(const U32* buf, U32 size, U32 padValue, U32 padSize, U32* crc);

/************************************************************************************************************
 * @brief       This function calculates the checksum of a given section.
 *              The function assumes there is an open session to the section with full or restricted access.
 * 
 * @param       qlibContext  qlib context object
 * @param[in]   sectionId    Section Id to read from
 * @param[in]   offset       Start offset inside the section
 * @param[in]   dataSize     Data size
 * @param[out]  crc          Result checksum value
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_UTILS_CalcCRCForSection(QLIB_CONTEXT_T* qlibContext, U32 sectionId, U32 offset, U32 dataSize, U32* crc);

/************************************************************************************************************
 * @brief       This function allows to calculate the CRC of a given data in chunks
 *              The function assumes that CRC on some data was already calculated and it adds new data to the CRC calculation.
 *              On first call, function should be called with previous crc as 0
 * @param[in]      buf       data buffer to be added to CRC calculation
 * @param[in]      size      data buffer size
 * @param[in, out] crc       previous / init CRC value on input and crc result on output
 *
 * @return
 * QLIB_STATUS__OK = 0              - no error occurred\n
 * QLIB_STATUS__INVALID_PARAMETER   - @p buf or @p crc is NULL\n
 * QLIB_STATUS__INVALID_PARAMETER   - @p size or @p padSize are not multiply of 4\n
 * QLIB_STATUS__(ERROR)             - Other error
 *
************************************************************************************************************/
QLIB_STATUS_T QLIB_UTILS_CalcCRCProgressive(const U32* buf, U32 size, U32* crc);

#ifdef __cplusplus
}
#endif

#endif // __QLIB_UTILS_CRC_H__
