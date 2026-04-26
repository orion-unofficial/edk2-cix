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

#ifndef __SHA512_ALT_H__
#define __SHA512_ALT_H__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define MBEDTLS_ERR_SHA512_ALLOC_FAILED                   -0x0010  /**< Failed to allocate memory. */
#define MBEDTLS_ERR_SHA512_INVALID_INPUT_LENGTH           -0x0032  /**< The data input has an invalid length. */

#define MBEDTLS_SHA512_MAGIC       (0x53484135U)         /* SHA5 */

struct te_dgst_ctx;

typedef struct mbedtls_sha512_context {
    uint32_t magic;
    bool is_dgst_init;
    bool is_384;
    struct te_dgst_ctx * dgst;
} mbedtls_sha512_context;

#endif

#ifdef __cplusplus
}
#endif
#endif
