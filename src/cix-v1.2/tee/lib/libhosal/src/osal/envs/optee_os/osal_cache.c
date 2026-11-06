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

#include <arm.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <tee/cache.h>
#include "osal_cache.h"

osal_err_t osal_cache_clean(uint8_t *buf, size_t size)
{
    return (osal_err_t)cache_operation(TEE_CACHECLEAN, buf, size);
}

osal_err_t osal_cache_flush(uint8_t *buf, size_t size)
{
    return (osal_err_t)cache_operation(TEE_CACHEFLUSH, buf, size);
}

osal_err_t osal_cache_invalidate(uint8_t *buf, size_t size)
{
    return (osal_err_t)cache_operation(TEE_CACHEINVALIDATE, buf, size);
}
