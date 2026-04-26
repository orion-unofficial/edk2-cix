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

#ifndef __SM4_ALT_H__
#define __SM4_ALT_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define MBEDTLS_ERR_SM4_ALLOC_FAILED                -0x0010  /**< Failed to allocate memory. */

#define MBEDTLS_SM4_MAGIC            (0x534D34U)    /* SM4 */
#define MBEDTLS_SM4_XTS_MAGIC        (0x53585453U)  /* SXTS */

struct te_xts_ctx;
struct te_cipher_ctx;

typedef struct mbedtls_sm4_context{
    uint32_t magic;
    struct te_cipher_ctx *cipher;
}mbedtls_sm4_context;

typedef struct mbedtls_sm4_xts_context{
    uint32_t magic;
    struct te_xts_ctx *xts;
}mbedtls_sm4_xts_context;

#endif

#ifdef __cplusplus
}
#endif
#endif
