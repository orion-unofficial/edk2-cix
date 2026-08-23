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

#ifndef __OSAL_THREAD_H__
#define __OSAL_THREAD_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *osal_thread_t;
typedef osal_err_t (*osal_thread_entry_t)(void *arg);
OSAL_API osal_err_t osal_thread_create(osal_thread_t *thread,
                                       osal_thread_entry_t entry,
                                       void *arg);
OSAL_API void osal_wait_thread_done(osal_thread_t thread);
OSAL_API void osal_thread_destroy(osal_thread_t thread);
OSAL_API uint32_t osal_thread_id(void);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_THREAD_H__ */
