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

#ifndef __DES_ALT_H__
#define __DES_ALT_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define MBEDTLS_ERR_DES_ALLOC_FAILED                      -0x0010  /**< Failed to allocate memory. */
#define MBEDTLS_ERR_DES_BAD_INPUT                         -0x000D /**< Bad input parameters to the function. */

#define MBEDTLS_DES_MAGIC        (0x444553U)     /* DES */
#define MBEDTLS_3DES_MAGIC       (0x33444553U)   /* 3DES */

struct te_cipher_ctx;

typedef struct mbedtls_des_context {
    uint32_t magic;
    uint32_t mode;
    struct te_cipher_ctx *cipher;
 } mbedtls_des_context;

typedef struct mbedtls_des3_context {
    uint32_t magic;
    uint32_t mode;
    struct te_cipher_ctx *cipher;
 } mbedtls_des3_context;

#endif

#ifdef __cplusplus
}
#endif
#endif