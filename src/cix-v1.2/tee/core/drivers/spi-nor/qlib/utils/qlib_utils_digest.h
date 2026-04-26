/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2021 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_utils_digest.h
* @brief      This file contains digest utility functions functions
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_UTILS_DIGEST_H__
#define __QLIB_UTILS_DIGEST_H__

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
 * @brief       This function calculates the digest of a given data
 *
 * @param[in]   buf      Section data buffer
 * @param[in]   size     Section data buffer size
 * @param[out]  digest   digest value
 *
 * @return
 * QLIB_STATUS__OK = 0              - no error occurred\n
 * QLIB_STATUS__INVALID_PARAMETER   - @p buf or @p digest is NULL\n
 * QLIB_STATUS__INVALID_PARAMETER   - @p is 0\n
 * QLIB_STATUS__(ERROR)             - Other error
************************************************************************************************************/
QLIB_STATUS_T QLIB_UTILS_CalcDigest(U32* buf, U32 size, U64* digest);

#ifndef Q2_API
/************************************************************************************************************
 * @brief       This function calculates the digest of a given data
 *
 * @param[in]   buf      data buffer
 * @param[in]   size     data buffer size
 * @param[in]   padValue padding after data value
 * @param[in]   padSize  padding after data size
 * @param[out]  digest   digest value
 *
 * @return
 * QLIB_STATUS__OK = 0              - no error occurred\n
 * QLIB_STATUS__INVALID_PARAMETER   - @p buf or @p digest is NULL\n
 * QLIB_STATUS__INVALID_PARAMETER   - @p is 0\n
 * QLIB_STATUS__(ERROR)             - Other error
************************************************************************************************************/
QLIB_STATUS_T QLIB_UTILS_CalcDigestWithPadding(const void* buf, U32 size, U8 padValue, U32 padSize, U64* digest);
#endif
#ifdef __cplusplus
}
#endif

#endif // __QLIB_UTILS_DIGEST_H__
