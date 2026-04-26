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
 *  The AES block cipher was designed by Vincent Rijmen and Joan Daemen.
 *
 *  http://csrc.nist.gov/encryption/aes/rijndael/Rijndael.pdf
 *  http://csrc.nist.gov/publications/fips/fips197/fips-197.pdf
 */
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_AES_C)

#include <string.h>

#include "mbedtls/aes.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"
#include "te_aes.h"
#include "te_xts.h"
#if defined(MBEDTLS_PADLOCK_C)
#include "mbedtls/padlock.h"
#endif
#if defined(MBEDTLS_AESNI_C)
#include "mbedtls/aesni.h"
#endif

#if defined(MBEDTLS_SELF_TEST)
#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#define mbedtls_printf printf
#endif /* MBEDTLS_PLATFORM_C */
#endif /* MBEDTLS_SELF_TEST */

#if defined(MBEDTLS_AES_ALT) && defined(CFG_CRYPTO_AES_CIX_ENG)
/* Parameter validation macros based on platform_util.h */
#define AES_VALIDATE_RET( cond )    \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_AES_BAD_INPUT_DATA )
#define AES_VALIDATE( cond )        \
    MBEDTLS_INTERNAL_VALIDATE( cond )

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_AES_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_AES_BAD_INPUT_DATA;
            break;
        case TE_ERROR_BAD_INPUT_LENGTH:
            errno = MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
            break;
        case TE_ERROR_BAD_KEY_LENGTH:
            errno = MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
            break;
        case TE_ERROR_GENERIC:
            errno = MBEDTLS_ERR_AES_FEATURE_UNAVAILABLE;
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

void mbedtls_aes_init( mbedtls_aes_context *ctx )
{
    AES_VALIDATE( ctx != NULL );
    if( ctx->cipher != NULL && MBEDTLS_AES_MAGIC == ctx->magic) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
    ctx->cipher = (te_cipher_ctx_t *)mbedtls_calloc(1, sizeof(*(ctx->cipher)));
    OSAL_ASSERT( ctx->cipher != NULL );
    OSAL_ASSERT(TE_SUCCESS == te_aes_init(ctx->cipher,
                                te_platform_get_drvhandle()));
    ctx->magic = MBEDTLS_AES_MAGIC;
}

void mbedtls_aes_free( mbedtls_aes_context *ctx )
{
    AES_VALIDATE( ctx != NULL );
    AES_VALIDATE( ctx->magic == MBEDTLS_AES_MAGIC );
    te_aes_free(ctx->cipher);
    mbedtls_free(ctx->cipher);
    ctx->cipher = NULL;
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

int mbedtls_aes_clone( mbedtls_aes_context *dst,
                       const mbedtls_aes_context *src )
{
    int ret = TE_SUCCESS;

    AES_VALIDATE_RET( src != NULL && src->cipher != NULL);
    AES_VALIDATE_RET( MBEDTLS_AES_MAGIC == src->magic );
    AES_VALIDATE_RET( dst != NULL);

    if (dst->cipher == NULL) {
        mbedtls_printf( "init aes clone dst!\n");
        mbedtls_aes_init( dst );
    }

    ret = te_aes_clone(src->cipher, dst->cipher);

    return _convert_retval_to_mbedtls(ret);
}

#if defined(MBEDTLS_CIPHER_MODE_XTS)
void mbedtls_aes_xts_init( mbedtls_aes_xts_context *ctx )
{
    AES_VALIDATE( ctx != NULL );
    if( ctx->xts != NULL && MBEDTLS_AES_XTS_MAGIC == ctx->magic) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
    ctx->xts = (te_xts_ctx_t *)mbedtls_calloc(1, sizeof(*(ctx->xts)));
    OSAL_ASSERT( ctx->xts != NULL );
    OSAL_ASSERT( TE_SUCCESS == te_xts_init(ctx->xts,
                                  te_platform_get_drvhandle(),
                                  TE_MAIN_ALGO_AES));
    ctx->magic = MBEDTLS_AES_XTS_MAGIC;
}

void mbedtls_aes_xts_free( mbedtls_aes_xts_context *ctx )
{
    AES_VALIDATE( ctx != NULL );
    AES_VALIDATE( ctx->magic == MBEDTLS_AES_XTS_MAGIC );
    te_xts_free(ctx->xts);
    mbedtls_free(ctx->xts);
    ctx->xts = NULL;
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

int mbedtls_aes_xts_clone( mbedtls_aes_xts_context *dst,
                           const mbedtls_aes_xts_context *src )
{
    int ret = TE_SUCCESS;

    AES_VALIDATE_RET( src != NULL && src->xts != NULL );
    AES_VALIDATE_RET( MBEDTLS_AES_XTS_MAGIC == src->magic );
    AES_VALIDATE_RET( dst != NULL && dst->xts != NULL );
    AES_VALIDATE_RET( MBEDTLS_AES_XTS_MAGIC == dst->magic );

    ret = te_xts_clone(src->xts, dst->xts);

    return _convert_retval_to_mbedtls(ret);
}
#endif /* MBEDTLS_CIPHER_MODE_XTS */

static void _mbedtls_sec_key_to_te_sec_key( te_sec_key_t *sec_key,
                                            mbedtls_aes_sec_key_t *key )
{
    sec_key->sel = (key->sel == MBEDTLS_AES_KL_KEY_MODEL) ?
                   TE_KL_KEY_MODEL : TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    osal_memcpy(sec_key->eks, key->eks,
                ((sizeof(sec_key->eks) > sizeof(key->eks)) ?
                 sizeof(key->eks) : sizeof(sec_key->eks)));
}

static void _mbedtls_sec_key_to_te_sec_key_v2( te_sec_key_v2_t *sec_key,
                                               mbedtls_aes_sec_key_v2_t *key )
{
    sec_key->sel = (key->sel == MBEDTLS_AES_KL_KEY_MODEL) ?
                   TE_KL_KEY_MODEL : TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    osal_memcpy(sec_key->eks, key->eks,
                ((sizeof(sec_key->eks) > sizeof(key->eks)) ?
                 sizeof(key->eks) : sizeof(sec_key->eks)));
}

/*
 * AES key schedule (encryption)
 */
#if defined(MBEDTLS_AES_SETKEY_ENC_ALT)
int mbedtls_aes_setkey_enc( mbedtls_aes_context *ctx, const unsigned char *key,
                    unsigned int keybits )
{
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    return _convert_retval_to_mbedtls(te_aes_setkey(ctx->cipher,
                                                    key, keybits));
}

int mbedtls_aes_setseckey_enc( mbedtls_aes_context *ctx,
                               mbedtls_aes_sec_key_t *key )
{
    te_sec_key_t sec_key = {0};
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    AES_VALIDATE_RET( (key != NULL) &&
                      ((key->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    _mbedtls_sec_key_to_te_sec_key(&sec_key, key);
    return _convert_retval_to_mbedtls(te_aes_setseckey(ctx->cipher,
                                                        &sec_key));
}

int mbedtls_aes_setseckey_v2_enc( mbedtls_aes_context *ctx,
                                  mbedtls_aes_sec_key_v2_t *key )
{
    te_sec_key_v2_t sec_key = {0};
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    AES_VALIDATE_RET( (key != NULL) &&
                      ((key->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    _mbedtls_sec_key_to_te_sec_key_v2(&sec_key, key);
    return _convert_retval_to_mbedtls(te_aes_setseckey_v2(ctx->cipher,
                                                          &sec_key));
}
#endif /* !MBEDTLS_AES_SETKEY_ENC_ALT */

/*
 * AES key schedule (decryption)
 */
#if defined(MBEDTLS_AES_SETKEY_DEC_ALT)
int mbedtls_aes_setkey_dec( mbedtls_aes_context *ctx, const unsigned char *key,
                    unsigned int keybits )
{
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    return _convert_retval_to_mbedtls(te_aes_setkey(ctx->cipher, key,
                                                    keybits));
}

int mbedtls_aes_setseckey_dec( mbedtls_aes_context *ctx,
                               mbedtls_aes_sec_key_t *key )
{
    te_sec_key_t sec_key = {0};
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    AES_VALIDATE_RET( (key != NULL) &&
                      ((key->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    _mbedtls_sec_key_to_te_sec_key(&sec_key, key);
    return _convert_retval_to_mbedtls(te_aes_setseckey(ctx->cipher, &sec_key));
}

int mbedtls_aes_setseckey_v2_dec( mbedtls_aes_context *ctx,
                                  mbedtls_aes_sec_key_v2_t *key )
{
    te_sec_key_v2_t sec_key = {0};
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    AES_VALIDATE_RET( (key != NULL) &&
                      ((key->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    _mbedtls_sec_key_to_te_sec_key_v2(&sec_key, key);
    return _convert_retval_to_mbedtls(te_aes_setseckey_v2(ctx->cipher, &sec_key));
}

#if defined(MBEDTLS_CIPHER_MODE_XTS)
int mbedtls_aes_xts_setkey_enc( mbedtls_aes_xts_context *ctx,
                                const unsigned char *key,
                                unsigned int keybits)
{
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_XTS_MAGIC );
    return _convert_retval_to_mbedtls(te_xts_setkey(ctx->xts,
                                                    key, keybits));
}

int mbedtls_aes_xts_setseckey_enc( mbedtls_aes_xts_context *ctx,
                                   mbedtls_aes_sec_key_t *key1,
                                   mbedtls_aes_sec_key_t *key2)
{
    te_sec_key_t sec_key1 = {0};
    te_sec_key_t sec_key2 = {0};
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_XTS_MAGIC );
    AES_VALIDATE_RET( (key1 != NULL) &&
                      ((key1->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key1->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    AES_VALIDATE_RET( (key2 != NULL) &&
                      ((key2->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key2->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    _mbedtls_sec_key_to_te_sec_key(&sec_key1, key1);
    _mbedtls_sec_key_to_te_sec_key(&sec_key2, key2);
    return _convert_retval_to_mbedtls(te_xts_setseckey( ctx->xts,
                                                        &sec_key1,
                                                        &sec_key2) );
}

int mbedtls_aes_xts_setseckey_v2_enc( mbedtls_aes_xts_context *ctx,
                                      mbedtls_aes_sec_key_v2_t *key1,
                                      mbedtls_aes_sec_key_v2_t *key2 )
{
    te_sec_key_v2_t sec_key1 = {0};
    te_sec_key_v2_t sec_key2 = {0};
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_XTS_MAGIC );
    AES_VALIDATE_RET( (key1 != NULL) &&
                      ((key1->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key1->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    AES_VALIDATE_RET( (key2 != NULL) &&
                      ((key2->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key2->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    _mbedtls_sec_key_to_te_sec_key_v2(&sec_key1, key1);
    _mbedtls_sec_key_to_te_sec_key_v2(&sec_key2, key2);
    return _convert_retval_to_mbedtls(te_xts_setseckey_v2( ctx->xts,
                                                           &sec_key1,
                                                           &sec_key2) );
}

int mbedtls_aes_xts_setkey_dec( mbedtls_aes_xts_context *ctx,
                                const unsigned char *key,
                                unsigned int keybits)
{
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_XTS_MAGIC );
    return _convert_retval_to_mbedtls(te_xts_setkey( ctx->xts, key,
                                                    keybits ));
}

int mbedtls_aes_xts_setseckey_dec( mbedtls_aes_xts_context *ctx,
                                   mbedtls_aes_sec_key_t *key1,
                                   mbedtls_aes_sec_key_t *key2 )
{
    te_sec_key_t sec_key1 = {0};
    te_sec_key_t sec_key2 = {0};
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_XTS_MAGIC );
    AES_VALIDATE_RET( (key1 != NULL) &&
                      ((key1->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key1->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    AES_VALIDATE_RET( (key2 != NULL) &&
                      ((key2->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key2->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    _mbedtls_sec_key_to_te_sec_key(&sec_key1, key1);
    _mbedtls_sec_key_to_te_sec_key(&sec_key2, key2);
    return _convert_retval_to_mbedtls( te_xts_setseckey(ctx->xts,
                                                        &sec_key1,
                                                        &sec_key2) );
}

int mbedtls_aes_xts_setseckey_v2_dec( mbedtls_aes_xts_context *ctx,
                                      mbedtls_aes_sec_key_v2_t *key1,
                                      mbedtls_aes_sec_key_v2_t *key2 )
{
    te_sec_key_v2_t sec_key1 = {0};
    te_sec_key_v2_t sec_key2 = {0};
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_XTS_MAGIC );
    AES_VALIDATE_RET( (key1 != NULL) &&
                      ((key1->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key1->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    AES_VALIDATE_RET( (key2 != NULL) &&
                      ((key2->sel == MBEDTLS_AES_KL_KEY_MODEL) ||
                       (key2->sel == MBEDTLS_AES_KL_KEY_ROOT)) );
    _mbedtls_sec_key_to_te_sec_key_v2( &sec_key1, key1 );
    _mbedtls_sec_key_to_te_sec_key_v2( &sec_key2, key2 );
    return _convert_retval_to_mbedtls( te_xts_setseckey_v2(ctx->xts,
                                                           &sec_key1,
                                                           &sec_key2) );
}
#endif /* MBEDTLS_CIPHER_MODE_XTS */

#endif /* MBEDTLS_AES_SETKEY_DEC_ALT */

/*
 * AES-ECB block encryption
 */
#if defined(MBEDTLS_AES_ENCRYPT_ALT)
int mbedtls_internal_aes_encrypt( mbedtls_aes_context *ctx,
                                  const unsigned char input[16],
                                  unsigned char output[16] )
{
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    return _convert_retval_to_mbedtls(te_aes_ecb(ctx->cipher,
                            TE_DRV_SCA_ENCRYPT,
                            ctx->cipher->crypt->blk_size,
                            input,
                            output));
}
#endif /* MBEDTLS_AES_ENCRYPT_ALT */

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_aes_encrypt( mbedtls_aes_context *ctx,
                          const unsigned char input[16],
                          unsigned char output[16] )
{
    mbedtls_internal_aes_encrypt( ctx, input, output );
}
#endif /* !MBEDTLS_DEPRECATED_REMOVED */

/*
 * AES-ECB block decryption
 */
#if defined(MBEDTLS_AES_DECRYPT_ALT)
int mbedtls_internal_aes_decrypt( mbedtls_aes_context *ctx,
                                  const unsigned char input[16],
                                  unsigned char output[16] )
{
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    return _convert_retval_to_mbedtls(te_aes_ecb(ctx->cipher,
                            TE_DRV_SCA_DECRYPT,
                            ctx->cipher->crypt->blk_size,
                            input,
                            output));
}
#endif /* MBEDTLS_AES_DECRYPT_ALT */

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_aes_decrypt( mbedtls_aes_context *ctx,
                          const unsigned char input[16],
                          unsigned char output[16] )
{
    mbedtls_internal_aes_decrypt( ctx, input, output );
}
#endif /* !MBEDTLS_DEPRECATED_REMOVED */

/*
 * AES-ECB block encryption/decryption
 */
int mbedtls_aes_crypt_ecb( mbedtls_aes_context *ctx,
                           int mode,
                           const unsigned char input[16],
                           unsigned char output[16] )
{
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    AES_VALIDATE_RET( mode == MBEDTLS_AES_ENCRYPT ||
                      mode == MBEDTLS_AES_DECRYPT );

    return _convert_retval_to_mbedtls(te_aes_ecb( ctx->cipher,
                             mode == MBEDTLS_AES_ENCRYPT ?
                              TE_DRV_SCA_ENCRYPT : TE_DRV_SCA_DECRYPT,
                             ctx->cipher->crypt->blk_size,
                             input,
                             output ));
}

#if defined(MBEDTLS_CIPHER_MODE_CBC)
/*
 * AES-CBC buffer encryption/decryption
 */
int mbedtls_aes_crypt_cbc( mbedtls_aes_context *ctx,
                    int mode,
                    size_t length,
                    unsigned char iv[16],
                    const unsigned char *input,
                    unsigned char *output )
{
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    AES_VALIDATE_RET( mode == MBEDTLS_AES_ENCRYPT ||
                      mode == MBEDTLS_AES_DECRYPT );

    return _convert_retval_to_mbedtls(te_aes_cbc( ctx->cipher,
                             mode == MBEDTLS_AES_ENCRYPT ?
                              TE_DRV_SCA_ENCRYPT : TE_DRV_SCA_DECRYPT,
                             length,
                             iv,
                             input,
                             output ));
}
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_XTS)
/*
 * AES-XTS buffer encryption/decryption
 */
int mbedtls_aes_crypt_xts( mbedtls_aes_xts_context *ctx,
                           int mode,
                           size_t length,
                           const unsigned char data_unit[16],
                           const unsigned char *input,
                           unsigned char *output )
{
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_XTS_MAGIC );
    AES_VALIDATE_RET( mode == MBEDTLS_AES_ENCRYPT ||
                      mode == MBEDTLS_AES_DECRYPT );
    if ( length > (1 << 24) ) {
        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }

    return _convert_retval_to_mbedtls(te_xts_crypt( ctx->xts,
                         mode == MBEDTLS_AES_ENCRYPT ?
                              TE_DRV_SCA_ENCRYPT : TE_DRV_SCA_DECRYPT,
                         length,
                         (uint8_t *)data_unit,
                         input,
                         output ));
}
#endif /* MBEDTLS_CIPHER_MODE_XTS */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
/*
 * AES-CFB128 buffer encryption/decryption
 */
int mbedtls_aes_crypt_cfb128( mbedtls_aes_context *ctx,
                       int mode,
                       size_t length,
                       size_t *iv_off,
                       unsigned char iv[16],
                       const unsigned char *input,
                       unsigned char *output )
{
    int c;
    size_t n;

    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    AES_VALIDATE_RET( mode == MBEDTLS_AES_ENCRYPT ||
                      mode == MBEDTLS_AES_DECRYPT );
    AES_VALIDATE_RET( iv_off != NULL );
    AES_VALIDATE_RET( iv != NULL );
    AES_VALIDATE_RET( input != NULL );
    AES_VALIDATE_RET( output != NULL );

    n = *iv_off;

    if( n > 15 )
        return( MBEDTLS_ERR_AES_BAD_INPUT_DATA );

    if( mode == MBEDTLS_AES_DECRYPT )
    {
        while( length-- )
        {
            if( n == 0 )
                mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, iv, iv );

            c = *input++;
            *output++ = (unsigned char)( c ^ iv[n] );
            iv[n] = (unsigned char) c;

            n = ( n + 1 ) & 0x0F;
        }
    }
    else
    {
        while( length-- )
        {
            if( n == 0 )
                mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, iv, iv );

            iv[n] = *output++ = (unsigned char)( iv[n] ^ *input++ );

            n = ( n + 1 ) & 0x0F;
        }
    }

    *iv_off = n;

    return( 0 );
}

/*
 * AES-CFB8 buffer encryption/decryption
 */
int mbedtls_aes_crypt_cfb8( mbedtls_aes_context *ctx,
                            int mode,
                            size_t length,
                            unsigned char iv[16],
                            const unsigned char *input,
                            unsigned char *output )
{
    unsigned char c;
    unsigned char ov[17];

    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    AES_VALIDATE_RET( mode == MBEDTLS_AES_ENCRYPT ||
                      mode == MBEDTLS_AES_DECRYPT );
    AES_VALIDATE_RET( iv != NULL );
    AES_VALIDATE_RET( input != NULL );
    AES_VALIDATE_RET( output != NULL );
    while( length-- )
    {
        memcpy( ov, iv, 16 );
        mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, iv, iv );

        if( mode == MBEDTLS_AES_DECRYPT )
            ov[16] = *input;

        c = *output++ = (unsigned char)( iv[0] ^ *input++ );

        if( mode == MBEDTLS_AES_ENCRYPT )
            ov[16] = c;

        memcpy( iv, ov + 1, 16 );
    }

    return( 0 );
}
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_OFB)
/*
 * AES-OFB (Output Feedback Mode) buffer encryption/decryption
 */
int mbedtls_aes_crypt_ofb( mbedtls_aes_context *ctx,
                           size_t length,
                           size_t *iv_off,
                           unsigned char iv[16],
                           const unsigned char *input,
                           unsigned char *output )
{
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    AES_VALIDATE_RET( iv_off != NULL );

    return _convert_retval_to_mbedtls( te_aes_ofb( ctx->cipher,
                                       length,
                                       iv_off,
                                       iv,
                                       input,
                                       output) );
}
#endif /* MBEDTLS_CIPHER_MODE_OFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
/*
 * AES-CTR buffer encryption/decryption
 */
int mbedtls_aes_crypt_ctr( mbedtls_aes_context *ctx,
                       size_t length,
                       size_t *nc_off,
                       unsigned char nonce_counter[16],
                       unsigned char stream_block[16],
                       const unsigned char *input,
                       unsigned char *output )
{
    AES_VALIDATE_RET( ctx != NULL );
    AES_VALIDATE_RET( ctx->magic == MBEDTLS_AES_MAGIC );
    AES_VALIDATE_RET( nc_off != NULL );

    return _convert_retval_to_mbedtls(te_aes_ctr( ctx->cipher,
                       length,
                       nc_off,
                       nonce_counter,
                       stream_block,
                       input,
                       output ));
}
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#endif /* MBEDTLS_AES_ALT */

#endif /* MBEDTLS_AES_C */
