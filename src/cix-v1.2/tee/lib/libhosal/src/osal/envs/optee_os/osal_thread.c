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
#include "osal_log.h"
#include "osal_assert.h"
#include "osal_thread.h"

osal_err_t osal_thread_create(osal_thread_t *thread,
                              osal_thread_entry_t entry,
                              void *arg)
{
    (void)thread;
    (void)entry;
    (void)arg;
    return OSAL_ERROR_NOT_IMPLEMENTED;
}

void osal_wait_thread_done(osal_thread_t thread)
{
    (void)thread;
}

void osal_thread_destroy(osal_thread_t thread)
{
    (void)thread;
}

uint32_t osal_thread_id(void)
{
    return 0;
}
