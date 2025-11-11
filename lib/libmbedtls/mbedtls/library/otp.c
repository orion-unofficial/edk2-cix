/*
 * Copyright (c) 2020, Arm Technology (China) Co., Ltd.
 * All rights reserved.
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,
 * any derivative work shall retain this copyright notice.
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_OTP_C)

#include "mbedtls/otp.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/platform.h"

#include <string.h>

#if defined(MBEDTLS_SELF_TEST)
#if defined(MBEDTLS_PLATFORM_C)
#else
#include <stdio.h>
#define mbedtls_printf printf
#endif /* MBEDTLS_PLATFORM_C */
#endif /* MBEDTLS_SELF_TEST */

#if !defined(MBEDTLS_OTP_ALT)
#define OTP_VALIDATE_RET( cond )    \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_OTP_BAD_INPUT_DATA )
#define OTP_VALIDATE( cond )        \
    MBEDTLS_INTERNAL_VALIDATE( cond )

void mbedtls_otp_init( mbedtls_otp_context *ctx )
{
   (void)ctx;
}

void mbedtls_otp_free( mbedtls_otp_context *ctx )
{
    (void)ctx;
}

int mbedtls_otp_read( mbedtls_otp_context *ctx,
                 size_t off,
                 uint8_t *buf,
                 size_t len )
{
    (void)ctx;
    (void)off;
    (void)buf;
    (void)len;
    return MBEDTLS_ERR_OTP_FEATURE_UNAVAILABLE;
}

int mbedtls_otp_get_conf(mbedtls_otp_context *ctx,
                    mbedtls_otp_conf *conf)
{
    (void)ctx;
    (void)conf;
    return MBEDTLS_ERR_OTP_FEATURE_UNAVAILABLE;
}

int mbedtls_otp_write( mbedtls_otp_context *ctx,
                  size_t off,
                  const uint8_t *buf,
                  size_t len )
{
    (void)ctx;
    (void)off;
    (void)buf;
    (void)len;
    return MBEDTLS_ERR_OTP_FEATURE_UNAVAILABLE;
}

int mbedtls_otp_get_vops( mbedtls_otp_context *ctx,
                      void **vops )
{
    (void)ctx;
    (void)vops;
    return MBEDTLS_ERR_OTP_FEATURE_UNAVAILABLE;
}

#endif /* !MBEDTLS_OTP_ALT */

#endif /* !MBEDTLS_OTP_ALT */
