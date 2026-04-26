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
/*
 *  The SHA-1 standard was published by NIST in 1993.
 *
 *  http://www.itl.nist.gov/fipspubs/fip180-1.htm
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_SHA1_C)

#include "mbedtls/sha1.h"
#include "mbedtls/platform_util.h"
#include "te_hash.h"
#include "mbedtls/platform.h"

#include <string.h>

#define SHA1_VALIDATE_RET(cond)                             \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_SHA1_BAD_INPUT_DATA )

#define SHA1_VALIDATE(cond)  MBEDTLS_INTERNAL_VALIDATE( cond )

#if defined(MBEDTLS_SHA1_ALT)

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_SHA1_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_SHA1_BAD_INPUT_DATA;
            break;
        case TE_ERROR_BAD_INPUT_LENGTH:
            errno = MBEDTLS_ERR_SHA1_INVALID_INPUT_LENGTH;
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

void mbedtls_sha1_init( mbedtls_sha1_context *ctx )
{
    SHA1_VALIDATE( ctx != NULL );
    if( ctx->dgst != NULL && ctx->magic == MBEDTLS_SHA1_MAGIC) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx) );
    ctx->dgst = (te_dgst_ctx_t *)mbedtls_calloc(1, sizeof(*(ctx->dgst)));
    if (ctx->dgst == NULL) {
        mbedtls_printf("#FATAL ERROR %s %d malloc failed OOM!\n", __func__, __LINE__);
        return ;
    }
    ctx->magic = MBEDTLS_SHA1_MAGIC;
    ctx->is_dgst_init = false;
}

void mbedtls_sha1_free( mbedtls_sha1_context *ctx )
{
    SHA1_VALIDATE( ctx != NULL );
    SHA1_VALIDATE( ctx->magic == MBEDTLS_SHA1_MAGIC );
    if (ctx->is_dgst_init) {
        te_dgst_free(ctx->dgst);
    }
    if (ctx->dgst != NULL) {
        mbedtls_free(ctx->dgst);
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

void mbedtls_sha1_clone( mbedtls_sha1_context *dst,
                         const mbedtls_sha1_context *src )
{
    int ret = TE_ERROR_GENERIC;
    SHA1_VALIDATE( dst != NULL );
    SHA1_VALIDATE( dst->magic == MBEDTLS_SHA1_MAGIC );
    SHA1_VALIDATE( src != NULL );
    SHA1_VALIDATE( src->magic == MBEDTLS_SHA1_MAGIC );
    ret = te_dgst_clone( src->dgst, dst->dgst );
    if ( ret != TE_SUCCESS ) {
        mbedtls_printf( "#FATAL ERROR %s %d clone failed\n", __func__, __LINE__ );
        return ;
    }
    dst->is_dgst_init = src->is_dgst_init;
}

/*
 * SHA-1 context setup
 */
int mbedtls_sha1_starts_ret( mbedtls_sha1_context *ctx )
{
    int ret = 0;
    SHA1_VALIDATE_RET( ctx != NULL );
    SHA1_VALIDATE_RET( ctx->magic == MBEDTLS_SHA1_MAGIC );
    if (!ctx->is_dgst_init) {
        ret = te_dgst_init( ctx->dgst,
                            te_platform_get_drvhandle(),
                            TE_ALG_SHA1 );
        if (TE_SUCCESS != ret) {
            goto _out_;
        } else {
            ctx->is_dgst_init = true;
        }
    }

    ret =  te_dgst_start( ctx->dgst );
    if ( TE_SUCCESS != ret ) {
        te_dgst_free( ctx->dgst );
        ctx->is_dgst_init = false;
    }

_out_:
    return _convert_retval_to_mbedtls(ret);
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha1_starts( mbedtls_sha1_context *ctx )
{
    mbedtls_sha1_starts_ret( ctx );
}
#endif

#if defined(MBEDTLS_SHA1_PROCESS_ALT) && defined(CFG_CRYPTO_SHA1_CIX_ENG)
int mbedtls_internal_sha1_process( mbedtls_sha1_context *ctx,
                                   const unsigned char data[64] )
{
    int ret = 0;
    SHA1_VALIDATE_RET( ctx != NULL );
    SHA1_VALIDATE_RET( ctx->magic == MBEDTLS_SHA1_MAGIC );

    if ( ctx->is_dgst_init == false ) {
        ret = mbedtls_sha1_starts_ret( ctx );
        SHA1_VALIDATE_RET( ret == 0 );
    }

    ret = te_dgst_update( ctx->dgst, data, ctx->dgst->crypt->blk_size );
    return _convert_retval_to_mbedtls( ret );
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha1_process( mbedtls_sha1_context *ctx,
                           const unsigned char data[64] )
{
    mbedtls_internal_sha1_process( ctx, data );
}
#endif
#endif /* !MBEDTLS_SHA1_PROCESS_ALT */

/*
 * SHA-1 process buffer
 */
int mbedtls_sha1_update_ret( mbedtls_sha1_context *ctx,
                             const unsigned char *input,
                             size_t ilen )
{
    int ret = 0;
    SHA1_VALIDATE_RET( ctx != NULL );
    SHA1_VALIDATE_RET( ilen == 0 || input != NULL );
    SHA1_VALIDATE_RET( ctx->magic == MBEDTLS_SHA1_MAGIC );
    ret = te_dgst_update( ctx->dgst, input, ilen);
    return _convert_retval_to_mbedtls(ret);
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha1_update( mbedtls_sha1_context *ctx,
                          const unsigned char *input,
                          size_t ilen )
{
    mbedtls_sha1_update_ret( ctx, input, ilen );
}
#endif

/*
 * SHA-1 final digest
 */
int mbedtls_sha1_finish_ret( mbedtls_sha1_context *ctx,
                             unsigned char output[20] )
{
    int ret = 0;
    SHA1_VALIDATE_RET( ctx != NULL );
    SHA1_VALIDATE_RET( ctx->magic == MBEDTLS_SHA1_MAGIC );
    SHA1_VALIDATE_RET( output != NULL );
    ret = te_dgst_finish( ctx->dgst, output );
    return _convert_retval_to_mbedtls(ret);
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha1_finish( mbedtls_sha1_context *ctx,
                          unsigned char output[20] )
{
    mbedtls_sha1_finish_ret( ctx, output );
}
#endif

#endif /* MBEDTLS_SHA1_ALT */

#endif /* MBEDTLS_SHA1_C */
