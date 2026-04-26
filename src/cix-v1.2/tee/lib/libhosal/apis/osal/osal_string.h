/**
 * Copyright (C), 2018-2020, Arm Technology (China) Co., Ltd.
 * All rights reserved
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and
 * any derivative work shall retain this copyright notice.
 *
 */
#ifndef __OSAL_STRING_H__
#define __OSAL_STRING_H__

#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define osal_memset memset
#define osal_memcpy memcpy
#define osal_memcmp memcmp
#define osal_strlen strlen
#define osal_strcpy strcpy
#define osal_strncpy strncpy
#define osal_strcmp strcmp
#define osal_strncmp strncmp
#define osal_snprintf snprintf

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_STRING_H__ */
