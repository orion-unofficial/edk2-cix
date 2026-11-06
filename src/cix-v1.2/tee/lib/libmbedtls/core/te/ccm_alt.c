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
 * Definition of CCM:
 * http://csrc.nist.gov/publications/nistpubs/800-38C/SP800-38C_updated-July20_2007.pdf
 * RFC 3610 "Counter with CBC-MAC (CCM)"
 *
 * Related:
 * RFC 5116 "An Interface and Algorithms for Authenticated Encryption"
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_CCM_C)

#include "mbedtls/ccm.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/platform.h"
#include "te_ccm.h"
#include <string.h>

#if defined(MBEDTLS_SELF_TEST) && defined(MBEDTLS_AES_C)
#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#define mbedtls_printf printf
#endif /* MBEDTLS_PLATFORM_C */
#endif /* MBEDTLS_SELF_TEST && MBEDTLS_AES_C */

#if defined(MBEDTLS_CCM_ALT)

#define CCM_VALIDATE_RET( cond ) \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_CCM_BAD_INPUT )
#define CCM_VALIDATE( cond ) \
    MBEDTLS_INTERNAL_VALIDATE( cond )

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_CCM_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
        case TE_ERROR_BAD_INPUT_DATA:
        case TE_ERROR_BAD_KEY_LENGTH:
        case TE_ERROR_BAD_INPUT_LENGTH:
            errno = MBEDTLS_ERR_CCM_BAD_INPUT;
            break;
        case TE_ERROR_SECURITY:
            errno = MBEDTLS_ERR_CCM_AUTH_FAILED;
            break;
        case TE_ERROR_NOT_SUPPORTED:
            errno = MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED;
            break;
        default:
            errno = MBEDTLS_ERR_CCM_HW_ACCEL_FAILED;
            break;
    }

    return errno;
}

#ifdef MBEDTLS_CAMELLIA_C

int sw_compatible_mbedtls_ccm_setkey( mbedtls_ccm_context *ctx,
                        mbedtls_cipher_id_t cipher,
                        const unsigned char *key,
                        unsigned int keybits )
{
    int ret;
    const mbedtls_cipher_info_t *cipher_info;

    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( key != NULL );

    cipher_info = mbedtls_cipher_info_from_values( cipher, keybits, MBEDTLS_MODE_ECB );
    if( cipher_info == NULL )
        return( MBEDTLS_ERR_CCM_BAD_INPUT );

    if( cipher_info->block_size != 16 )
        return( MBEDTLS_ERR_CCM_BAD_INPUT );

    mbedtls_cipher_free( &ctx->cipher_ctx );

    if( ( ret = mbedtls_cipher_setup( &ctx->cipher_ctx, cipher_info ) ) != 0 )
        return( ret );

    if( ( ret = mbedtls_cipher_setkey( &ctx->cipher_ctx, key, keybits,
                               MBEDTLS_ENCRYPT ) ) != 0 )
    {
        return( ret );
    }

    ctx->cipher_id = cipher;
    return( 0 );
}

int sw_compatible_mbedtls_ccm_setseckey( mbedtls_ccm_context *ctx,
                        mbedtls_cipher_id_t cipher,
                        mbedtls_ccm_sec_key_t *key )
{
    (void)ctx;
    (void)cipher;
    (void)key;
    return MBEDTLS_ERR_CCM_FEATURE_UNAVAILABLE;
}

int sw_compatible_mbedtls_ccm_setseckey_v2( mbedtls_ccm_context *ctx,
                        mbedtls_cipher_id_t cipher,
                        mbedtls_ccm_sec_key_v2_t *key )
{
    (void)ctx;
    (void)cipher;
    (void)key;
    return MBEDTLS_ERR_CCM_FEATURE_UNAVAILABLE;
}

void sw_compatible_mbedtls_ccm_free( mbedtls_ccm_context *ctx )
{
    if( ctx == NULL )
        return;
    mbedtls_cipher_free( &ctx->cipher_ctx );
}

int sw_compatible_mbedtls_ccm_starts( mbedtls_ccm_context *ctx,
                        int mode,
                        const unsigned char *iv,
                        size_t iv_len,
                        size_t tag_len,
                        size_t add_len,
                        size_t payload_len )
{
    (void)ctx;
    (void)mode;
    (void)iv;
    (void)iv_len;
    (void)tag_len;
    (void)add_len;
    (void)payload_len;
    return MBEDTLS_ERR_CCM_FEATURE_UNAVAILABLE;
}

int sw_compatible_mbedtls_ccm_update_aad( mbedtls_ccm_context *ctx,
                            size_t add_len,
                            const unsigned char *add)
{
    (void)ctx;
    (void)add_len;
    (void)add;
    return MBEDTLS_ERR_CCM_FEATURE_UNAVAILABLE;
}

int sw_compatible_mbedtls_ccm_update( mbedtls_ccm_context *ctx,
                        size_t length,
                        const unsigned char *input,
                        unsigned char *output )
{
    (void)ctx;
    (void)length;
    (void)input;
    (void)output;
    return MBEDTLS_ERR_CCM_FEATURE_UNAVAILABLE;
}

int sw_compatible_mbedtls_ccm_finish( mbedtls_ccm_context *ctx,
                        unsigned char *tag,
                        size_t tag_len )
{
    (void)ctx;
    (void)tag;
    (void)tag_len;
    return MBEDTLS_ERR_CCM_FEATURE_UNAVAILABLE;
}

int sw_compatible_mbedtls_ccm_clone( mbedtls_ccm_context *dst,
                       const mbedtls_ccm_context *src )
{
    (void)dst;
    (void)src;
    return MBEDTLS_ERR_CCM_FEATURE_UNAVAILABLE;
}

/*
 * Macros for common operations.
 * Results in smaller compiled code than static inline functions.
 */

/*
 * Update the CBC-MAC state in y using a block in b
 * (Always using b as the source helps the compiler optimise a bit better.)
 */
#define UPDATE_CBC_MAC                                                      \
    for( i = 0; i < 16; i++ )                                               \
        y[i] ^= b[i];                                                       \
                                                                            \
    if( ( ret = mbedtls_cipher_update( &ctx->cipher_ctx, y, 16, y, &olen ) ) != 0 ) \
        return( ret );

/*
 * Encrypt or decrypt a partial block with CTR
 * Warning: using b for temporary storage! src and dst must not be b!
 * This avoids allocating one more 16 bytes buffer while allowing src == dst.
 */
#define CTR_CRYPT( dst, src, len  )                                            \
    if( ( ret = mbedtls_cipher_update( &ctx->cipher_ctx, ctr, 16, b, &olen ) ) != 0 )  \
        return( ret );                                                         \
                                                                               \
    for( i = 0; i < len; i++ )                                                 \
        dst[i] = src[i] ^ b[i];

/*
 * Authenticated encryption or decryption
 */
static int sw_compatible_ccm_auth_crypt( mbedtls_ccm_context *ctx, int mode, size_t length,
                           const unsigned char *iv, size_t iv_len,
                           const unsigned char *add, size_t add_len,
                           const unsigned char *input, unsigned char *output,
                           unsigned char *tag, size_t tag_len )
{
    int ret;
    unsigned char i;
    unsigned char q;
    size_t len_left, olen;
    unsigned char b[16];
    unsigned char y[16];
    unsigned char ctr[16];
    const unsigned char *src;
    unsigned char *dst;

    /*
     * Check length requirements: SP800-38C A.1
     * Additional requirement: a < 2^16 - 2^8 to simplify the code.
     * 'length' checked later (when writing it to the first block)
     *
     * Also, loosen the requirements to enable support for CCM* (IEEE 802.15.4).
     */
    if( tag_len == 2 || tag_len > 16 || tag_len % 2 != 0 )
        return( MBEDTLS_ERR_CCM_BAD_INPUT );

    /* Also implies q is within bounds */
    if( iv_len < 7 || iv_len > 13 )
        return( MBEDTLS_ERR_CCM_BAD_INPUT );

    if( add_len > 0xFF00 )
        return( MBEDTLS_ERR_CCM_BAD_INPUT );

    q = 16 - 1 - (unsigned char) iv_len;

    /*
     * First block B_0:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)
     * iv_len+1 .. 15       length
     *
     * With flags as (bits):
     * 7        0
     * 6        add present?
     * 5 .. 3   (t - 2) / 2
     * 2 .. 0   q - 1
     */
    b[0] = 0;
    b[0] |= ( add_len > 0 ) << 6;
    b[0] |= ( ( tag_len - 2 ) / 2 ) << 3;
    b[0] |= q - 1;

    memcpy( b + 1, iv, iv_len );

    for( i = 0, len_left = length; i < q; i++, len_left >>= 8 )
        b[15-i] = (unsigned char)( len_left & 0xFF );

    if( len_left > 0 )
        return( MBEDTLS_ERR_CCM_BAD_INPUT );

    /* Start CBC-MAC with first block */
    memset( y, 0, 16 );
    UPDATE_CBC_MAC;

    /*
     * If there is additional data, update CBC-MAC with
     * add_len, add, 0 (padding to a block boundary)
     */
    if( add_len > 0 )
    {
        size_t use_len;
        len_left = add_len;
        src = add;

        memset( b, 0, 16 );
        b[0] = (unsigned char)( ( add_len >> 8 ) & 0xFF );
        b[1] = (unsigned char)( ( add_len      ) & 0xFF );

        use_len = len_left < 16 - 2 ? len_left : 16 - 2;
        memcpy( b + 2, src, use_len );
        len_left -= use_len;
        src += use_len;

        UPDATE_CBC_MAC;

        while( len_left > 0 )
        {
            use_len = len_left > 16 ? 16 : len_left;

            memset( b, 0, 16 );
            memcpy( b, src, use_len );
            UPDATE_CBC_MAC;

            len_left -= use_len;
            src += use_len;
        }
    }

    /*
     * Prepare counter block for encryption:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)
     * iv_len+1 .. 15       counter (initially 1)
     *
     * With flags as (bits):
     * 7 .. 3   0
     * 2 .. 0   q - 1
     */
    ctr[0] = q - 1;
    memcpy( ctr + 1, iv, iv_len );
    memset( ctr + 1 + iv_len, 0, q );
    ctr[15] = 1;

    /*
     * Authenticate and {en,de}crypt the message.
     *
     * The only difference between encryption and decryption is
     * the respective order of authentication and {en,de}cryption.
     */
    len_left = length;
    src = input;
    dst = output;

    while( len_left > 0 )
    {
        size_t use_len = len_left > 16 ? 16 : len_left;

        if( mode == MBEDTLS_CCM_ENCRYPT )
        {
            memset( b, 0, 16 );
            memcpy( b, src, use_len );
            UPDATE_CBC_MAC;
        }

        CTR_CRYPT( dst, src, use_len );

        if( mode == MBEDTLS_CCM_DECRYPT )
        {
            memset( b, 0, 16 );
            memcpy( b, dst, use_len );
            UPDATE_CBC_MAC;
        }

        dst += use_len;
        src += use_len;
        len_left -= use_len;

        /*
         * Increment counter.
         * No need to check for overflow thanks to the length check above.
         */
        for( i = 0; i < q; i++ )
            if( ++ctr[15-i] != 0 )
                break;
    }

    /*
     * Authentication: reset counter and crypt/mask internal tag
     */
    for( i = 0; i < q; i++ )
        ctr[15-i] = 0;

    CTR_CRYPT( y, y, 16 );
    memcpy( tag, y, tag_len );

    return( 0 );
}

/*
 * Authenticated encryption
 */
int sw_compatible_mbedtls_ccm_star_encrypt_and_tag( mbedtls_ccm_context *ctx, size_t length,
                         const unsigned char *iv, size_t iv_len,
                         const unsigned char *add, size_t add_len,
                         const unsigned char *input, unsigned char *output,
                         unsigned char *tag, size_t tag_len )
{
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( iv != NULL );
    CCM_VALIDATE_RET( add_len == 0 || add != NULL );
    CCM_VALIDATE_RET( length == 0 || input != NULL );
    CCM_VALIDATE_RET( length == 0 || output != NULL );
    CCM_VALIDATE_RET( tag_len == 0 || tag != NULL );
    return( sw_compatible_ccm_auth_crypt( ctx, MBEDTLS_CCM_ENCRYPT, length, iv, iv_len,
                            add, add_len, input, output, tag, tag_len ) );
}

int sw_compatible_mbedtls_ccm_encrypt_and_tag( mbedtls_ccm_context *ctx, size_t length,
                         const unsigned char *iv, size_t iv_len,
                         const unsigned char *add, size_t add_len,
                         const unsigned char *input, unsigned char *output,
                         unsigned char *tag, size_t tag_len )
{
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( iv != NULL );
    CCM_VALIDATE_RET( add_len == 0 || add != NULL );
    CCM_VALIDATE_RET( length == 0 || input != NULL );
    CCM_VALIDATE_RET( length == 0 || output != NULL );
    CCM_VALIDATE_RET( tag_len == 0 || tag != NULL );
    if( tag_len == 0 )
        return( MBEDTLS_ERR_CCM_BAD_INPUT );

    return( sw_compatible_mbedtls_ccm_star_encrypt_and_tag( ctx, length, iv, iv_len, add,
                add_len, input, output, tag, tag_len ) );
}

/*
 * Authenticated decryption
 */
int sw_compatible_mbedtls_ccm_star_auth_decrypt( mbedtls_ccm_context *ctx, size_t length,
                      const unsigned char *iv, size_t iv_len,
                      const unsigned char *add, size_t add_len,
                      const unsigned char *input, unsigned char *output,
                      const unsigned char *tag, size_t tag_len )
{
    int ret;
    unsigned char check_tag[16];
    unsigned char i;
    int diff;

    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( iv != NULL );
    CCM_VALIDATE_RET( add_len == 0 || add != NULL );
    CCM_VALIDATE_RET( length == 0 || input != NULL );
    CCM_VALIDATE_RET( length == 0 || output != NULL );
    CCM_VALIDATE_RET( tag_len == 0 || tag != NULL );

    if( ( ret = sw_compatible_ccm_auth_crypt( ctx, MBEDTLS_CCM_DECRYPT, length,
                                iv, iv_len, add, add_len,
                                input, output, check_tag, tag_len ) ) != 0 )
    {
        return( ret );
    }

    /* Check tag in "constant-time" */
    for( diff = 0, i = 0; i < tag_len; i++ )
        diff |= tag[i] ^ check_tag[i];

    if( diff != 0 )
    {
        mbedtls_platform_zeroize( output, length );
        return( MBEDTLS_ERR_CCM_AUTH_FAILED );
    }

    return( 0 );
}

int sw_compatible_mbedtls_ccm_auth_decrypt( mbedtls_ccm_context *ctx, size_t length,
                      const unsigned char *iv, size_t iv_len,
                      const unsigned char *add, size_t add_len,
                      const unsigned char *input, unsigned char *output,
                      const unsigned char *tag, size_t tag_len )
{
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( iv != NULL );
    CCM_VALIDATE_RET( add_len == 0 || add != NULL );
    CCM_VALIDATE_RET( length == 0 || input != NULL );
    CCM_VALIDATE_RET( length == 0 || output != NULL );
    CCM_VALIDATE_RET( tag_len == 0 || tag != NULL );

    if( tag_len == 0 )
        return( MBEDTLS_ERR_CCM_BAD_INPUT );

    return( sw_compatible_mbedtls_ccm_star_auth_decrypt( ctx, length, iv, iv_len, add,
                add_len, input, output, tag, tag_len ) );
}

#endif

/*
 * Initialize context
 */
void mbedtls_ccm_init( mbedtls_ccm_context *ctx )
{
    CCM_VALIDATE( ctx != NULL );
    if (ctx->crypt != NULL && ctx->magic == MBEDTLS_CCM_MAGIC) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
    ctx->crypt = (te_ccm_ctx_t *)mbedtls_calloc(1, sizeof( *ctx->crypt ));
    OSAL_ASSERT(NULL != ctx->crypt);
    ctx->magic = MBEDTLS_CCM_MAGIC;
}

int mbedtls_ccm_setkey( mbedtls_ccm_context *ctx,
                        mbedtls_cipher_id_t cipher,
                        const unsigned char *key,
                        unsigned int keybits )
{
    int ret = 0;
    int malg = 0;
    te_drv_handle h = NULL;
    mbedtls_ccm_context tmp ={0};
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );

    switch (cipher) {
    case MBEDTLS_CIPHER_ID_AES:
        malg = TE_MAIN_ALGO_AES;
        break;
    case MBEDTLS_CIPHER_ID_SM4:
        malg = TE_MAIN_ALGO_SM4;
        break;
#if defined(MBEDTLS_CAMELLIA_C)
    case MBEDTLS_CIPHER_ID_CAMELLIA:
        return sw_compatible_mbedtls_ccm_setkey(ctx, cipher, key, keybits);
#endif
    default:
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
    h = te_platform_get_drvhandle();
    if ( NULL == h ) {
        OSAL_LOG_ERR("%s +%d te_platform_get_drvhandle Failed!\n",
                        __FILE__, __LINE__);
        return MBEDTLS_ERR_CCM_HW_ACCEL_FAILED;
    }
    /** once cipher changed should free current active one
     *  and create a new one */
    if ( ctx->init && (cipher != ctx->cipher_id) ) {
        mbedtls_ccm_init( &tmp );
        ret = te_ccm_init( tmp.crypt, h, malg );
        if ( TE_SUCCESS != ret  ) {
            mbedtls_ccm_free(&tmp);
            goto _out_;
        }
        tmp.init = true;
        tmp.cipher_id = cipher;
        ret = te_ccm_setkey( tmp.crypt, key, keybits );
        if ( TE_SUCCESS != ret ) {
            mbedtls_ccm_free(&tmp);
            goto _out_;
        }
        /** free the old one and replace with a new one */
        mbedtls_ccm_free( ctx );
        osal_memcpy( ctx, &tmp, sizeof(*ctx) );
    } else {
        if (!ctx->init) {
            ret = te_ccm_init(ctx->crypt, h, malg);
            if ( TE_SUCCESS != ret ) {
                goto _out_;
            }
            ctx->init = true;
            ctx->cipher_id = cipher;
        }
        ret = te_ccm_setkey(ctx->crypt, key, keybits);
    }

_out_:
    return _convert_retval_to_mbedtls(ret);
}

static void _mbedtls_sec_key_to_te_sec_key( te_sec_key_t *sec_key,
                                            mbedtls_ccm_sec_key_t *key )
{
    sec_key->sel = (key->sel == MBEDTLS_CCM_KL_KEY_MODEL) ?
                   TE_KL_KEY_MODEL : TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    memcpy(sec_key->eks, key->eks,
           sizeof(sec_key->eks) > sizeof(key->eks)
           ? sizeof(key->eks) : sizeof(sec_key->eks));
}

int mbedtls_ccm_setseckey( mbedtls_ccm_context *ctx,
                        mbedtls_cipher_id_t cipher,
                        mbedtls_ccm_sec_key_t *key )
{
    int ret = 0;
    int malg = 0;
    te_drv_handle h = NULL;
    mbedtls_ccm_context tmp = {0};
    te_sec_key_t sec_key = {0};
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( (key != NULL) &&
                      ((key->sel == MBEDTLS_CCM_KL_KEY_MODEL) ||
                       (key->sel == MBEDTLS_CCM_KL_KEY_ROOT)) );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );

    switch (cipher) {
    case MBEDTLS_CIPHER_ID_AES:
       malg = TE_MAIN_ALGO_AES;
        break;
    case MBEDTLS_CIPHER_ID_SM4:
       malg = TE_MAIN_ALGO_SM4;
        break;
#if defined(MBEDTLS_CAMELLIA_C)
    case MBEDTLS_CIPHER_ID_CAMELLIA:
        return sw_compatible_mbedtls_ccm_setseckey(ctx, cipher, key);
#endif
    default:
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
    h = te_platform_get_drvhandle();
    if ( NULL == h ) {
        OSAL_LOG_ERR("%s +%d te_platform_get_drvhandle Failed!\n",
                        __FILE__, __LINE__);
        return MBEDTLS_ERR_CCM_HW_ACCEL_FAILED;
    }
    _mbedtls_sec_key_to_te_sec_key(&sec_key, key);
    /** once cipher changed should free current active one
     *  and create a new one */
    if ( ctx->init && (cipher != ctx->cipher_id) ) {
        mbedtls_ccm_init( &tmp );
        ret = te_ccm_init( tmp.crypt, h, malg );
        if ( TE_SUCCESS != ret ) {
            mbedtls_ccm_free(&tmp);
            goto _out_;
        }
        tmp.init = true;
        tmp.cipher_id = cipher;
        ret = te_ccm_setseckey(tmp.crypt, &sec_key);
        if ( TE_SUCCESS != ret ) {
            mbedtls_ccm_free(&tmp);
            goto _out_;
        }
        /** free the old one and replace with the new one */
        mbedtls_ccm_free( ctx );
        osal_memcpy( ctx, &tmp, sizeof(*ctx) );
    } else {
        if ( !ctx->init ) {
            ret = te_ccm_init( ctx->crypt, h, malg );
            if (TE_SUCCESS != ret) {
                goto _out_;
            }
            ctx->init = true;
            ctx->cipher_id = cipher;
        }
        ret = te_ccm_setseckey(ctx->crypt, &sec_key);
    }

_out_:
    return _convert_retval_to_mbedtls(ret);
}

static void _mbedtls_sec_key_to_te_sec_key_v2( te_sec_key_v2_t *sec_key,
                                            mbedtls_ccm_sec_key_v2_t *key )
{
    sec_key->sel = (key->sel == MBEDTLS_CCM_KL_KEY_MODEL) ?
                   TE_KL_KEY_MODEL : TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    memcpy(sec_key->eks, key->eks,
           sizeof(sec_key->eks) > sizeof(key->eks)
           ? sizeof(key->eks) : sizeof(sec_key->eks));
}

int mbedtls_ccm_setseckey_v2( mbedtls_ccm_context *ctx,
                        mbedtls_cipher_id_t cipher,
                        mbedtls_ccm_sec_key_v2_t *key )
{
    int ret = 0;
    int malg = 0;
    te_drv_handle h = NULL;
    mbedtls_ccm_context tmp = {0};
    te_sec_key_v2_t sec_key = {0};
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( (key != NULL) &&
                      ((key->sel == MBEDTLS_CCM_KL_KEY_MODEL) ||
                       (key->sel == MBEDTLS_CCM_KL_KEY_ROOT)) );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );

    switch (cipher) {
    case MBEDTLS_CIPHER_ID_AES:
       malg = TE_MAIN_ALGO_AES;
        break;
#if defined(MBEDTLS_CAMELLIA_C)
    case MBEDTLS_CIPHER_ID_CAMELLIA:
        return sw_compatible_mbedtls_ccm_setseckey_v2(ctx, cipher, key);
#endif
    default:
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
    h = te_platform_get_drvhandle();
    if ( NULL == h ) {
        OSAL_LOG_ERR("%s +%d te_platform_get_drvhandle Failed!\n",
                        __FILE__, __LINE__);
        return MBEDTLS_ERR_CCM_HW_ACCEL_FAILED;
    }
    _mbedtls_sec_key_to_te_sec_key_v2(&sec_key, key);
    /** once cipher changed should free current active one
     *  and create a new one */
    if ( ctx->init && (cipher != ctx->cipher_id) ) {
        mbedtls_ccm_init( &tmp );
        ret = te_ccm_init( tmp.crypt, h, malg );
        if ( TE_SUCCESS != ret ) {
            mbedtls_ccm_free(&tmp);
            goto _out_;
        }
        tmp.init = true;
        tmp.cipher_id = cipher;
        ret = te_ccm_setseckey_v2(tmp.crypt, &sec_key);
        if ( TE_SUCCESS != ret ) {
            mbedtls_ccm_free(&tmp);
            goto _out_;
        }
        /** free the old one and replace with the new one */
        mbedtls_ccm_free( ctx );
        osal_memcpy( ctx, &tmp, sizeof(*ctx) );
    } else {
        if ( !ctx->init ) {
            ret = te_ccm_init( ctx->crypt, h, malg );
            if (TE_SUCCESS != ret) {
                goto _out_;
            }
            ctx->init = true;
            ctx->cipher_id = cipher;
        }
        ret = te_ccm_setseckey_v2(ctx->crypt, &sec_key);
    }

_out_:
    return _convert_retval_to_mbedtls(ret);
}

/*
 * Free context
 */
void mbedtls_ccm_free( mbedtls_ccm_context *ctx )
{
    CCM_VALIDATE( ctx != NULL );
    CCM_VALIDATE( ctx->magic == MBEDTLS_CCM_MAGIC );
    if ( ctx->init ) {
        te_ccm_free( ctx->crypt );
        ctx->init = false;
    }
    mbedtls_free( ctx->crypt );
#if defined(MBEDTLS_CAMELLIA_C)
    sw_compatible_mbedtls_ccm_free(ctx);
#endif
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

int mbedtls_ccm_starts( mbedtls_ccm_context *ctx,
                        int mode,
                        const unsigned char *iv,
                        size_t iv_len,
                        size_t tag_len,
                        size_t add_len,
                        size_t payload_len)
{
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );
    /* keep compliant with software mbedtls, request add_len lt 0xFF00 */
    if( add_len > 0xFF00 )
        return( MBEDTLS_ERR_CCM_BAD_INPUT );

#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            CCM_VALIDATE_RET( ctx->init );
            return _convert_retval_to_mbedtls(te_ccm_start(ctx->crypt,
                                        mode == MBEDTLS_CCM_ENCRYPT ?
                                        TE_DRV_SCA_ENCRYPT : TE_DRV_SCA_DECRYPT,
                                        iv,
                                        iv_len,
                                        tag_len,
                                        add_len,
                                        payload_len));
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_ccm_starts(ctx, mode,
                                                    iv, iv_len,
                                                    tag_len, add_len,
                                                    payload_len);
        default:
            return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
#endif
}

int mbedtls_ccm_update_aad( mbedtls_ccm_context *ctx,
                            size_t add_len,
                            const unsigned char *add)
{
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );
#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            CCM_VALIDATE_RET( ctx->init );
            return _convert_retval_to_mbedtls(te_ccm_update_aad(ctx->crypt,
                                      add,
                                      add_len));
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_ccm_update_aad(ctx,
                                                        add_len, add);
        default:
            return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
#endif
}

int mbedtls_ccm_update( mbedtls_ccm_context *ctx,
                        size_t length,
                        const unsigned char *input,
                        unsigned char *output )
{
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );
#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            CCM_VALIDATE_RET( ctx->init );
            return _convert_retval_to_mbedtls(te_ccm_update(ctx->crypt,
                                      length,
                                      input,
                                      output));
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_ccm_update(ctx, length,
                                                    input, output);
        default:
            return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
#endif
}

int mbedtls_ccm_finish( mbedtls_ccm_context *ctx,
                        unsigned char *tag,
                        size_t tag_len )
{
    int ret = TE_SUCCESS;
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );
#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            CCM_VALIDATE_RET( ctx->init );
            ret = te_ccm_finish(ctx->crypt, tag, tag_len);
            /* keep compliant with gcm let mbedtls_ccm_auth_decrypt to verify tag */
            if (ret == (int)TE_ERROR_SECURITY) {
                ret = TE_SUCCESS;
            }
            return _convert_retval_to_mbedtls(ret);
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_ccm_finish(ctx, tag, tag_len);
        default:
            return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
#endif
}

int mbedtls_ccm_clone( mbedtls_ccm_context *dst,
                       const mbedtls_ccm_context *src )
{
    int ret = TE_SUCCESS;

    CCM_VALIDATE_RET( dst != NULL && src != NULL );
    CCM_VALIDATE_RET( src->magic == MBEDTLS_CCM_MAGIC );
    CCM_VALIDATE_RET( dst->magic == MBEDTLS_CCM_MAGIC );

#if defined(MBEDTLS_CAMELLIA_C)
    switch(src->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            CCM_VALIDATE_RET( src->init );
            ret = te_ccm_clone( src->crypt, dst->crypt );
            if (ret == TE_SUCCESS) {
                dst->init = true;
            }
            dst->cipher_id = src->cipher_id;
            return _convert_retval_to_mbedtls(ret);
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_ccm_clone(dst, src);
        default:
            return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
#endif
}

/*
 * Authenticated encryption or decryption
 */
static int ccm_auth_crypt( mbedtls_ccm_context *ctx, int mode, size_t length,
                           const unsigned char *iv, size_t iv_len,
                           const unsigned char *add, size_t add_len,
                           const unsigned char *input, unsigned char *output,
                           unsigned char *tag, size_t tag_len )
{
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );
    /* keep compliant with software mbedtls, request add_len lt 0xFF00 */
    if( add_len > 0xFF00 )
        return( MBEDTLS_ERR_CCM_BAD_INPUT );

#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            CCM_VALIDATE_RET( ctx->init );
            return _convert_retval_to_mbedtls(te_ccm_crypt(ctx->crypt,
                                            mode == MBEDTLS_CCM_ENCRYPT ?
                                            TE_DRV_SCA_ENCRYPT : TE_DRV_SCA_DECRYPT,
                                            length,
                                            (uint8_t *)iv,
                                            iv_len,
                                            add,
                                            add_len,
                                            input,
                                            output,
                                            tag,
                                            tag_len));
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_ccm_auth_crypt(ctx, mode, length,iv, iv_len,
                                    add, add_len, input, output, tag, tag_len);
        default:
            return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
#endif
}

/*
 * Authenticated encryption
 */
int mbedtls_ccm_star_encrypt_and_tag( mbedtls_ccm_context *ctx, size_t length,
                         const unsigned char *iv, size_t iv_len,
                         const unsigned char *add, size_t add_len,
                         const unsigned char *input, unsigned char *output,
                         unsigned char *tag, size_t tag_len )
{
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );
#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            return ccm_auth_crypt( ctx, MBEDTLS_CCM_ENCRYPT, length, iv, iv_len,
                            add, add_len, input, output, tag, tag_len );
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_ccm_star_encrypt_and_tag(  ctx, length,
                                                                    iv, iv_len,
                                                                    add, add_len,
                                                                    input, output,
                                                                    tag, tag_len );
        default:
            return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
#endif
}

int mbedtls_ccm_encrypt_and_tag( mbedtls_ccm_context *ctx, size_t length,
                         const unsigned char *iv, size_t iv_len,
                         const unsigned char *add, size_t add_len,
                         const unsigned char *input, unsigned char *output,
                         unsigned char *tag, size_t tag_len )
{
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );
#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            return( mbedtls_ccm_star_encrypt_and_tag( ctx, length, iv, iv_len, add,
                        add_len, input, output, tag, tag_len ) );
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_ccm_encrypt_and_tag(  ctx, length,
                                                                iv, iv_len,
                                                                add, add_len,
                                                                input, output,
                                                                tag, tag_len );
        default:
            return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
#endif
}

/*
 * Authenticated decryption
 */
int mbedtls_ccm_star_auth_decrypt( mbedtls_ccm_context *ctx, size_t length,
                      const unsigned char *iv, size_t iv_len,
                      const unsigned char *add, size_t add_len,
                      const unsigned char *input, unsigned char *output,
                      const unsigned char *tag, size_t tag_len )
{
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );
#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            return ccm_auth_crypt( ctx, MBEDTLS_CCM_DECRYPT, length,
                                        iv, iv_len, add, add_len,
                                        input, output,
                                        (unsigned char *)tag, tag_len );
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_ccm_star_auth_decrypt(  ctx, length,
                                                                iv, iv_len,
                                                                add, add_len,
                                                                input, output,
                                                                tag, tag_len );
        default:
            return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
#endif
}

int mbedtls_ccm_auth_decrypt( mbedtls_ccm_context *ctx, size_t length,
                      const unsigned char *iv, size_t iv_len,
                      const unsigned char *add, size_t add_len,
                      const unsigned char *input, unsigned char *output,
                      const unsigned char *tag, size_t tag_len )
{
    CCM_VALIDATE_RET( ctx != NULL );
    CCM_VALIDATE_RET( ctx->magic == MBEDTLS_CCM_MAGIC );
#if defined(MBEDTLS_CAMELLIA_C)
    switch(ctx->cipher_id) {
        case MBEDTLS_CIPHER_ID_AES:
        case MBEDTLS_CIPHER_ID_SM4:
#endif
            return( mbedtls_ccm_star_auth_decrypt( ctx, length, iv, iv_len, add,
                        add_len, input, output, tag, tag_len ) );
#if defined(MBEDTLS_CAMELLIA_C)
        case MBEDTLS_CIPHER_ID_CAMELLIA:
            return sw_compatible_mbedtls_ccm_auth_decrypt(  ctx, length,
                                                            iv, iv_len,
                                                            add, add_len,
                                                            input, output,
                                                            tag, tag_len );
        default:
            return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
#endif
}

#endif /* !MBEDTLS_CCM_ALT */

#endif /* MBEDTLS_CCM_C */
