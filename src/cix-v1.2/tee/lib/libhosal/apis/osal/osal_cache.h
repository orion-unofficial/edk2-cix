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

#ifndef __OSAL_CACHE_H__
#define __OSAL_CACHE_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The cache line size is tight with different platform
 */
#define OSAL_CACHE_LINE_SIZE	(64U)

OSAL_API osal_err_t osal_cache_clean(uint8_t *buf, size_t size);
OSAL_API osal_err_t osal_cache_flush(uint8_t *buf, size_t size);
OSAL_API osal_err_t osal_cache_invalidate(uint8_t *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_CACHE_H__ */
