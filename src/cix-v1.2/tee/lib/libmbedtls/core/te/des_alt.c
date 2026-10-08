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
 *  DES, on which TDES is based, was originally designed by Horst Feistel
 *  at IBM in 1974, and was adopted as a standard by NIST (formerly NBS).
 *
 *  http://csrc.nist.gov/publications/fips/fips46-3/fips46-3.pdf
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_DES_C)

#include "mbedtls/des.h"
#include "mbedtls/platform_util.h"
#include "te_des.h"

#include <string.h>
#include "mbedtls/platform.h"

/* Parameter validation macros */
#define DES_VALIDATE_RET( cond ) \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_DES_BAD_INPUT )
#define DES_VALIDATE( cond ) \
    MBEDTLS_INTERNAL_VALIDATE( cond )

#if defined(MBEDTLS_DES_ALT)

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_DES_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_DES_BAD_INPUT;
            break;
        case TE_ERROR_BAD_INPUT_LENGTH:
            errno = MBEDTLS_ERR_DES_INVALID_INPUT_LENGTH;
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

void mbedtls_des_init( mbedtls_des_context *ctx )
{
    DES_VALIDATE(NULL != ctx);
    if( ctx->cipher != NULL && MBEDTLS_DES_MAGIC == ctx->magic) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
    ctx->cipher = (te_cipher_ctx_t *)mbedtls_calloc(1, sizeof(*ctx->cipher));
    OSAL_ASSERT(NULL != ctx->cipher);
    OSAL_ASSERT(TE_SUCCESS == te_des_init(ctx->cipher,
                        te_platform_get_drvhandle()));
    ctx->magic = MBEDTLS_DES_MAGIC;
}

void mbedtls_des_free( mbedtls_des_context *ctx )
{
    DES_VALIDATE(NULL != ctx);
    DES_VALIDATE(MBEDTLS_DES_MAGIC == ctx->magic);
    te_des_free(ctx->cipher);
    mbedtls_free(ctx->cipher);
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

int mbedtls_des_clone( mbedtls_des_context *dst,
                       const mbedtls_des_context *src )
{
    int ret = TE_SUCCESS;

    DES_VALIDATE_RET( src != NULL && src->cipher != NULL);
    DES_VALIDATE_RET( MBEDTLS_DES_MAGIC == src->magic );
    DES_VALIDATE_RET( dst != NULL);

    if (dst->cipher == NULL) {
        mbedtls_printf( "init des clone dst!\n");
        mbedtls_des_init( dst );
    }

    ret = te_des_clone(src->cipher, dst->cipher);
    if (ret == TE_SUCCESS) {
        dst->mode = src->mode;
    }

    return _convert_retval_to_mbedtls(ret);
}

void mbedtls_des3_init( mbedtls_des3_context *ctx )
{
    DES_VALIDATE(NULL != ctx);
    if( ctx->cipher != NULL && MBEDTLS_3DES_MAGIC == ctx->magic) {
        mbedtls_printf( "#WARN %s %d ctx may double init\n", __func__, __LINE__ );
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
    ctx->cipher = (te_cipher_ctx_t *)mbedtls_calloc(1, sizeof(*ctx->cipher));
    OSAL_ASSERT(NULL != ctx->cipher);
    OSAL_ASSERT(TE_SUCCESS == te_tdes_init(ctx->cipher,
                                   te_platform_get_drvhandle()));
    ctx->magic = MBEDTLS_3DES_MAGIC;
}

void mbedtls_des3_free( mbedtls_des3_context *ctx )
{
    DES_VALIDATE(NULL != ctx);
    DES_VALIDATE(MBEDTLS_3DES_MAGIC == ctx->magic);
    te_tdes_free(ctx->cipher);
    mbedtls_free(ctx->cipher);
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

int mbedtls_des3_clone( mbedtls_des3_context *dst,
                        const mbedtls_des3_context *src )
{
    int ret = TE_SUCCESS;

    DES_VALIDATE_RET( src != NULL && src->cipher != NULL);
    DES_VALIDATE_RET( MBEDTLS_3DES_MAGIC == src->magic );
    DES_VALIDATE_RET( dst != NULL);

    if (dst->cipher == NULL) {
        mbedtls_printf( "init des3 clone dst!\n");
        mbedtls_des3_init( dst );
    }

    ret = te_tdes_clone(src->cipher, dst->cipher);
    if (ret == TE_SUCCESS) {
        dst->mode = src->mode;
    }

    return _convert_retval_to_mbedtls(ret);
}

static const unsigned char odd_parity_table[128] = { 1,  2,  4,  7,  8,
        11, 13, 14, 16, 19, 21, 22, 25, 26, 28, 31, 32, 35, 37, 38, 41, 42, 44,
        47, 49, 50, 52, 55, 56, 59, 61, 62, 64, 67, 69, 70, 73, 74, 76, 79, 81,
        82, 84, 87, 88, 91, 93, 94, 97, 98, 100, 103, 104, 107, 109, 110, 112,
        115, 117, 118, 121, 122, 124, 127, 128, 131, 133, 134, 137, 138, 140,
        143, 145, 146, 148, 151, 152, 155, 157, 158, 161, 162, 164, 167, 168,
        171, 173, 174, 176, 179, 181, 182, 185, 186, 188, 191, 193, 194, 196,
        199, 200, 203, 205, 206, 208, 211, 213, 214, 217, 218, 220, 223, 224,
        227, 229, 230, 233, 234, 236, 239, 241, 242, 244, 247, 248, 251, 253,
        254 };

void mbedtls_des_key_set_parity( unsigned char key[MBEDTLS_DES_KEY_SIZE] )
{
    int i;

    for( i = 0; i < MBEDTLS_DES_KEY_SIZE; i++ )
        key[i] = odd_parity_table[key[i] / 2];
}

/*
 * Check the given key's parity, returns 1 on failure, 0 on SUCCESS
 */
int mbedtls_des_key_check_key_parity( const unsigned char key[MBEDTLS_DES_KEY_SIZE] )
{
    int i;

    for( i = 0; i < MBEDTLS_DES_KEY_SIZE; i++ )
        if( key[i] != odd_parity_table[key[i] / 2] )
            return( 1 );

    return( 0 );
}

/*
 * Table of weak and semi-weak keys
 *
 * Source: http://en.wikipedia.org/wiki/Weak_key
 *
 * Weak:
 * Alternating ones + zeros (0x0101010101010101)
 * Alternating 'F' + 'E' (0xFEFEFEFEFEFEFEFE)
 * '0xE0E0E0E0F1F1F1F1'
 * '0x1F1F1F1F0E0E0E0E'
 *
 * Semi-weak:
 * 0x011F011F010E010E and 0x1F011F010E010E01
 * 0x01E001E001F101F1 and 0xE001E001F101F101
 * 0x01FE01FE01FE01FE and 0xFE01FE01FE01FE01
 * 0x1FE01FE00EF10EF1 and 0xE01FE01FF10EF10E
 * 0x1FFE1FFE0EFE0EFE and 0xFE1FFE1FFE0EFE0E
 * 0xE0FEE0FEF1FEF1FE and 0xFEE0FEE0FEF1FEF1
 *
 */

#define WEAK_KEY_COUNT 16

static const unsigned char weak_key_table[WEAK_KEY_COUNT][MBEDTLS_DES_KEY_SIZE] =
{
    { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
    { 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE },
    { 0x1F, 0x1F, 0x1F, 0x1F, 0x0E, 0x0E, 0x0E, 0x0E },
    { 0xE0, 0xE0, 0xE0, 0xE0, 0xF1, 0xF1, 0xF1, 0xF1 },

    { 0x01, 0x1F, 0x01, 0x1F, 0x01, 0x0E, 0x01, 0x0E },
    { 0x1F, 0x01, 0x1F, 0x01, 0x0E, 0x01, 0x0E, 0x01 },
    { 0x01, 0xE0, 0x01, 0xE0, 0x01, 0xF1, 0x01, 0xF1 },
    { 0xE0, 0x01, 0xE0, 0x01, 0xF1, 0x01, 0xF1, 0x01 },
    { 0x01, 0xFE, 0x01, 0xFE, 0x01, 0xFE, 0x01, 0xFE },
    { 0xFE, 0x01, 0xFE, 0x01, 0xFE, 0x01, 0xFE, 0x01 },
    { 0x1F, 0xE0, 0x1F, 0xE0, 0x0E, 0xF1, 0x0E, 0xF1 },
    { 0xE0, 0x1F, 0xE0, 0x1F, 0xF1, 0x0E, 0xF1, 0x0E },
    { 0x1F, 0xFE, 0x1F, 0xFE, 0x0E, 0xFE, 0x0E, 0xFE },
    { 0xFE, 0x1F, 0xFE, 0x1F, 0xFE, 0x0E, 0xFE, 0x0E },
    { 0xE0, 0xFE, 0xE0, 0xFE, 0xF1, 0xFE, 0xF1, 0xFE },
    { 0xFE, 0xE0, 0xFE, 0xE0, 0xFE, 0xF1, 0xFE, 0xF1 }
};

int mbedtls_des_key_check_weak( const unsigned char key[MBEDTLS_DES_KEY_SIZE] )
{
    int i;

    for( i = 0; i < WEAK_KEY_COUNT; i++ )
        if( memcmp( weak_key_table[i], key, MBEDTLS_DES_KEY_SIZE) == 0 )
            return( 1 );

    return( 0 );
}

/*
 * DES key schedule (56-bit, encryption)
 */
int mbedtls_des_setkey_enc( mbedtls_des_context *ctx,
                    const unsigned char key[MBEDTLS_DES_KEY_SIZE] )
{
    DES_VALIDATE_RET( ctx != NULL );
    DES_VALIDATE_RET(MBEDTLS_DES_MAGIC == ctx->magic);
    ctx->mode = MBEDTLS_DES_ENCRYPT;
    return _convert_retval_to_mbedtls(te_des_setkey(ctx->cipher, key));
}

/*
 * DES key schedule (56-bit, decryption)
 */
int mbedtls_des_setkey_dec( mbedtls_des_context *ctx,
                        const unsigned char key[MBEDTLS_DES_KEY_SIZE] )
{
    DES_VALIDATE_RET( ctx != NULL );
    DES_VALIDATE_RET(MBEDTLS_DES_MAGIC == ctx->magic);
    ctx->mode = MBEDTLS_DES_DECRYPT;
    return _convert_retval_to_mbedtls(te_des_setkey(ctx->cipher, key));
}

/*
 * Triple-DES set key (112-bit)
 */
static int _mbedtls_des3_set2key( mbedtls_des3_context *ctx,
                      const unsigned char key[MBEDTLS_DES_KEY_SIZE * 2] )
{
    uint8_t _key[MBEDTLS_DES_KEY_SIZE * 3] = {0};
    int ret = 0;

    osal_memcpy(_key, key, MBEDTLS_DES_KEY_SIZE * 2);
    osal_memcpy(_key + MBEDTLS_DES_KEY_SIZE * 2,
                key, MBEDTLS_DES_KEY_SIZE);
    ret = te_tdes_setkey(ctx->cipher, (const uint8_t *)_key);
    mbedtls_platform_zeroize( _key,  sizeof( _key ) );
    return _convert_retval_to_mbedtls(ret);
}

/*
 * Triple-DES key schedule (112-bit, encryption)
 */
int mbedtls_des3_set2key_enc( mbedtls_des3_context *ctx,
                      const unsigned char key[MBEDTLS_DES_KEY_SIZE * 2] )
{
    DES_VALIDATE_RET( ctx != NULL );
    DES_VALIDATE_RET(MBEDTLS_3DES_MAGIC == ctx->magic);
    DES_VALIDATE_RET( key != NULL );
    ctx->mode = MBEDTLS_DES_ENCRYPT;
    return _convert_retval_to_mbedtls(_mbedtls_des3_set2key(ctx, key));
}

/*
 * Triple-DES key schedule (112-bit, decryption)
 */
int mbedtls_des3_set2key_dec( mbedtls_des3_context *ctx,
                      const unsigned char key[MBEDTLS_DES_KEY_SIZE * 2] )
{
    DES_VALIDATE_RET( ctx != NULL );
    DES_VALIDATE_RET(MBEDTLS_3DES_MAGIC == ctx->magic);
    DES_VALIDATE_RET( key != NULL );
    ctx->mode = MBEDTLS_DES_DECRYPT;
    return _convert_retval_to_mbedtls(_mbedtls_des3_set2key(ctx, key));
}

/*
 * Triple-DES key schedule (168-bit, encryption)
 */
int mbedtls_des3_set3key_enc( mbedtls_des3_context *ctx,
                      const unsigned char key[MBEDTLS_DES_KEY_SIZE * 3] )
{
    DES_VALIDATE_RET( ctx != NULL );
    DES_VALIDATE_RET(MBEDTLS_3DES_MAGIC == ctx->magic);
    ctx->mode = MBEDTLS_DES_ENCRYPT;
    return _convert_retval_to_mbedtls(te_tdes_setkey(ctx->cipher, key));
}

/*
 * Triple-DES key schedule (168-bit, decryption)
 */
int mbedtls_des3_set3key_dec( mbedtls_des3_context *ctx,
                      const unsigned char key[MBEDTLS_DES_KEY_SIZE * 3] )
{
    DES_VALIDATE_RET( ctx != NULL );
    DES_VALIDATE_RET(MBEDTLS_3DES_MAGIC == ctx->magic);
    ctx->mode = MBEDTLS_DES_DECRYPT;
    return _convert_retval_to_mbedtls(te_tdes_setkey(ctx->cipher, key));
}

/*
 * DES-ECB block encryption/decryption
 */
#if defined(MBEDTLS_DES_CRYPT_ECB_ALT)
int mbedtls_des_crypt_ecb( mbedtls_des_context *ctx,
                    const unsigned char input[8],
                    unsigned char output[8] )
{
    DES_VALIDATE_RET( ctx != NULL );
    DES_VALIDATE_RET(MBEDTLS_DES_MAGIC == ctx->magic);
    DES_VALIDATE_RET( ctx->mode == MBEDTLS_DES_ENCRYPT ||
                      ctx->mode == MBEDTLS_DES_DECRYPT );

    return _convert_retval_to_mbedtls(te_des_ecb( ctx->cipher,
                             ctx->mode == MBEDTLS_DES_ENCRYPT ?
                                TE_DRV_SCA_ENCRYPT : TE_DRV_SCA_DECRYPT,
                             ctx->cipher->crypt->blk_size,
                             input,
                             output ));
}
#endif /* MBEDTLS_DES_CRYPT_ECB_ALT */

#if defined(MBEDTLS_CIPHER_MODE_CBC)
/*
 * DES-CBC buffer encryption/decryption
 */
int mbedtls_des_crypt_cbc( mbedtls_des_context *ctx,
                    int mode,
                    size_t length,
                    unsigned char iv[8],
                    const unsigned char *input,
                    unsigned char *output )
{
    DES_VALIDATE_RET( ctx != NULL );
    DES_VALIDATE_RET(MBEDTLS_DES_MAGIC == ctx->magic);
    DES_VALIDATE_RET( mode == MBEDTLS_DES_ENCRYPT ||
                      mode == MBEDTLS_DES_DECRYPT );

    return _convert_retval_to_mbedtls(te_des_cbc(ctx->cipher,
                            mode == MBEDTLS_DES_ENCRYPT ?
                                TE_DRV_SCA_ENCRYPT : TE_DRV_SCA_DECRYPT,
                            length,
                            iv,
                            input,
                            output));
}
#endif /* MBEDTLS_CIPHER_MODE_CBC */

/*
 * 3DES-ECB block encryption/decryption
 */
#if defined(MBEDTLS_DES3_CRYPT_ECB_ALT)
int mbedtls_des3_crypt_ecb( mbedtls_des3_context *ctx,
                     const unsigned char input[8],
                     unsigned char output[8] )
{
    DES_VALIDATE_RET( ctx != NULL );
    DES_VALIDATE_RET(MBEDTLS_3DES_MAGIC == ctx->magic);
    DES_VALIDATE_RET( ctx->mode == MBEDTLS_DES_ENCRYPT ||
                      ctx->mode == MBEDTLS_DES_DECRYPT );

    return _convert_retval_to_mbedtls(te_tdes_ecb(ctx->cipher,
                             ctx->mode == MBEDTLS_DES_ENCRYPT ?
                                TE_DRV_SCA_ENCRYPT : TE_DRV_SCA_DECRYPT,
                             ctx->cipher->crypt->blk_size,
                             input,
                             output));
}
#endif /* MBEDTLS_DES3_CRYPT_ECB_ALT */

#if defined(MBEDTLS_CIPHER_MODE_CBC)
/*
 * 3DES-CBC buffer encryption/decryption
 */
int mbedtls_des3_crypt_cbc( mbedtls_des3_context *ctx,
                     int mode,
                     size_t length,
                     unsigned char iv[8],
                     const unsigned char *input,
                     unsigned char *output )
{
    DES_VALIDATE_RET( ctx != NULL );
    DES_VALIDATE_RET(MBEDTLS_3DES_MAGIC == ctx->magic);
    DES_VALIDATE_RET( mode == MBEDTLS_DES_ENCRYPT ||
                      mode == MBEDTLS_DES_DECRYPT );

    return _convert_retval_to_mbedtls(te_tdes_cbc(ctx->cipher,
                             mode == MBEDTLS_DES_ENCRYPT ?
                                TE_DRV_SCA_ENCRYPT : TE_DRV_SCA_DECRYPT,
                             length,
                             iv,
                             input,
                             output));
}
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#endif /* MBEDTLS_DES_ALT */

#endif /* MBEDTLS_DES_C */
