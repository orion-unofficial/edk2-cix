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

#ifndef __GCM_ALT_H__
#define __GCM_ALT_H__

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/* Error codes in range 0x0020-0x0022 */
#define MBEDTLS_ERR_GCM_INVALID_KEY_LENGTH                -0x0020  /**< Invalid key length. */
#define MBEDTLS_ERR_GCM_INVALID_INPUT_LENGTH              -0x0022  /**< Invalid data input length. */

#define MBEDTLS_ERR_GCM_ALLOC_FAILED         -0x6180  /**< Failed to allocate memory. */

#define MBEDTLS_GCM_MAGIC       (0x47434DU)     /* GCM */

struct te_gcm_ctx;

typedef struct mbedtls_gcm_context {
    uint32_t magic;
    bool init;                            /*!< HW crypto engine initialization status */
    struct te_gcm_ctx *gcm;
    mbedtls_cipher_id_t cipher_id;
#if defined(MBEDTLS_CAMELLIA_C)
    mbedtls_cipher_context_t cipher_ctx;  /*!< The cipher context used. */
    uint64_t HL[16];                      /*!< Precalculated HTable low. */
    uint64_t HH[16];                      /*!< Precalculated HTable high. */
    uint64_t len;                         /*!< The total length of the encrypted data. */
    uint64_t add_len;                     /*!< The total length of the additional data. */
    unsigned char base_ectr[16];          /*!< The first ECTR for tag. */
    unsigned char y[16];                  /*!< The Y working value. */
    unsigned char buf[16];                /*!< The buf working value. */
    int mode;                             /*!< The operation to perform:
                                               #MBEDTLS_GCM_ENCRYPT or
                                               #MBEDTLS_GCM_DECRYPT. */
#endif
} mbedtls_gcm_context;

#endif

#ifdef __cplusplus
}
#endif
#endif
