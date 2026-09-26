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

#ifndef __OSAL_MUTEX_H__
#define __OSAL_MUTEX_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *osal_mutex_t;
OSAL_API osal_err_t osal_mutex_create(osal_mutex_t *mutex);
OSAL_API void osal_mutex_lock(osal_mutex_t mutex);
OSAL_API void osal_mutex_unlock(osal_mutex_t mutex);
OSAL_API void osal_mutex_destroy(osal_mutex_t mutex);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_MUTEX_H__ */
