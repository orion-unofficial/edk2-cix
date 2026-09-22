/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2021 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_utils_digest.c
* @brief      This file contains digest utility functions
*
* ### project qlib
*
************************************************************************************************************/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib_utils_digest.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_UTILS_CalcDigest(U32* buf, U32 size, U64* digest)
{
    _256BIT hash_result;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    if (NULL == digest)
    {
        return QLIB_STATUS__INVALID_PARAMETER;
    }

    if (NULL == buf || 0u == size)
    {
        *digest = 0u;
        return QLIB_STATUS__INVALID_PARAMETER;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Calculate digest                                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_HASH(hash_result, buf, size));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set output                                                                                          */
    /*-----------------------------------------------------------------------------------------------------*/
    (void)memcpy((void*)digest, (void*)&hash_result[6], sizeof(U64));

    return QLIB_STATUS__OK;
}

#ifndef Q2_API
QLIB_STATUS_T QLIB_UTILS_CalcDigestWithPadding(const void* buf, U32 size, U8 padValue, U32 padSize, U64* digest)
{
    _256BIT hash_result;
    void*   ctx;
    U8      padBuf[16];

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    if (NULL == digest)
    {
        return QLIB_STATUS__INVALID_PARAMETER;
    }

    if (NULL == buf || 0u == size)
    {
        *digest = 0u;
        return QLIB_STATUS__INVALID_PARAMETER;
    }

    (void)memset((void*)padBuf, (int)padValue, sizeof(padBuf));
    /*-----------------------------------------------------------------------------------------------------*/
    /* Calculate digest                                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(PLAT_HASH_Init(&ctx, QLIB_HASH_OPT_NONE) == 0, QLIB_STATUS__HARDWARE_FAILURE);
    QLIB_ASSERT_RET(PLAT_HASH_Update(ctx, buf, size) == 0, QLIB_STATUS__HARDWARE_FAILURE);

    while (padSize > 0u)
    {
        U32 padBytes = MIN(padSize, sizeof(padBuf));
        QLIB_ASSERT_RET(PLAT_HASH_Update(ctx, padBuf, padBytes) == 0, QLIB_STATUS__HARDWARE_FAILURE);
        padSize -= padBytes;
    }

    QLIB_ASSERT_RET(PLAT_HASH_Finish(ctx, hash_result) == 0, QLIB_STATUS__HARDWARE_FAILURE);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set output                                                                                          */
    /*-----------------------------------------------------------------------------------------------------*/
    (void)memcpy((void*)digest, (void*)&hash_result[6], sizeof(U64));

    return QLIB_STATUS__OK;
}
#endif
