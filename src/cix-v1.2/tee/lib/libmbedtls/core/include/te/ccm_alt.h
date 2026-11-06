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

#ifndef __CCM_ALT_H__
#define __CCM_ALT_H__

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
#define MBEDTLS_ERR_CCM_INVALID_KEY_LENGTH                -0x0020  /**< Invalid key length. */
#define MBEDTLS_ERR_CCM_INVALID_INPUT_LENGTH              -0x0022  /**< Invalid data input length. */

#define MBEDTLS_ERR_CCM_ALLOC_FAILED                      -0x0010  /**< Memory allocation failed. */

#define MBEDTLS_CCM_MAGIC       (0x43434DU) /* CCM */

struct te_ccm_ctx;

typedef struct mbedtls_ccm_context {
    uint32_t magic;
    bool init;                              /*!< HW crypto engine initialization status */
    struct te_ccm_ctx *crypt;
    mbedtls_cipher_id_t cipher_id;
#if defined(MBEDTLS_CAMELLIA_C)
    mbedtls_cipher_context_t cipher_ctx;    /*!< The cipher context used. */
#endif
} mbedtls_ccm_context;

#endif

#ifdef __cplusplus
}
#endif
#endif
