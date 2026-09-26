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

#if defined(MBEDTLS_KASUMI_C)

#if defined(CFG_MBEDTLS_TE)
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"
#endif /* CFG_MBEDTLS_TE */

#if defined(MBEDTLS_KASUMI_ALT)
#include "mbedtls/kasumi.h"

#include "te_f8.h"
#include "te_f9.h"

#define MBEDTLS_KEYBITS_128     (128U)

#if defined(CFG_MBEDTLS_TE)
/* Parameter validation macros based on platform_util.h */
#define KASUMI_VALIDATE_RET( cond )                   \
    MBEDTLS_INTERNAL_VALIDATE_RET( (cond), MBEDTLS_ERR_KASUMI_BAD_INPUT_DATA )
#define KASUMI_VALIDATE( cond )                       \
    MBEDTLS_INTERNAL_VALIDATE( cond )
#else /* CFG_MBEDTLS_TE */
#define mbedtls_printf  OSAL_LOG_INFO
#define mbedtls_calloc  osal_calloc
#define mbedtls_free    osal_free
/* Parameter validation macros */
#define KASUMI_VALIDATE_RET( cond )                   \
     do {                                             \
        if (!(cond)) {                                \
            return MBEDTLS_ERR_KASUMI_BAD_INPUT_DATA; \
        }                                             \
     } while (0)

#define KASUMI_VALIDATE( cond )                       \
     do {                                             \
        if (!(cond)) {                                \
            return;                                   \
        }                                             \
     } while (0)

extern te_drv_handle te_platform_get_drvhandle(void);
#endif /* !CFG_MBEDTLS_TE */

static int _convert_retval_to_mbedtls( int errno )
{
    switch (errno) {
        case TE_SUCCESS:
            errno = 0;
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_KASUMI_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_KASUMI_BAD_INPUT_DATA;
            break;
        case TE_ERROR_BAD_KEY_LENGTH:
            errno = MBEDTLS_ERR_KASUMI_INVALID_KEY_LENGTH;
            break;
        default:
            errno = MBEDTLS_ERR_KASUMI_HW_ACCEL_FAILED;
            break;
    }
    return errno;
}

void mbedtls_f8_init( mbedtls_f8_context *ctx )
{
    te_f8_ctx_t *f8 = NULL;
    int ret = 0;
    KASUMI_VALIDATE( ctx != NULL );
    if ( (MBEDTLS_F8_MAGIC == ctx->magic) && (ctx->f8 != NULL) ) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    f8 = (te_f8_ctx_t *)mbedtls_calloc( 1, sizeof(*f8) );
    OSAL_ASSERT( f8 != NULL );
    ret = te_f8_init( f8, te_platform_get_drvhandle(), TE_MAIN_ALGO_KASUMI );
    OSAL_ASSERT( TE_SUCCESS == ret);
    osal_memset( ctx, 0x00, sizeof(*ctx) );
    ctx->f8 = f8;
    ctx->magic = MBEDTLS_F8_MAGIC;
}

void mbedtls_f8_free( mbedtls_f8_context *ctx )
{
    KASUMI_VALIDATE( (ctx != NULL) && (MBEDTLS_F8_MAGIC == ctx->magic) &&
                     (ctx->f8 != NULL));
    (void)te_f8_free( ctx->f8 );
    mbedtls_free( ctx->f8 );
    osal_memset( ctx, 0x00, sizeof(*ctx) );
}

int mbedtls_f8_setkey( mbedtls_f8_context *ctx,
                       const unsigned char key[16] )
{
    int ret = TE_SUCCESS;

    KASUMI_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_F8_MAGIC == ctx->magic) );
    ret = te_f8_setkey( ctx->f8, key, MBEDTLS_KEYBITS_128);
    return _convert_retval_to_mbedtls( ret );
}

static void _mbedtls_sec_key_to_te_sec_key( te_sec_key_t *sec_key,
                                            const mbedtls_klad_seckey_t *key )
{
    sec_key->sel = (MBEDTLS_KL_KEY_MODEL == key->sel) ? TE_KL_KEY_MODEL :
                                                        TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    osal_memcpy( sec_key->eks, key->eks,
                 sizeof(sec_key->eks) > sizeof(key->eks) ?
                 sizeof(key->eks) : sizeof(sec_key->eks) );
}

int mbedtls_f8_setseckey( mbedtls_f8_context *ctx,
                          const mbedtls_klad_seckey_t *key )
{
    int ret = TE_SUCCESS;
    te_sec_key_t keydesc = {0};

    KASUMI_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_F8_MAGIC == ctx->magic) &&
                         (key != NULL) && ((MBEDTLS_KL_KEY_MODEL == key->sel) ||
                                           (MBEDTLS_KL_KEY_ROOT == key->sel)) );
    _mbedtls_sec_key_to_te_sec_key( &keydesc, key );
    ret = te_f8_setseckey( ctx->f8, &keydesc );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_f8_starts( mbedtls_f8_context *ctx,
                       uint32_t count, uint32_t bearer, uint32_t dir )
{
    int ret = TE_SUCCESS;

    KASUMI_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_F8_MAGIC == ctx->magic) );
    ret = te_f8_start( ctx->f8, count, bearer, dir );

    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_f8_update( mbedtls_f8_context *ctx,
                       size_t size, const unsigned char *input,
                       unsigned char *output )
{
    int ret = TE_SUCCESS;

    KASUMI_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_F8_MAGIC == ctx->magic) );
    ret = te_f8_update( ctx->f8, size, input, output );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_f8_finish( mbedtls_f8_context *ctx )
{
    int ret = TE_SUCCESS;

    KASUMI_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_F8_MAGIC == ctx->magic) );
    ret = te_f8_finish( ctx->f8 );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_f8_crypt( const unsigned char key[16],
                      uint32_t count, uint32_t bearer, uint32_t dir,
                      size_t size, const unsigned char *input,
                      unsigned char *output )
{
    int ret = 0;
    mbedtls_f8_context ctx = {0};

    mbedtls_f8_init( &ctx );
    ret = mbedtls_f8_setkey( &ctx, key);
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f8_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f8_update( &ctx, size, input, output );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f8_finish( &ctx );

fin:
    mbedtls_f8_free( &ctx );
    return ret;
}

int mbedtls_f8_crypt_seckey( const mbedtls_klad_seckey_t *key,
                             uint32_t count, uint32_t bearer, uint32_t dir,
                             size_t size, const unsigned char *input,
                             unsigned char *output )
{
    int ret = 0;
    mbedtls_f8_context ctx = {0};

    mbedtls_f8_init( &ctx );
    ret = mbedtls_f8_setseckey( &ctx, key );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f8_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f8_update( &ctx, size, input, output );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f8_finish( &ctx );

fin:
    mbedtls_f8_free( &ctx );
    return ret;
}

void mbedtls_f9_init( mbedtls_f9_context *ctx )
{
    te_f9_ctx_t *f9 = NULL;
    int ret = 0;

    KASUMI_VALIDATE( ctx != NULL );
    if ( (MBEDTLS_F9_MAGIC == ctx->magic) && (ctx->f9 != NULL) ) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    f9 = (te_f9_ctx_t *)mbedtls_calloc( 1, sizeof(*f9) );
    OSAL_ASSERT( f9 != NULL );
    ret = te_f9_init( f9, te_platform_get_drvhandle(), TE_MAIN_ALGO_KASUMI );
    OSAL_ASSERT( ret == TE_SUCCESS );
    osal_memset( ctx, 0x00, sizeof(*ctx) );
    ctx->f9 = f9;
    ctx->magic = MBEDTLS_F9_MAGIC;
}

void mbedtls_f9_free( mbedtls_f9_context *ctx )
{
    KASUMI_VALIDATE( (ctx != NULL) && (MBEDTLS_F9_MAGIC == ctx->magic) &&
                     (ctx->f9 != NULL));
    (void)te_f9_free( ctx->f9 );
    mbedtls_free( ctx->f9 );
    osal_memset( ctx, 0x00, sizeof(*ctx) );
}

int mbedtls_f9_setkey( mbedtls_f9_context *ctx,
                       const unsigned char key[16] )
{
    int ret = TE_SUCCESS;

    KASUMI_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_F9_MAGIC == ctx->magic) );
    ret = te_f9_setkey( ctx->f9, key, MBEDTLS_KEYBITS_128);
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_f9_setseckey( mbedtls_f9_context *ctx,
                          const mbedtls_klad_seckey_t *key )
{
    int ret = TE_SUCCESS;
    te_sec_key_t keydesc = {0};

    KASUMI_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_F9_MAGIC == ctx->magic) &&
                         (key != NULL) && ((MBEDTLS_KL_KEY_MODEL == key->sel) ||
                                           (MBEDTLS_KL_KEY_ROOT == key->sel)) );
    _mbedtls_sec_key_to_te_sec_key( &keydesc, key );
    ret = te_f9_setseckey( ctx->f9, &keydesc );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_f9_starts( mbedtls_f9_context *ctx,
                       uint32_t count, uint32_t fresh, uint32_t dir )
{
    int ret = TE_SUCCESS;

    KASUMI_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_F9_MAGIC == ctx->magic) );
    ret = te_f9_start( ctx->f9, count, fresh, dir );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_f9_update( mbedtls_f9_context *ctx,
                       size_t size, const unsigned char *input )
{
    int ret = TE_SUCCESS;

    KASUMI_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_F9_MAGIC == ctx->magic) );
    ret = te_f9_update( ctx->f9, size, input );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_f9_finish( mbedtls_f9_context *ctx, unsigned char output[4] )
{
    int ret = TE_SUCCESS;

    KASUMI_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_F9_MAGIC == ctx->magic) );
    ret = te_f9_finish( ctx->f9, output );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_f9_mac( const unsigned char key[16],
                    uint32_t count, uint32_t fresh, uint32_t dir,
                    size_t size, const unsigned char *input,
                    unsigned char output[4] )
{
    int ret = 0;
    mbedtls_f9_context ctx = {0};

    mbedtls_f9_init( &ctx );
    ret = mbedtls_f9_setkey( &ctx, key);
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f9_starts( &ctx, count, fresh, dir );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f9_update( &ctx, size, input );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f9_finish( &ctx, output );

fin:
    mbedtls_f9_free( &ctx );
    return ret;
}

int mbedtls_f9_mac_seckey( const mbedtls_klad_seckey_t *key,
                           uint32_t count, uint32_t fresh, uint32_t dir,
                           size_t size, const unsigned char *input,
                           unsigned char output[4] )
{
    int ret = TE_SUCCESS;
    mbedtls_f9_context ctx = {0};

    mbedtls_f9_init( &ctx );
    ret = mbedtls_f9_setseckey( &ctx, key );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f9_starts( &ctx, count, fresh, dir );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f9_update( &ctx, size, input );
    if ( ret != 0 ) {
        goto fin;
    }
    ret = mbedtls_f9_finish( &ctx, output );

fin:
    mbedtls_f9_free( &ctx );
    return ret;
}

#if defined(MBEDTLS_SELF_TEST)

static int mbedtls_f8_self_test( int verbose )
{
    int ret = 0;
    uint32_t count = 0xFA556B26;
    uint32_t bearer = 0x03;
    uint32_t dir = 1;
    uint8_t *out = NULL;
    uint8_t input[] = {
        0xC3, 0x4C, 0x05, 0x2C, 0xC0, 0xDA, 0x8D, 0x73,
        0x45, 0x1A, 0xFE, 0x5F, 0x03, 0xBE, 0x29, 0x7F
        };
    uint8_t key[] = {
        0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
        0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
        };
    uint8_t expect_output[] = {
        0x1A, 0xEE, 0xC5, 0x64, 0x0F, 0x8B, 0x62, 0xAA,
        0x65, 0xE5, 0xEF, 0x3F, 0x90, 0x5A, 0x71, 0x0E
        };
    mbedtls_f8_context ctx = {0};
    uint8_t *in_ptr= NULL;

    mbedtls_f8_init( &ctx );
    ret = mbedtls_f8_setkey( &ctx, key );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_f8_setkey failed(-%X)\n", -ret );
        }
        goto err;
    }

    ret = mbedtls_f8_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_f8_starts failed(-%X)\n", -ret );
        }
        goto err;
    }

    in_ptr = (uint8_t *)mbedtls_calloc( 1, sizeof(input) );
    if ( NULL == in_ptr ) {
        ret = MBEDTLS_ERR_KASUMI_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_calloc failed, OOM!\n" );
        }
        goto err;
    }
    osal_memcpy( in_ptr, input, sizeof(input) );
    out = (uint8_t *)mbedtls_calloc( 1, sizeof(input) );
    if ( NULL == out ) {
        ret = MBEDTLS_ERR_KASUMI_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_calloc failed, OOM!\n" );
        }
        goto err_calloc_out;
    }
    ret = mbedtls_f8_update( &ctx, sizeof(input), in_ptr, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_f8_starts failed(-%X)\n", -ret );
        }
        goto err_update;
    }

    ret = mbedtls_f8_finish( &ctx );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_f8_finish failed(-%X)\n", -ret );
        }
        goto fin;
    }

    ret = osal_memcmp( out, expect_output, sizeof(expect_output) );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "output mismatched!!!\n" );
            OSAL_LOG_INFO_DUMP_DATA( "expected:", expect_output, sizeof(expect_output) );
            OSAL_LOG_INFO_DUMP_DATA( "actual:", out, sizeof(input) );
        }
        goto err_update;
    }

    ret = mbedtls_f8_crypt( key, count, bearer, dir,
                            sizeof(input), in_ptr, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_f8_crypt failed(-%X)\n", ret );
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
fin:
err_update:
    if ( out != NULL ) {
        mbedtls_free( out );
    }
err_calloc_out:
    if ( in_ptr != NULL ) {
        mbedtls_free( in_ptr );
    }
err:
    mbedtls_f8_free( &ctx );
    return ret;
}

static int mbedtls_f9_self_test(int verbose)
{
    int ret = 0;
    uint32_t count = 0x38A6F056;
    uint32_t fresh = 0xB8AEFDA9;
    uint32_t dir = 0;
    uint8_t expect_output[] = {0x46, 0xE0, 0x0D, 0x4B};
    uint8_t *out = NULL;
    uint8_t input[] = {
        0x33, 0x32, 0x34, 0x62, 0x63, 0x39, 0x38, 0x61,
        0x37, 0x34, 0x79
        };
    uint8_t key[] = {
        0x2B, 0xD6, 0x45, 0x9F, 0x82, 0xC5, 0xB3, 0x00,
        0x95, 0x2C, 0x49, 0x10, 0x48, 0x81, 0xFF, 0x48
        };
    mbedtls_f9_context ctx = {0};
    uint8_t *in_ptr= NULL;

    mbedtls_f9_init( &ctx );
    ret = mbedtls_f9_setkey( &ctx, key );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_f9_setkey failed(-%X)\n", -ret );
        }
        goto err;
    }

    ret = mbedtls_f9_starts( &ctx, count, fresh, dir );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_f9_starts failed(-%X)\n", -ret );
        }
        goto err;
    }
    in_ptr = (uint8_t *)mbedtls_calloc( 1, sizeof(input) );
    if ( NULL == in_ptr ) {
        ret = MBEDTLS_ERR_KASUMI_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_calloc failed, OOM!\n" );
        }
        goto err;
    }
    osal_memcpy( in_ptr, input, sizeof(input) );
    out = (uint8_t *)mbedtls_calloc( 1, sizeof(expect_output) );
    if ( NULL == out ) {
        ret = MBEDTLS_ERR_KASUMI_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_calloc failed, OOM!\n" );
        }
        goto err_calloc_out;
    }

    ret = mbedtls_f9_update( &ctx, sizeof(input), in_ptr );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_f9_update failed(-%X)\n", -ret );
        }
        goto fin;
    }

    ret = mbedtls_f9_finish( &ctx, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_f9_finish failed(-%X)\n", -ret );
        }
        goto fin;
    }

    ret = osal_memcmp( out, expect_output, sizeof(expect_output) );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "output mismatched!!!\n" );
            OSAL_LOG_INFO_DUMP_DATA( "expected:", expect_output, sizeof(expect_output) );
            OSAL_LOG_INFO_DUMP_DATA( "actual:", out, sizeof(out) );
        }
        goto fin;
    }

    ret = mbedtls_f9_mac( key, count, fresh, dir,
                          sizeof(input), in_ptr, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_f9_mac failed(-%X)\n", ret );
        }
        goto fin;
    }

    ret = osal_memcmp( out, expect_output, sizeof(expect_output) );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "output mismatched!!!\n" );
            OSAL_LOG_INFO_DUMP_DATA( "expected:", expect_output, sizeof(expect_output) );
            OSAL_LOG_INFO_DUMP_DATA( "actual:", out, sizeof(out) );
        }
    }

fin:
    if ( out != NULL ) {
        mbedtls_free( out );
    }
err_calloc_out:
    if ( in_ptr != NULL ) {
        mbedtls_free( in_ptr );
    }
err:
    mbedtls_f9_free( &ctx );
    return ret;
}

int mbedtls_kasumi_self_test( int verbose )
{
    int ret = 0;

    if ( verbose != 0 ) {
        mbedtls_printf("KASUMI f8 test ");
    }
    ret = mbedtls_f8_self_test( verbose );
    if ( ret != 0 ) {
        goto exit;
    }
    if ( verbose != 0 ) {
        mbedtls_printf(" passed \n");
        mbedtls_printf("KASUMI f9 test ");
    }
    ret = mbedtls_f9_self_test( verbose );
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

#endif  /* MBEDTLS_SELF_TEST */
#endif  /* MBEDTLS_KASUMI_ALT */
#endif  /* MBEDTLS_KASUMI_C */
