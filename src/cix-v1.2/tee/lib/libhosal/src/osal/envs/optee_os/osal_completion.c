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

#include <kernel/mutex.h>
#include "osal_completion.h"
#include "osal_mem.h"
#include "osal_assert.h"
#include "utils_ext.h"

osal_err_t osal_completion_init(osal_completion_t *comp)
{
    OSAL_ASSERT(comp != NULL);
    return OSAL_SUCCESS;
}

void osal_completion_destroy(osal_completion_t *comp)
{
    OSAL_ASSERT(comp != NULL);
    return;
}

void osal_completion_wait(osal_completion_t *comp)
{
    OSAL_ASSERT(comp != NULL);
    return;
}

void osal_completion_signal(osal_completion_t *comp)
{
    OSAL_ASSERT(comp != NULL);
    return;
}

void osal_completion_broadcast(osal_completion_t *comp)
{
    OSAL_ASSERT(comp != NULL);
    return;
}

void osal_completion_reset(osal_completion_t *comp)
{
    OSAL_ASSERT(comp != NULL);
    return;
}

