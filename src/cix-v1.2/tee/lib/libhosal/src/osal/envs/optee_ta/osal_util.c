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

#include "osal_assert.h"
#include "osal_utils.h"
#include <tee_internal_api.h>

uint32_t osal_read_timestamp_ms(void)
{
    TEE_Time time = {0};

    TEE_GetREETime(&time);

    return time.seconds * 1000 + time.millis;
}

void osal_delay_us(uint32_t us)
{
    (void)(us);
    /* Not implement */
}

void osal_delay_ms(uint32_t ms)
{
    uint32_t start = osal_read_timestamp_ms();
    uint32_t end   = start + ms;

    if (end < start) {
        while (osal_read_timestamp_ms() >= start)
            ;
        while (osal_read_timestamp_ms() <= end)
            ;
    } else {
        while (osal_read_timestamp_ms() <= end)
            ;
    }
}

void osal_sleep_ms(uint32_t ms)
{
    TEE_Result tee_res = TEE_ERROR_GENERIC;

    if ((tee_res = TEE_Wait(ms)) != TEE_SUCCESS) {
        TEE_Panic(tee_res);
    }
}

uintptr_t osal_virt_to_phys(void *va)
{
    (void)(va);
    return 0;
}

void *osal_phys_to_virt(uintptr_t pa)
{
    (void)(pa);
    return NULL;
}