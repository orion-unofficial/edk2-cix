/*
 * Copyright (c) 2020-2021, Arm Technology (China) Co., Ltd.
 * All rights reserved.
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else  /* !MBEDTLS_CONFIG_FILE */
#include MBEDTLS_CONFIG_FILE
#endif /* MBEDTLS_CONFIG_FILE */

#if defined(MBEDTLS_SNOW3G_C)
#if defined(CFG_MBEDTLS_TE)
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"

#endif /* CFG_MBEDTLS_TE */
#if defined(MBEDTLS_SNOW3G_ALT)
#include "mbedtls/snow3g.h"

#include "te_uea2.h"
#include "te_uia2.h"

#define MBEDTLS_KEYBITS_128     (128U)

#if defined(CFG_MBEDTLS_TE)
/* Parameter validation macros based on platform_util.h */
#define SNOW3G_VALIDATE_RET( cond )    \
    MBEDTLS_INTERNAL_VALIDATE_RET( (cond), MBEDTLS_ERR_SNOW3G_BAD_INPUT_DATA )
#define SNOW3G_VALIDATE( cond )        \
    MBEDTLS_INTERNAL_VALIDATE( (cond) )
#else /* CFG_MBEDTLS_TE */
#define mbedtls_printf  OSAL_LOG_INFO
#define mbedtls_calloc  osal_calloc
#define mbedtls_free    osal_free
/* Parameter validation macros */
#define SNOW3G_VALIDATE_RET( cond )                   \
     do {                                             \
        if (!(cond)) {                                \
            return MBEDTLS_ERR_SNOW3G_BAD_INPUT_DATA; \
        }                                             \
     } while (0)

#define SNOW3G_VALIDATE( cond )        \
     do {                              \
        if (!(cond)) {                 \
            return;                    \
        }                              \
     } while (0)

extern te_drv_handle te_platform_get_drvhandle(void);
#endif /* !CFG_MBEDTLS_TE */

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            errno = 0;
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_SNOW3G_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_SNOW3G_BAD_INPUT_DATA;
            break;
        case TE_ERROR_BAD_KEY_LENGTH:
            errno = MBEDTLS_ERR_SNOW3G_INVALID_KEY_LENGTH;
            break;
        default:
            errno = MBEDTLS_ERR_SNOW3G_HW_ACCEL_FAILED;
            break;
    }

    return errno;
}

void mbedtls_uea2_init( mbedtls_uea2_context *ctx )
{
    te_uea2_ctx_t *uea2 = NULL;
    int ret = TE_SUCCESS;
    SNOW3G_VALIDATE( ctx != NULL );
    if ( (MBEDTLS_UEA2_MAGIC == ctx->magic) && (ctx->uea2 != NULL) ) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    uea2 = (te_uea2_ctx_t *)mbedtls_calloc( 1, sizeof(*uea2) );
    OSAL_ASSERT( uea2 != NULL );
    ret = te_uea2_init( uea2, te_platform_get_drvhandle(), TE_MAIN_ALGO_SNOW3G );
    OSAL_ASSERT( ret == TE_SUCCESS );
    osal_memset( ctx, 0x00, sizeof(*ctx) );
    ctx->uea2 = uea2;
    ctx->magic = MBEDTLS_UEA2_MAGIC;
}

void mbedtls_uea2_free( mbedtls_uea2_context *ctx )
{
    SNOW3G_VALIDATE( (ctx != NULL) && (MBEDTLS_UEA2_MAGIC == ctx->magic) &&
                     (ctx->uea2 != NULL));
    (void)te_uea2_free(ctx->uea2);
    mbedtls_free(ctx->uea2);
    osal_memset(ctx, 0x00, sizeof(*ctx));
}

int mbedtls_uea2_setkey( mbedtls_uea2_context *ctx,
                         const unsigned char key[16] )
{
    int ret = TE_SUCCESS;

    SNOW3G_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_UEA2_MAGIC == ctx->magic) );
    ret = te_uea2_setkey( ctx->uea2, key, MBEDTLS_KEYBITS_128);
    return _convert_retval_to_mbedtls( ret );
}

static void _mbedtls_sec_key_to_te_sec_key( te_sec_key_t *sec_key,
                                            const mbedtls_klad_seckey_t *key )
{
    sec_key->sel = (MBEDTLS_KL_KEY_MODEL == key->sel) ? TE_KL_KEY_MODEL :
                                                        TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    osal_memcpy(sec_key->eks, key->eks,
                sizeof(sec_key->eks) > sizeof(key->eks) ?
                sizeof(key->eks) : sizeof(sec_key->eks));
}

int mbedtls_uea2_setseckey( mbedtls_uea2_context *ctx,
                            const mbedtls_klad_seckey_t *key )
{
    int ret = TE_SUCCESS;
    te_sec_key_t keydesc = {0};

    SNOW3G_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_UEA2_MAGIC == ctx->magic) &&
                         (key != NULL) && ((MBEDTLS_KL_KEY_MODEL == key->sel) ||
                                           (MBEDTLS_KL_KEY_ROOT == key->sel)) );
    _mbedtls_sec_key_to_te_sec_key( &keydesc, key );
    ret = te_uea2_setseckey( ctx->uea2, &keydesc );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_uea2_starts( mbedtls_uea2_context *ctx,
                         uint32_t count, uint32_t bearer, uint32_t dir )
{
    int ret = TE_SUCCESS;
    uint8_t iv[16] = {0};

    SNOW3G_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_UEA2_MAGIC == ctx->magic) );
    ret = te_uea2_build_iv( iv, count, bearer, dir );
    if (ret != TE_SUCCESS) {
        goto err_buildiv;
    }
    ret = te_uea2_start( ctx->uea2, iv );
err_buildiv:
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_uea2_update( mbedtls_uea2_context *ctx,
                         size_t size, const unsigned char *input,
                         unsigned char *output )
{
    int ret = TE_SUCCESS;

    SNOW3G_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_UEA2_MAGIC == ctx->magic) );
    ret = te_uea2_update( ctx->uea2, size, input, output );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_uea2_finish( mbedtls_uea2_context *ctx )
{
    int ret = TE_SUCCESS;

    SNOW3G_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_UEA2_MAGIC == ctx->magic) );
    ret = te_uea2_finish( ctx->uea2 );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_uea2_crypt( const unsigned char key[16],
                        uint32_t count, uint32_t bearer, uint32_t dir,
                        size_t size, const unsigned char *input,
                        unsigned char *output )
{
    int ret = 0;
    mbedtls_uea2_context ctx = {0};

    mbedtls_uea2_init( &ctx );
    ret = mbedtls_uea2_setkey( &ctx, key);
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uea2_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uea2_update( &ctx, size, input, output );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uea2_finish( &ctx );

fin:
    mbedtls_uea2_free( &ctx );
    return ret;
}

int mbedtls_uea2_crypt_seckey( const mbedtls_klad_seckey_t *key,
                               uint32_t count, uint32_t bearer, uint32_t dir,
                               size_t size, const unsigned char *input,
                               unsigned char *output )
{
    int ret = 0;
    mbedtls_uea2_context ctx = {0};

    mbedtls_uea2_init( &ctx );
    ret = mbedtls_uea2_setseckey( &ctx, key );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uea2_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uea2_update( &ctx, size, input, output );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uea2_finish( &ctx );

fin:
    mbedtls_uea2_free( &ctx );
    return ret;
}

void mbedtls_uia2_init( mbedtls_uia2_context *ctx )
{
    te_uia2_ctx_t *uia2 = NULL;
    int ret = TE_SUCCESS;
    SNOW3G_VALIDATE( ctx != NULL );
    if ( (MBEDTLS_UIA2_MAGIC == ctx->magic) && (ctx->uia2 != NULL) ) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    uia2 = (te_uia2_ctx_t *)mbedtls_calloc( 1, sizeof(*uia2) );
    OSAL_ASSERT( uia2 != NULL );
    ret = te_uia2_init( uia2, te_platform_get_drvhandle(), TE_MAIN_ALGO_SNOW3G );
    OSAL_ASSERT( ret == TE_SUCCESS );
    osal_memset( ctx, 0x00, sizeof(*ctx) );
    ctx->uia2 = uia2;
    ctx->magic = MBEDTLS_UIA2_MAGIC;
}

void mbedtls_uia2_free( mbedtls_uia2_context *ctx )
{
    SNOW3G_VALIDATE( (ctx != NULL) && (MBEDTLS_UIA2_MAGIC == ctx->magic) &&
                     (ctx->uia2 != NULL));
    (void)te_uia2_free(ctx->uia2);
    mbedtls_free(ctx->uia2);
    osal_memset(ctx, 0x00, sizeof(*ctx));
}

int mbedtls_uia2_setkey( mbedtls_uia2_context *ctx,
                         const unsigned char key[16] )
{
    int ret = TE_SUCCESS;

    SNOW3G_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_UIA2_MAGIC == ctx->magic) );
    ret = te_uia2_setkey( ctx->uia2, key, MBEDTLS_KEYBITS_128);
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_uia2_setseckey( mbedtls_uia2_context *ctx,
                            const mbedtls_klad_seckey_t *key )
{
    int ret = TE_SUCCESS;
    te_sec_key_t keydesc = {0};

    SNOW3G_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_UIA2_MAGIC == ctx->magic) &&
                         (key != NULL) && ((MBEDTLS_KL_KEY_MODEL == key->sel) ||
                                           (MBEDTLS_KL_KEY_ROOT == key->sel)) );
    _mbedtls_sec_key_to_te_sec_key( &keydesc, key );
    ret = te_uia2_setseckey( ctx->uia2, &keydesc );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_uia2_starts( mbedtls_uia2_context *ctx,
                         uint32_t count, uint32_t fresh, uint32_t dir )
{
    int ret = TE_SUCCESS;

    SNOW3G_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_UIA2_MAGIC == ctx->magic) );
    ret = te_uia2_start( ctx->uia2, count, fresh, dir );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_uia2_update( mbedtls_uia2_context *ctx,
                         size_t size, const unsigned char *input )
{
    int ret = TE_SUCCESS;

    SNOW3G_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_UIA2_MAGIC == ctx->magic) );
    ret = te_uia2_update( ctx->uia2, size, input );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_uia2_finish( mbedtls_uia2_context *ctx, unsigned char output[4] )
{
    int ret = TE_SUCCESS;

    SNOW3G_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_UIA2_MAGIC == ctx->magic) );
    ret = te_uia2_finish( ctx->uia2, output );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_uia2_mac( const unsigned char key[16],
                      uint32_t count, uint32_t fresh, uint32_t dir,
                      size_t size, const unsigned char *input,
                      unsigned char output[4] )
{
    int ret = 0;
    mbedtls_uia2_context ctx = {0};

    mbedtls_uia2_init( &ctx );
    ret = mbedtls_uia2_setkey( &ctx, key);
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uia2_starts( &ctx, count, fresh, dir );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uia2_update( &ctx, size, input );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uia2_finish( &ctx, output );

fin:
    mbedtls_uia2_free( &ctx );
    return ret;
}

int mbedtls_uia2_mac_seckey( const mbedtls_klad_seckey_t *key,
                             uint32_t count, uint32_t fresh, uint32_t dir,
                             size_t size, const unsigned char *input,
                             unsigned char output[4] )
{
    int ret = 0;
    mbedtls_uia2_context ctx = {0};

    mbedtls_uia2_init( &ctx );
    ret = mbedtls_uia2_setseckey( &ctx, key );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uia2_starts( &ctx, count, fresh, dir );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uia2_update( &ctx, size, input );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_uia2_finish( &ctx, output );

fin:
    mbedtls_uia2_free( &ctx );
    return ret;
}

#if defined(MBEDTLS_SELF_TEST)

static int mbedtls_uea2_self_test( int verbose )
{
    mbedtls_uea2_context ctx = {0};
    uint8_t input[] = {0x10, 0x11, 0x12, 0x31, 0xE0, 0x60, 0x25, 0x3A,
                       0x43, 0xFD, 0x3F, 0x57, 0xE3, 0x76, 0x07, 0xAB};
    uint8_t key[] = {0xEF, 0xA8, 0xB2, 0x22, 0x9E, 0x72, 0x0C, 0x2A,
                     0x7C, 0x36, 0xEA, 0x55, 0xE9, 0x60, 0x56, 0x95};
    uint32_t count = 0xE28BCF7B;
    uint32_t bearer = 0x18;
    uint32_t dir = 0;
    uint8_t expect_output[] = {0xE0, 0xDA, 0x15, 0xCA, 0x8E, 0x25, 0x54, 0xF5,
                               0xE5, 0x6C, 0x94, 0x68, 0xDC, 0x6C, 0x7C, 0x12};
    uint8_t *out = NULL;
    uint8_t *in_ptr= NULL;
    int ret = 0;

    mbedtls_uea2_init( &ctx );
    ret = mbedtls_uea2_setkey( &ctx, key );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_uea2_setkey failed(-%X)\n", -ret );
        }
        goto err;
    }

    ret = mbedtls_uea2_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_uea2_starts failed(-%X)\n", -ret );
        }
        goto err;
    }

    in_ptr = (uint8_t *)mbedtls_calloc( 1, sizeof(input) );
    if ( NULL == in_ptr ) {
        ret = MBEDTLS_ERR_SNOW3G_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_calloc failed, OOM!\n" );
        }
        goto err_aloc_in;
    }
    osal_memcpy( in_ptr, input, sizeof(input) );
    out = (uint8_t *)mbedtls_calloc( 1, sizeof(expect_output) );
    if ( NULL == out ) {
        ret = MBEDTLS_ERR_SNOW3G_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_calloc failed, OOM!\n" );
        }
        goto err_aloc_out;
    }
    ret = mbedtls_uea2_update( &ctx, sizeof(input), in_ptr, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_uea2_update failed(-%X)\n", -ret );
        }
        goto err_update;
    }

    ret = osal_memcmp( out, expect_output, sizeof(expect_output) );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "output mismatched!!!\n" );
            OSAL_LOG_INFO_DUMP_DATA( "expected:", expect_output, sizeof(expect_output) );
            OSAL_LOG_INFO_DUMP_DATA( "actual:", out, sizeof(input) );
            goto err_update;
        }
    }
    ret = mbedtls_uea2_finish( &ctx );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_uea2_finish failed(-%X)\n", -ret );
        }
        goto err_finish;
    }

    /* test all-in-one api */
    ret = mbedtls_uea2_crypt( key, count, bearer, dir, sizeof(input), in_ptr, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_uea2_crypt failed(-%X)\n", ret );
        }
        goto err_update;
    }

    ret = osal_memcmp( out, expect_output, sizeof(expect_output) );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "output mismatched!!!\n" );
            OSAL_LOG_INFO_DUMP_DATA( "expected:", expect_output, sizeof(expect_output) );
            OSAL_LOG_INFO_DUMP_DATA( "actual:", out, sizeof(input) );
        }
    }

err_finish:
err_update:
    mbedtls_free( out );
err_aloc_out:
    mbedtls_free( in_ptr );
err_aloc_in:
err:
    mbedtls_uea2_free( &ctx );
    return ret;
}

static int mbedtls_uia2_self_test( int verbose )
{
    mbedtls_uia2_context ctx = {0};
    uint8_t input[] = {0xD0, 0xA7, 0xD4, 0x63, 0xDF, 0x9F, 0xB2, 0xB2,
                       0x78, 0x83, 0x3F, 0xA0, 0x2E, 0x23, 0x5A, 0xA1,
                       0x72, 0xBD, 0x97, 0x0C, 0x14, 0x73, 0xE1, 0x29,
                       0x07, 0xFB, 0x64, 0x8B, 0x65, 0x99, 0xAA, 0xA0,
                       0xB2, 0x4A, 0x03, 0x86, 0x65, 0x42, 0x2B, 0x20,
                       0xA4, 0x99, 0x27, 0x6A, 0x50, 0x42, 0x70, 0x09};
    uint8_t key[] = {0xC7, 0x36, 0xC6, 0xAA, 0xB2, 0x2B, 0xFF, 0xF9,
                     0x1E, 0x26, 0x98, 0xD2, 0xE2, 0x2A, 0xD5, 0x7E};
    uint32_t count = 0x14793E41;
    uint32_t  fresh = 0x0397E8FD;
    uint32_t dir = 1;
    uint8_t expect_output[] = {0x38, 0xB5, 0x54, 0xC0};
    uint8_t *out = NULL;
    uint8_t *in_ptr= NULL;
    int ret = 0;

    mbedtls_uia2_init( &ctx );
    ret = mbedtls_uia2_setkey( &ctx, key );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_uia2_setkey failed(-%X)\n", -ret );
        }
        goto err;
    }

    ret = mbedtls_uia2_starts( &ctx, count, fresh, dir );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_uia2_starts failed(-%X)\n", -ret );
        }
        goto err;
    }

    in_ptr = (uint8_t *)mbedtls_calloc( 1, sizeof(input) );
    if ( NULL == in_ptr ) {
        ret = MBEDTLS_ERR_SNOW3G_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_calloc failed, OOM!\n" );
        }
        goto err_aloc_in;
    }
    osal_memcpy( in_ptr, input, sizeof(input) );
    out = (uint8_t *)mbedtls_calloc( 1, sizeof(expect_output) );
    if ( NULL == out ) {
        ret = MBEDTLS_ERR_SNOW3G_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_calloc failed, OOM!\n" );
        }
        goto err_aloc_out;
    }
    ret = mbedtls_uia2_update( &ctx, sizeof(input), in_ptr );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_uia2_starts failed(-%X)\n", -ret );
        }
        goto err_update;
    }
    ret = mbedtls_uia2_finish( &ctx, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_uia2_finish failed(-%X)\n", -ret );
        }
        goto err_fin;
    }
    ret = osal_memcmp( out, expect_output, sizeof(expect_output) );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "output mismatched!!!\n" );
            OSAL_LOG_INFO_DUMP_DATA( "expected:", expect_output, sizeof(expect_output) );
            OSAL_LOG_INFO_DUMP_DATA( "actual:", out, sizeof(expect_output) );
        }
        goto err_dat;
    }

    /* test all-in-one api */
    ret = mbedtls_uia2_mac( key, count, fresh, dir, sizeof(input), in_ptr, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_uea2_crypt failed(-%X)\n", ret );
        }
        goto err_dat;
    }

    ret = osal_memcmp( out, expect_output, sizeof(expect_output) );
    if (ret != 0) {
        if ( verbose != 0 ) {
            mbedtls_printf( "output mismatched!!!\n" );
            OSAL_LOG_INFO_DUMP_DATA( "expected:", expect_output, sizeof(expect_output) );
            OSAL_LOG_INFO_DUMP_DATA( "actual:", out, sizeof(expect_output) );
        }
    }

err_dat:
err_fin:
err_update:
    mbedtls_free( out );
err_aloc_out:
    mbedtls_free( in_ptr );
err_aloc_in:
err:
    mbedtls_uia2_free( &ctx );
    return ret;
}

int mbedtls_snow3g_self_test( int verbose )
{
    int ret = 0;

    if ( verbose != 0 ) {
        mbedtls_printf("snow3g uea2 test ");
    }
    ret = mbedtls_uea2_self_test( verbose );
    if ( ret != 0 ) {
        goto exit;
    }
    if ( verbose != 0 ) {
        mbedtls_printf(" passed \n");
        mbedtls_printf("snow3g uia2 test ");
    }
    ret = mbedtls_uia2_self_test( verbose );
    if ( ret != 0 ) {
        goto exit;
    }
    if ( verbose != 0 ) {
        mbedtls_printf(" passed \n");
    }

exit:
    if ( (ret != 0) && (verbose != 0) ) {
        mbedtls_printf(" failed \n");
    }
    return ret;
}


#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_SNOW3G_ALT */
#endif /* MBEDTLS_SNOW3G_C */
