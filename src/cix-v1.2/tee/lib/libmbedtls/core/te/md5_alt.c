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
 *  The MD5 algorithm was designed by Ron Rivest in 1991.
 *
 *  http://www.ietf.org/rfc/rfc1321.txt
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_MD5_C)

#include "mbedtls/md5.h"
#include "mbedtls/platform_util.h"
#include "te_hash.h"
#include "mbedtls/platform.h"

#include <string.h>

#define MD5_VALIDATE_RET(cond)                           \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_MD5_BAD_INPUT_DATA )
#define MD5_VALIDATE(cond)  MBEDTLS_INTERNAL_VALIDATE( cond )

#if defined(MBEDTLS_MD5_ALT)

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_MD5_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_MD5_BAD_INPUT_DATA;
            break;
        case TE_ERROR_BAD_INPUT_LENGTH:
            errno = MBEDTLS_ERR_MD5_INVALID_INPUT_LENGTH;
            break;
        default:
            break;
    }

    return errno;
}

void mbedtls_md5_init( mbedtls_md5_context *ctx )
{
    MD5_VALIDATE( ctx != NULL );
    if( ctx->dgst != NULL && MBEDTLS_MD5_MAGIC == ctx->magic) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx) );
    ctx->dgst = (te_dgst_ctx_t *)mbedtls_calloc(1, sizeof(*(ctx->dgst)));
    if (ctx->dgst == NULL) {
        mbedtls_printf( "#FATAL ERROR %s %d malloc failed OOM!\n", __func__, __LINE__ );
        return ;
    }
    ctx->magic = MBEDTLS_MD5_MAGIC;
    ctx->is_dgst_init = false;
}

void mbedtls_md5_free( mbedtls_md5_context *ctx )
{
    MD5_VALIDATE(NULL != ctx);
    MD5_VALIDATE(MBEDTLS_MD5_MAGIC == ctx->magic);
    if (ctx->is_dgst_init) {
        te_dgst_free(ctx->dgst);
    }
    if (ctx->dgst != NULL) {
        mbedtls_free(ctx->dgst);
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

void mbedtls_md5_clone( mbedtls_md5_context *dst,
                        const mbedtls_md5_context *src )
{
    int ret = TE_ERROR_GENERIC;
    MD5_VALIDATE( dst != NULL );
    MD5_VALIDATE(MBEDTLS_MD5_MAGIC == dst->magic);
    MD5_VALIDATE( src != NULL );
    MD5_VALIDATE(MBEDTLS_MD5_MAGIC == src->magic);
    ret = te_dgst_clone( src->dgst, dst->dgst );
    if ( ret != TE_SUCCESS ) {
        mbedtls_printf( "#FATAL ERROR %s %d clone failed\n", __func__, __LINE__ );
        return ;
    }

    dst->is_dgst_init = src->is_dgst_init;
}

/*
 * MD5 context setup
 */
int mbedtls_md5_starts_ret( mbedtls_md5_context *ctx )
{
    int ret = 0;
    MD5_VALIDATE_RET( ctx != NULL );
    MD5_VALIDATE_RET(MBEDTLS_MD5_MAGIC == ctx->magic);
    if (!ctx->is_dgst_init) {
        ret = te_dgst_init( ctx->dgst,
                            te_platform_get_drvhandle(),
                            TE_ALG_MD5 );
        if (0 != ret) {
            return ret;
        } else {
            ctx->is_dgst_init = true;
        }
    }

    ret =  te_dgst_start( ctx->dgst );
    if ( 0 != ret ) {
        te_dgst_free( ctx->dgst );
        ctx->is_dgst_init = false;
    }
    return _convert_retval_to_mbedtls(ret);
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_md5_starts( mbedtls_md5_context *ctx )
{
    mbedtls_md5_starts_ret( ctx );
}
#endif

#if defined(MBEDTLS_MD5_PROCESS_ALT)
int mbedtls_internal_md5_process( mbedtls_md5_context *ctx,
                                  const unsigned char data[64] )
{
    int ret = 0;
    MD5_VALIDATE_RET( ctx != NULL );
    MD5_VALIDATE_RET( MBEDTLS_MD5_MAGIC == ctx->magic );

    if ( ctx->is_dgst_init == false ) {
        ret = mbedtls_md5_starts_ret( ctx );
        MD5_VALIDATE_RET( ret == 0 );
    }

    ret = te_dgst_update( ctx->dgst, data, ctx->dgst->crypt->blk_size );
    return _convert_retval_to_mbedtls( ret );
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_md5_process( mbedtls_md5_context *ctx,
                          const unsigned char data[64] )
{
    mbedtls_internal_md5_process( ctx, data );
}
#endif
#endif /* MBEDTLS_MD5_PROCESS_ALT */

/*
 * MD5 process buffer
 */
int mbedtls_md5_update_ret( mbedtls_md5_context *ctx,
                            const unsigned char *input,
                            size_t ilen )
{
    MD5_VALIDATE_RET( ctx != NULL );
    MD5_VALIDATE_RET( ilen == 0 || input != NULL );
    MD5_VALIDATE_RET(MBEDTLS_MD5_MAGIC == ctx->magic);

    return _convert_retval_to_mbedtls(te_dgst_update(ctx->dgst, input, ilen));
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_md5_update( mbedtls_md5_context *ctx,
                         const unsigned char *input,
                         size_t ilen )
{
    mbedtls_md5_update_ret( ctx, input, ilen );
}
#endif

/*
 * MD5 final digest
 */
int mbedtls_md5_finish_ret( mbedtls_md5_context *ctx,
                            unsigned char output[16] )
{
    int ret = 0;
    MD5_VALIDATE_RET( ctx != NULL );
    MD5_VALIDATE_RET(MBEDTLS_MD5_MAGIC == ctx->magic);
    MD5_VALIDATE_RET( output != NULL );
    ret = te_dgst_finish( ctx->dgst, output );
    return _convert_retval_to_mbedtls(ret);
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_md5_finish( mbedtls_md5_context *ctx,
                         unsigned char output[16] )
{
    mbedtls_md5_finish_ret( ctx, output );
}
#endif

#endif /* MBEDTLS_MD5_ALT */

#endif /* MBEDTLS_MD5_C */
