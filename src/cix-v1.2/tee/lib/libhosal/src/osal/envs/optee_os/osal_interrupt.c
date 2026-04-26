/**
 * Copyright (C), 2018-2020, Arm Technology (China) Co., Ltd.
 * All rights reserved
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#include <kernel/thread.h>
#include <kernel/interrupt.h>
#include <util.h>
#include "osal_interrupt.h"
#include "osal_assert.h"
#include "osal_mem.h"

typedef struct optee_itr_handler {
    struct itr_handler itr;
    osal_intr_handler_t uhandler;
    void *uparam;
} optee_itr_handler_t;

static enum itr_return osal_itr_cb(struct itr_handler *h)
{
    optee_itr_handler_t *ih = container_of(h, optee_itr_handler_t, itr);

    OSAL_ASSERT(ih->uhandler);
    ih->uhandler(ih->uparam);
    return ITRR_HANDLED;
}

unsigned long osal_intr_lock(void)
{
    return thread_mask_exceptions(THREAD_EXCP_FOREIGN_INTR |
                                  THREAD_EXCP_NATIVE_INTR);
}

void osal_intr_unlock(unsigned long flag)
{
    thread_unmask_exceptions((uint32_t)flag);
}

osal_err_t osal_irq_request(osal_intr_ctx_t *intr_ctx,
                            int32_t intr_num,
                            osal_intr_handler_t intr_handler,
                            void *para)
{
    optee_itr_handler_t *ih = NULL;

    OSAL_ASSERT(intr_ctx && intr_handler);
    ih = osal_calloc(1, sizeof(*ih));
    if (NULL == ih) {
        return OSAL_ERROR_OUT_OF_MEMORY;
    }

    ih->itr.it      = intr_num;
    ih->itr.flags   = ITRF_TRIGGER_LEVEL;
    ih->itr.handler = osal_itr_cb;
    ih->uhandler    = intr_handler;
    ih->uparam      = para;
    itr_add(&ih->itr);
    itr_enable(intr_num);

    intr_ctx->ctx = ih;
    return OSAL_SUCCESS;
}

void osal_irq_free(osal_intr_ctx_t *intr_ctx)
{
    optee_itr_handler_t *ih = NULL;

    OSAL_ASSERT(intr_ctx != NULL);
    ih = (optee_itr_handler_t *)intr_ctx->ctx;
    OSAL_ASSERT(ih != NULL);
    itr_free(&ih->itr);

    osal_free(ih);
    intr_ctx->ctx = NULL;
}
