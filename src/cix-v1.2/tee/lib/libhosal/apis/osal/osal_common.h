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

#ifndef __OSAL_COMMON_H__
#define __OSAL_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#if defined(OSAL_ENV_UBOOT)
#include <common.h>
#include <malloc.h>
#elif defined(OSAL_ENV_LINUX_KERNEL)
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#else
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#endif

#define OSAL_API

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_COMMON_H__ */
