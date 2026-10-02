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
 *  The SHA-512 Secure Hash Standard was published by NIST in 2002.
 *
 *  http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_SHA512_C)

#include "mbedtls/sha512.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/platform.h"
#include "te_hash.h"

#if defined(_MSC_VER) || defined(__WATCOMC__)
  #define UL64(x) x##ui64
#else
  #define UL64(x) x##ULL
#endif

#include <string.h>

#define SHA512_VALIDATE_RET(cond)                           \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_SHA512_BAD_INPUT_DATA )
#define SHA512_VALIDATE(cond)  MBEDTLS_INTERNAL_VALIDATE( cond )

#if defined(MBEDTLS_SHA512_ALT)

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_SHA512_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_SHA512_BAD_INPUT_DATA;
            break;
        case TE_ERROR_BAD_INPUT_LENGTH:
            errno = MBEDTLS_ERR_SHA512_INVALID_INPUT_LENGTH;
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

void mbedtls_sha512_init( mbedtls_sha512_context *ctx )
{
    SHA512_VALIDATE( ctx != NULL );
    if( ctx->dgst != NULL && MBEDTLS_SHA512_MAGIC == ctx->magic) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx) );
    ctx->dgst = (te_dgst_ctx_t *)mbedtls_calloc(1, sizeof(*(ctx->dgst)));
    if (ctx->dgst == NULL) {
        mbedtls_printf("#FATAL ERROR %s %d malloc failed OOM!\n", __func__, __LINE__);
        return ;
    }
    ctx->magic = MBEDTLS_SHA512_MAGIC;
    ctx->is_dgst_init = false;
}

void mbedtls_sha512_free( mbedtls_sha512_context *ctx )
{
    SHA512_VALIDATE( ctx != NULL );
    SHA512_VALIDATE( ctx->magic == MBEDTLS_SHA512_MAGIC );
    if (ctx->is_dgst_init) {
        te_dgst_free(ctx->dgst);
    }
    if (ctx->dgst != NULL) {
        mbedtls_free(ctx->dgst);
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

void mbedtls_sha512_clone( mbedtls_sha512_context *dst,
                           const mbedtls_sha512_context *src )
{
    int ret = TE_ERROR_GENERIC;
    SHA512_VALIDATE( dst != NULL );
    SHA512_VALIDATE( dst->magic == MBEDTLS_SHA512_MAGIC );
    SHA512_VALIDATE( src != NULL );
    SHA512_VALIDATE( src->magic == MBEDTLS_SHA512_MAGIC );
    ret = te_dgst_clone( src->dgst, dst->dgst );
    if ( ret != TE_SUCCESS ) {
        mbedtls_printf( "#FATAL ERROR %s %d clone failed\n", __func__, __LINE__ );
        return ;
    }
    dst->is_dgst_init = src->is_dgst_init;
    dst->is_384 = src->is_384;
}

/*
 * SHA-512 context setup
 */
int mbedtls_sha512_starts_ret( mbedtls_sha512_context *ctx, int is384 )
{
    int ret = 0;
    SHA512_VALIDATE_RET( ctx != NULL );
    SHA512_VALIDATE_RET( ctx->magic == MBEDTLS_SHA512_MAGIC );
    if (!ctx->is_dgst_init || is384 != ctx->is_384) {
        /*
         * free the initialized but inconsistent ctx in advance.
         */
        if (ctx->is_dgst_init && is384 != ctx->is_384) {
            te_dgst_free( ctx->dgst );
            ctx->is_dgst_init = false;
        }

        ret = te_dgst_init( ctx->dgst,
                            te_platform_get_drvhandle(),
                            is384 ? TE_ALG_SHA384 : TE_ALG_SHA512 );
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
    ctx->is_384 = is384;

_out_:
    return _convert_retval_to_mbedtls(ret);
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha512_starts( mbedtls_sha512_context *ctx,
                            int is384 )
{
    mbedtls_sha512_starts_ret( ctx, is384 );
}
#endif

#if defined(MBEDTLS_SHA512_PROCESS_ALT)

int mbedtls_internal_sha512_process( mbedtls_sha512_context *ctx,
                                     const unsigned char data[128] )
{
    int ret = 0;
    SHA512_VALIDATE_RET( ctx != NULL );
    SHA512_VALIDATE_RET( ctx->magic == MBEDTLS_SHA512_MAGIC );

    if ( ctx->is_dgst_init == false ) {
        ret = mbedtls_sha512_starts_ret( ctx, 0 );
        SHA512_VALIDATE_RET( ret == 0 );
    }

    ret = te_dgst_update( ctx->dgst, data, ctx->dgst->crypt->blk_size );
    return _convert_retval_to_mbedtls( ret );
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha512_process( mbedtls_sha512_context *ctx,
                             const unsigned char data[128] )
{
    mbedtls_internal_sha512_process( ctx, data );
}
#endif
#endif /* MBEDTLS_SHA512_PROCESS_ALT */

/*
 * SHA-512 process buffer
 */
int mbedtls_sha512_update_ret( mbedtls_sha512_context *ctx,
                               const unsigned char *input,
                               size_t ilen )
{
    SHA512_VALIDATE_RET( ctx != NULL );
    SHA512_VALIDATE_RET( ilen == 0 || input != NULL );
    SHA512_VALIDATE_RET( ctx->magic == MBEDTLS_SHA512_MAGIC );
    return _convert_retval_to_mbedtls(te_dgst_update(ctx->dgst, input, ilen));
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha512_update( mbedtls_sha512_context *ctx,
                            const unsigned char *input,
                            size_t ilen )
{
    mbedtls_sha512_update_ret( ctx, input, ilen );
}
#endif

/*
 * SHA-512 final digest
 */
int mbedtls_sha512_finish_ret( mbedtls_sha512_context *ctx,
                               unsigned char output[64] )
{
    int ret = 0;
    SHA512_VALIDATE_RET( ctx != NULL );
    SHA512_VALIDATE_RET( ctx->magic == MBEDTLS_SHA512_MAGIC );
    SHA512_VALIDATE_RET( output != NULL );
    ret = te_dgst_finish( ctx->dgst, output );
    return _convert_retval_to_mbedtls(ret);
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sha512_finish( mbedtls_sha512_context *ctx,
                            unsigned char output[64] )
{
    mbedtls_sha512_finish_ret( ctx, output );
}
#endif

#endif /* MBEDTLS_SHA512_ALT */

#endif /* MBEDTLS_SHA512_C */
