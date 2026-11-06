/*
 * Copyright (c) 2022, Arm Technology (China) Co., Ltd.
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
 * http://csrc.nist.gov/publications/nistpubs/800-38D/SP-800-38D.pdf
 *
 * See also:
 * [MGV] http://csrc.nist.gov/groups/ST/toolkit/BCM/documents/proposedmodes/gcm/gcm-revised-spec.pdf
 *
 * We use the algorithm described as Shoup's method with 4-bit tables in
 * [MGV] 4.1, pp. 12-13, to enhance speed without using too much memory.
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_GCM_C)

#include "mbedtls/gcm.h"
#include "mbedtls/platform_util.h"
#include "te_gcm.h"
#include "mbedtls/platform.h"
#include <string.h>

#if defined(MBEDTLS_GCM_ALT)

/* Parameter validation macros */
#define GCM_VALIDATE_RET( cond ) \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_GCM_BAD_INPUT )
#define GCM_VALIDATE( cond ) \
    MBEDTLS_INTERNAL_VALIDATE( cond )

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_GCM_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
        case TE_ERROR_BAD_INPUT_DATA:
            errno = MBEDTLS_ERR_GCM_BAD_INPUT;
            break;
        case TE_ERROR_BAD_INPUT_LENGTH:
            errno = MBEDTLS_ERR_GCM_INVALID_INPUT_LENGTH;
            break;
        case TE_ERROR_BAD_KEY_LENGTH:
            errno = MBEDTLS_ERR_GCM_INVALID_KEY_LENGTH;
            break;
        case TE_ERROR_SECURITY:
            errno = MBEDTLS_ERR_GCM_AUTH_FAILED;
            break;
        case TE_ERROR_NOT_SUPPORTED:
            errno = MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED;
            break;
        default:
            errno = MBEDTLS_ERR_GCM_HW_ACCEL_FAILED;
            break;
    }

    return errno;
}


#ifdef MBEDTLS_CAMELLIA_C


/*
 * 32-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT32_BE
#define GET_UINT32_BE(n,b,i)                            \
{                                                       \
    (n) = ( (uint32_t) (b)[(i)    ] << 24 )             \
        | ( (uint32_t) (b)[(i) + 1] << 16 )             \
        | ( (uint32_t) (b)[(i) + 2] <<  8 )             \
        | ( (uint32_t) (b)[(i) + 3]       );            \
}
#endif

#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n,b,i)                            \
{                                                       \
    (b)[(i)    ] = (unsigned char) ( (n) >> 24 );       \
    (b)[(i) + 1] = (unsigned char) ( (n) >> 16 );       \
    (b)[(i) + 2] = (unsigned char) ( (n) >>  8 );       \
    (b)[(i) + 3] = (unsigned char) ( (n)       );       \
}
#endif

/*
 * Initialize a context
 */
void sw_compatible_mbedtls_gcm_init( mbedtls_gcm_context *ctx )
{
    GCM_VALIDATE( ctx != NULL );
    memset( ctx, 0, sizeof( mbedtls_gcm_context ) );
}

/*
 * Precompute small multiples of H, that is set
 *      HH[i] || HL[i] = H times i,
 * where i is seen as a field element as in [MGV], ie high-order bits
 * correspond to low powers of P. The result is stored in the same way, that
 * is the high-order bit of HH corresponds to P^0 and the low-order bit of HL
 * corresponds to P^127.
 */
static int gcm_gen_table( mbedtls_gcm_context *ctx )
{
    int ret, i, j;
    uint64_t hi, lo;
    uint64_t vl, vh;
    unsigned char h[16];
    size_t olen = 0;

    memset( h, 0, 16 );
    if( ( ret = mbedtls_cipher_update( &ctx->cipher_ctx, h, 16, h, &olen ) ) != 0 )
        return( ret );

    /* pack h as two 64-bits ints, big-endian */
    GET_UINT32_BE( hi, h,  0  );
    GET_UINT32_BE( lo, h,  4  );
    vh = (uint64_t) hi << 32 | lo;

    GET_UINT32_BE( hi, h,  8  );
    GET_UINT32_BE( lo, h,  12 );
    vl = (uint64_t) hi << 32 | lo;

    /* 8 = 1000 corresponds to 1 in GF(2^128) */
    ctx->HL[8] = vl;
    ctx->HH[8] = vh;

#if defined(MBEDTLS_AESNI_C) && defined(MBEDTLS_HAVE_X86_64)
    /* With CLMUL support, we need only h, not the rest of the table */
    if( mbedtls_aesni_has_support( MBEDTLS_AESNI_CLMUL ) )
        return( 0 );
#endif

    /* 0 corresponds to 0 in GF(2^128) */
    ctx->HH[0] = 0;
    ctx->HL[0] = 0;

    for( i = 4; i > 0; i >>= 1 )
    {
        uint32_t T = ( vl & 1 ) * 0xe1000000U;
        vl  = ( vh << 63 ) | ( vl >> 1 );
        vh  = ( vh >> 1 ) ^ ( (uint64_t) T << 32);

        ctx->HL[i] = vl;
        ctx->HH[i] = vh;
    }

    for( i = 2; i <= 8; i *= 2 )
    {
        uint64_t *HiL = ctx->HL + i, *HiH = ctx->HH + i;
        vh = *HiH;
        vl = *HiL;
        for( j = 1; j < i; j++ )
        {
            HiH[j] = vh ^ ctx->HH[j];
            HiL[j] = vl ^ ctx->HL[j];
        }
    }

    return( 0 );
}

int sw_compatible_mbedtls_gcm_setkey( mbedtls_gcm_context *ctx,
                        mbedtls_cipher_id_t cipher,
                        const unsigned char *key,
                        unsigned int keybits )
{
    int ret;
    const mbedtls_cipher_info_t *cipher_info;

    GCM_VALIDATE_RET( ctx != NULL );
    GCM_VALIDATE_RET( key != NULL );
    GCM_VALIDATE_RET( keybits == 128 || keybits == 192 || keybits == 256 );

    cipher_info = mbedtls_cipher_info_from_values( cipher, keybits, MBEDTLS_MODE_ECB );
    if( cipher_info == NULL )
        return( MBEDTLS_ERR_GCM_BAD_INPUT );

    if( cipher_info->block_size != 16 )
        return( MBEDTLS_ERR_GCM_BAD_INPUT );

    mbedtls_cipher_free( &ctx->cipher_ctx );

    if( ( ret = mbedtls_cipher_setup( &ctx->cipher_ctx, cipher_info ) ) != 0 )
        return( ret );

    if( ( ret = mbedtls_cipher_setkey( &ctx->cipher_ctx, key, keybits,
                               MBEDTLS_ENCRYPT ) ) != 0 )
    {
        return( ret );
    }

    if( ( ret = gcm_gen_table( ctx ) ) != 0 )
        return( ret );

    ctx->cipher_id = cipher;
    ctx->init = true;
    return( 0 );
}

int sw_compatible_mbedtls_gcm_setseckey( mbedtls_gcm_context *ctx,
                                        mbedtls_cipher_id_t cipher,
                                        mbedtls_gcm_sec_key_t *key )
{
    (void)ctx;
    (void)cipher;
    (void)key;
    return MBEDTLS_ERR_GCM_FEATURE_UNAVAILABLE;
}

int sw_compatible_mbedtls_gcm_setseckey_v2( mbedtls_gcm_context *ctx,
                                        mbedtls_cipher_id_t cipher,
                                        mbedtls_gcm_sec_key_v2_t *key )
{
    (void)ctx;
    (void)cipher;
    (void)key;
    return MBEDTLS_ERR_GCM_FEATURE_UNAVAILABLE;
}

/*
 * Shoup's method for multiplication use this table with
 *      last4[x] = x times P^128
 * where x and last4[x] are seen as elements of GF(2^128) as in [MGV]
 */
static const uint64_t last4[16] =
{
    0x0000, 0x1c20, 0x3840, 0x2460,
    0x7080, 0x6ca0, 0x48c0, 0x54e0,
    0xe100, 0xfd20, 0xd940, 0xc560,
    0x9180, 0x8da0, 0xa9c0, 0xb5e0
};

/*
 * Sets output to x times H using the precomputed tables.
 * x and output are seen as elements of GF(2^128) as in [MGV].
 */
static void gcm_mult( mbedtls_gcm_context *ctx, const unsigned char x[16],
                      unsigned char output[16] )
{
    int i = 0;
    unsigned char lo, hi, rem;
    uint64_t zh, zl;

#if defined(MBEDTLS_AESNI_C) && defined(MBEDTLS_HAVE_X86_64)
    if( mbedtls_aesni_has_support( MBEDTLS_AESNI_CLMUL ) ) {
        unsigned char h[16];

        PUT_UINT32_BE( ctx->HH[8] >> 32, h,  0 );
        PUT_UINT32_BE( ctx->HH[8],       h,  4 );
        PUT_UINT32_BE( ctx->HL[8] >> 32, h,  8 );
        PUT_UINT32_BE( ctx->HL[8],       h, 12 );

        mbedtls_aesni_gcm_mult( output, x, h );
        return;
    }
#endif /* MBEDTLS_AESNI_C && MBEDTLS_HAVE_X86_64 */

    lo = x[15] & 0xf;

    zh = ctx->HH[lo];
    zl = ctx->HL[lo];

    for( i = 15; i >= 0; i-- )
    {
        lo = x[i] & 0xf;
        hi = x[i] >> 4;

        if( i != 15 )
        {
            rem = (unsigned char) zl & 0xf;
            zl = ( zh << 60 ) | ( zl >> 4 );
            zh = ( zh >> 4 );
            zh ^= (uint64_t) last4[rem] << 48;
            zh ^= ctx->HH[lo];
            zl ^= ctx->HL[lo];

        }

        rem = (unsigned char) zl & 0xf;
        zl = ( zh << 60 ) | ( zl >> 4 );
        zh = ( zh >> 4 );
        zh ^= (uint64_t) last4[rem] << 48;
        zh ^= ctx->HH[hi];
        zl ^= ctx->HL[hi];
    }

    PUT_UINT32_BE( zh >> 32, output, 0 );
    PUT_UINT32_BE( zh, output, 4 );
    PUT_UINT32_BE( zl >> 32, output, 8 );
    PUT_UINT32_BE( zl, output, 12 );
}

int sw_compatible_mbedtls_gcm_starts( mbedtls_gcm_context *ctx,
                int mode,
                const unsigned char *iv,
                size_t iv_len,
                const unsigned char *add,
                size_t add_len )
{
    int ret;
    unsigned char work_buf[16];
    size_t i;
    const unsigned char *p;
    size_t use_len, olen = 0;

    GCM_VALIDATE_RET( ctx != NULL );
    GCM_VALIDATE_RET( iv != NULL );
    GCM_VALIDATE_RET( add_len == 0 || add != NULL );

    /* IV and AD are limited to 2^64 bits, so 2^61 bytes */
    /* IV is not allowed to be zero length */
    if( iv_len == 0 ||
      ( (uint64_t) iv_len  ) >> 61 != 0 ||
      ( (uint64_t) add_len ) >> 61 != 0 )
    {
        return( MBEDTLS_ERR_GCM_BAD_INPUT );
    }

    memset( ctx->y, 0x00, sizeof(ctx->y) );
    memset( ctx->buf, 0x00, sizeof(ctx->buf) );

    ctx->mode = mode;
    ctx->len = 0;
    ctx->add_len = 0;

    if( iv_len == 12 )
    {
        memcpy( ctx->y, iv, iv_len );
        ctx->y[15] = 1;
    }
    else
    {
        memset( work_buf, 0x00, 16 );
        PUT_UINT32_BE( iv_len * 8, work_buf, 12 );

        p = iv;
        while( iv_len > 0 )
        {
            use_len = ( iv_len < 16 ) ? iv_len : 16;

            for( i = 0; i < use_len; i++ )
                ctx->y[i] ^= p[i];

            gcm_mult( ctx, ctx->y, ctx->y );

            iv_len -= use_len;
            p += use_len;
        }

        for( i = 0; i < 16; i++ )
            ctx->y[i] ^= work_buf[i];

        gcm_mult( ctx, ctx->y, ctx->y );
    }

    if( ( ret = mbedtls_cipher_update( &ctx->cipher_ctx, ctx->y, 16, ctx->base_ectr,
                             &olen ) ) != 0 )
    {
        return( ret );
    }

    ctx->add_len = add_len;
    p = add;
    while( add_len > 0 )
    {
        use_len = ( add_len < 16 ) ? add_len : 16;

        for( i = 0; i < use_len; i++ )
            ctx->buf[i] ^= p[i];

        gcm_mult( ctx, ctx->buf, ctx->buf );

        add_len -= use_len;
        p += use_len;
    }

    return( 0 );
}

int sw_compatible_mbedtls_gcm_update_aad( mbedtls_gcm_context *ctx,
                            size_t add_len,
                            const unsigned char *add )
{
    (void)ctx;
    (void)add_len;
    (void)add;
    return MBEDTLS_ERR_GCM_FEATURE_UNAVAILABLE;
}

int sw_compatible_mbedtls_gcm_update( mbedtls_gcm_context *ctx,
                                    size_t length,
                                    const unsigned char *input,
                                    unsigned char *output )
{
    int ret;
    unsigned char ectr[16];
    size_t i;
    const unsigned char *p;
    unsigned char *out_p = output;
    size_t use_len, olen = 0;

    GCM_VALIDATE_RET( ctx != NULL );
    GCM_VALIDATE_RET( length == 0 || input != NULL );
    GCM_VALIDATE_RET( length == 0 || output != NULL );

    if( output > input && (size_t) ( output - input ) < length )
        return( MBEDTLS_ERR_GCM_BAD_INPUT );

    /* Total length is restricted to 2^39 - 256 bits, ie 2^36 - 2^5 bytes
     * Also check for possible overflow */
    if( ctx->len + length < ctx->len ||
        (uint64_t) ctx->len + length > 0xFFFFFFFE0ull )
    {
        return( MBEDTLS_ERR_GCM_BAD_INPUT );
    }

    ctx->len += length;

    p = input;
    while( length > 0 )
    {
        use_len = ( length < 16 ) ? length : 16;

        for( i = 16; i > 12; i-- )
            if( ++ctx->y[i - 1] != 0 )
                break;

        if( ( ret = mbedtls_cipher_update( &ctx->cipher_ctx, ctx->y, 16, ectr,
                                   &olen ) ) != 0 )
        {
            return( ret );
        }

        for( i = 0; i < use_len; i++ )
        {
            if( ctx->mode == MBEDTLS_GCM_DECRYPT )
                ctx->buf[i] ^= p[i];
            out_p[i] = ectr[i] ^ p[i];
            if( ctx->mode == MBEDTLS_GCM_ENCRYPT )
                ctx->buf[i] ^= out_p[i];
        }

        gcm_mult( ctx, ctx->buf, ctx->buf );

        length -= use_len;
        p += use_len;
        out_p += use_len;
    }

    return( 0 );
}

int sw_compatible_mbedtls_gcm_finish( mbedtls_gcm_context *ctx,
                unsigned char *tag,
                size_t tag_len )
{
    unsigned char work_buf[16];
    size_t i;
    uint64_t orig_len;
    uint64_t orig_add_len;

    GCM_VALIDATE_RET( ctx != NULL );
    GCM_VALIDATE_RET( tag != NULL );

    orig_len = ctx->len * 8;
    orig_add_len = ctx->add_len * 8;

    if( tag_len > 16 || tag_len < 4 )
        return( MBEDTLS_ERR_GCM_BAD_INPUT );

    memcpy( tag, ctx->base_ectr, tag_len );

    if( orig_len || orig_add_len )
    {
        memset( work_buf, 0x00, 16 );

        PUT_UINT32_BE( ( orig_add_len >> 32 ), work_buf, 0  );
        PUT_UINT32_BE( ( orig_add_len       ), work_buf, 4  );
        PUT_UINT32_BE( ( orig_len     >> 32 ), work_buf, 8  );
        PUT_UINT32_BE( ( orig_len           ), work_buf, 12 );

        for( i = 0; i < 16; i++ )
            ctx->buf[i] ^= work_buf[i];

        gcm_mult( ctx, ctx->buf, ctx->buf );

        for( i = 0; i < tag_len; i++ )
            tag[i] ^= ctx->buf[i];
    }

    return( 0 );
}

int sw_compatible_mbedtls_gcm_auth_decrypt( mbedtls_gcm_context *ctx,
                      size_t length,
                      const unsigned char *iv,
                      size_t iv_len,
                      const unsigned char *add,
                      size_t add_len,
                      const unsigned char *tag,
                      size_t tag_len,
                      const unsigned char *input,
                      unsigned char *output )
{
    int ret;
    unsigned char check_tag[16];
    size_t i;
    int diff;

    GCM_VALIDATE_RET( ctx != NULL );
    GCM_VALIDATE_RET( iv != NULL );
    GCM_VALIDATE_RET( add_len == 0 || add != NULL );
    GCM_VALIDATE_RET( tag != NULL );
    GCM_VALIDATE_RET( length == 0 || input != NULL );
    GCM_VALIDATE_RET( length == 0 || output != NULL );

    if( ( ret = mbedtls_gcm_crypt_and_tag( ctx, MBEDTLS_GCM_DECRYPT, length,
                                   iv, iv_len, add, add_len,
                                   input, output, tag_len, check_tag ) ) != 0 )
    {
        return( ret );
    }

    /* Check tag in "constant-time" */
    for( diff = 0, i = 0; i < tag_len; i++ )
        diff |= tag[i] ^ check_tag[i];

    if( diff != 0 )
    {
        mbedtls_platform_zeroize( output, length );
        return( MBEDTLS_ERR_GCM_AUTH_FAILED );
    }

    return( 0 );
}

int sw_compatible_mbedtls_gcm_clone( mbedtls_gcm_context *dst,
                       const mbedtls_gcm_context *src )
{
    (void)dst;
    (void)src;
    return MBEDTLS_ERR_GCM_FEATURE_UNAVAILABLE;
}

void sw_compatible_mbedtls_gcm_free( mbedtls_gcm_context *ctx )
{
    if( ctx == NULL )
        return;
    mbedtls_cipher_free( &ctx->cipher_ctx );
}

#endif

/*
 * Initialize a context
 */
void mbedtls_gcm_init( mbedtls_gcm_context *ctx )
{
    GCM_VALIDATE( ctx != NULL );
    if ( (MBEDTLS_GCM_MAGIC == ctx->magic)
         && (NULL != ctx->gcm)) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
    ctx->gcm = (te_gcm_ctx_t *)mbedtls_calloc(1, sizeof(*ctx->gcm));
    OSAL_ASSERT(NULL != ctx->gcm);
    ctx->magic = MBEDTLS_GCM_MAGIC;
}

int mbedtls_gcm_setkey( mbedtls_gcm_context *ctx,
                        mbedtls_cipher_id_t cipher,
                        const unsigned char *key,
                        unsigned int keybits )
{
    int ret = 0;
    int malg = 0;
    mbedtls_gcm_context tmp = {0};
    GCM_VALIDATE_RET(NULL != ctx);
    GCM_VALIDATE_RET(MBEDTLS_GCM_MAGIC == ctx->magic);

    switch (cipher) {
    case MBEDTLS_CIPHER_ID_AES:
       malg = TE_MAIN_ALGO_AES;
        break;
    case MBEDTLS_CIPHER_ID_SM4:
       malg = TE_MAIN_ALGO_SM4;
        break;
#if defined(MBEDTLS_CAMELLIA_C)
    case MBEDTLS_CIPHER_ID_CAMELLIA:
        return sw_compatible_mbedtls_gcm_setkey(ctx, cipher, key, keybits);
#endif
    default:
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }
    /** once cipher changed should free current active one
     *  and create a new one */
    if ( ctx->init && (cipher != ctx->cipher_id) ) {
        mbedtls_gcm_init(&tmp);
        ret = te_gcm_init( tmp.gcm, te_platform_get_drvhandle(), malg );
        if ( TE_SUCCESS != ret ) {
            mbedtls_gcm_free(&tmp);
            goto _out_;
        }
        tmp.init = true;
        tmp.cipher_id = cipher;
        ret = te_gcm_setkey(tmp.gcm, key, keybits);
        if ( TE_SUCCESS != ret ) {
            mbedtls_gcm_free(&tmp);
            goto _out_;
        }
        mbedtls_gcm_free(ctx);
        osal_memcpy(ctx, &tmp, sizeof(*ctx));
    } else {
        if ( !ctx->init ) {
            ret = te_gcm_init( ctx->gcm, te_platform_get_drvhandle(), malg );
            if ( TE_SUCCESS != ret ) {
                goto _out_;
            }
            ctx->init = true;
            ctx->cipher_id = cipher;
        }
        ret = te_gcm_setkey(ctx->gcm, key, keybits);
    }
_out_:
    return _convert_retval_to_mbedtls(ret);
}

static void _mbedtls_sec_key_to_te_sec_key( te_sec_key_t *sec_key,
                                            mbedtls_gcm_sec_key_t *key )
{
    sec_key->sel = (key->sel == MBEDTLS_GCM_KL_KEY_MODEL) ?
                   TE_KL_KEY_MODEL : TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    memcpy(sec_key->eks, key->eks,
           sizeof(sec_key->eks) > sizeof(key->eks)
           ? sizeof(key->eks) : sizeof(sec_key->eks));
}

int mbedtls_gcm_setseckey( mbedtls_gcm_context *ctx,
                        mbedtls_cipher_id_t cipher,
                        mbedtls_gcm_sec_key_t *key )
{
    int ret = 0;
    int malg = 0;
    te_sec_key_t sec_key = {0};
    mbedtls_gcm_context tmp = {0};
    GCM_VALIDATE_RET(NULL != ctx);
    GCM_VALIDATE_RET( (key != NULL) &&
                      ((key->sel == MBEDTLS_GCM_KL_KEY_MODEL) ||
                       (key->sel == MBEDTLS_GCM_KL_KEY_ROOT)) );
    GCM_VALIDATE_RET(MBEDTLS_GCM_MAGIC == ctx->magic);

    switch (cipher) {
    case MBEDTLS_CIPHER_ID_AES:
       malg = TE_MAIN_ALGO_AES;
        break;
    case MBEDTLS_CIPHER_ID_SM4:
       malg = TE_MAIN_ALGO_SM4;
        break;
#if defined(MBEDTLS_CAMELLIA_C)
    case MBEDTLS_CIPHER_ID_CAMELLIA:
        return sw_compatible_mbedtls_gcm_setseckey(ctx, cipher, key);
#endif
    default:
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    _mbedtls_sec_key_to_te_sec_key(&sec_key, key);
    /** once cipher changed should free current active one
     *  and create a new one */
    if ( ctx->init && (cipher != ctx->cipher_id) ) {
        mbedtls_gcm_init(&tmp);
        ret = te_gcm_init( tmp.gcm, te_platform_get_drvhandle(), malg );
        if ( TE_SUCCESS != ret ) {
            mbedtls_gcm_free(&tmp);
            goto _out_;
        }
        tmp.init = true;
        tmp.cipher_id = cipher;
        ret = te_gcm_setseckey(tmp.gcm, &sec_key);
        if ( TE_SUCCESS != ret ) {
            mbedtls_gcm_free(&tmp);
            goto _out_;
        }
        mbedtls_gcm_free(ctx);
        osal_memcpy(ctx, &tmp, sizeof(*ctx));
    } else {
        if ( !ctx->init ) {
            ret = te_gcm_init( ctx->gcm, te_platform_get_drvhandle(), malg );
            if ( TE_SUCCESS != ret ) {
                goto _out_;
            }
            ctx->init = true;
            ctx->cipher_id = cipher;
        }
        ret = te_gcm_setseckey(ctx->gcm, &sec_key);
    }

_out_:
    return _convert_retval_to_mbedtls(ret);
}

static void _mbedtls_sec_key_to_te_sec_key_v2( te_sec_key_v2_t *sec_key,
                                            mbedtls_gcm_sec_key_v2_t *key )
{
    sec_key->sel = (key->sel == MBEDTLS_GCM_KL_KEY_MODEL) ?
                   TE_KL_KEY_MODEL : TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    memcpy(sec_key->eks, key->eks,
           sizeof(sec_key->eks) > sizeof(key->eks)
           ? sizeof(key->eks) : sizeof(sec_key->eks));
}

int mbedtls_gcm_setseckey_v2( mbedtls_gcm_context *ctx,
                        mbedtls_cipher_id_t cipher,
                        mbedtls_gcm_sec_key_v2_t *key )
{
    int ret = 0;
    int malg = 0;
    te_sec_key_v2_t sec_key = {0};
    mbedtls_gcm_context tmp = {0};
    GCM_VALIDATE_RET(NULL != ctx);
    GCM_VALIDATE_RET( (key != NULL) &&
                      ((key->sel == MBEDTLS_GCM_KL_KEY_MODEL) ||
                       (key->sel == MBEDTLS_GCM_KL_KEY_ROOT)) );
    GCM_VALIDATE_RET(MBEDTLS_GCM_MAGIC == ctx->magic);

    switch (cipher) {
    case MBEDTLS_CIPHER_ID_AES:
       malg = TE_MAIN_ALGO_AES;
        break;
#if defined(MBEDTLS_CAMELLIA_C)
    case MBEDTLS_CIPHER_ID_CAMELLIA:
        return sw_compatible_mbedtls_gcm_setseckey_v2(ctx, cipher, key);
#endif
    default:
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    _mbedtls_sec_key_to_te_sec_key_v2(&sec_key, key);
    /** once cipher changed should free current active one
     *  and create a new one */
    if ( ctx->init && (cipher != ctx->cipher_id) ) {
        mbedtls_gcm_init(&tmp);
        ret = te_gcm_init( tmp.gcm, te_platform_get_drvhandle(), malg );
        if ( TE_SUCCESS != ret ) {
            mbedtls_gcm_free(&tmp);
            goto _out_;
        }
        tmp.init = true;
        tmp.cipher_id = cipher;
        ret = te_gcm_setseckey_v2(tmp.gcm, &sec_key);
        if ( TE_SUCCESS != ret ) {
            mbedtls_gcm_free(&tmp);
            goto _out_;
        }
        mbedtls_gcm_free(ctx);
        osal_memcpy(ctx, &tmp, sizeof(*ctx));
    } else {
        if ( !ctx->init ) {
            ret = te_gcm_init( ctx->gcm, te_platform_get_drvhandle(), malg );
            if ( TE_SUCCESS != ret ) {
                goto _out_;
            }
            ctx->init = true;
            ctx->cipher_id = cipher;
        }
        ret = te_gcm_setseckey_v2(ctx->gcm, &sec_key);
    }

_out_:
    return _convert_retval_to_mbedtls(ret);
}

int mbedtls_gcm_starts( mbedtls_gcm_context *ctx,
                int mode,
                const unsigned char *iv,
                size_t iv_len,
                const unsigned char *add,
                size_t add_len )
{
    int ret = TE_SUCCESS;
    GCM_VALIDATE_RET( ctx != NULL );
    GCM_VALIDATE_RET( MBEDTLS_GCM_MAGIC == ctx->magic );

#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            GCM_VALIDATE_RET( ctx->init );
            ret = te_gcm_start(ctx->gcm,
                                    mode == MBEDTLS_GCM_ENCRYPT ? \
                                    TE_DRV_SCA_ENCRYPT : TE_DRV_SCA_DECRYPT,
                                    (uint8_t *)iv,
                                    iv_len);
            if (TE_SUCCESS != ret) {
                goto _out_;
            }

            ret = te_gcm_update_aad(ctx->gcm, add, add_len);
#if defined(MBEDTLS_CAMELLIA_C)
            break;
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_gcm_starts(ctx, mode,
                                                    iv, iv_len,
                                                    add, add_len);
        default:
            return MBEDTLS_ERR_GCM_BAD_INPUT;
    }
#endif
_out_:
    return _convert_retval_to_mbedtls(ret);
}

int mbedtls_gcm_update_aad( mbedtls_gcm_context *ctx,
                            size_t add_len,
                            const unsigned char *add )
{
    GCM_VALIDATE_RET( ctx != NULL );
    GCM_VALIDATE_RET( MBEDTLS_GCM_MAGIC == ctx->magic );
#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            GCM_VALIDATE_RET( ctx->init );
            return _convert_retval_to_mbedtls(
                                    te_gcm_update_aad(ctx->gcm, add, add_len));
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_gcm_update_aad(ctx,
                                                        add_len, add);
        default:
            return MBEDTLS_ERR_GCM_BAD_INPUT;
    }
#endif
}

int mbedtls_gcm_update( mbedtls_gcm_context *ctx,
                size_t length,
                const unsigned char *input,
                unsigned char *output )
{
    GCM_VALIDATE_RET( ctx != NULL );
    GCM_VALIDATE_RET( MBEDTLS_GCM_MAGIC == ctx->magic );
#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            GCM_VALIDATE_RET( ctx->init );
            return _convert_retval_to_mbedtls(te_gcm_update(ctx->gcm,
                                                length,
                                                input,
                                                output));
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_gcm_update(ctx, length,
                                                    input, output);
        default:
            return MBEDTLS_ERR_GCM_BAD_INPUT;
    }
#endif
}

int mbedtls_gcm_finish( mbedtls_gcm_context *ctx,
                unsigned char *tag,
                size_t tag_len )
{
    int ret = TE_SUCCESS;
    GCM_VALIDATE_RET( ctx != NULL );
    GCM_VALIDATE_RET( MBEDTLS_GCM_MAGIC == ctx->magic );
#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            GCM_VALIDATE_RET( ctx->init );
            ret = te_gcm_finish(ctx->gcm, tag, tag_len);
            /* keep compliant with software mbedtls let mbedtls_gcm_auth_decrypt to verify tag */
            if (ret == (int)TE_ERROR_SECURITY) {
                ret = TE_SUCCESS;
            }
            return _convert_retval_to_mbedtls(ret);
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_gcm_finish(ctx, tag, tag_len);
        default:
            return MBEDTLS_ERR_GCM_BAD_INPUT;
    }
#endif
}

int mbedtls_gcm_crypt_and_tag( mbedtls_gcm_context *ctx,
                       int mode,
                       size_t length,
                       const unsigned char *iv,
                       size_t iv_len,
                       const unsigned char *add,
                       size_t add_len,
                       const unsigned char *input,
                       unsigned char *output,
                       size_t tag_len,
                       unsigned char *tag )
{
    int ret;

    if( ( ret = mbedtls_gcm_starts( ctx, mode, iv, iv_len, add, add_len ) ) != 0 )
        return( ret );

    if( ( ret = mbedtls_gcm_update( ctx, length, input, output ) ) != 0 )
        return( ret );

    if( ( ret = mbedtls_gcm_finish( ctx, tag, tag_len ) ) != 0 )
        return( ret );

    return( 0 );
}

int mbedtls_gcm_auth_decrypt( mbedtls_gcm_context *ctx,
                      size_t length,
                      const unsigned char *iv,
                      size_t iv_len,
                      const unsigned char *add,
                      size_t add_len,
                      const unsigned char *tag,
                      size_t tag_len,
                      const unsigned char *input,
                      unsigned char *output )
{
    int ret = TE_SUCCESS;
    unsigned char check_tag[16] = {0};
    size_t i;
    int diff;
    GCM_VALIDATE_RET( tag != NULL );
    ret =  mbedtls_gcm_crypt_and_tag( ctx, MBEDTLS_GCM_DECRYPT, length,
                                   iv, iv_len, add, add_len,
                                   input, output, tag_len,
                                   check_tag );

    if (ret != TE_SUCCESS)
    {
        return ret;
    }
    /* Check tag in "constant-time" */
    for( diff = 0, i = 0; i < tag_len; i++ )
        diff |= tag[i] ^ check_tag[i];

    if( diff != 0 )
    {
        mbedtls_platform_zeroize( output, length );
        return( MBEDTLS_ERR_GCM_AUTH_FAILED );
    }
    return ret;
}

int mbedtls_gcm_clone( mbedtls_gcm_context *dst,
                       const mbedtls_gcm_context *src )
{
    int ret = TE_SUCCESS;

    GCM_VALIDATE_RET( dst != NULL && src != NULL );
    GCM_VALIDATE_RET( MBEDTLS_GCM_MAGIC == src->magic );
    GCM_VALIDATE_RET( MBEDTLS_GCM_MAGIC == dst->magic );

#if defined(MBEDTLS_CAMELLIA_C)
    switch(src->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
        GCM_VALIDATE_RET( src->init );
        ret = te_gcm_clone( src->gcm, dst->gcm );
        if (ret == TE_SUCCESS) {
            dst->init = true;
        }

        dst->cipher_id = src->cipher_id;
        return _convert_retval_to_mbedtls(ret);
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_gcm_clone(dst, src);
        default:
            return MBEDTLS_ERR_GCM_BAD_INPUT;
    }
#endif
}

void mbedtls_gcm_free( mbedtls_gcm_context *ctx )
{
    GCM_VALIDATE(NULL != ctx);
    GCM_VALIDATE( MBEDTLS_GCM_MAGIC == ctx->magic );
    if( ctx->init ) {
        te_gcm_free(ctx->gcm);
        ctx->init = false;
    }
    mbedtls_free(ctx->gcm);
#if defined(MBEDTLS_CAMELLIA_C)
    sw_compatible_mbedtls_gcm_free(ctx);
#endif
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

#endif /* MBEDTLS_GCM_ALT */

#endif /* MBEDTLS_GCM_C */
