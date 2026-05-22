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

#ifndef __OSAL_MEM_BARRIER_H__
#define __OSAL_MEM_BARRIER_H__

#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* write memory barrier */
OSAL_API void osal_wmb(void);

/* read memory barrier */
OSAL_API void osal_rmb(void);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_MEM_BARRIER_H__ */