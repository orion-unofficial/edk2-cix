/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_platform_sha256.c
* @brief      This file contains software specific implementations of PLAT_SHA256.
*
*
* ### project qlib
*
************************************************************************************************************/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include <string.h>
#include "qlib_platform.h"
#include "common_platform_sha256.h"

#ifndef SM3_HASH
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 DEFINE                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

//If not defined SM3_HASH use PLAT_SHA256 as the default

//Replace the platform SHA with SHA256 algorithm
#define QLIB_PLAT_SHA256_Init   PLAT_HASH_Init
#define QLIB_PLAT_SHA256_Update PLAT_HASH_Update
#define QLIB_PLAT_SHA256_Finish PLAT_HASH_Finish
#define QLIB_PLAT_SHA256 PLAT_HASH
#define QLIB_PLAT_HASH_Async PLAT_HASH_Async
#define QLIB_PLAT_HASH_Async_WaitWhileBusy PLAT_HASH_Async_WaitWhileBusy

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                        LOCAL FUNCTION PROTOTYPES                                        */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#ifdef Q2_API
int QLIB_PLAT_SHA256_Init(void** ctx, QLIB_HASH_OPT_T opt);
int QLIB_PLAT_SHA256_Update(void* ctx, const void* data, uint32_t dataSize);
int QLIB_PLAT_SHA256_Finish(void* ctx, uint32_t* output);
#else
void QLIB_PLAT_SHA256(uint32_t* output, const void* data, uint32_t dataSize);
void QLIB_PLAT_HASH_Async(uint32_t* output, const void* data, uint32_t dataSize);
void QLIB_PLAT_HASH_Async_WaitWhileBusy(void);
#endif
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

int QLIB_PLAT_SHA256_Init(void** ctx, QLIB_HASH_OPT_T opt)
{
    int ret;
    (void)opt;
    ret = PLAT_SHA256_Init(ctx);
    return ret;
}

int QLIB_PLAT_SHA256_Update(void* ctx, const void* data, uint32_t dataSize)
{
    int ret;
    ret = PLAT_SHA256_Update(ctx, data, dataSize);
    return ret;
}

int QLIB_PLAT_SHA256_Finish(void* ctx, uint32_t* output)
{
    int ret;
    ret = PLAT_SHA256_Finish(ctx, output);
    return ret;
}

void QLIB_PLAT_SHA256(uint32_t* output, const void* data, uint32_t dataSize)
{
    void* ctx;
    (void)PLAT_SHA256_Init(&ctx);
    (void)PLAT_SHA256_Update(ctx, data, dataSize);
    (void)PLAT_SHA256_Finish(ctx, output);
}

void QLIB_PLAT_HASH_Async(uint32_t* output, const void* data, uint32_t dataSize)
{
    QLIB_PLAT_SHA256(output, data, dataSize);
}

void QLIB_PLAT_HASH_Async_WaitWhileBusy(void)
{
}
#endif //SM3_HASH

