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
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_HMAC_C)

#include <string.h>

#include "mbedtls/md.h"
#include "mbedtls/md_internal.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"
#include "te_hmac.h"

#if defined(MBEDTLS_HMAC_ALT)
/* Parameter validation macros based on platform_util.h */
#define HMAC_VALIDATE_RET( cond )    \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_MD_BAD_INPUT_DATA )
#define HMAC_VALIDATE( cond )        \
    MBEDTLS_INTERNAL_VALIDATE( cond )

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_MD_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_MD_BAD_INPUT_DATA;
            break;
        case TE_ERROR_BAD_INPUT_LENGTH:
            errno = MBEDTLS_ERR_HMAC_INVALID_INPUT_LENGTH;
            break;
        default:
            break;
    }

    return errno;
}

#if defined(MBEDTLS_MD2_C) || \
    defined(MBEDTLS_MD4_C) || \
    defined(MBEDTLS_RIPEMD160_C)
static int sw_compatible_hmac_starts( mbedtls_hmac_context *hmac_ctx,
                                      const unsigned char *key, size_t keylen )
{
    int ret;
    unsigned char sum[MBEDTLS_MD_MAX_SIZE];
    unsigned char *ipad, *opad;
    size_t i;
    mbedtls_md_context_t *ctx = hmac_ctx->md_ctx;

    if( keylen > (size_t) ctx->md_info->block_size )
    {
        if( ( ret = ctx->md_info->starts_func( ctx->md_ctx ) ) != 0 )
            goto cleanup;
        if( ( ret = ctx->md_info->update_func( ctx->md_ctx, key, keylen ) ) != 0 )
            goto cleanup;
        if( ( ret = ctx->md_info->finish_func( ctx->md_ctx, sum ) ) != 0 )
            goto cleanup;

        keylen = ctx->md_info->size;
        key = sum;
    }

    ipad = (unsigned char *) hmac_ctx->sw_hmac;
    opad = (unsigned char *) hmac_ctx->sw_hmac + ctx->md_info->block_size;

    memset( ipad, 0x36, ctx->md_info->block_size );
    memset( opad, 0x5C, ctx->md_info->block_size );

    for( i = 0; i < keylen; i++ )
    {
        ipad[i] = (unsigned char)( ipad[i] ^ key[i] );
        opad[i] = (unsigned char)( opad[i] ^ key[i] );
    }

    if( ( ret = ctx->md_info->starts_func( ctx->md_ctx ) ) != 0 )
        goto cleanup;
    if( ( ret = ctx->md_info->update_func( ctx->md_ctx, ipad,
                                           ctx->md_info->block_size ) ) != 0 )
        goto cleanup;

cleanup:
    mbedtls_platform_zeroize( sum, sizeof( sum ) );

    return( ret );
}

static int sw_compatible_hmac_starts_with_seckey( mbedtls_hmac_context *ctx,
                                                  mbedtls_hmac_sec_key_t *key )
{
    (void)ctx;
    (void)key;
    return MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE;
}

static int sw_compatible_hmac_starts_with_seckey_v2( mbedtls_hmac_context *ctx,
                                                mbedtls_hmac_sec_key_v2_t *key )
{
    (void)ctx;
    (void)key;
    return MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE;
}

static int sw_compatible_hmac_update( mbedtls_hmac_context *hmac_ctx, const unsigned char *input, size_t ilen )
{
    mbedtls_md_context_t *ctx = hmac_ctx->md_ctx;
    return( ctx->md_info->update_func( ctx->md_ctx, input, ilen ) );
}

static int sw_compatible_hmac_finish( mbedtls_hmac_context *hmac_ctx, unsigned char *output )
{
    int ret;
    unsigned char tmp[MBEDTLS_MD_MAX_SIZE];
    unsigned char *opad;
    mbedtls_md_context_t *ctx = hmac_ctx->md_ctx;

    opad = (unsigned char *) hmac_ctx->sw_hmac + ctx->md_info->block_size;

    if( ( ret = ctx->md_info->finish_func( ctx->md_ctx, tmp ) ) != 0 )
        return( ret );
    if( ( ret = ctx->md_info->starts_func( ctx->md_ctx ) ) != 0 )
        return( ret );
    if( ( ret = ctx->md_info->update_func( ctx->md_ctx, opad,
                                           ctx->md_info->block_size ) ) != 0 )
        return( ret );
    if( ( ret = ctx->md_info->update_func( ctx->md_ctx, tmp,
                                           ctx->md_info->size ) ) != 0 )
        return( ret );
    return( ctx->md_info->finish_func( ctx->md_ctx, output ) );
}

static int sw_compatible_hmac_reset( mbedtls_hmac_context *hmac_ctx )
{
    int ret;
    unsigned char *ipad;
    mbedtls_md_context_t *ctx = hmac_ctx->md_ctx;

    ipad = (unsigned char *) hmac_ctx->sw_hmac;

    if( ( ret = ctx->md_info->starts_func( ctx->md_ctx ) ) != 0 )
        return( ret );
    return( ctx->md_info->update_func( ctx->md_ctx, ipad,
                                       ctx->md_info->block_size ) );
}

static int sw_compatible_hmac_init( mbedtls_hmac_context *hmac_ctx )
{
    mbedtls_md_context_t *ctx = hmac_ctx->md_ctx;
    hmac_ctx->sw_hmac = mbedtls_calloc( 2, ctx->md_info->block_size );
    if( hmac_ctx->sw_hmac == NULL )
    {
        ctx->md_info->ctx_free_func( ctx->md_ctx );
        ctx->md_ctx = NULL;
        return( MBEDTLS_ERR_MD_ALLOC_FAILED );
    }

    return ( 0 );
}

static void sw_compatible_hmac_free( mbedtls_hmac_context *hmac_ctx )
{
    mbedtls_md_context_t *ctx = hmac_ctx->md_ctx;
    mbedtls_platform_zeroize( hmac_ctx->sw_hmac, 2 * ctx->md_info->block_size );
    mbedtls_free( hmac_ctx->sw_hmac );
    mbedtls_platform_zeroize( hmac_ctx, sizeof(mbedtls_hmac_context) );
    return;
}

static int sw_compatible_hmac_clone( mbedtls_hmac_context *dhmac,
                                     const mbedtls_hmac_context *shmac )
{
    mbedtls_md_context_t *src = shmac->md_ctx;
    if (NULL == dhmac->sw_hmac) {
        dhmac->sw_hmac = mbedtls_calloc( 2, src->md_info->block_size );
        if( dhmac->sw_hmac == NULL )
            return( MBEDTLS_ERR_MD_ALLOC_FAILED );
    }
    memcpy(dhmac->sw_hmac, shmac->sw_hmac, 2 * src->md_info->block_size);

    return ( 0 );
}
#endif /* MBEDTLS_MD2_C || MBEDTLS_MD4_C || MBEDTLS_RIPEMD160_C */

int mbedtls_md_hmac_init( mbedtls_md_context_t *ctx )
{
#define BYTE_BITS       (8U)
    int ret = 0;
    int alg = 0;
    mbedtls_hmac_context *hmac_ctx = NULL;
    te_drv_handle h = NULL;

    hmac_ctx = (mbedtls_hmac_context *)ctx->hmac_ctx;

    if( NULL != hmac_ctx && hmac_ctx->magic == MBEDTLS_HMAC_MAGIC ) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    hmac_ctx = (mbedtls_hmac_context *)mbedtls_calloc(1,
                                    sizeof(*hmac_ctx) + sizeof(te_hmac_ctx_t));
    if ( NULL == hmac_ctx ) {
        return MBEDTLS_ERR_MD_ALLOC_FAILED;
    }

    hmac_ctx->magic = MBEDTLS_HMAC_MAGIC;
    hmac_ctx->md_ctx = ctx;

#if defined(MBEDTLS_MD2_C) || \
    defined(MBEDTLS_MD4_C) || \
    defined(MBEDTLS_RIPEMD160_C)
    if( ( MBEDTLS_MD_MD2 == ctx->md_info->type ||
          MBEDTLS_MD_MD4 == ctx->md_info->type ||
          MBEDTLS_MD_RIPEMD160 == ctx->md_info->type ) ) {
        ctx->hmac_ctx = hmac_ctx;
        ret = sw_compatible_hmac_init( hmac_ctx );
        if( ret ) {
            ctx->hmac_ctx = NULL;
            mbedtls_platform_zeroize( hmac_ctx, sizeof(*hmac_ctx) );
            mbedtls_free(hmac_ctx);
        }
        return ret;
    }
#endif
    switch ( ctx->md_info->type ) {
        case MBEDTLS_MD_MD5:
            alg = TE_ALG_HMAC_MD5;
            break;
        case MBEDTLS_MD_SHA1:
            alg = TE_ALG_HMAC_SHA1;
            break;
        case MBEDTLS_MD_SHA224:
            alg = TE_ALG_HMAC_SHA224;
            break;
        case MBEDTLS_MD_SHA256:
            alg = TE_ALG_HMAC_SHA256;
            break;
        case MBEDTLS_MD_SHA384:
            alg = TE_ALG_HMAC_SHA384;
            break;
        case MBEDTLS_MD_SHA512:
            alg = TE_ALG_HMAC_SHA512;
            break;
        case MBEDTLS_MD_SM3:
            alg = TE_ALG_HMAC_SM3;
            break;
        default:
            mbedtls_platform_zeroize( hmac_ctx, sizeof(*hmac_ctx) );
            mbedtls_free(hmac_ctx);
            return ( MBEDTLS_ERR_MD_BAD_INPUT_DATA );
    }

    h =  te_platform_get_drvhandle();
    if ( NULL == h ) {
        OSAL_LOG_ERR("%s +%d te_platform_get_drvhandle Failed!\n",
                        __FILE__, __LINE__);
        mbedtls_platform_zeroize( hmac_ctx, sizeof(*hmac_ctx) );
        mbedtls_free(hmac_ctx);
        return MBEDTLS_ERR_HMAC_HW_ACCEL_FAILED;
    }
    hmac_ctx->hmac = (te_hmac_ctx_t *)(hmac_ctx + 1);
    ret = te_hmac_init(hmac_ctx->hmac, h, alg);
    if( TE_SUCCESS != ret ){
        mbedtls_platform_zeroize( hmac_ctx, sizeof(*hmac_ctx) );
        mbedtls_free(hmac_ctx);
        goto _out_;
    }

    ctx->hmac_ctx = hmac_ctx;

_out_:
    return _convert_retval_to_mbedtls(ret);
}

int mbedtls_md_hmac_starts( mbedtls_md_context_t *ctx,
                            const unsigned char *key,
                            size_t keylen )
{
    int ret = 0;
    mbedtls_hmac_context *hmac_ctx = NULL;

    if ( ( ret = mbedtls_md_hmac_init( &ctx ) ) != 0 )
        return ret;

    if( NULL == ctx || NULL == ctx->md_info || NULL == ctx->hmac_ctx ||
        NULL == key )
        return( MBEDTLS_ERR_MD_BAD_INPUT_DATA );

    hmac_ctx = (mbedtls_hmac_context *)ctx->hmac_ctx;
    if( hmac_ctx->magic != MBEDTLS_HMAC_MAGIC )
        return( MBEDTLS_ERR_MD_BAD_INPUT_DATA );

#if defined(MBEDTLS_MD2_C) || \
    defined(MBEDTLS_MD4_C) || \
    defined(MBEDTLS_RIPEMD160_C)
    if( ( MBEDTLS_MD_MD2 == ctx->md_info->type ||
          MBEDTLS_MD_MD4 == ctx->md_info->type ||
          MBEDTLS_MD_RIPEMD160 == ctx->md_info->type ) ) {
        return sw_compatible_hmac_starts( hmac_ctx, key, keylen );
    }
#endif

    ret = te_hmac_start((te_hmac_ctx_t *)hmac_ctx->hmac,
                         key, keylen * BYTE_BITS);

    return _convert_retval_to_mbedtls(ret);
}

static void _mbedtls_sec_key_to_te_sec_key( te_sec_key_t *sec_key,
                                        const mbedtls_hmac_sec_key_t *key )
{
    sec_key->sel = (key->sel == MBEDTLS_HMAC_KL_KEY_MODEL) ?
                   TE_KL_KEY_MODEL : TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    osal_memcpy( sec_key->eks, key->eks,
                 sizeof(sec_key->eks) > sizeof(key->eks) ?
                    sizeof(key->eks) : sizeof(sec_key->eks) );
}

int mbedtls_md_hmac_starts_with_seckey( mbedtls_md_context_t *ctx,
                                        mbedtls_hmac_sec_key_t *key )
{
    int ret = 0;
    mbedtls_hmac_context *hmac_ctx = NULL;
    te_sec_key_t skey = {0};

    if( (NULL == ctx) || (NULL == ctx->md_info) || (NULL == ctx->hmac_ctx) ||
        (NULL == key) || ((key->sel != MBEDTLS_HMAC_KL_KEY_MODEL) &&
        (key->sel != MBEDTLS_HMAC_KL_KEY_ROOT)) )
        return( MBEDTLS_ERR_MD_BAD_INPUT_DATA );

    hmac_ctx = (mbedtls_hmac_context *)ctx->hmac_ctx;
    if( hmac_ctx->magic != MBEDTLS_HMAC_MAGIC )
        return( MBEDTLS_ERR_MD_BAD_INPUT_DATA );

#if defined(MBEDTLS_MD2_C) || \
    defined(MBEDTLS_MD4_C) || \
    defined(MBEDTLS_RIPEMD160_C)
    if( ( MBEDTLS_MD_MD2 == ctx->md_info->type ||
          MBEDTLS_MD_MD4 == ctx->md_info->type ||
          MBEDTLS_MD_RIPEMD160 == ctx->md_info->type ) ) {
        return sw_compatible_hmac_starts_with_seckey( hmac_ctx, key );
    }
#endif

    _mbedtls_sec_key_to_te_sec_key( &skey, key );
    ret = te_hmac_start2( (te_hmac_ctx_t *)hmac_ctx->hmac, &skey );

    return _convert_retval_to_mbedtls(ret);
}

static void _mbedtls_sec_key_to_te_sec_key_v2( te_sec_key_v2_t *sec_key,
                                        const mbedtls_hmac_sec_key_v2_t *key )
{
    sec_key->sel = (key->sel == MBEDTLS_HMAC_KL_KEY_MODEL) ?
                   TE_KL_KEY_MODEL : TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    osal_memcpy( sec_key->eks, key->eks,
                 sizeof(sec_key->eks) > sizeof(key->eks) ?
                    sizeof(key->eks) : sizeof(sec_key->eks) );
}

int mbedtls_md_hmac_starts_with_seckey_v2( mbedtls_md_context_t *ctx,
                                           mbedtls_hmac_sec_key_v2_t *key )
{
    int ret = 0;
    mbedtls_hmac_context *hmac_ctx = NULL;
    te_sec_key_v2_t skey = {0};

    if( (NULL == ctx) || (NULL == ctx->md_info) || (NULL == ctx->hmac_ctx) ||
        (NULL == key)  || ((key->sel != MBEDTLS_HMAC_KL_KEY_MODEL) &&
        (key->sel != MBEDTLS_HMAC_KL_KEY_ROOT)))
        return( MBEDTLS_ERR_MD_BAD_INPUT_DATA );

    hmac_ctx = (mbedtls_hmac_context *)ctx->hmac_ctx;
    if( hmac_ctx->magic != MBEDTLS_HMAC_MAGIC )
        return( MBEDTLS_ERR_MD_BAD_INPUT_DATA );

#if defined(MBEDTLS_MD2_C) || \
    defined(MBEDTLS_MD4_C) || \
    defined(MBEDTLS_RIPEMD160_C)
    if( ( MBEDTLS_MD_MD2 == ctx->md_info->type ||
          MBEDTLS_MD_MD4 == ctx->md_info->type ||
          MBEDTLS_MD_RIPEMD160 == ctx->md_info->type ) ) {
        return sw_compatible_hmac_starts_with_seckey_v2( hmac_ctx, key );
    }
#endif

    _mbedtls_sec_key_to_te_sec_key_v2( &skey, key );
    ret = te_hmac_start2_v2( (te_hmac_ctx_t *)hmac_ctx->hmac, &skey );

    return _convert_retval_to_mbedtls(ret);
}

int mbedtls_md_hmac_update( mbedtls_md_context_t *ctx,
                            const unsigned char *input,
                            size_t ilen )
{
    mbedtls_hmac_context *hmac_ctx = NULL;

    if( ctx == NULL || ctx->md_info == NULL || ctx->hmac_ctx == NULL )
        return( MBEDTLS_ERR_MD_BAD_INPUT_DATA );

    hmac_ctx = ( mbedtls_hmac_context *)ctx->hmac_ctx;
    HMAC_VALIDATE_RET(hmac_ctx->magic == MBEDTLS_HMAC_MAGIC);

#if defined(MBEDTLS_MD2_C) || \
    defined(MBEDTLS_MD4_C) || \
    defined(MBEDTLS_RIPEMD160_C)
    if( ( MBEDTLS_MD_MD2 == ctx->md_info->type ||
          MBEDTLS_MD_MD4 == ctx->md_info->type ||
          MBEDTLS_MD_RIPEMD160 == ctx->md_info->type ) ) {
        return sw_compatible_hmac_update( hmac_ctx, input, ilen );
    }
#endif

    return _convert_retval_to_mbedtls(
                    te_hmac_update((te_hmac_ctx_t *)hmac_ctx->hmac,
                    input, ilen));
}

int mbedtls_md_hmac_finish( mbedtls_md_context_t *ctx,
                            unsigned char *output )
{
    mbedtls_hmac_context *hmac_ctx = NULL;

    if(ctx == NULL || ctx->md_info == NULL || ctx->hmac_ctx == NULL)
        return( MBEDTLS_ERR_MD_BAD_INPUT_DATA );

    hmac_ctx = ( mbedtls_hmac_context *)ctx->hmac_ctx;
    HMAC_VALIDATE_RET( hmac_ctx->magic == MBEDTLS_HMAC_MAGIC );

#if defined(MBEDTLS_MD2_C) || \
    defined(MBEDTLS_MD4_C) || \
    defined(MBEDTLS_RIPEMD160_C)
    if( ( MBEDTLS_MD_MD2 == ctx->md_info->type ||
          MBEDTLS_MD_MD4 == ctx->md_info->type ||
          MBEDTLS_MD_RIPEMD160 == ctx->md_info->type ) ) {
        return sw_compatible_hmac_finish( hmac_ctx, output );
    }
#endif

    return _convert_retval_to_mbedtls(
                    te_hmac_finish((te_hmac_ctx_t *)hmac_ctx->hmac,
                                    output,
                                    ctx->md_info->size));
}

int mbedtls_md_hmac_reset( mbedtls_md_context_t *ctx )
{
    mbedtls_hmac_context *hmac_ctx = NULL;

    if( ctx == NULL || ctx->md_info == NULL || ctx->hmac_ctx == NULL ) {
        return( MBEDTLS_ERR_MD_BAD_INPUT_DATA );
    }

    hmac_ctx = ( mbedtls_hmac_context *)ctx->hmac_ctx;
    HMAC_VALIDATE_RET( hmac_ctx->magic == MBEDTLS_HMAC_MAGIC );

#if defined(MBEDTLS_MD2_C) || \
    defined(MBEDTLS_MD4_C) || \
    defined(MBEDTLS_RIPEMD160_C)
    if( ( MBEDTLS_MD_MD2 == ctx->md_info->type ||
          MBEDTLS_MD_MD4 == ctx->md_info->type ||
          MBEDTLS_MD_RIPEMD160 == ctx->md_info->type ) ) {
        return sw_compatible_hmac_reset( hmac_ctx );
    }
#endif

    return _convert_retval_to_mbedtls(
                te_hmac_reset((te_hmac_ctx_t *)hmac_ctx->hmac));
}

void mbedtls_hmac_free( mbedtls_hmac_context *hmac_ctx )
{
    if( hmac_ctx == NULL || hmac_ctx->magic != MBEDTLS_HMAC_MAGIC ) {
        return;
    }

#if defined(MBEDTLS_MD2_C) || \
    defined(MBEDTLS_MD4_C) || \
    defined(MBEDTLS_RIPEMD160_C)
    if( hmac_ctx->md_ctx == NULL || hmac_ctx->md_ctx->md_info == NULL ) {
        return;
    }

    if( ( MBEDTLS_MD_MD2 == hmac_ctx->md_ctx->md_info->type ||
          MBEDTLS_MD_MD4 == hmac_ctx->md_ctx->md_info->type ||
          MBEDTLS_MD_RIPEMD160 == hmac_ctx->md_ctx->md_info->type ) ) {
        return sw_compatible_hmac_free( hmac_ctx );
    }
#endif

    OSAL_ASSERT( TE_SUCCESS == te_hmac_free( hmac_ctx->hmac ) );
    mbedtls_platform_zeroize( hmac_ctx->hmac, sizeof(*hmac_ctx->hmac) );
    mbedtls_platform_zeroize( hmac_ctx, sizeof(*hmac_ctx) );

    return;
}

int mbedtls_hmac_clone( mbedtls_hmac_context *dhmac,
                        const mbedtls_hmac_context *shmac )
{
    int ret = 0;
    if( NULL == dhmac || dhmac->magic != MBEDTLS_HMAC_MAGIC ||
        NULL == shmac || shmac->magic != MBEDTLS_HMAC_MAGIC ) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

#if defined(MBEDTLS_MD2_C) || \
    defined(MBEDTLS_MD4_C) || \
    defined(MBEDTLS_RIPEMD160_C)
    if( NULL == dhmac->md_ctx || NULL == shmac->md_ctx ||
        NULL == dhmac->md_ctx->md_info || NULL == shmac->md_ctx->md_info ||
        dhmac->md_ctx->md_info->type != shmac->md_ctx->md_info->type ) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    if( ( MBEDTLS_MD_MD2 == shmac->md_ctx->md_info->type ||
          MBEDTLS_MD_MD4 == shmac->md_ctx->md_info->type ||
          MBEDTLS_MD_RIPEMD160 == shmac->md_ctx->md_info->type ) ) {
        return sw_compatible_hmac_clone( dhmac, shmac );
    }
#endif

    ret = te_hmac_clone( shmac->hmac, dhmac->hmac );
    return _convert_retval_to_mbedtls( ret );
}
#endif /* MBEDTLS_HMAC_ALT */

#if defined(MBEDTLS_SELF_TEST)

static const unsigned char hmac_test_key[][128] =
{
    { 0x23, 0xd9, 0x0a, 0xa8, 0x65, 0xc2, 0xc8, 0x90,
      0x9b, 0x5b, 0x0e, 0x40, 0x0e, 0x61, 0x64, 0x85,
      0xd9, 0x4c, 0xff, 0x0e, 0xad, 0x15, 0x1a, 0x23,
      0x23, 0x41, 0xdd, 0x9f, 0xe6, 0x65, 0x73, 0xe9,
      0x8a, 0x42, 0x44, 0x3d, 0xa3, 0x32, 0x81, 0x38,
      0xa6, 0x58, 0x72, 0xcf, 0x56, 0x81, 0x50, 0x7e,
      0x7d, 0xf0, 0x06, 0xee, 0x61, 0xd4, 0xeb, 0xaa,
      0x1b, 0xf7, 0x50, 0x2d, 0x56, 0xd9, 0x29, 0x72,
      0x52, 0xa1, 0x37, 0x59, 0xbe, 0x7a, 0x88, 0x74,
      0x25, 0xd6, 0x58, 0x15, 0xf8, 0x32, 0x35, 0xb1,
      0x7c, 0x33, 0x84, 0x9b, 0xc1, 0xa6, 0x46, 0x95,
      0xa5, 0xe4, 0x85, 0xad, 0x57, 0x72, 0x35, 0x90,
      0x19, 0xda, 0xdf, 0xb5, 0x10, 0xd4, 0xd8, 0x0d,
      0x4b, 0x84, 0x4f, 0xb6, 0x04, 0x11, 0x1b, 0x19,
      0x44, 0x87, 0x8e, 0x7c, 0x7a, 0x60, 0x90, 0x76,
      0x59, 0x56, 0x31, 0xc6, 0x4a, 0x5b, 0x83, 0x45 },
    { 0x9e },
};

static const size_t hmac_test_keylen[] =
{
    128, 1
};

static const unsigned char hmac_test_buf[][128] =
{
    { 0xb7, 0x2a, 0x3a, 0xd7, 0x25, 0x5c, 0x58, 0x77,
      0x66, 0x3c, 0x09, 0x9d, 0x50, 0x4c, 0xde, 0x3a,
      0xb9, 0x67, 0xdd, 0x1f, 0x69, 0x98, 0x75, 0xa8,
      0x12, 0xd1, 0x38, 0xd7, 0xcf, 0xe7, 0xcf, 0x4e,
      0xda, 0x1c, 0xc2, 0x6e, 0x95, 0xe0, 0x8b, 0x6c,
      0x2b, 0x40, 0x9c, 0xd5, 0x29, 0xbf, 0xe2, 0x90,
      0x1d, 0x55, 0x36, 0x99, 0x93, 0x99, 0xf7, 0xa3,
      0xd7, 0xe2, 0xe2, 0x28, 0xde, 0x57, 0x5c, 0x7e,
      0x83, 0xd6, 0xc5, 0xa7, 0x25, 0xc9, 0x19, 0xc5,
      0xbf, 0x79, 0x1a, 0xc8, 0xd2, 0x01, 0xf2, 0xe4,
      0xee, 0x43, 0xe7, 0x2e, 0x9d, 0xae, 0x78, 0xe1,
      0x25, 0xfc, 0xd2, 0xfc, 0x29, 0xea, 0x95, 0x5b,
      0x2a, 0xcd, 0x26, 0xe5, 0xdc, 0x8a, 0x3e, 0x14,
      0xe6, 0x73, 0x1b, 0x39, 0x9e, 0x87, 0xb5, 0xb6,
      0x35, 0x67, 0x2e, 0x1b, 0xde, 0x09, 0x32, 0x36,
      0x0f, 0xb9, 0xbd, 0x86, 0xa6, 0xaa, 0x57, 0x09 },
    {},
};

static const size_t hmac_test_buflen[] =
{
    128, 0
};

static const unsigned char hmac_test_md5[][16] =
{
    { 0xbf, 0x9e, 0x11, 0x08, 0x22, 0x87, 0xd6, 0xe7,
      0x30, 0xf3, 0xae, 0xd5, 0x58, 0xd1, 0xba, 0x89 },
    { 0xe6, 0x68, 0x5d, 0xe3, 0x97, 0x3f, 0x4f, 0x65,
      0x46, 0x9d, 0x5c, 0x1e, 0x08, 0xf9, 0xf3, 0x10 },
};

static const unsigned char hmac_test_sha1[][20] =
{
    { 0xd0, 0xda, 0x7b, 0x9a, 0x53, 0x0f, 0xf9, 0x1b,
      0x83, 0xae, 0xc5, 0x46, 0x1a, 0xf8, 0x3f, 0x5b,
      0xdc, 0x7f, 0x0d, 0xd1 },
    { 0x9f, 0x0b, 0x61, 0x3b, 0x0e, 0xf2, 0x4d, 0x23,
      0x14, 0x06, 0x83, 0x3a, 0xaf, 0x70, 0x44, 0x83,
      0x4a, 0xd6, 0xb6, 0x55 },
};

static const unsigned char hmac_test_sha224[][28] =
{
    { 0x77, 0xbe, 0x88, 0x59, 0xe7, 0x4a, 0xee, 0x1e,
      0x3e, 0xe4, 0xee, 0x89, 0x8b, 0x31, 0xde, 0xa5,
      0x64, 0x03, 0x9a, 0x8f, 0x48, 0x70, 0x71, 0xf4,
      0x17, 0xd9, 0x12, 0x6f },
    { 0xab, 0xc2, 0x19, 0x8b, 0x63, 0x70, 0xc5, 0x5c,
      0x99, 0x43, 0x9d, 0x6b, 0x72, 0x38, 0x89, 0xdc,
      0x16, 0x7d, 0x08, 0xd3, 0x9a, 0x5c, 0x18, 0xdd,
      0xbf, 0x49, 0xdd, 0x44 },
};

static const unsigned char hmac_test_sha256[][32] =
{
    { 0x6b, 0xa3, 0x08, 0x71, 0xaf, 0x34, 0x66, 0x2d,
      0x75, 0x72, 0xa0, 0x59, 0x96, 0x8d, 0x2e, 0x4b,
      0xa6, 0x22, 0x9d, 0xdc, 0x7e, 0x6f, 0xe9, 0x88,
      0x92, 0x40, 0x6e, 0xe5, 0x47, 0x2d, 0xc1, 0xae },
    { 0xc5, 0xb5, 0xe7, 0x1e, 0x40, 0x9c, 0x1d, 0x76,
      0x85, 0xd6, 0x31, 0x0b, 0xb4, 0xa9, 0xd5, 0xa7,
      0x89, 0x2c, 0x18, 0xb6, 0xdd, 0xdf, 0x85, 0x41,
      0x9a, 0x7a, 0x91, 0xab, 0x14, 0x2a, 0x9d, 0xd6 },
};

static const unsigned char hmac_test_sha384[][48] =
{
    { 0x22, 0x7e, 0x60, 0xc8, 0x1a, 0xa0, 0x96, 0x47,
      0x92, 0x0e, 0xf1, 0x05, 0x3b, 0x85, 0x2f, 0xdc,
      0xa8, 0x12, 0x35, 0xce, 0xc8, 0xfb, 0xa8, 0x28,
      0x3c, 0xf6, 0x86, 0xaf, 0x42, 0xa1, 0x22, 0x4a,
      0xb4, 0x3d, 0xc6, 0x26, 0x6f, 0x8b, 0x56, 0x48,
      0x71, 0x0c, 0xb5, 0x90, 0x96, 0xa7, 0xcf, 0x77 },
    { 0x1c, 0xbc, 0x68, 0x3c, 0xa2, 0xa2, 0xba, 0x6d,
      0x60, 0x59, 0x6d, 0x29, 0xbe, 0xf7, 0x51, 0xaf,
      0xc1, 0x69, 0x41, 0xaa, 0x3c, 0xc7, 0x1f, 0xce,
      0x7e, 0x72, 0xba, 0x73, 0xf1, 0xfa, 0x34, 0x60,
      0x19, 0xf0, 0x2d, 0x35, 0x23, 0x6a, 0xfb, 0xcc,
      0x4a, 0x1e, 0x11, 0x87, 0x01, 0x0c, 0x0f, 0x32 },
};

static const unsigned char hmac_test_sha512[][64] =
{
    { 0x2c, 0x6f, 0xff, 0xa3, 0xe8, 0x02, 0xcc, 0xc1,
      0xdd, 0x08, 0x51, 0x7e, 0x31, 0x55, 0x05, 0xda,
      0x00, 0x76, 0x20, 0xcd, 0xe1, 0xbc, 0x45, 0x19,
      0x37, 0x06, 0xf7, 0x14, 0xdc, 0xf5, 0x96, 0x5b,
      0x6c, 0x05, 0xcd, 0xef, 0x7d, 0xf8, 0xe1, 0x70,
      0x6e, 0xf7, 0x7b, 0x35, 0xe1, 0x5e, 0x83, 0x84,
      0x1f, 0xc8, 0x77, 0x43, 0x79, 0x19, 0xc1, 0xa8,
      0xbd, 0x70, 0x97, 0xf7, 0xde, 0x20, 0xdb, 0xf0 },
    { 0x91, 0x3a, 0x5f, 0x5c, 0x3c, 0x55, 0x98, 0xfb,
      0x19, 0xd2, 0x39, 0x01, 0xef, 0xa2, 0x9a, 0xf4,
      0xbf, 0xab, 0xa9, 0x7a, 0xb7, 0xf7, 0xb2, 0x2b,
      0xe3, 0x0d, 0xf7, 0x41, 0x24, 0xf9, 0x51, 0xb7,
      0xf4, 0xe0, 0x78, 0x8a, 0x8c, 0xc0, 0xfd, 0x1a,
      0x16, 0xc3, 0x4f, 0x81, 0x4f, 0x4f, 0xbd, 0xb4,
      0xb7, 0x74, 0xee, 0xb4, 0xb9, 0xe7, 0xb4, 0x47,
      0xd6, 0x77, 0xa3, 0x62, 0x88, 0xf7, 0xf0, 0xb6 },
};

static const unsigned char hmac_test_sm3[][32] =
{
    { 0xda, 0xd6, 0x47, 0xfa, 0x2b, 0x09, 0xe1, 0x4f,
      0xbc, 0x88, 0xb7, 0xe8, 0x5b, 0xee, 0xef, 0x40,
      0x7a, 0x15, 0xd3, 0x4c, 0x08, 0xe9, 0xdb, 0xec,
      0x88, 0x06, 0x83, 0xcc, 0x91, 0xa7, 0xeb, 0x11 },
    { 0x20, 0xf4, 0xf9, 0xbb, 0x2f, 0x8d, 0xe7, 0xf8,
      0x62, 0xc5, 0x8a, 0x23, 0x98, 0x09, 0x09, 0x2b,
      0xf4, 0x7f, 0x3a, 0x8e, 0xfc, 0x4a, 0x95, 0xfe,
      0x49, 0x85, 0xb6, 0x8d, 0x65, 0x24, 0x1e, 0x2c },
};

static int mbedtls_hmac_test( const char *md_name,
                              int index,
                              int verbose,
                              const unsigned char *key,
                              size_t klen,
                              const unsigned char *in,
                              size_t ilen,
                              const unsigned char *exp,
                              size_t elen )
{
    int ret = 0;
    mbedtls_md_context_t ctx;
    unsigned char mac[MBEDTLS_MD_MAX_SIZE];

    if( verbose != 0 )
        mbedtls_printf( "  HMAC-%-8s test #%d: ", md_name, index );

    mbedtls_md_init( &ctx );
    ret = mbedtls_md_setup( &ctx, mbedtls_md_info_from_string(md_name), 1 );
    if (ret != 0)
        goto fail;

    if ( ( ret = mbedtls_md_hmac_starts( &ctx, key, klen ) ) != 0 )
        goto fail;

    if ( ( ret = mbedtls_md_hmac_update( &ctx, in, ilen ) ) != 0 )
        goto fail;

    if ( ( ret = mbedtls_md_hmac_finish( &ctx, mac ) ) != 0 )
        goto fail;

    if ( memcmp( mac, exp, elen ) != 0 )
    {
        ret = 1;
        goto fail;
    }

    if( verbose != 0 )
        mbedtls_printf( "passed\n" );

    goto exit;

fail:
    if ( verbose )
        mbedtls_printf( "failed\n" );

exit:
    mbedtls_md_free( &ctx );

    return (ret);
}

int mbedtls_hmac_self_test( int verbose )
{
    int ret = 0;
    size_t i = 0;
    size_t ntests = sizeof(hmac_test_buf) / sizeof(hmac_test_buf[0]);

#define MBED_HMAC_TEST_MD(NAME, name) do {                       \
    for (i = 0; i < ntests; i++)                                 \
    {                                                            \
        ret = mbedtls_hmac_test( #NAME,                          \
                                 i,                              \
                                 verbose,                        \
                                 hmac_test_key[i],               \
                                 hmac_test_keylen[i],            \
                                 hmac_test_buf[i],               \
                                 hmac_test_buflen[i],            \
                                 hmac_test_##name [i],           \
                                 sizeof(hmac_test_##name [i]) ); \
        if ( ret != 0 )                                          \
            goto exit;                                           \
    }                                                            \
} while( 0 )

    MBED_HMAC_TEST_MD(MD5, md5);        /* HMAC-MD5 */
    MBED_HMAC_TEST_MD(SHA1, sha1);      /* HMAC-SHA1 */
    MBED_HMAC_TEST_MD(SHA224, sha224);  /* HMAC-SHA224 */
    MBED_HMAC_TEST_MD(SHA256, sha256);  /* HMAC-SHA256 */
    MBED_HMAC_TEST_MD(SHA384, sha384);  /* HMAC-SHA384 */
    MBED_HMAC_TEST_MD(SHA512, sha512);  /* HMAC-SHA512 */
    MBED_HMAC_TEST_MD(SM3, sm3);        /* HMAC-SM3 */

    if( verbose != 0 )
        mbedtls_printf( "\n" );

exit:
    return (ret);
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_HMAC_C */
