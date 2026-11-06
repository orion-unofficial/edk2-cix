/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_common.c
* @brief      This file contains common qlib functions
*
* ### project qlib
*
************************************************************************************************************/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib.h"

#ifdef Q2_API
QLIB_STATUS_T QLIB_HASH(U32* output, const void* data, U32 dataSize)
{
    PLAT_HASH(output, data, dataSize);
    return QLIB_STATUS__OK;
}

#ifdef QLIB_HASH_OPTIMIZATION_ENABLED
QLIB_STATUS_T QLIB_HASH_Async(QLIB_CONTEXT_T* qlibContext, U32* output, const void* data, U32 dataSize)
{
    TOUCH(qlibContext);
    PLAT_HASH_Async(output, data, dataSize);
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_HASH_Async_WaitWhileBusy(QLIB_CONTEXT_T* qlibContext)
{
    TOUCH(qlibContext);
    PLAT_HASH_Async_WaitWhileBusy();
    return QLIB_STATUS__OK;
}
#endif // QLIB_HASH_OPTIMIZATION_ENABLED
#else
QLIB_STATUS_T QLIB_HASH(U32* output, const void* data, U32 dataSize)
{
    void*         ctx;
    QLIB_STATUS_T ret;
    QLIB_ASSERT_RET(PLAT_HASH_Init(&ctx,
                                   (dataSize == 55u && ADDRESS_ALIGNED32(data)) ? QLIB_HASH_OPT_FIXED_55_ALIGNED
                                                                                : QLIB_HASH_OPT_NONE) == 0,
                    QLIB_STATUS__HARDWARE_FAILURE);
    QLIB_ASSERT_WITH_ERROR_GOTO(PLAT_HASH_Update(ctx, data, dataSize) == 0, QLIB_STATUS__HARDWARE_FAILURE, ret, error);
    QLIB_ASSERT_WITH_ERROR_GOTO(PLAT_HASH_Finish(ctx, output) == 0, QLIB_STATUS__HARDWARE_FAILURE, ret, error);
    return QLIB_STATUS__OK;
error:
    (void)PLAT_HASH_Finish(ctx, output); // to clear hash context on failure
    return ret;
}

#ifdef QLIB_HASH_OPTIMIZATION_ENABLED
QLIB_STATUS_T QLIB_HASH_Async(QLIB_CONTEXT_T* qlibContext, U32* output, const void* data, U32 dataSize)
{
    QLIB_STATUS_T ret;
    QLIB_ASSERT_RET(PLAT_HASH_Init(&qlibContext->hashState.context,
                                   ((dataSize == 55u) && ADDRESS_ALIGNED32(data)) ? QLIB_HASH_OPT_FIXED_55_ALIGNED
                                                                                  : QLIB_HASH_OPT_NONE) == 0,
                    QLIB_STATUS__HARDWARE_FAILURE);
    QLIB_ASSERT_WITH_ERROR_GOTO(PLAT_HASH_Update(qlibContext->hashState.context, data, dataSize) == 0,
                                QLIB_STATUS__HARDWARE_FAILURE,
                                ret,
                                error);
    qlibContext->hashState.outputData = output;
    return QLIB_STATUS__OK;
error:
    (void)PLAT_HASH_Finish(qlibContext->hashState.context, output);
    qlibContext->hashState.context = NULL;
    return ret;
}

QLIB_STATUS_T QLIB_HASH_Async_WaitWhileBusy(QLIB_CONTEXT_T* qlibContext)
{
    int ret;
    QLIB_ASSERT_RET(qlibContext->hashState.context != NULL && qlibContext->hashState.outputData != NULL,
                    QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    ret                               = PLAT_HASH_Finish(qlibContext->hashState.context, qlibContext->hashState.outputData);
    qlibContext->hashState.outputData = NULL;
    qlibContext->hashState.context    = NULL;
    QLIB_ASSERT_RET(ret == 0, QLIB_STATUS__HARDWARE_FAILURE);
    return QLIB_STATUS__OK;
}
#endif // QLIB_HASH_OPTIMIZATION_ENABLED
#endif // Q2_API
