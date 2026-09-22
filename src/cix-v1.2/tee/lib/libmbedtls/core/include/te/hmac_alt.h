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

#ifndef __HMAC_ALT_H__
#define __HMAC_ALT_H__

#if !defined(MBEDTLS_CONFIG_FILE)
#include "config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define MBEDTLS_ERR_HMAC_INVALID_KEY_LENGTH                -0x0020  /**< Invalid key length. */
#define MBEDTLS_ERR_HMAC_INVALID_INPUT_LENGTH              -0x0022  /**< Invalid data input length. */

#define MBEDTLS_ERR_HMAC_ALLOC_FAILED                      -0x0010  /**< Memory allocation failed. */
#define MBEDTLS_ERR_HMAC_HW_ACCEL_FAILED                   -0x0011  /**< HMAC hardware accelerator failed. */

#define MBEDTLS_HMAC_MAGIC              (0x484D4143U)               /* HMAC */

struct te_hmac_ctx;

typedef struct mbedtls_hmac_context {
    uint32_t magic;
    struct te_hmac_ctx *hmac;
    mbedtls_md_context_t *md_ctx;
#if defined(MBEDTLS_MD2_C) || \
    defined(MBEDTLS_MD4_C) || \
    defined(MBEDTLS_RIPEMD160_C)
    void *sw_hmac;
#endif
}mbedtls_hmac_context;


/**
 * \brief          This function releases and clears the specified hmac context.
 *
 * \param ctx      The HMAC context to clear.
 *                 If this is \c NULL, this function does nothing.
 *                 Otherwise, the context must have been at least initialized.
 */
void mbedtls_hmac_free( mbedtls_hmac_context *ctx );

/**
 * \brief           This function clones the state of a HMAC operation.
 *
 *                  This function is usually included in the mbedtls_md_clone().
 *                  It is rare to directly call this function elsewhere.
 *
 * \param dst       The HMAC context to clone to.
 * \param src       The HMAC context to clone from.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                  failure.
 */
int mbedtls_hmac_clone( mbedtls_hmac_context *dst,
                        const mbedtls_hmac_context *src );

/**
 * \brief          HMAC checkup routine.
 *
 * \return         \c 0 on success.
 * \return         \c 1 on failure.
 */
int mbedtls_hmac_self_test( int verbose );

#endif

#ifdef __cplusplus
}
#endif
#endif
