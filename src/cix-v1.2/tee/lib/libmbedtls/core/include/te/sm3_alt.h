/*
 * Copyright (c) 2020, Arm Technology (China) Co., Ltd.
 * All rights reserved.
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#ifndef __SM3_ALT_H__
#define __SM3_ALT_H__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define MBEDTLS_ERR_SM3_ALLOC_FAILED                   -0x0010  /**< Failed to allocate memory. */
#define MBEDTLS_ERR_SM3_INVALID_INPUT_LENGTH           -0x0032  /**< The data input has an invalid length. */

#define MBEDTLS_SM3_MAGIC           (0x534D33U) /* SM3 */

struct te_dgst_ctx;

typedef struct mbedtls_sm3_context {
    uint32_t magic;
    bool is_dgst_init;
    struct te_dgst_ctx *dgst;
}mbedtls_sm3_context;

#endif

#ifdef __cplusplus
}
#endif
#endif