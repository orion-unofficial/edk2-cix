/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2021 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_utils_crc.c
* @brief      This file contains CRC utility functions
*
* ### project qlib
*
************************************************************************************************************/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib_utils_crc.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                DEFINITIONS                                              */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

#define QLIB_UTILS_CRC_READ_BUFFER_SIZE _256B_
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                CONSTANTS                                                */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
static const U32 table32[] = {
    0xb8bc6765u, 0xaa09c88bu, 0x8f629757u, 0xc5b428efu, 0x5019579fu, 0xa032af3eu, 0x9b14583du, 0xed59b63bu,
    0x01c26a37u, 0x0384d46eu, 0x0709a8dcu, 0x0e1351b8u, 0x1c26a370u, 0x384d46e0u, 0x709a8dc0u, 0xe1351b80u,
    0x191b3141u, 0x32366282u, 0x646cc504u, 0xc8d98a08u, 0x4ac21251u, 0x958424a2u, 0xf0794f05u, 0x3b83984bu,
    0x77073096u, 0xee0e612cu, 0x076dc419u, 0x0edb8832u, 0x1db71064u, 0x3b6e20c8u, 0x76dc4190u, 0xedb88320u,
};

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                          FORWARD DECLARATION                                            */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
static U32 QLIB_UTILS_CRC_churn32_L(U32 x);

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_UTILS_CalcCRCWithPadding(const U32* buf, U32 size, U32 padValue, U32 padSize, U32* crc)
{
    U32 res = 0xFFFFFFFFu;
    U32 i   = 0;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    if (NULL == crc)
    {
        return QLIB_STATUS__INVALID_PARAMETER;
    }

    if (NULL == buf)
    {
        *crc = 0;
        return QLIB_STATUS__INVALID_PARAMETER;
    }

    //size is multiple of 4 bytes
    QLIB_ASSERT_RET(0u == (size % (sizeof(U32))), QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(0u == (padSize % (sizeof(U32))), QLIB_STATUS__INVALID_PARAMETER);

    for (i = 0; i < size / sizeof(U32); ++i)
    {
        res = QLIB_UTILS_CRC_churn32_L(buf[i] ^ res);
    }

    for (i = 0; i < padSize / sizeof(U32); ++i)
    {
        res = QLIB_UTILS_CRC_churn32_L(padValue ^ res);
    }

    *crc = (res ^ 0xFFFFFFFFu);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_UTILS_CalcCRCForSection(QLIB_CONTEXT_T* qlibContext, U32 sectionId, U32 offset, U32 dataSize, U32* crc)
{
    U32 res = 0xFFFFFFFFu;
    U32 i   = 0;
    U32 readBuf[QLIB_UTILS_CRC_READ_BUFFER_SIZE / sizeof(U32)];
    U32 readSize;

    while (dataSize > 0u)
    {
        readSize = MIN(dataSize, sizeof(readBuf));
        QLIB_STATUS_RET_CHECK(QLIB_Read(qlibContext, (U8*)readBuf, sectionId, offset, readSize, TRUE, FALSE));
        for (i = 0; i < (readSize / sizeof(U32)); i++)
        {
            res = QLIB_UTILS_CRC_churn32_L(readBuf[i] ^ res);
        }
        dataSize -= readSize;
        offset += readSize;
    }
    *crc = (res ^ 0xFFFFFFFFu);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_UTILS_CalcCRCProgressive(const U32* buf, U32 size, U32* crc)
{
    U32 res;
    U32 i = 0;

    //size is multiple of 4 bytes
    QLIB_ASSERT_RET(buf != NULL, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(crc != NULL, QLIB_STATUS__INVALID_PARAMETER);
    //size is multiple of 4 bytes
    QLIB_ASSERT_RET(0u == (size % (sizeof(U32))), QLIB_STATUS__INVALID_PARAMETER);

    res = (*crc) ^ 0xFFFFFFFFu;
    for (i = 0; i < size / sizeof(U32); i++)
    {
        res = QLIB_UTILS_CRC_churn32_L(buf[i] ^ res);
    }

    *crc = (res ^ 0xFFFFFFFFu);
    return QLIB_STATUS__OK;
}

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                             LOCAL FUNCTIONS                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
static U32 QLIB_UTILS_CRC_churn32_L(U32 x)
{
    U32 res = 0;
    U32 i   = 0;
    for (i = 0; i < 32u; i++)
    {
        if ((x & 1u) == 1u)
        {
            res ^= table32[i];
        }
        x >>= 1;
    }
    return res;
}
