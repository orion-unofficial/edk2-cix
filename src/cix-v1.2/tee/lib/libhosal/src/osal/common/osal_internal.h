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

#ifndef __OSAL_INTERNAL_H__
#define __OSAL_INTERNAL_H__

#include "utils_ext.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void *osal_env_alloc(size_t size);
void *osal_env_zalloc(size_t size);
void osal_env_free(void *ptr);
unsigned long osal_mem_debug_lock(void);
void osal_mem_debug_unlock(unsigned long flags);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_INTERNAL_H__ */
