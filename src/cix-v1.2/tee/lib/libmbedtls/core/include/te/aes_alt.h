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

#ifndef __AES_ALT_H__
#define __AES_ALT_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__
#define MBEDTLS_ERR_AES_ALLOC_FAILED               -0x0010  /**< Failed to allocate memory. */


#define MBEDTLS_AES_MAGIC           (0x414553U)     /* AES*/
#define MBEDTLS_AES_XTS_MAGIC       (0x41585453U)   /* AXTS*/

struct te_xts_ctx;
struct te_cipher_ctx;

#if defined(CFG_CRYPTO_AES_CIX_ENG)
typedef struct mbedtls_aes_context_{
    uint32_t magic;
    struct te_cipher_ctx *cipher;
}mbedtls_aes_context;
#elif defined(CFG_CRYPTO_AES_ARM_CE)
typedef struct mbedtls_aes_context {
	uint32_t key[60];
	unsigned int round_count;
} mbedtls_aes_context;
#endif

typedef struct mbedtls_aes_xts_context_{
    uint32_t magic;
    struct te_xts_ctx *xts;
}mbedtls_aes_xts_context;

#endif

#ifdef __cplusplus
}
#endif
#endif
