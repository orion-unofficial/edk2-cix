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

#ifndef __OSAL_ATOMIC_H__
#define __OSAL_ATOMIC_H__

#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *osal_atomic_t;
OSAL_API void osal_atomic_store(osal_atomic_t *atomic, uint32_t data);
OSAL_API uint32_t osal_atomic_load(osal_atomic_t *atomic);

/* return the new value after inc */
OSAL_API uint32_t osal_atomic_inc(osal_atomic_t *atomic);

/* return the new value after dec */
OSAL_API uint32_t osal_atomic_dec(osal_atomic_t *atomic);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_ATOMIC_H__ */
