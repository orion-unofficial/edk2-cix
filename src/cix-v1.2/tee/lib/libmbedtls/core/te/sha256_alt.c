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
 *  The SHA-256 Secure Hash Standard was published by NIST in 2002.
 *
 *  http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_SHA256_C)

#include "mbedtls/sha256.h"
#include "mbedtls/platform_util.h"
#include "te_hash.h"
#include "mbedtls/platform.h"

#include <string.h>

#define SHA256_VALIDATE_RET(cond)                           \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_SHA256_BAD_INPUT_DATA )
#define SHA256_VALIDATE(cond)  MBEDTLS_INTERNAL_VALIDATE( cond )

#if defined(MBEDTLS_SHA256_ALT)

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_SHA256_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
            break;
        case TE_ERROR_BAD_INPUT_LENGTH:
            errno = MBEDTLS_ERR_SHA256_INVALID_INPUT_LENGTH;
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

void mbedtls_sha256_init( mbedtls_sha256_context *ctx )
{
    SHA256_VALIDATE( ctx != NULL );
    if( ctx->dgst != NULL && MBEDTLS_SHA256_MAGIC == ctx->magic) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx) );
    ctx->dgst = (te_dgst_ctx_t *)mbedtls_calloc(1, sizeof(*(ctx->dgst)));
    if (ctx->dgst == NULL) {
        mbedtls_printf("#FATAL ERROR %s %d malloc failed OOM!\n", __func__, __LINE__);
        return ;
    }
    ctx->magic = MBEDTLS_SHA256_MAGIC;
    ctx->is_dgst_init = false;
}

void mbedtls_sha256_free( mbedtls_sha256_context *ctx )
{
    SHA256_VALIDATE( ctx != NULL );
    SHA256_VALIDATE( ctx->magic == MBEDTLS_SHA256_MAGIC );
    if (ctx->is_dgst_init) {
        te_dgst_free(ctx->dgst);
    }
    if (ctx->dgst != NULL) {
        mbedtls_free(ctx->dgst);
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

void mbedtls_sha256_clone( mbedtls_sha256_context *dst,
                           const mbedtls_sha256_context *src )
{
    int ret = TE_ERROR_GENERIC;
    SHA256_VALIDATE( dst != NULL );
    SHA256_VALIDATE( dst->magic == MBEDTLS_SHA256_MAGIC );
    SHA256_VALIDATE( src != NULL );
    SHA256_VALIDATE( src->magic == MBEDTLS_SHA256_MAGIC );
    ret = te_dgst_clone( src->dgst, dst->dgst );
    if ( ret != TE_SUCCESS ) {
        mbedtls_printf( "#FATAL ERROR %s %d clone failed\n", __func__, __LINE__ );
        return ;
    }
    dst->is_dgst_init = src->is_dgst_init;
    dst->is_224 = src->is_224;
}

/*
 * SHA-256 context setup
 */
int mbedtls_sha256_starts_ret( mbedtls_sha256_context *ctx, int is224 )
{
    int ret = 0;

    SHA256_VALIDATE_RET( ctx != NULL );
    SHA256_VALIDATE_RET( ctx->magic == MBEDTLS_SHA256_MAGIC );
    if (!ctx->is_dgst_init || is224 != ctx->is_224) {
        /*
         * free the initialized but inconsistent ctx in advance.
         */
        if (ctx->is_dgst_init && is224 != ctx->is_224) {
            te_dgst_free( ctx->dgst );
            ctx->is_dgst_init = false;
        }

        ret = te_dgst_init( ctx->dgst,
                            te_platform_get_drvhandle(),
                            is224 ? TE_ALG_SHA224 : TE_ALG_SHA256 );
        if (0 != ret) {
            goto _out_;
        } else {
            ctx->is_dgst_init = true;
        }
    }

    ret = te_dgst_start( ctx->dgst );
    if ( 0 != ret ) {
        te_dgst_free( ctx->dgst );
        ctx->is_dgst_init = false;
        goto _out_;
    }
    ctx->is_224 = is224;

_out_:
    return _convert_retval_to_mbedtls(ret);
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha256_starts( mbedtls_sha256_context *ctx,
                            int is224 )
{
    mbedtls_sha256_starts_ret( ctx, is224 );
}
#endif

#if defined(MBEDTLS_SHA256_PROCESS_ALT) && defined(CFG_CRYPTO_SHA256_CIX_ENG)

int mbedtls_internal_sha256_process( mbedtls_sha256_context *ctx,
                                const unsigned char data[64] )
{
    int ret = 0;
    SHA256_VALIDATE_RET( ctx != NULL );
    SHA256_VALIDATE_RET( ctx->magic == MBEDTLS_SHA256_MAGIC );

    if ( ctx->is_dgst_init == false ) {
        ret = mbedtls_sha256_starts_ret( ctx, 0 );
        SHA256_VALIDATE_RET( ret == 0 );
    }

    ret = te_dgst_update( ctx->dgst, data, ctx->dgst->crypt->blk_size );
    return _convert_retval_to_mbedtls( ret );
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha256_process( mbedtls_sha256_context *ctx,
                             const unsigned char data[64] )
{
    mbedtls_internal_sha256_process( ctx, data );
}
#endif
#endif /* MBEDTLS_SHA256_PROCESS_ALT */

/*
 * SHA-256 process buffer
 */
int mbedtls_sha256_update_ret( mbedtls_sha256_context *ctx,
                               const unsigned char *input,
                               size_t ilen )
{
    SHA256_VALIDATE_RET( ctx != NULL );
    SHA256_VALIDATE_RET( ilen == 0 || input != NULL );
    SHA256_VALIDATE_RET( ctx->magic == MBEDTLS_SHA256_MAGIC );

    return _convert_retval_to_mbedtls(te_dgst_update(ctx->dgst, input, ilen));
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha256_update( mbedtls_sha256_context *ctx,
                            const unsigned char *input,
                            size_t ilen )
{
    mbedtls_sha256_update_ret( ctx, input, ilen );
}
#endif

/*
 * SHA-256 final digest
 */
int mbedtls_sha256_finish_ret( mbedtls_sha256_context *ctx,
                               unsigned char output[32] )
{
    int ret = 0;
    SHA256_VALIDATE_RET( ctx != NULL );
    SHA256_VALIDATE_RET( ctx->magic == MBEDTLS_SHA256_MAGIC );
    SHA256_VALIDATE_RET( output != NULL );
    ret = te_dgst_finish( ctx->dgst, output );
    return _convert_retval_to_mbedtls(ret);
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha256_finish( mbedtls_sha256_context *ctx,
                            unsigned char output[32] )
{
    mbedtls_sha256_finish_ret( ctx, output );
}
#endif

#endif /* MBEDTLS_SHA256_ALT */

#endif /* MBEDTLS_SHA256_C */
