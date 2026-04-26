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
#include "driver/te_drv_otp.h"

#include <string.h>

#if defined(MBEDTLS_SELF_TEST)
#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#define mbedtls_printf printf
#endif /* MBEDTLS_PLATFORM_C */
#endif /* MBEDTLS_SELF_TEST */

#define OTP_VALIDATE_RET(cond)                             \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_OTP_BAD_INPUT_DATA )

#define OTP_VALIDATE(cond)  MBEDTLS_INTERNAL_VALIDATE( cond )

#if defined(MBEDTLS_OTP_ALT)

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_OTP_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
        case TE_ERROR_BAD_INPUT_DATA:
            errno = MBEDTLS_ERR_OTP_BAD_INPUT_DATA;
            break;
        case TE_ERROR_BAD_INPUT_LENGTH:
            errno = MBEDTLS_ERR_OTP_BAD_INPUT_DATA;
            break;
        case TE_ERROR_OVERFLOW:
            errno = MBEDTLS_ERR_OTP_OVERFLOW;
            break;
        case TE_ERROR_NOT_SUPPORTED:
            errno = MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED;
            break;
        case TE_ERROR_ACCESS_DENIED:
        case TE_ERROR_SECURITY:
            errno = MBEDTLS_ERR_OTP_ACCESS_DENIED;
            break;
        default:
            errno = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
            break;
    }

    return errno;
}


void mbedtls_otp_init( mbedtls_otp_context *ctx )
{
    OTP_VALIDATE( ctx != NULL );
    if (NULL != ctx) {
        if (MBEDTLS_OTP_MAGIC == ctx->magic &&
            ctx->otp_drv != NULL) {
            mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
        }
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
    ctx->otp_drv = (te_otp_drv_t *)te_drv_get(te_platform_get_drvhandle(),
                                      TE_DRV_TYPE_OTP);
    OSAL_ASSERT(ctx->otp_drv != NULL);
    ctx->magic = MBEDTLS_OTP_MAGIC;
}

void mbedtls_otp_free( mbedtls_otp_context *ctx )
{
    OTP_VALIDATE( ctx != NULL );
    OTP_VALIDATE( ctx->magic == MBEDTLS_OTP_MAGIC );
    OTP_VALIDATE( ctx->otp_drv != NULL );
    te_drv_put(te_platform_get_drvhandle(), TE_DRV_TYPE_OTP);
    mbedtls_platform_zeroize(ctx, sizeof(*ctx));
}

int mbedtls_otp_read( mbedtls_otp_context *ctx,
                      size_t off,
                      uint8_t *buf,
                      size_t len )
{
    OTP_VALIDATE_RET( ctx != NULL );
    OTP_VALIDATE_RET( ctx->magic == MBEDTLS_OTP_MAGIC );
    return _convert_retval_to_mbedtls(
                te_otp_read(ctx->otp_drv, off, buf, len));
}

int mbedtls_otp_get_conf(mbedtls_otp_context *ctx,
                         mbedtls_otp_conf *conf)
{
    int ret = 0;
    te_otp_conf_t _config = {0};

    (void) ctx;
    OTP_VALIDATE_RET( conf != NULL );
    ret = te_otp_get_conf( NULL, &_config );
    if (ret == TE_SUCCESS) {
        conf->otp_exist = _config.otp_exist;
        conf->otp_tst_sz = _config.otp_tst_sz;
        conf->otp_s_sz = _config.otp_s_sz;
        conf->otp_ns_sz = _config.otp_ns_sz;
        conf->otp_skey_sz = _config.otp_skey_sz;
    }
    return _convert_retval_to_mbedtls(ret);
}

int mbedtls_otp_write( mbedtls_otp_context *ctx,
                  size_t off,
                  const uint8_t *buf,
                  size_t len )
{
    OTP_VALIDATE_RET( ctx != NULL );
    OTP_VALIDATE_RET( ctx->magic == MBEDTLS_OTP_MAGIC );
    OTP_VALIDATE_RET( buf != NULL );

    return _convert_retval_to_mbedtls(
                        te_otp_write(ctx->otp_drv, off, buf, len));
}

#ifdef CFG_OTP_WITH_PUF

static int get_vops(struct te_otp_drv *drv, emem_puf_ops_t **pvops)
{
    int ret = TE_SUCCESS;
    union {
        void *v;
        emem_puf_ops_t *vops;
    } u = { NULL, };

    ret = te_otp_get_vops( drv, &u.v );
    if (TE_SUCCESS == ret) {
        *pvops = u.vops;
    }

    return ret;
}

int mbedtls_otp_puf_enroll(mbedtls_otp_context *ctx)
{
    int ret = TE_SUCCESS;
    emem_puf_ops_t *vops = NULL;
    OTP_VALIDATE_RET( ctx != NULL );
    OTP_VALIDATE_RET( ctx->magic == MBEDTLS_OTP_MAGIC );
    ret = get_vops( ctx->otp_drv, &vops );
    if (TE_SUCCESS != ret) {
        return _convert_retval_to_mbedtls(ret);
    }
    return _convert_retval_to_mbedtls(vops->enroll(ctx->otp_drv->hctx));
}

int mbedtls_otp_puf_quality_check(mbedtls_otp_context *ctx)
{
    int ret = TE_SUCCESS;
    emem_puf_ops_t *vops = NULL;
    OTP_VALIDATE_RET( ctx != NULL );
    OTP_VALIDATE_RET( ctx->magic == MBEDTLS_OTP_MAGIC );
    ret = get_vops( ctx->otp_drv, &vops );
    if (TE_SUCCESS != ret) {
        return _convert_retval_to_mbedtls(ret);
    }
    return _convert_retval_to_mbedtls(vops->quality_check(ctx->otp_drv->hctx));
}

int mbedtls_otp_puf_initial_margin_read( mbedtls_otp_context *ctx,
                                            size_t off,
                                            uint8_t *buf,
                                            size_t len )
{
    int ret = TE_SUCCESS;
    emem_puf_ops_t *vops = NULL;
    OTP_VALIDATE_RET( ctx != NULL );
    OTP_VALIDATE_RET( ctx->magic == MBEDTLS_OTP_MAGIC );
    ret = get_vops( ctx->otp_drv, &vops );
    if (TE_SUCCESS != ret) {
        return _convert_retval_to_mbedtls(ret);
    }
    return _convert_retval_to_mbedtls(
                vops->init_margin_read(ctx->otp_drv->hctx, off, buf, len));
}

int mbedtls_otp_puf_pgm_margin_read( mbedtls_otp_context *ctx,
                                            size_t off,
                                            uint8_t *buf,
                                            size_t len )
{
    int ret = TE_SUCCESS;
    emem_puf_ops_t *vops = NULL;
    OTP_VALIDATE_RET( ctx != NULL );
    OTP_VALIDATE_RET( ctx->magic == MBEDTLS_OTP_MAGIC );
    ret = get_vops( ctx->otp_drv, &vops );
    if (TE_SUCCESS != ret) {
        return _convert_retval_to_mbedtls(ret);
    }
    return _convert_retval_to_mbedtls(
                vops->pgm_margin_read(ctx->otp_drv->hctx, off, buf, len));
}
#endif /* CFG_OTP_WITH_PUF */
#endif /* MBEDTLS_OTP_ALT */

#if defined(MBEDTLS_SELF_TEST)

int mbedtls_otp_self_test( int verbose )
{
    int ret = 0;
    mbedtls_otp_context ctx;
    mbedtls_otp_conf conf = {0};
    unsigned char buf[32];    /* max otp field size */
    unsigned int wofs = 0;

    mbedtls_otp_init( &ctx );

    if( verbose != 0 )
        mbedtls_printf( "  OTP get_conf : " );

    if ( ( ret = mbedtls_otp_get_conf( &ctx, &conf ) ) != 0 )
        goto fail;

    if ( !conf.otp_exist )
        goto fail;

    if( verbose != 0 )
        mbedtls_printf( "passed\n" );

    if( verbose != 0 )
        mbedtls_printf( "  OTP read     : " );

#define MBED_OTP_TEST_READ(NAME) do {                     \
    OSAL_ASSERT(MBEDTLS_OTP_##NAME##_SIZE <= sizeof(buf));\
    ret = mbedtls_otp_read( &ctx,                         \
                            MBEDTLS_OTP_##NAME##_OFFSET,  \
                            buf,                          \
                            MBEDTLS_OTP_##NAME##_SIZE);   \
    if ( ret != 0 )                                       \
        goto fail;                                        \
} while (0)

    MBED_OTP_TEST_READ(MODEL_ID);
    MBED_OTP_TEST_READ(MODEL_KEY);
    MBED_OTP_TEST_READ(DEVICE_ID);
    MBED_OTP_TEST_READ(DEVICE_RK);
    MBED_OTP_TEST_READ(SEC_BOOT_HASH);
    MBED_OTP_TEST_READ(LCS);
    MBED_OTP_TEST_READ(SEC_BOOT_HASH);

    if( verbose != 0 )
        mbedtls_printf( "passed\n" );

    if( verbose != 0 )
        mbedtls_printf( "  OTP write    : " );

    /* set bit[0] of the last byte of the usr_sec_rgn */
    wofs = MBEDTLS_OTP_NSEC_REGION_OFFSET +
           conf.otp_ns_sz + conf.otp_s_sz - 1;
    if ( ( ret = mbedtls_otp_read( &ctx, wofs, buf, 1 ) ) != 0 )
        goto fail;

    buf[0] |= 1;
    if ( ( ret = mbedtls_otp_write( &ctx, wofs, buf, 1 ) ) != 0 )
        goto fail;

    if ( ( ret = mbedtls_otp_read( &ctx, wofs, buf, 1 ) ) != 0 )
        goto fail;

    if ( !( buf[0] & 1 ) )
        goto fail;

    if( verbose != 0 )
        mbedtls_printf( "passed\n\n" );

    goto exit;

fail:
    if ( verbose )
        mbedtls_printf( "failed\n" );

exit:
    mbedtls_otp_free( &ctx );

    return ( ret );
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_OTP_C */
