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

#ifndef __OSAL_SEM_H__
#define __OSAL_SEM_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *osal_sem_t;
OSAL_API osal_err_t osal_sem_create(osal_sem_t *sem, uint32_t init_value);
OSAL_API void osal_sem_post(osal_sem_t sem);
OSAL_API void osal_sem_wait(osal_sem_t sem);
OSAL_API void osal_sem_destroy(osal_sem_t sem);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_SEM_H__ */
