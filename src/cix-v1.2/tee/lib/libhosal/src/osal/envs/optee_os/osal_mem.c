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

#include "osal_mem.h"
#include "osal_log.h"
#include "osal_assert.h"
#include "osal_string.h"
#include "../../common/osal_internal.h"
#include <malloc.h>
#include <kernel/spinlock.h>

#ifdef CFG_OSAL_MEM_DEBUG
static unsigned int alock = SPINLOCK_UNLOCK;
#endif

void *osal_env_alloc(size_t size)
{
    return malloc(size);
}

void *osal_env_zalloc(size_t size)
{
    return calloc(1, size);
}

void osal_env_free(void *ptr)
{
    if (ptr) {
        free(ptr);
    }
    return;
}

#ifdef CFG_OSAL_MEM_DEBUG
unsigned long osal_mem_debug_lock(void)
{
    unsigned long flags = 0;
    flags               = cpu_spin_lock_xsave(&alock);
    return flags;
}

void osal_mem_debug_unlock(unsigned long flags)
{
    cpu_spin_unlock_xrestore(&alock, flags);
    return;
}
#endif
