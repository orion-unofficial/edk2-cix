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

#if defined(MBEDTLS_ZUC_C)
#if defined(CFG_MBEDTLS_TE)
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"

#endif /* CFG_MBEDTLS_TE */
#if defined(MBEDTLS_ZUC_ALT)
#include "mbedtls/zuc.h"

#include "te_eea3.h"
#include "te_eia3.h"

#define MBEDTLS_KEYBITS_128     (128U)

#if defined(CFG_MBEDTLS_TE)
/* Parameter validation macros based on platform_util.h */
#define ZUC_VALIDATE_RET( cond )    \
    MBEDTLS_INTERNAL_VALIDATE_RET( (cond), MBEDTLS_ERR_ZUC_BAD_INPUT_DATA )
#define ZUC_VALIDATE( cond )        \
    MBEDTLS_INTERNAL_VALIDATE( (cond) )
#else /* CFG_MBEDTLS_TE */
#define mbedtls_printf  OSAL_LOG_INFO
#define mbedtls_calloc  osal_calloc
#define mbedtls_free    osal_free
/* Parameter validation macros */
#define ZUC_VALIDATE_RET( cond )                      \
     do {                                             \
        if (!(cond)) {                                \
            return MBEDTLS_ERR_ZUC_BAD_INPUT_DATA;    \
        }                                             \
     } while (0)

#define ZUC_VALIDATE( cond )           \
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
            errno = MBEDTLS_ERR_ZUC_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_ZUC_BAD_INPUT_DATA;
            break;
        case TE_ERROR_BAD_KEY_LENGTH:
            errno = MBEDTLS_ERR_ZUC_INVALID_KEY_LENGTH;
            break;
        default:
            errno = MBEDTLS_ERR_ZUC_HW_ACCEL_FAILED;
            break;
    }

    return errno;
}

void mbedtls_eea3_init( mbedtls_eea3_context *ctx )
{
    int ret = TE_SUCCESS;
    te_eea3_ctx_t *eea3 = NULL;

    ZUC_VALIDATE( ctx != NULL );

    if ( (MBEDTLS_EEA3_MAGIC == ctx->magic) && (ctx->eea3 != NULL) ) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }

    eea3 = (te_eea3_ctx_t *)mbedtls_calloc( 1, sizeof(*eea3) );
    OSAL_ASSERT( eea3 != NULL );

    ret = te_eea3_init( eea3, te_platform_get_drvhandle(), TE_MAIN_ALGO_ZUC );
    OSAL_ASSERT( ret == TE_SUCCESS );

    osal_memset( ctx, 0x00, sizeof(*ctx) );
    ctx->eea3 = eea3;
    ctx->magic = MBEDTLS_EEA3_MAGIC;
}

void mbedtls_eea3_free( mbedtls_eea3_context *ctx )
{
    ZUC_VALIDATE( (ctx != NULL) && (ctx->eea3 != NULL) &&
                  (MBEDTLS_EEA3_MAGIC == ctx->magic) );

    (void)te_eea3_free(ctx->eea3);
    mbedtls_free(ctx->eea3);
    osal_memset(ctx, 0x00, sizeof(*ctx));

    return;
}

int mbedtls_eea3_setkey( mbedtls_eea3_context *ctx,
                         const unsigned char key[16] )
{
    int ret = TE_SUCCESS;

    ZUC_VALIDATE_RET( (ctx != NULL) &&
                      (MBEDTLS_EEA3_MAGIC == ctx->magic) );

    ret = te_eea3_setkey( ctx->eea3, key, MBEDTLS_KEYBITS_128);
    return _convert_retval_to_mbedtls( ret );
}

static void _mbedtls_sec_key_to_te_sec_key( te_sec_key_t *sec_key,
                                            const mbedtls_klad_seckey_t *key )
{
    sec_key->sel = (MBEDTLS_KL_KEY_MODEL == key->sel) ?
                    TE_KL_KEY_MODEL : TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;

    osal_memcpy(sec_key->eks, key->eks,
                sizeof(sec_key->eks) > sizeof(key->eks) ?
                sizeof(key->eks) : sizeof(sec_key->eks));

    return;
}

int mbedtls_eea3_setseckey( mbedtls_eea3_context *ctx,
                            const mbedtls_klad_seckey_t *key )
{
    int ret = TE_SUCCESS;
    te_sec_key_t keydesc = {0};

    ZUC_VALIDATE_RET( (ctx != NULL) && (MBEDTLS_EEA3_MAGIC == ctx->magic) &&
                      (key != NULL) && ((MBEDTLS_KL_KEY_MODEL == key->sel) ||
                                        (MBEDTLS_KL_KEY_ROOT == key->sel)) );
    _mbedtls_sec_key_to_te_sec_key( &keydesc, key );

    ret = te_eea3_setseckey( ctx->eea3, &keydesc );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_eea3_starts( mbedtls_eea3_context *ctx,
                         uint32_t count, uint32_t bearer, uint32_t dir )
{
    int ret = TE_SUCCESS;

    ZUC_VALIDATE_RET( (ctx != NULL) &&
                      (MBEDTLS_EEA3_MAGIC == ctx->magic) );

    ret = te_eea3_start( ctx->eea3, count, bearer, dir );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_eea3_update( mbedtls_eea3_context *ctx,
                         size_t size, const unsigned char *input,
                         unsigned char *output )
{
    int ret = TE_SUCCESS;

    ZUC_VALIDATE_RET( (ctx != NULL) &&
                      (MBEDTLS_EEA3_MAGIC == ctx->magic) );

    ret = te_eea3_update( ctx->eea3, size, input, output );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_eea3_finish( mbedtls_eea3_context *ctx )
{
    int ret = TE_SUCCESS;

    ZUC_VALIDATE_RET( (ctx != NULL) &&
                      (MBEDTLS_EEA3_MAGIC == ctx->magic) );

    ret = te_eea3_finish( ctx->eea3 );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_eea3_crypt( const unsigned char key[16],
                        uint32_t count, uint32_t bearer, uint32_t dir,
                        size_t size, const unsigned char *input,
                        unsigned char *output )
{
    int ret = 0;
    mbedtls_eea3_context ctx = {0};

    mbedtls_eea3_init( &ctx );

    ret = mbedtls_eea3_setkey( &ctx, key);
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eea3_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eea3_update( &ctx, size, input, output );
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eea3_finish( &ctx );

fin:
    mbedtls_eea3_free( &ctx );
    return ( ret );
}

int mbedtls_eea3_crypt_seckey( const mbedtls_klad_seckey_t *key,
                               uint32_t count, uint32_t bearer, uint32_t dir,
                               size_t size, const unsigned char *input,
                               unsigned char *output )
{
    int ret = 0;
    mbedtls_eea3_context ctx = {0};

    mbedtls_eea3_init( &ctx );

    ret = mbedtls_eea3_setseckey( &ctx, key );
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eea3_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eea3_update( &ctx, size, input, output );
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eea3_finish( &ctx );

fin:
    mbedtls_eea3_free( &ctx );
    return ( ret );
}

void mbedtls_eia3_init( mbedtls_eia3_context *ctx )
{
    int ret = TE_SUCCESS;
    te_eia3_ctx_t *eia3 = NULL;

    ZUC_VALIDATE( ctx != NULL );
    if ( (MBEDTLS_EIA3_MAGIC == ctx->magic) &&
         (ctx->eia3 != NULL) ) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }

    eia3 = (te_eia3_ctx_t *)mbedtls_calloc( 1, sizeof(*eia3) );
    OSAL_ASSERT( eia3 != NULL );

    ret = te_eia3_init( eia3, te_platform_get_drvhandle(), TE_MAIN_ALGO_ZUC );
    OSAL_ASSERT( ret == TE_SUCCESS );

    osal_memset( ctx, 0x00, sizeof(*ctx) );
    ctx->eia3 = eia3;
    ctx->magic = MBEDTLS_EIA3_MAGIC;

    return;
}

void mbedtls_eia3_free( mbedtls_eia3_context *ctx )
{
    ZUC_VALIDATE( (ctx != NULL) && (ctx->eia3 != NULL) &&
                  (MBEDTLS_EIA3_MAGIC == ctx->magic) );

    (void)te_eia3_free(ctx->eia3);
    mbedtls_free(ctx->eia3);
    osal_memset(ctx, 0x00, sizeof(*ctx));

    return;
}

int mbedtls_eia3_setkey( mbedtls_eia3_context *ctx,
                         const unsigned char key[16] )
{
    int ret = TE_SUCCESS;

    ZUC_VALIDATE_RET( (ctx != NULL) &&
                      (MBEDTLS_EIA3_MAGIC == ctx->magic) );

    ret = te_eia3_setkey( ctx->eia3, key, MBEDTLS_KEYBITS_128);
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_eia3_setseckey( mbedtls_eia3_context *ctx,
                            const mbedtls_klad_seckey_t *key )
{
    int ret = TE_SUCCESS;
    te_sec_key_t keydesc = {0};

    ZUC_VALIDATE_RET( (ctx != NULL) && (key != NULL) &&
                      (MBEDTLS_EIA3_MAGIC == ctx->magic)  &&
                        ((MBEDTLS_KL_KEY_MODEL == key->sel) ||
                         (MBEDTLS_KL_KEY_ROOT == key->sel)) );
    _mbedtls_sec_key_to_te_sec_key( &keydesc, key );

    ret = te_eia3_setseckey( ctx->eia3, &keydesc );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_eia3_starts( mbedtls_eia3_context *ctx,
                         uint32_t count, uint32_t bearer, uint32_t dir )
{
    int ret = TE_SUCCESS;

    ZUC_VALIDATE_RET( (ctx != NULL) &&
                      (MBEDTLS_EIA3_MAGIC == ctx->magic) );

    ret = te_eia3_start( ctx->eia3, count, bearer, dir );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_eia3_update( mbedtls_eia3_context *ctx,
                         size_t size, const unsigned char *input )
{
    int ret = TE_SUCCESS;

    ZUC_VALIDATE_RET( (ctx != NULL) &&
                      (MBEDTLS_EIA3_MAGIC == ctx->magic) );

    ret = te_eia3_update( ctx->eia3, size, input );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_eia3_finish( mbedtls_eia3_context *ctx, unsigned char output[4] )
{
    int ret = TE_SUCCESS;

    ZUC_VALIDATE_RET( (ctx != NULL) &&
                      (MBEDTLS_EIA3_MAGIC == ctx->magic) );

    ret = te_eia3_finish( ctx->eia3, output );
    return _convert_retval_to_mbedtls( ret );
}

int mbedtls_eia3_mac( const unsigned char key[16],
                      uint32_t count, uint32_t bearer, uint32_t dir,
                      size_t size, const unsigned char *input,
                      unsigned char output[4] )
{
    int ret = 0;
    mbedtls_eia3_context ctx = {0};

    mbedtls_eia3_init( &ctx );

    ret = mbedtls_eia3_setkey( &ctx, key);
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eia3_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eia3_update( &ctx, size, input );
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eia3_finish( &ctx, output );

fin:
    mbedtls_eia3_free( &ctx );
    return ( ret );
}

int mbedtls_eia3_mac_seckey( const mbedtls_klad_seckey_t *key,
                             uint32_t count, uint32_t bearer, uint32_t dir,
                             size_t size, const unsigned char *input,
                             unsigned char output[4] )
{
    int ret = 0;
    mbedtls_eia3_context ctx = {0};

    mbedtls_eia3_init( &ctx );

    ret = mbedtls_eia3_setseckey( &ctx, key );
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eia3_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eia3_update( &ctx, size, input );
    if ( ret != 0 ) {
        goto fin;
    }

    ret = mbedtls_eia3_finish( &ctx, output );

fin:
    mbedtls_eia3_free( &ctx );
    return ( ret );
}

#if defined(MBEDTLS_SELF_TEST)
static int mbedtls_eea3_self_test( int verbose )
{
    int ret = 0;
    uint32_t count = 0x66035492;
    uint32_t bearer = 0x0F;
    uint32_t dir = 0;
    uint8_t key[] = {
        0x17, 0x3d, 0x14, 0xba, 0x50, 0x03, 0x73, 0x1d, 0x7a, 0x60, 0x04, 0x94, 0x70, 0xf0, 0x0a, 0x29
    };
    uint8_t *in_ptr= NULL;
    uint8_t input[] = {
        0x6c, 0xf6, 0x53, 0x40, 0x73, 0x55, 0x52, 0xab, 0x0c, 0x97, 0x52, 0xfa, 0x6f, 0x90, 0x25, 0xfe,
        0x0b, 0xd6, 0x75, 0xd9, 0x00, 0x58, 0x75, 0xb2
    };
    uint8_t *out = NULL;
    uint8_t expect_output[] = {
        0xa6, 0xc8, 0x5f, 0xc6, 0x6a, 0xfb, 0x85, 0x33, 0xaa, 0xfc, 0x25, 0x18, 0xdf, 0xe7, 0x84, 0x94,
        0x0e, 0xe1, 0xe4, 0xb0, 0x30, 0x23, 0x8c, 0xc8
    };
    mbedtls_eea3_context ctx = {0};

    mbedtls_eea3_init( &ctx );

    ret = mbedtls_eea3_setkey( &ctx, key );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_eea3_setkey failed(-%X)\n", -ret );
        }
        goto fin;
    }

    ret = mbedtls_eea3_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_eea3_starts failed(-%X)\n", -ret );
        }
        goto fin;
    }

    in_ptr = (uint8_t *)mbedtls_calloc( 1, sizeof(input) );
    if ( NULL == in_ptr ) {
        ret = MBEDTLS_ERR_ZUC_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_calloc failed, OOM!\n" );
        }
        goto fin;
    }
    osal_memcpy( in_ptr, input, sizeof(input) );

    out = (uint8_t *)mbedtls_calloc( 1, sizeof(input) );
    if ( NULL == out ) {
        ret = MBEDTLS_ERR_ZUC_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf("mbedtls_calloc failed, OOM!\n");
        }
        goto fin;
    }

    ret = mbedtls_eea3_update( &ctx, sizeof(input), in_ptr, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_eea3_update failed(-%X)\n", -ret );
        }
        goto fin;
    }

    ret = mbedtls_eea3_finish( &ctx );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_eea3_finish failed(-%X)\n", -ret );
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
        goto fin;
    }

    /* test all-in-one api */
    osal_memset( out, 0x00, sizeof(expect_output) );
    ret = mbedtls_eea3_crypt( key, count, bearer, dir, sizeof(input), in_ptr, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_eea3_crypt failed(-%X)\n", ret );
        }
        goto fin;
    }

    ret = osal_memcmp( out, expect_output, sizeof(expect_output) );
    if (ret != 0) {
        if ( verbose != 0 ) {
            mbedtls_printf("output mismatched!!!\n");
            OSAL_LOG_INFO_DUMP_DATA("expected:", expect_output, sizeof(expect_output));
            OSAL_LOG_INFO_DUMP_DATA("actual:", out, sizeof(input));
        }
    }

fin:
    if ( out ) {
        mbedtls_free( out );
    }
    if ( in_ptr ) {
        mbedtls_free( in_ptr );
    }
    mbedtls_eea3_free( &ctx );
    return ret;
}

static int mbedtls_eia3_self_test( int verbose )
{
    int ret = 0;
    uint32_t count = 0xa94059da;
    uint32_t bearer = 0x0a;
    uint32_t dir = 1;
    uint8_t key[] = {
        0xc9, 0xe6, 0xce, 0xc4, 0x60, 0x7c, 0x72, 0xdb, 0x00, 0x0a, 0xef, 0xa8, 0x83, 0x85, 0xab, 0x0a
    };
    uint8_t *in_ptr= NULL;
    uint8_t input[] = {
        0x98, 0x3b, 0x41, 0xd4, 0x7d, 0x78, 0x0c, 0x9e, 0x1a, 0xd1, 0x1d, 0x7e, 0xb7, 0x03, 0x91, 0xb1,
        0xde, 0x0b, 0x35, 0xda, 0x2d, 0xc6, 0x2f, 0x83, 0xe7, 0xb7, 0x8d, 0x63, 0x06, 0xca, 0x0e, 0xa0,
        0x7e, 0x94, 0x1b, 0x7b, 0xe9, 0x13, 0x48, 0xf9, 0xfc, 0xb1, 0x70, 0xe2, 0x21, 0x7f, 0xec, 0xd9,
        0x7f, 0x9f, 0x68, 0xad, 0xb1, 0x6e, 0x5d, 0x7d, 0x21, 0xe5, 0x69, 0xd2, 0x80, 0xed, 0x77, 0x5c,
        0xeb, 0xde, 0x3f, 0x40, 0x93, 0xc5, 0x38, 0x81
    };
    uint8_t *out = NULL;
    uint8_t expect_output[] = {
        0x8e, 0x48, 0xc7, 0xd5
    };
    mbedtls_eia3_context ctx = {0};

    mbedtls_eia3_init( &ctx );

    ret = mbedtls_eia3_setkey( &ctx, key );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_eia3_setkey failed(-%X)\n", -ret );
        }
        goto fin;
    }

    ret = mbedtls_eia3_starts( &ctx, count, bearer, dir );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_eia3_starts failed(-%X)\n", -ret );
        }
        goto fin;
    }

    in_ptr = (uint8_t *)mbedtls_calloc( 1, sizeof(input) );
    if (NULL == in_ptr) {
        ret = MBEDTLS_ERR_ZUC_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_calloc failed, OOM!\n" );
        }
        goto fin;
    }
    osal_memcpy( in_ptr, input, sizeof(input) );

    out = (uint8_t *)mbedtls_calloc( 1, sizeof(expect_output) );
    if ( NULL == out ) {
        ret = MBEDTLS_ERR_ZUC_ALLOC_FAILED;
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_calloc failed, OOM!\n" );
        }
        goto fin;
    }

    ret = mbedtls_eia3_update( &ctx, sizeof(input), in_ptr );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_eia3_update failed(-%X)\n", -ret );
        }
        goto fin;
    }

    ret = mbedtls_eia3_finish( &ctx, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_eia3_finish failed(-%X)\n", -ret );
        }
        goto fin;
    }

    ret = osal_memcmp( out, expect_output, sizeof(expect_output) );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "output mismatched!!!\n" );
            OSAL_LOG_INFO_DUMP_DATA( "expected:", expect_output, sizeof(expect_output) );
            OSAL_LOG_INFO_DUMP_DATA( "actual:", out, sizeof(expect_output) );
        }
        goto fin;
    }

    /* test all-in-one api */
    osal_memset( out, 0x00, sizeof(expect_output) );
    ret = mbedtls_eia3_mac( key, count, bearer, dir, sizeof(input), in_ptr, out );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "mbedtls_eia3_mac failed(-%X)\n", ret );
        }
        goto fin;
    }

    ret = osal_memcmp( out, expect_output, sizeof(expect_output) );
    if ( ret != 0 ) {
        if ( verbose != 0 ) {
            mbedtls_printf( "output mismatched!!!\n" );
            OSAL_LOG_INFO_DUMP_DATA( "expected:", expect_output, sizeof(expect_output) );
            OSAL_LOG_INFO_DUMP_DATA( "actual:", out, sizeof(expect_output) );
        }
    }

fin:
    if ( out ) {
        mbedtls_free( out );
    }
    if ( in_ptr ) {
        mbedtls_free( in_ptr );
    }
    mbedtls_eia3_free( &ctx );
    return ret;
}

int mbedtls_zuc_self_test( int verbose )
{
    int ret = 0;

    if ( verbose != 0 ) {
        mbedtls_printf("  zuc eea3 test ");
    }
    ret = mbedtls_eea3_self_test( verbose );
    if ( ret != 0 ) {
        goto exit;
    }
    if ( verbose != 0 ) {
        mbedtls_printf(" passed \n");
        mbedtls_printf("  zuc eia3 test ");
    }
    ret = mbedtls_eia3_self_test( verbose );
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

    mbedtls_printf( "\n" );
    return ret;
}

#endif /* MBEDTLS_SELF_TEST */
#endif /* MBEDTLS_ZUC_ALT */
#endif /* MBEDTLS_ZUC_C */
