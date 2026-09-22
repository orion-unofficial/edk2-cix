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

#include "atomic.h"
#include "osal_atomic.h"

void osal_atomic_store(osal_atomic_t *atomic, uint32_t data)
{
    atomic_store_u32((uint32_t *)(atomic), data);
}

uint32_t osal_atomic_load(osal_atomic_t *atomic)
{
    return atomic_load_u32((uint32_t *)(atomic));
}

uint32_t osal_atomic_inc(osal_atomic_t *atomic)
{
    return atomic_inc32((volatile uint32_t *)(atomic));
}

uint32_t osal_atomic_dec(osal_atomic_t *atomic)
{
    return atomic_dec32((volatile uint32_t *)(atomic));
}
