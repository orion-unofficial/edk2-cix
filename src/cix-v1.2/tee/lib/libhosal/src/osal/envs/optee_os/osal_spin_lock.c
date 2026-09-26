/*
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
#include <kernel/spinlock.h>
#include "osal_spin_lock.h"
#include "osal_mem.h"
#include "osal_log.h"
#include "osal_assert.h"

typedef struct optee_spin_ctx {
    unsigned int lock;
    unsigned int state;
} optee_spin_ctx_t;

osal_err_t osal_spin_lock_init(osal_spin_lock_t *lock)
{
    optee_spin_ctx_t *spin = NULL;
    OSAL_ASSERT(lock != NULL);

    spin = osal_calloc(1, sizeof(*spin));
    if (!spin) {
        return OSAL_ERROR_OUT_OF_MEMORY;
    }

    spin->lock = SPINLOCK_UNLOCK;
    lock->ctx  = (void *)spin;
    return OSAL_SUCCESS;
}

void osal_spin_lock_destroy(osal_spin_lock_t *lock)
{
    OSAL_ASSERT(lock != NULL);

    osal_free(lock->ctx);
    lock->ctx = NULL;
    return;
}

void osal_spin_lock(osal_spin_lock_t *lock)
{
    unsigned int flags     = 0;
    optee_spin_ctx_t *spin = NULL;
    OSAL_ASSERT(lock != NULL);

    spin = (optee_spin_ctx_t *)lock->ctx;
    /*
     * optee requires to mask the non-secure intr when holding a spinlock.
     */
    flags = thread_mask_exceptions(THREAD_EXCP_FOREIGN_INTR);
    cpu_spin_lock(&spin->lock);
    spin->state = flags;
    return;
}

void osal_spin_unlock(osal_spin_lock_t *lock)
{
    unsigned int flags     = 0;
    optee_spin_ctx_t *spin = NULL;
    OSAL_ASSERT(lock != NULL);

    spin  = (optee_spin_ctx_t *)lock->ctx;
    flags = spin->state;
    cpu_spin_unlock(&spin->lock);
    thread_unmask_exceptions(flags);
    return;
}

void osal_spin_lock_irqsave(osal_spin_lock_t *lock, unsigned long *flags)
{
    optee_spin_ctx_t *spin = NULL;
    OSAL_ASSERT(lock != NULL);
    OSAL_ASSERT(flags != NULL);

    spin   = (optee_spin_ctx_t *)lock->ctx;
    *flags = cpu_spin_lock_xsave(&spin->lock);
    return;
}

void osal_spin_unlock_irqrestore(osal_spin_lock_t *lock, unsigned long flags)
{
    optee_spin_ctx_t *spin = NULL;
    OSAL_ASSERT(lock != NULL);

    spin = (optee_spin_ctx_t *)lock->ctx;
    cpu_spin_unlock_xrestore(&spin->lock, flags);
    return;
}
