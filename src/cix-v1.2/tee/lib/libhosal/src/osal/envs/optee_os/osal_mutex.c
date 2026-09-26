/**
 * Copyright (C), 2018-2020, Arm Technology (China) Co., Ltd.
 * All rights reserved
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and
 * any derivative work shall retain this copyright notice.
 *
 */

#include <kernel/mutex.h>
#include "osal_mutex.h"
#include "osal_mem.h"
#include "osal_log.h"
#include "osal_assert.h"

typedef struct mutex_ctx {
    struct mutex mut;
} mutex_ctx_t;

osal_err_t osal_mutex_create(osal_mutex_t *mutex)
{
    mutex_ctx_t *ctx = NULL;

    OSAL_ASSERT(mutex != NULL);
    ctx = (mutex_ctx_t *)osal_calloc(1, sizeof(mutex_ctx_t));
    if (NULL == ctx) {
        return OSAL_ERROR_OUT_OF_MEMORY;
    }

    mutex_init(&ctx->mut);
    *mutex = (osal_mutex_t *)ctx;
    return OSAL_SUCCESS;
}

void osal_mutex_lock(osal_mutex_t mutex)
{
    mutex_ctx_t *ctx = (mutex_ctx_t *)mutex;

    OSAL_ASSERT(ctx != NULL);
    mutex_lock(&ctx->mut);
    return;
}

void osal_mutex_unlock(osal_mutex_t mutex)
{
    mutex_ctx_t *ctx = (mutex_ctx_t *)mutex;

    OSAL_ASSERT(ctx != NULL);
    mutex_unlock(&ctx->mut);
    return;
}

void osal_mutex_destroy(osal_mutex_t mutex)
{
    mutex_ctx_t *ctx = (mutex_ctx_t *)mutex;

    OSAL_ASSERT(ctx != NULL);
    mutex_destroy(&ctx->mut);
    osal_free(ctx);
    return;
}
