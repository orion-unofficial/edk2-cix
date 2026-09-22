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
/*
* SM3 is a cryptographic hash function used in the Chinese National
* Standard. It was published by the State Cryptography Administration
* (Chinese: 国家密码管理局) on 2010-12-17[1][2] as
* "GM/T 0004-2012: SM3 cryptographic hash algorithm".
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_SM3_C)

#include "mbedtls/sm3.h"
#include "mbedtls/platform_util.h"
#include "te_hash.h"
#include "mbedtls/platform.h"

#define SM3_VALIDATE_RET(cond)                             \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_SM3_BAD_INPUT_DATA )

#define SM3_VALIDATE(cond)  MBEDTLS_INTERNAL_VALIDATE( cond )

#if defined(MBEDTLS_SM3_ALT)

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_SM3_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_SM3_BAD_INPUT_DATA;
            break;
        case TE_ERROR_BAD_INPUT_LENGTH:
            errno = MBEDTLS_ERR_SM3_INVALID_INPUT_LENGTH;
            break;
        case TE_ERROR_NOT_SUPPORTED:
            errno = MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED;
            break;
        default:
            errno = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
            break;
    }

    return errno;
}

void mbedtls_sm3_init( mbedtls_sm3_context *ctx )
{
    SM3_VALIDATE(ctx != NULL);
    if((ctx->dgst != NULL) && (ctx->magic == MBEDTLS_SM3_MAGIC)) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
    ctx->dgst = (te_dgst_ctx_t *)mbedtls_calloc(1, sizeof(*(ctx->dgst)));
    if (ctx->dgst == NULL) {
        mbedtls_printf("%s %d malloc failed OOM!\n", __func__, __LINE__);
        return ;
    }
    ctx->magic = MBEDTLS_SM3_MAGIC;
    ctx->is_dgst_init = false;
}

void mbedtls_sm3_free( mbedtls_sm3_context *ctx )
{
    SM3_VALIDATE( ctx != NULL );
    SM3_VALIDATE( ctx->magic == MBEDTLS_SM3_MAGIC );
    if (ctx->is_dgst_init) {
        te_dgst_free(ctx->dgst);
    }
    if (ctx->dgst != NULL) {
        mbedtls_free(ctx->dgst);
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

void mbedtls_sm3_clone( mbedtls_sm3_context *dst,
                         const mbedtls_sm3_context *src )
{
    int ret = TE_ERROR_GENERIC;
    SM3_VALIDATE( dst != NULL );
    SM3_VALIDATE( dst->magic == MBEDTLS_SM3_MAGIC );
    SM3_VALIDATE( src != NULL );
    SM3_VALIDATE( src->magic == MBEDTLS_SM3_MAGIC );
    ret = te_dgst_clone( src->dgst, dst->dgst );
    if ( ret != TE_SUCCESS ) {
        mbedtls_printf( "#FATAL ERROR %s %d clone failed\n", __func__, __LINE__ );
        return ;
    }
    dst->is_dgst_init = src->is_dgst_init;
}

/*
 * SM3 context setup
 */
int mbedtls_sm3_starts_ret( mbedtls_sm3_context *ctx )
{
    int ret = 0;

    SM3_VALIDATE_RET( ctx != NULL );
    SM3_VALIDATE_RET( ctx->magic == MBEDTLS_SM3_MAGIC );
    if (!ctx->is_dgst_init) {
        ret = te_dgst_init( ctx->dgst,
                            te_platform_get_drvhandle(),
                            TE_ALG_SM3 );
        if (TE_SUCCESS != ret) {
            goto _out_;
        } else {
            ctx->is_dgst_init = true;
        }
    }

    ret = te_dgst_start( ctx->dgst );
    if (TE_SUCCESS != ret) {
        ctx->is_dgst_init = false;
        te_dgst_free(ctx->dgst);
    }

_out_:
    return _convert_retval_to_mbedtls(ret);
}

#if !defined(MBEDTLS_SM3_PROCESS_ALT)
int mbedtls_internal_sm3_process( mbedtls_sm3_context *ctx,
                                   const unsigned char data[64] )
{
    int ret = 0;
    SM3_VALIDATE_RET( ctx != NULL );
    SM3_VALIDATE_RET( ctx->magic == MBEDTLS_SM3_MAGIC );

    if ( ctx->is_dgst_init == false ) {
        ret = mbedtls_sm3_starts_ret( ctx );
        SM3_VALIDATE_RET( ret == 0 );
    }

    ret = te_dgst_update( ctx->dgst, data, ctx->dgst->crypt->blk_size );
    return _convert_retval_to_mbedtls( ret );
}
#endif /* MBEDTLS_SM3_PROCESS_ALT */

/*
 * SM3 process buffer
 */
int mbedtls_sm3_update_ret( mbedtls_sm3_context *ctx,
                             const unsigned char *input,
                             size_t ilen )
{
    int ret = 0;
    SM3_VALIDATE_RET( ctx != NULL );
    SM3_VALIDATE_RET( ilen == 0 || input != NULL );
    SM3_VALIDATE_RET( ctx->magic == MBEDTLS_SM3_MAGIC );
    ret = te_dgst_update(ctx->dgst, input, ilen);
    return _convert_retval_to_mbedtls(ret);
}

/*
 * SM3 final digest
 */
int mbedtls_sm3_finish_ret( mbedtls_sm3_context *ctx,
                             unsigned char output[32] )
{
    int ret = 0;
    SM3_VALIDATE_RET( ctx != NULL );
    SM3_VALIDATE_RET( ctx->magic == MBEDTLS_SM3_MAGIC );
    SM3_VALIDATE_RET( output != NULL );
    ret = te_dgst_finish(ctx->dgst, output);
    return _convert_retval_to_mbedtls(ret);
}

/*
 * output = SM3( input buffer )
 */
int mbedtls_sm3_ret( const unsigned char *input,
                      size_t ilen,
                      unsigned char output[32] )
{
    int ret;
    mbedtls_sm3_context ctx;

    SM3_VALIDATE_RET( ilen == 0 || input != NULL );
    SM3_VALIDATE_RET( (unsigned char *)output != NULL );

    mbedtls_sm3_init( &ctx );

    if( ( ret = mbedtls_sm3_starts_ret( &ctx ) ) != 0 )
        goto exit;

    if( ( ret = mbedtls_sm3_update_ret( &ctx, input, ilen ) ) != 0 )
        goto exit;

    if( ( ret = mbedtls_sm3_finish_ret( &ctx, output ) ) != 0 )
        goto exit;

exit:
    mbedtls_sm3_free( &ctx );

    return( ret );
}

#endif /* MBEDTLS_SM3_ALT */

#endif /* MBEDTLS_SM3_C */
