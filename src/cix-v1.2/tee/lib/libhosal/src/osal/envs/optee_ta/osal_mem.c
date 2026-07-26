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
#include "../../common/osal_internal.h"
#include <tee_internal_api.h>

void *osal_env_alloc(size_t size)
{
    return TEE_Malloc((uint32_t)size, TEE_MALLOC_FILL_ZERO);
}

void osal_env_free(void *ptr)
{
    if (ptr)
        TEE_Free(ptr);
    return;
}

void *osal_env_zalloc(size_t size)
{
    return TEE_Malloc((uint32_t)size, TEE_MALLOC_FILL_ZERO);
}

#ifdef CFG_OSAL_MEM_DEBUG
unsigned long osal_mem_debug_lock(void)
{
    return 0UL;
}

void osal_mem_debug_unlock(unsigned long flags)
{
    (void)flags;
    return;
}
#endif