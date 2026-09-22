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
 * SM4 (formerly SMS4) is a block cipher used in the Chinese National
 * Standard for Wireless LAN WAPI (Wired Authentication and Privacy
 * Infrastructure). On March 21, 2012, the Chinese government published
 *  the industrial standard "GM/T 0002-2012 SM4 Block Cipher Algorithm",
 * officially renaming SMS4 to SM4.
 */
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_SM4_C)

#include <string.h>

#include "mbedtls/sm4.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"

#if defined(MBEDTLS_SELF_TEST)
#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#define mbedtls_printf printf
#endif /* MBEDTLS_PLATFORM_C */
#endif /* MBEDTLS_SELF_TEST */

#if !defined(MBEDTLS_SM4_ALT)
/* Parameter validation macros based on platform_util.h */
#define SM4_VALIDATE_RET( cond )    \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_SM4_BAD_INPUT_DATA )
#define SM4_VALIDATE( cond )        \
    MBEDTLS_INTERNAL_VALIDATE( cond )

/*
 * 32-bit integer manipulation macros (little endian)
 */
#ifndef GET_UINT32_BE
#define GET_UINT32_BE(n,b,i)                            \
do {                                                    \
    (n) = ( (uint32_t) (b)[(i)    ] << 24 )             \
        | ( (uint32_t) (b)[(i) + 1] << 16 )             \
        | ( (uint32_t) (b)[(i) + 2] <<  8 )             \
        | ( (uint32_t) (b)[(i) + 3]       );            \
} while( 0 )
#endif

#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n,b,i)                            \
do {                                                    \
    (b)[(i)    ] = (unsigned char) ( (n) >> 24 );       \
    (b)[(i) + 1] = (unsigned char) ( (n) >> 16 );       \
    (b)[(i) + 2] = (unsigned char) ( (n) >>  8 );       \
    (b)[(i) + 3] = (unsigned char) ( (n)       );       \
} while( 0 )
#endif

#if !defined(MBEDTLS_SM4_SETKEY_ENC_ALT) || !defined(MBEDTLS_SM4_SETKEY_DEC_ALT)

/* The system parameters FK */
static const uint32_t FK[4] = {0xa3b1bac6, 0x56aa3350, 0x677d9197, 0xb27022dc};

/* The const parameters CK */
static const uint32_t CK[32] = {
    0x00070E15, 0x1C232A31, 0x383F464D, 0x545B6269, 0x70777E85, 0x8C939AA1,
    0xA8AFB6BD, 0xC4CBD2D9, 0xE0E7EEF5, 0xFC030A11, 0x181F262D, 0x343B4249,
    0x50575E65, 0x6C737A81, 0x888F969D, 0xA4ABB2B9, 0xC0C7CED5, 0xDCE3EAF1,
    0xF8FF060D, 0x141B2229, 0x30373E45, 0x4C535A61, 0x686F767D, 0x848B9299,
    0xA0A7AEB5, 0xBCC3CAD1, 0xD8DFE6ED, 0xF4FB0209, 0x10171E25, 0x2C333A41,
    0x484F565D, 0x646B7279};

#endif /* !MBEDTLS_SM4_SETKEY_ENC_ALT || !MBEDTLS_SM4_SETKEY_DEC_ALT*/

/* The SM4 Sbox */
static const unsigned char Sbox[256] = {
    0xD6, 0x90, 0xE9, 0xFE, 0xCC, 0xE1, 0x3D, 0xB7, 0x16, 0xB6, 0x14, 0xC2,
    0x28, 0xFB, 0x2C, 0x05, 0x2B, 0x67, 0x9A, 0x76, 0x2A, 0xBE, 0x04, 0xC3,
    0xAA, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99, 0x9C, 0x42, 0x50, 0xF4,
    0x91, 0xEF, 0x98, 0x7A, 0x33, 0x54, 0x0B, 0x43, 0xED, 0xCF, 0xAC, 0x62,
    0xE4, 0xB3, 0x1C, 0xA9, 0xC9, 0x08, 0xE8, 0x95, 0x80, 0xDF, 0x94, 0xFA,
    0x75, 0x8F, 0x3F, 0xA6, 0x47, 0x07, 0xA7, 0xFC, 0xF3, 0x73, 0x17, 0xBA,
    0x83, 0x59, 0x3C, 0x19, 0xE6, 0x85, 0x4F, 0xA8, 0x68, 0x6B, 0x81, 0xB2,
    0x71, 0x64, 0xDA, 0x8B, 0xF8, 0xEB, 0x0F, 0x4B, 0x70, 0x56, 0x9D, 0x35,
    0x1E, 0x24, 0x0E, 0x5E, 0x63, 0x58, 0xD1, 0xA2, 0x25, 0x22, 0x7C, 0x3B,
    0x01, 0x21, 0x78, 0x87, 0xD4, 0x00, 0x46, 0x57, 0x9F, 0xD3, 0x27, 0x52,
    0x4C, 0x36, 0x02, 0xE7, 0xA0, 0xC4, 0xC8, 0x9E, 0xEA, 0xBF, 0x8A, 0xD2,
    0x40, 0xC7, 0x38, 0xB5, 0xA3, 0xF7, 0xF2, 0xCE, 0xF9, 0x61, 0x15, 0xA1,
    0xE0, 0xAE, 0x5D, 0xA4, 0x9B, 0x34, 0x1A, 0x55, 0xAD, 0x93, 0x32, 0x30,
    0xF5, 0x8C, 0xB1, 0xE3, 0x1D, 0xF6, 0xE2, 0x2E, 0x82, 0x66, 0xCA, 0x60,
    0xC0, 0x29, 0x23, 0xAB, 0x0D, 0x53, 0x4E, 0x6F, 0xD5, 0xDB, 0x37, 0x45,
    0xDE, 0xFD, 0x8E, 0x2F, 0x03, 0xFF, 0x6A, 0x72, 0x6D, 0x6C, 0x5B, 0x51,
    0x8D, 0x1B, 0xAF, 0x92, 0xBB, 0xDD, 0xBC, 0x7F, 0x11, 0xD9, 0x5C, 0x41,
    0x1F, 0x10, 0x5A, 0xD8, 0x0A, 0xC1, 0x31, 0x88, 0xA5, 0xCD, 0x7B, 0xBD,
    0x2D, 0x74, 0xD0, 0x12, 0xB8, 0xE5, 0xB4, 0xB0, 0x89, 0x69, 0x97, 0x4A,
    0x0C, 0x96, 0x77, 0x7E, 0x65, 0xB9, 0xF1, 0x09, 0xC5, 0x6E, 0xC6, 0x84,
    0x18, 0xF0, 0x7D, 0xEC, 0x3A, 0xDC, 0x4D, 0x20, 0x79, 0xEE, 0x5F, 0x3E,
    0xD7, 0xCB, 0x39, 0x48};

/* Function to call Sbox */
static inline uint32_t SBOX(uint32_t A)
{
    uint32_t B = 0;

    B = (((uint32_t)(Sbox[(A >> 24) & 0xFF]) << 24) |
         ((uint32_t)(Sbox[(A >> 16) & 0xFF]) << 16) |
         ((uint32_t)(Sbox[(A >> 8) & 0xFF]) << 8) |
         ((uint32_t)(Sbox[(A >> 0) & 0xFF]) << 0));
    return B;
}

#define ROTL(x,n) (((x) << (n)) | (((x) & 0xFFFFFFFF) >> (32 - (n))))

/* Function L: linear transform */
#define L(B)    \
    ((B) ^ ROTL(B, 2) ^ ROTL(B, 10) ^ ROTL(B, 18) ^ ROTL(B, 24))

/* Function L' */
#define L_(B)    \
    ((B) ^ ROTL(B, 13) ^ ROTL(B, 23))

/* The T function */
static inline uint32_t T(uint32_t A)
{
    uint32_t B;

    B = SBOX(A);
    return (L(B));
}
/* The T' function */
static inline uint32_t T_(uint32_t A)
{
    uint32_t B;

    B = SBOX(A);
    return (L_(B));
}

/* Function F */
#define F(x0, x1, x2, x3, rk)   \
    ((x0) ^ T((x1) ^ (x2) ^ (x3) ^ rk))

/**
 * This function calculate K[i] where i = 0 ~ 31
 * rk[i] = K[i + 4], so K[i] = rk[i - 4] for i >= 4.
 */
static inline uint32_t KI(uint32_t i, uint32_t *K, uint32_t *rk)
{
    if (i < 4) {
        return K[i];
    } else {
        return rk[i - 4];
    }
}

/**
 * This function calculate X[i] where i = 0 ~ 31
 * X[i + 4] = F(X[i], X[i + 1], X[i + 2], X[i + 3], rk[i])
 */
static inline uint32_t XI(uint32_t i, uint32_t *X, uint32_t *rk)
{
    if (i < 4) {
        return X[i];
    } else {
        return F(X[i - 4], X[i - 3], X[i - 2], X[i - 1], rk[i - 4]);
    }
}

void mbedtls_sm4_init( mbedtls_sm4_context *ctx )
{
    SM4_VALIDATE( ctx != NULL );

    memset( ctx, 0, sizeof( mbedtls_sm4_context ) );
}

void mbedtls_sm4_free( mbedtls_sm4_context *ctx )
{
    if( ctx == NULL )
        return;

    mbedtls_platform_zeroize( ctx, sizeof( mbedtls_sm4_context ) );
}

int mbedtls_sm4_clone( mbedtls_sm4_context *dst,
                       const mbedtls_sm4_context *src )
{
    SM4_VALIDATE( dst != NULL );
    SM4_VALIDATE( src != NULL );

    *dst = *src;
    return ( 0 );
}

#if defined(MBEDTLS_CIPHER_MODE_XTS)
void mbedtls_sm4_xts_init( mbedtls_sm4_xts_context *ctx )
{
    (void)ctx;
}

void mbedtls_sm4_xts_free( mbedtls_sm4_xts_context *ctx )
{
    (void)ctx;
}

int mbedtls_sm4_xts_clone( mbedtls_sm4_xts_context *dst,
                           const mbedtls_sm4_xts_context *src )
{
    (void)dst;
    (void)src;
    return MBEDTLS_ERR_SM4_FEATURE_UNAVAILABLE;
}
#endif /* MBEDTLS_CIPHER_MODE_XTS */

#if !defined(MBEDTLS_SM4_SETKEY_ENC_ALT) && !defined(MBEDTLS_SM4_SETKEY_DEC_ALT)
/**
 * Software implementation of keyladder algorithm using SM4-ECB
 * All of secure_key, EK1 and EK2 size MUST be 128 bits, EK3 bits can be 128 or
 * 256 bits. The outkey size equals to ek3_bits.
 */
static int mbedtls_sm4_keyladder_kdf(unsigned char *outkey,
                                     const unsigned char *secure_key,
                                     const unsigned char *ek1,
                                     const unsigned char *ek2,
                                     const unsigned char *ek3,
                                     size_t ek3_bits)
{
    int ret;
    mbedtls_sm4_context ctx;
    unsigned char tmp_buf[32];

    SM4_VALIDATE_RET( outkey != NULL );
    SM4_VALIDATE_RET( secure_key != NULL );
    SM4_VALIDATE_RET( ek1 != NULL );
    SM4_VALIDATE_RET( ek2 != NULL );
    SM4_VALIDATE_RET( ek3 != NULL );
    SM4_VALIDATE_RET( ek3_bits == 128 || ek3_bits == 256 );

    /* 1st dec */
    mbedtls_sm4_init( &ctx );
    if( ( ret = mbedtls_sm4_setkey_dec( &ctx, secure_key, 128 ) ) != 0 )
        goto exit;
    if ( ( ret = mbedtls_sm4_crypt_ecb( &ctx, MBEDTLS_SM4_DECRYPT, ek1,
                                        tmp_buf ) ) != 0 )
        goto exit;
    mbedtls_sm4_free( &ctx );

    /* 2nd dec */
    mbedtls_sm4_init( &ctx );
    if( ( ret = mbedtls_sm4_setkey_dec( &ctx, tmp_buf, 128 ) ) != 0 )
        goto exit;
    if ( ( ret = mbedtls_sm4_crypt_ecb( &ctx, MBEDTLS_SM4_DECRYPT, ek2,
                                        tmp_buf ) ) != 0 )
        goto exit;
    mbedtls_sm4_free( &ctx );

    /* 3rd dec */
    mbedtls_sm4_init( &ctx );
    if( ( ret = mbedtls_sm4_setkey_dec( &ctx, tmp_buf, 128 ) ) != 0 )
        goto exit;
    if ( ( ret = mbedtls_sm4_crypt_ecb( &ctx, MBEDTLS_SM4_DECRYPT, ek3,
                                        tmp_buf ) ) != 0 )
        goto exit;
    if ( ek3_bits == 256 ) {
        if ( ( ret = mbedtls_sm4_crypt_ecb( &ctx, MBEDTLS_SM4_DECRYPT, ek3 + 16,
                                        tmp_buf + 16 ) ) != 0 )
        goto exit;
    }

    /* copy to outkey */
    memcpy( outkey, tmp_buf, ek3_bits / 8 );

exit:
    mbedtls_sm4_free( &ctx );
    return( ret );
}
#endif /* !MBEDTLS_SM4_SETKEY_ENC_ALT && !MBEDTLS_SM4_SETKEY_DEC_ALT */

/*
 * SM4 key schedule (encryption)
 */
#if !defined(MBEDTLS_SM4_SETKEY_ENC_ALT)
int mbedtls_sm4_setkey_enc( mbedtls_sm4_context *ctx, const unsigned char *key,
                            unsigned int keybits )
{
    unsigned int i;
    uint32_t K[4];

    SM4_VALIDATE_RET( ctx != NULL );
    SM4_VALIDATE_RET( key != NULL );
    SM4_VALIDATE_RET( keybits == 128 );

    /* init MK0, MK1, MK2, MK3. use variable Ki to save MKi */
    GET_UINT32_BE(K[0], key, 0);
    GET_UINT32_BE(K[1], key, 4);
    GET_UINT32_BE(K[2], key, 8);
    GET_UINT32_BE(K[3], key, 12);

    /* init K0, K1, K2, K3 */
    for (i = 0; i < 4; i++) {
        K[i] = K[i] ^ FK[i];
    }

    /**
     * Init rk[i]:
     * rk[i] = K[i + 4] = K[i] ^ T_(K[i + 1] ^ K[i + 2] ^ K[i + 3] ^ CK[i])
     * Note that: K[i + 4] == rk[i], so:
     **/

    for (i = 0; i < 32; i++) {
#if 0
        K[i + 4] = K[i + 1] ^ K[i + 2] ^ K[i + 3] ^ CK[i];
        K[i + 4]   = T_(K[i + 4]);
        K[i + 4]   = K[i] ^ K[i + 4];
#else
        ctx->rk[i] = KI(i, K, ctx->rk) ^
                     T_(KI(i + 1, K, ctx->rk) ^ KI(i + 2, K, ctx->rk) ^
                        KI(i + 3, K, ctx->rk) ^ CK[i]);
#endif
    }
#if 0
    for (i = 0; i < 32; i++) {
        ctx->rk[i] = K[i + 4];
    }
#endif
    return( 0 );
}

/**
 * Software implementation of mbedtls_sm4_setseckey_enc, just for reference.
 * On device, it's recomanded to use ALT implementation of this function.
 *
 * \note This software implementation reads root key and model key from
 * two files:
 *
 * .root_key.bin
 * .model_key.bin
 * Both root key and model key MUST have size of 16 bytes.
 *
 */
#define ROOT_KEY_FILE   ".root_key.bin"
#define MODEL_KEY_FILE  ".model_key.bin"
int mbedtls_sm4_setseckey_enc( mbedtls_sm4_context *ctx, mbedtls_sm4_sec_key_t *key )
{
    int ret;
    unsigned char secure_key[16];
    unsigned char keyladder_key[32];
#if defined(MBEDTLS_FS_IO)
    FILE *f;
#endif

    SM4_VALIDATE_RET( ctx != NULL );
    SM4_VALIDATE_RET( key != NULL );
    SM4_VALIDATE_RET( key->ek3bits == 128 );

#if defined(MBEDTLS_FS_IO)
    switch ( key->sel )
    {
        case MBEDTLS_SM4_KL_KEY_MODEL: {
            if( ( f = fopen( MODEL_KEY_FILE, "rb" ) ) == NULL ) {
                ret = ( MBEDTLS_ERR_SM4_BAD_INPUT_DATA );
                break;
            }
            if( fread( secure_key, 1, 16, f ) != 16 ) {
                ret = ( MBEDTLS_ERR_SM4_INVALID_KEY_LENGTH );
                break;
            }
            fclose( f );
            ret = 0;
            break;
        }
        case MBEDTLS_SM4_KL_KEY_ROOT: {
            if( ( f = fopen( ROOT_KEY_FILE, "rb" ) ) == NULL ) {
                ret = ( MBEDTLS_ERR_SM4_BAD_INPUT_DATA );
                break;
            }
            if( fread( secure_key, 1, 16, f ) != 16 ) {
                ret = ( MBEDTLS_ERR_SM4_INVALID_KEY_LENGTH );
                break;
            }
            fclose( f );
            ret = 0;
            break;
        }
        default : return( MBEDTLS_ERR_SM4_INVALID_KEY_LENGTH );
    }
#else
    ret = MBEDTLS_ERR_SM4_BAD_INPUT_DATA;
#endif

    if( ret )
        goto exit;

    /* call keyladder to derive key */
    if ( ( ret = mbedtls_sm4_keyladder_kdf( keyladder_key,
                                            (const unsigned char *)secure_key,
                                            (const unsigned char *)(key->ek1),
                                            (const unsigned char *)(key->ek2),
                                            (const unsigned char *)(key->ek3),
                                            key->ek3bits ) ) != 0 )
        goto exit;

    if ( ( ret = mbedtls_sm4_setkey_enc( ctx,
                                         (const unsigned char *)keyladder_key,
                                         key->ek3bits ) ) != 0 )
        goto exit;

exit:
    return( ret );
}

#endif /* !MBEDTLS_SM4_SETKEY_ENC_ALT */

/*
 * SM4 key schedule (decryption)
 */
#if !defined(MBEDTLS_SM4_SETKEY_DEC_ALT)
int mbedtls_sm4_setkey_dec( mbedtls_sm4_context *ctx, const unsigned char *key,
                    unsigned int keybits )
{
    int ret;
    int i;
    uint32_t tmp;

    SM4_VALIDATE_RET( ctx != NULL );
    SM4_VALIDATE_RET( key != NULL );
    SM4_VALIDATE_RET( keybits == 128 );

    if ( ( ret = mbedtls_sm4_setkey_enc( ctx, key, keybits ) ) != 0)
        goto exit;

    /* invert rk 0 - 31 */
    for (i = 0; i < 16; i++) {
        tmp = ctx->rk[32 - i - 1];
        ctx->rk[32 - i - 1] = ctx->rk[i];
        ctx->rk[i]      = tmp;
    }

exit:
    return( ret );
}

/**
 * Software implementation of mbedtls_sm4_setseckey_dec, just for reference.
 * On device, it's recomanded to use ALT implementation of this function.
 *
 * \note This software implementation reads root key and model key from
 * two files:
 *
 * root_key.bin
 * model_key.bin
 * Both root key and model key MUST have size of 16 bytes.
 *
 */
int mbedtls_sm4_setseckey_dec( mbedtls_sm4_context *ctx, mbedtls_sm4_sec_key_t *key )
{
    int ret;
    unsigned char secure_key[16];
    unsigned char keyladder_key[32];
#if defined(MBEDTLS_FS_IO)
    FILE *f;
#endif

    SM4_VALIDATE_RET( ctx != NULL );
    SM4_VALIDATE_RET( key != NULL );
    SM4_VALIDATE_RET( key->ek3bits == 128 );

#if defined(MBEDTLS_FS_IO)
    switch ( key->sel )
    {
        case MBEDTLS_SM4_KL_KEY_MODEL: {
            if( ( f = fopen( MODEL_KEY_FILE, "rb" ) ) == NULL ) {
                ret = ( MBEDTLS_ERR_SM4_BAD_INPUT_DATA );
                break;
            }
            if( fread( secure_key, 1, 16, f ) != 16 ) {
                ret = ( MBEDTLS_ERR_SM4_INVALID_KEY_LENGTH );
                break;
            }
            fclose( f );
            ret = 0;
            break;
        }
        case MBEDTLS_SM4_KL_KEY_ROOT: {
            if( ( f = fopen( ROOT_KEY_FILE, "rb" ) ) == NULL ) {
                ret = ( MBEDTLS_ERR_SM4_BAD_INPUT_DATA );
                break;
            }
            if( fread( secure_key, 1, 16, f ) != 16 ) {
                ret = ( MBEDTLS_ERR_SM4_INVALID_KEY_LENGTH );
                break;
            }
            fclose( f );
            ret = 0;
            break;
        }
        default : return( MBEDTLS_ERR_SM4_INVALID_KEY_LENGTH );
    }
#else
    ret = MBEDTLS_ERR_SM4_BAD_INPUT_DATA;
#endif

    if( ret )
        goto exit;

    /* call keyladder to derive key */
    if ( ( ret = mbedtls_sm4_keyladder_kdf( keyladder_key,
                                            (const unsigned char *)secure_key,
                                            (const unsigned char *)(key->ek1),
                                            (const unsigned char *)(key->ek2),
                                            (const unsigned char *)(key->ek3),
                                            key->ek3bits ) ) != 0 )
        goto exit;

    if ( ( ret = mbedtls_sm4_setkey_dec( ctx,
                                         (const unsigned char *)keyladder_key,
                                         key->ek3bits ) ) != 0 )
        goto exit;

exit:
    return( ret );
}

#if defined(MBEDTLS_CIPHER_MODE_XTS)
int mbedtls_sm4_xts_setkey_enc( mbedtls_sm4_xts_context *ctx,
                                const unsigned char *key,
                                unsigned int keybits)
{
    (void)ctx;
    (void)key;
    (void)keybits;
    return MBEDTLS_ERR_SM4_FEATURE_UNAVAILABLE;
}

int mbedtls_sm4_xts_setseckey_enc( mbedtls_sm4_xts_context *ctx,
                                   mbedtls_sm4_sec_key_t *key1,
                                   mbedtls_sm4_sec_key_t *key2 )
{
    (void)ctx;
    (void)key1;
    (void)key2;
    return MBEDTLS_ERR_SM4_FEATURE_UNAVAILABLE;
}

int mbedtls_sm4_xts_setkey_dec( mbedtls_sm4_xts_context *ctx,
                                const unsigned char *key,
                                unsigned int keybits)
{
    (void)ctx;
    (void)key;
    (void)keybits;
    return MBEDTLS_ERR_SM4_FEATURE_UNAVAILABLE;
}

int mbedtls_sm4_xts_setseckey_dec( mbedtls_sm4_xts_context *ctx,
                                   mbedtls_sm4_sec_key_t *key1,
                                   mbedtls_sm4_sec_key_t *key2 )
{
    (void)ctx;
    (void)key1;
    (void)key2;
    return MBEDTLS_ERR_SM4_FEATURE_UNAVAILABLE;
}

#endif /* MBEDTLS_CIPHER_MODE_XTS */

#endif /* MBEDTLS_SM4_SETKEY_DEC_ALT */

/*
 * SM4-ECB block encryption
 */
#if !defined(MBEDTLS_SM4_ENCRYPT_ALT)
int mbedtls_internal_sm4_encrypt( mbedtls_sm4_context *ctx,
                                  const unsigned char input[16],
                                  unsigned char output[16] )
{
    int i;
    uint32_t X[36];

    GET_UINT32_BE( X[0], input,  0 );
    GET_UINT32_BE( X[1], input,  4 );
    GET_UINT32_BE( X[2], input,  8 );
    GET_UINT32_BE( X[3], input,  12 );

    for (i = 0; i < 36; i++) {
        X[i] = XI(i, X, ctx->rk);
    }

    PUT_UINT32_BE( X[35], output,  0 );
    PUT_UINT32_BE( X[34], output,  4 );
    PUT_UINT32_BE( X[33], output,  8 );
    PUT_UINT32_BE( X[32], output,  12 );

    return( 0 );
}
#endif /* MBEDTLS_SM4_ENCRYPT_ALT */

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sm4_encrypt( mbedtls_sm4_context *ctx,
                          const unsigned char input[16],
                          unsigned char output[16] )
{
    mbedtls_internal_sm4_encrypt( ctx, input, output );
}
#endif /* !MBEDTLS_DEPRECATED_REMOVED */

/*
 * SM4-ECB block decryption
 */
#if !defined(MBEDTLS_SM4_DECRYPT_ALT)
int mbedtls_internal_sm4_decrypt( mbedtls_sm4_context *ctx,
                                  const unsigned char input[16],
                                  unsigned char output[16] )
{
    return mbedtls_internal_sm4_encrypt( ctx, input, output );
}
#endif /* MBEDTLS_SM4_DECRYPT_ALT */

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sm4_decrypt( mbedtls_sm4_context *ctx,
                          const unsigned char input[16],
                          unsigned char output[16] )
{
    mbedtls_internal_sm4_decrypt( ctx, input, output );
}
#endif /* !MBEDTLS_DEPRECATED_REMOVED */

/*
 * SM4-ECB block encryption/decryption
 */
int mbedtls_sm4_crypt_ecb( mbedtls_sm4_context *ctx,
                           int mode,
                           const unsigned char input[16],
                           unsigned char output[16] )
{
    SM4_VALIDATE_RET( ctx != NULL );
    SM4_VALIDATE_RET( input != NULL );
    SM4_VALIDATE_RET( output != NULL );
    SM4_VALIDATE_RET( mode == MBEDTLS_SM4_ENCRYPT ||
                      mode == MBEDTLS_SM4_DECRYPT );

    if( mode == MBEDTLS_SM4_ENCRYPT )
        return( mbedtls_internal_sm4_encrypt( ctx, input, output ) );
    else
        return( mbedtls_internal_sm4_decrypt( ctx, input, output ) );
}

#if defined(MBEDTLS_CIPHER_MODE_CBC)
/*
 * SM4-CBC buffer encryption/decryption
 */
int mbedtls_sm4_crypt_cbc( mbedtls_sm4_context *ctx,
                    int mode,
                    size_t length,
                    unsigned char iv[16],
                    const unsigned char *input,
                    unsigned char *output )
{
    int i;
    unsigned char temp[16];

    SM4_VALIDATE_RET( ctx != NULL );
    SM4_VALIDATE_RET( mode == MBEDTLS_SM4_ENCRYPT ||
                      mode == MBEDTLS_SM4_DECRYPT );
    SM4_VALIDATE_RET( iv != NULL );
    SM4_VALIDATE_RET( input != NULL );
    SM4_VALIDATE_RET( output != NULL );

    if( length % 16 )
        return( MBEDTLS_ERR_SM4_INVALID_INPUT_LENGTH );

    if( mode == MBEDTLS_SM4_DECRYPT )
    {
        while( length > 0 )
        {
            memcpy( temp, input, 16 );
            mbedtls_sm4_crypt_ecb( ctx, mode, input, output );

            for( i = 0; i < 16; i++ )
                output[i] = (unsigned char)( output[i] ^ iv[i] );

            memcpy( iv, temp, 16 );

            input  += 16;
            output += 16;
            length -= 16;
        }
    }
    else
    {
        while( length > 0 )
        {
            for( i = 0; i < 16; i++ )
                output[i] = (unsigned char)( input[i] ^ iv[i] );

            mbedtls_sm4_crypt_ecb( ctx, mode, output, output );
            memcpy( iv, output, 16 );

            input  += 16;
            output += 16;
            length -= 16;
        }
    }

    return( 0 );
}
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_XTS)
/*
 * SM4-XTS buffer encryption/decryption
 */
int mbedtls_sm4_crypt_xts( mbedtls_sm4_xts_context *ctx,
                           int mode,
                           size_t length,
                           unsigned char data_unit[16],
                           const unsigned char *input,
                           unsigned char *output )
{
    (void)ctx;
    (void)mode;
    (void)length;
    (void)data_unit;
    (void)input;
    (void)output;
    return MBEDTLS_ERR_SM4_FEATURE_UNAVAILABLE;
}
#endif /* MBEDTLS_CIPHER_MODE_XTS */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
/*
 * SM4-CFB128 buffer encryption/decryption
 */
int mbedtls_sm4_crypt_cfb128( mbedtls_sm4_context *ctx,
                       int mode,
                       size_t length,
                       size_t *iv_off,
                       unsigned char iv[16],
                       const unsigned char *input,
                       unsigned char *output )
{
    (void)ctx;
    (void)mode;
    (void)length;
    (void)iv_off;
    (void)iv;
    (void)input;
    (void)output;
    return MBEDTLS_ERR_SM4_FEATURE_UNAVAILABLE;
}

/*
 * SM4-CFB8 buffer encryption/decryption
 */
int mbedtls_sm4_crypt_cfb8( mbedtls_sm4_context *ctx,
                            int mode,
                            size_t length,
                            unsigned char iv[16],
                            const unsigned char *input,
                            unsigned char *output )
{
    (void)ctx;
    (void)mode;
    (void)length;
    (void)iv;
    (void)input;
    (void)output;
    return MBEDTLS_ERR_SM4_FEATURE_UNAVAILABLE;
}
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_OFB)
/*
 * SM4-OFB (Output Feedback Mode) buffer encryption/decryption
 */
int mbedtls_sm4_crypt_ofb( mbedtls_sm4_context *ctx,
                           size_t length,
                           size_t *iv_off,
                           unsigned char iv[16],
                           const unsigned char *input,
                           unsigned char *output )
{
    (void)ctx;
    (void)length;
    (void)iv_off;
    (void)iv;
    (void)input;
    (void)output;
    return MBEDTLS_ERR_SM4_FEATURE_UNAVAILABLE;
}
#endif /* MBEDTLS_CIPHER_MODE_OFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
/*
 * SM4-CTR buffer encryption/decryption
 */
int mbedtls_sm4_crypt_ctr( mbedtls_sm4_context *ctx,
                       size_t length,
                       size_t *nc_off,
                       unsigned char nonce_counter[16],
                       unsigned char stream_block[16],
                       const unsigned char *input,
                       unsigned char *output )
{
    int c, i;
    size_t n;

    SM4_VALIDATE_RET( ctx != NULL );
    SM4_VALIDATE_RET( nc_off != NULL );
    SM4_VALIDATE_RET( nonce_counter != NULL );
    SM4_VALIDATE_RET( stream_block != NULL );
    SM4_VALIDATE_RET( input != NULL );
    SM4_VALIDATE_RET( output != NULL );

    n = *nc_off;

    if ( n > 0x0F )
        return( MBEDTLS_ERR_SM4_BAD_INPUT_DATA );

    while( length-- )
    {
        if( n == 0 ) {
            mbedtls_sm4_crypt_ecb( ctx, MBEDTLS_SM4_ENCRYPT, nonce_counter, stream_block );

            for( i = 16; i > 0; i-- )
                if( ++nonce_counter[i - 1] != 0 )
                    break;
        }
        c = *input++;
        *output++ = (unsigned char)( c ^ stream_block[n] );

        n = ( n + 1 ) & 0x0F;
    }

    *nc_off = n;

    return( 0 );
}
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#endif /* !MBEDTLS_SM4_ALT */

#if defined(MBEDTLS_SELF_TEST)
static const unsigned char sm4_ecb_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
static const unsigned char sm4_key[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
static const unsigned char sm4_ecb_ciphertext[16] = {
    0x68, 0x1E, 0xDF, 0x34, 0xD2, 0x06, 0x96, 0x5E,
    0x86, 0xB3, 0xE9, 0x4F, 0x53, 0x6E, 0x42, 0x46};

static const unsigned char sm4_ecb_1000000_ciphertext[16] = {
    0x59, 0x52, 0x98, 0xC7, 0xC6, 0xFD, 0x27, 0x1F,
    0x04, 0x02, 0xF8, 0x04, 0xC3, 0x3D, 0x3F, 0x66};

#define SEC_SM4_KEY_128                                                        \
    (uint8_t                                                                   \
         *)"\xD4\xCC\x0E\x78\x59\x8B\xEF\x73\x91\x84\xA2\xAC\x91\x9A\x70\x87"  \
           "\x34\xE7\x9C\xD5\x4B\x9D\x15\xC2\xF0\x6A\xB8\x5E\x28\xB8\xF4\xE7"  \
           "\xF4\xC5\x89\xFA\x1F\x3E\xC8\xFE\x3F\xD2\x75\x17\xC8\xF2\x21\xA1"
#define KEY_128                                                                \
    (uint8_t                                                                   \
         *)"\x3B\xE0\x2B\xC3\x15\x76\xBC\x1E\x78\x33\x30\xAE\xB1\xAE\xEB\xC8"
#define SEC_KEK                                                                \
    (uint8_t                                                                   \
         *)"\xF2\x32\x28\xF9\x89\x0E\xDC\xC4\xC2\x32\x72\xE1\x12\xA8\x2B\x89"

/*
 * SM4 test vectors
 */
static const unsigned char sm4_test_ecb_dec[1][16] =
{
    { 0xe1, 0x17, 0xfe, 0x4c, 0x7b, 0x27, 0x34, 0xc3,
      0x73, 0x81, 0xcc, 0xa9, 0xc0, 0x37, 0x2c, 0x68 }
};

static const unsigned char sm4_test_ecb_enc[1][16] =
{
    { 0xcb, 0xc9, 0x96, 0x84, 0x04, 0xb0, 0xb6, 0x15,
      0xa4, 0x43, 0xf0, 0x74, 0xbc, 0xd0, 0x81, 0x1b }
};

#if defined(MBEDTLS_CIPHER_MODE_CBC)
/*
 * SM4 CBC test vectors
 */
static const unsigned char sm4_test_cbc_dec[1][16] =
{
    { 0x27, 0x17, 0x0e, 0x86, 0x9a, 0x04, 0xcc, 0xc5,
      0x78, 0x29, 0xa1, 0x28, 0xf1, 0x27, 0x8c, 0x23 }
};

static const unsigned char sm4_test_cbc_enc[1][16] =
{
    { 0xee, 0xd7, 0xc3, 0x05, 0x7b, 0x17, 0x2a, 0xf7,
      0xcf, 0x9a, 0xd8, 0x06, 0x32, 0x36, 0xf4, 0x78 }
};
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
/*
 * SM4-CFB128 test vectors
 */
static const unsigned char sm4_test_cfb128_key[1][16] =
{
    { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
      0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C },
};

static const unsigned char sm4_test_cfb128_iv[16] =
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

static const unsigned char sm4_test_cfb128_pt[64] =
{
    0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96,
    0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17, 0x2A,
    0xAE, 0x2D, 0x8A, 0x57, 0x1E, 0x03, 0xAC, 0x9C,
    0x9E, 0xB7, 0x6F, 0xAC, 0x45, 0xAF, 0x8E, 0x51,
    0x30, 0xC8, 0x1C, 0x46, 0xA3, 0x5C, 0xE4, 0x11,
    0xE5, 0xFB, 0xC1, 0x19, 0x1A, 0x0A, 0x52, 0xEF,
    0xF6, 0x9F, 0x24, 0x45, 0xDF, 0x4F, 0x9B, 0x17,
    0xAD, 0x2B, 0x41, 0x7B, 0xE6, 0x6C, 0x37, 0x10
};

static const unsigned char sm4_test_cfb128_ct[1][64] =
{
    { 0xbc, 0x71, 0x0d, 0x76, 0x2d, 0x07, 0x0b, 0x26,
      0x36, 0x1d, 0xa8, 0x2b, 0x54, 0x56, 0x5e, 0x46,
      0xa4, 0xcd, 0x42, 0x78, 0x6a, 0x3a, 0x52, 0x93,
      0xa3, 0xc6, 0xcb, 0xc1, 0x23, 0xf0, 0xb3, 0x54,
      0x40, 0x70, 0x55, 0xb1, 0xc1, 0xa5, 0xd9, 0x98,
      0x2c, 0x18, 0x7d, 0x5c, 0x3e, 0xe0, 0xce, 0xd8,
      0x4b, 0x82, 0xc4, 0x0f, 0x2f, 0x0a, 0x4e, 0x03,
      0x41, 0x79, 0x7f, 0x1f, 0x30, 0x7b, 0x80, 0x47 }
};
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_OFB)
/*
 * SM4-OFB test vectors
 */
static const unsigned char sm4_test_ofb_key[1][16] =
{
    { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
      0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C }
};

static const unsigned char sm4_test_ofb_iv[16] =
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

static const unsigned char sm4_test_ofb_pt[64] =
{
    0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96,
    0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17, 0x2A,
    0xAE, 0x2D, 0x8A, 0x57, 0x1E, 0x03, 0xAC, 0x9C,
    0x9E, 0xB7, 0x6F, 0xAC, 0x45, 0xAF, 0x8E, 0x51,
    0x30, 0xC8, 0x1C, 0x46, 0xA3, 0x5C, 0xE4, 0x11,
    0xE5, 0xFB, 0xC1, 0x19, 0x1A, 0x0A, 0x52, 0xEF,
    0xF6, 0x9F, 0x24, 0x45, 0xDF, 0x4F, 0x9B, 0x17,
    0xAD, 0x2B, 0x41, 0x7B, 0xE6, 0x6C, 0x37, 0x10
};

static const unsigned char sm4_test_ofb_ct[1][64] =
{
    { 0xbc, 0x71, 0x0d, 0x76, 0x2d, 0x07, 0x0b, 0x26,
      0x36, 0x1d, 0xa8, 0x2b, 0x54, 0x56, 0x5e, 0x46,
      0x07, 0xa0, 0xc6, 0x28, 0x34, 0x74, 0x0a, 0xd3,
      0x24, 0x0d, 0x23, 0x91, 0x25, 0xe1, 0x16, 0x21,
      0xd4, 0x76, 0xb2, 0x1c, 0xc9, 0xf0, 0x49, 0x51,
      0xf0, 0x74, 0x1d, 0x2e, 0xf9, 0xe0, 0x94, 0x98,
      0x15, 0x84, 0xfc, 0x14, 0x2b, 0xf1, 0x3a, 0xa6,
      0x26, 0xb8, 0x2f, 0x9d, 0x7d, 0x07, 0x6c, 0xce }
};
#endif /* MBEDTLS_CIPHER_MODE_OFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
/*
 * SM4-CTR test vectors
 */

static const unsigned char sm4_test_ctr_key[3][16] =
{
    { 0xAE, 0x68, 0x52, 0xF8, 0x12, 0x10, 0x67, 0xCC,
      0x4B, 0xF7, 0xA5, 0x76, 0x55, 0x77, 0xF3, 0x9E },
    { 0x7E, 0x24, 0x06, 0x78, 0x17, 0xFA, 0xE0, 0xD7,
      0x43, 0xD6, 0xCE, 0x1F, 0x32, 0x53, 0x91, 0x63 },
    { 0x76, 0x91, 0xBE, 0x03, 0x5E, 0x50, 0x20, 0xA8,
      0xAC, 0x6E, 0x61, 0x85, 0x29, 0xF9, 0xA0, 0xDC }
};

static const unsigned char sm4_test_ctr_nonce_counter[3][16] =
{
    { 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
    { 0x00, 0x6C, 0xB6, 0xDB, 0xC0, 0x54, 0x3B, 0x59,
      0xDA, 0x48, 0xD9, 0x0B, 0x00, 0x00, 0x00, 0x01 },
    { 0x00, 0xE0, 0x01, 0x7B, 0x27, 0x77, 0x7F, 0x3F,
      0x4A, 0x17, 0x86, 0xF0, 0x00, 0x00, 0x00, 0x01 }
};

static const unsigned char sm4_test_ctr_pt[3][48] =
{
    { 0x53, 0x69, 0x6E, 0x67, 0x6C, 0x65, 0x20, 0x62,
      0x6C, 0x6F, 0x63, 0x6B, 0x20, 0x6D, 0x73, 0x67 },

    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F },

    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
      0x20, 0x21, 0x22, 0x23 }
};

static const unsigned char sm4_test_ctr_ct[3][48] =
{
    { 0x20, 0x9b, 0x77, 0x31, 0xd3, 0x65, 0xdb, 0xab,
      0x9e, 0x48, 0x74, 0x7e, 0xbd, 0x13, 0x83, 0xeb },
    { 0x33, 0xe0, 0x28, 0x01, 0x92, 0xed, 0xc9, 0x1e,
      0x97, 0x35, 0xd9, 0x4a, 0xec, 0xd4, 0xbc, 0x23,
      0x4f, 0x35, 0x9f, 0x1c, 0x55, 0x1f, 0xe0, 0x27,
      0xe0, 0xdf, 0xc5, 0x43, 0xbc, 0xb0, 0x23, 0x94 },
    { 0xe9, 0xbf, 0xca, 0x67, 0xae, 0xe9, 0x74, 0x96,
      0x76, 0x06, 0x55, 0xe1, 0xbe, 0x30, 0x58, 0x37,
      0x0b, 0xd5, 0xaa, 0x0b, 0xe1, 0x1d, 0x25, 0x9f,
      0x74, 0xa8, 0xc1, 0xa3, 0x4e, 0x08, 0x05, 0x6a,
      0x83, 0xfd, 0x06, 0x2b }
};

static const int sm4_test_ctr_len[3] =
    { 16, 32, 36 };
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#if defined(MBEDTLS_CIPHER_MODE_XTS)
/*
 * SM4-XTS test vectors
 */
static const unsigned char sm4_test_xts_key[][32] =
{
    { 0xb2, 0x8d, 0x39, 0x08, 0x57, 0x41, 0x6f, 0x33,
      0x8c, 0xb5, 0xb7, 0xe2, 0xa4, 0x55, 0x16, 0xb0,
      0x7f, 0x51, 0x5b, 0x34, 0xa3, 0x4a, 0xa2, 0xca,
      0x9f, 0xb1, 0x3f, 0xe0, 0x95, 0xaf, 0x80, 0xba }
};

static const unsigned char sm4_test_xts_pt32[][32] =
{
    { 0xc3, 0x16, 0x3f, 0x77, 0xbb, 0x19, 0xc7, 0x65,
      0x7e, 0x43, 0x11, 0xa7, 0xbb, 0xa6, 0xe9, 0x43,
      0x62, 0x80, 0x9f, 0xad, 0xc1, 0x7e, 0xdd, 0xa3,
      0x79, 0x7d, 0x32, 0x50, 0xe6, 0xa6, 0xfd, 0xad },
};

static const unsigned char sm4_test_xts_ct32[][32] =
{
    { 0x04, 0x6e, 0x67, 0x7d, 0xaf, 0x24, 0x84, 0x01,
      0xf9, 0xe5, 0xb9, 0xe9, 0xbc, 0xbc, 0x38, 0x0f,
      0xd7, 0x13, 0xcf, 0x14, 0x55, 0x49, 0xa3, 0x11,
      0x0a, 0xa0, 0x57, 0x8e, 0xe6, 0xbb, 0xbc, 0xb4 },
};

static const unsigned char sm4_test_xts_data_unit[][16] =
{
   { 0xad, 0xca, 0xa7, 0xff, 0x79, 0xea, 0x4c, 0xf1,
     0x1d, 0x1d, 0x88, 0x4f, 0xed, 0x0b, 0x40, 0x44 },
};

#endif /* MBEDTLS_CIPHER_MODE_XTS */

/*
 * Checkup routine
 */
static int mbedtls_sm4_self_test2( int verbose )
{
    int ret = 0, i, j, u, mode;
    unsigned int keybits;
    unsigned char key[32];
    unsigned char buf[64];
    const unsigned char *sm4_tests;
#if defined(MBEDTLS_CIPHER_MODE_CBC) || defined(MBEDTLS_CIPHER_MODE_CFB)
    unsigned char iv[16];
#endif
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    unsigned char prv[16];
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR) || defined(MBEDTLS_CIPHER_MODE_CFB) || \
    defined(MBEDTLS_CIPHER_MODE_OFB)
    size_t offset;
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR) || defined(MBEDTLS_CIPHER_MODE_XTS)
    int len;
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    unsigned char nonce_counter[16];
    unsigned char stream_block[16];
#endif
    mbedtls_sm4_context ctx;

    memset( key, 0, sizeof(key) );
    mbedtls_sm4_init( &ctx );

    /*
     * ECB mode
     */
    for( i = 0; i < 2; i++ )
    {
        u = i >> 1;
        keybits = 128;
        mode = i & 1;

        if( verbose != 0 )
            mbedtls_printf( "  SM4-ECB-%3d (%s): ", keybits,
                            ( mode == MBEDTLS_SM4_DECRYPT ) ? "dec" : "enc" );

        memset( buf, 0, 16 );

        if( mode == MBEDTLS_SM4_DECRYPT )
        {
            ret = mbedtls_sm4_setkey_dec( &ctx, key, keybits );
            sm4_tests = sm4_test_ecb_dec[u];
        }
        else
        {
            ret = mbedtls_sm4_setkey_enc( &ctx, key, keybits );
            sm4_tests = sm4_test_ecb_enc[u];
        }

        /*
         * SM4-192 is an optional feature that may be unavailable when
         * there is an alternative underlying implementation i.e. when
         * MBEDTLS_SM4_ALT is defined.
         */
        if( ret == MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED && keybits == 192 )
        {
            mbedtls_printf( "skipped\n" );
            continue;
        }
        else if( ret != 0 )
        {
            goto exit;
        }

        for( j = 0; j < 10000; j++ )
        {
            ret = mbedtls_sm4_crypt_ecb( &ctx, mode, buf, buf );
            if( ret != 0 )
                goto exit;
        }

        if( memcmp( buf, sm4_tests, 16 ) != 0 )
        {
            ret = 1;
            goto exit;
        }

        if( verbose != 0 )
            mbedtls_printf( "passed\n" );
    }

    if( verbose != 0 )
        mbedtls_printf( "\n" );

#if defined(MBEDTLS_CIPHER_MODE_CBC)
    /*
     * CBC mode
     */
    for( i = 0; i < 2; i++ )
    {
        u = i >> 1;
        keybits = 128;
        mode = i & 1;

        if( verbose != 0 )
            mbedtls_printf( "  SM4-CBC-%3d (%s): ", keybits,
                            ( mode == MBEDTLS_SM4_DECRYPT ) ? "dec" : "enc" );

        memset( iv , 0, 16 );
        memset( prv, 0, 16 );
        memset( buf, 0, 16 );

        if( mode == MBEDTLS_SM4_DECRYPT )
        {
            ret = mbedtls_sm4_setkey_dec( &ctx, key, keybits );
            sm4_tests = sm4_test_cbc_dec[u];
        }
        else
        {
            ret = mbedtls_sm4_setkey_enc( &ctx, key, keybits );
            sm4_tests = sm4_test_cbc_enc[u];
        }

        /*
         * SM4-192 is an optional feature that may be unavailable when
         * there is an alternative underlying implementation i.e. when
         * MBEDTLS_SM4_ALT is defined.
         */
        if( ret == MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED && keybits == 192 )
        {
            mbedtls_printf( "skipped\n" );
            continue;
        }
        else if( ret != 0 )
        {
            goto exit;
        }

        for( j = 0; j < 10000; j++ )
        {
            if( mode == MBEDTLS_SM4_ENCRYPT )
            {
                unsigned char tmp[16];

                memcpy( tmp, prv, 16 );
                memcpy( prv, buf, 16 );
                memcpy( buf, tmp, 16 );
            }

            ret = mbedtls_sm4_crypt_cbc( &ctx, mode, 16, iv, buf, buf );
            if( ret != 0 )
                goto exit;

        }

        if( memcmp( buf, sm4_tests, 16 ) != 0 )
        {
            ret = 1;
            goto exit;
        }

        if( verbose != 0 )
            mbedtls_printf( "passed\n" );
    }

    if( verbose != 0 )
        mbedtls_printf( "\n" );
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
    /*
     * CFB128 mode
     */
    for( i = 0; i < 2; i++ )
    {
        u = i >> 1;
        keybits = 128;
        mode = i & 1;

        if( verbose != 0 )
            mbedtls_printf( "  SM4-CFB128-%3d (%s): ", keybits,
                            ( mode == MBEDTLS_SM4_DECRYPT ) ? "dec" : "enc" );

        memcpy( iv,  sm4_test_cfb128_iv, 16 );
        memcpy( key, sm4_test_cfb128_key[u], keybits / 8 );

        offset = 0;
        ret = mbedtls_sm4_setkey_enc( &ctx, key, keybits );
        /*
         * SM4-192 is an optional feature that may be unavailable when
         * there is an alternative underlying implementation i.e. when
         * MBEDTLS_SM4_ALT is defined.
         */
        if( ret == MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED && keybits == 192 )
        {
            mbedtls_printf( "skipped\n" );
            continue;
        }
        else if( ret != 0 )
        {
            goto exit;
        }

        if( mode == MBEDTLS_SM4_DECRYPT )
        {
            memcpy( buf, sm4_test_cfb128_ct[u], 64 );
            sm4_tests = sm4_test_cfb128_pt;
        }
        else
        {
            memcpy( buf, sm4_test_cfb128_pt, 64 );
            sm4_tests = sm4_test_cfb128_ct[u];
        }

        ret = mbedtls_sm4_crypt_cfb128( &ctx, mode, 64, &offset, iv, buf, buf );
        if( ret != 0 )
            goto exit;

        if( memcmp( buf, sm4_tests, 64 ) != 0 )
        {
            ret = 1;
            goto exit;
        }

        if( verbose != 0 )
            mbedtls_printf( "passed\n" );
    }

    if( verbose != 0 )
        mbedtls_printf( "\n" );
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_OFB)
    /*
     * OFB mode
     */
    for( i = 0; i < 2; i++ )
    {
        u = i >> 1;
        keybits = 128;
        mode = i & 1;

        if( verbose != 0 )
            mbedtls_printf( "  SM4-OFB-%3d (%s): ", keybits,
                            ( mode == MBEDTLS_SM4_DECRYPT ) ? "dec" : "enc" );

        memcpy( iv,  sm4_test_ofb_iv, 16 );
        memcpy( key, sm4_test_ofb_key[u], keybits / 8 );

        offset = 0;
        ret = mbedtls_sm4_setkey_enc( &ctx, key, keybits );
        /*
         * SM4-192 is an optional feature that may be unavailable when
         * there is an alternative underlying implementation i.e. when
         * MBEDTLS_SM4_ALT is defined.
         */
        if( ret == MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED && keybits == 192 )
        {
            mbedtls_printf( "skipped\n" );
            continue;
        }
        else if( ret != 0 )
        {
            goto exit;
        }

        if( mode == MBEDTLS_SM4_DECRYPT )
        {
            memcpy( buf, sm4_test_ofb_ct[u], 64 );
            sm4_tests = sm4_test_ofb_pt;
        }
        else
        {
            memcpy( buf, sm4_test_ofb_pt, 64 );
            sm4_tests = sm4_test_ofb_ct[u];
        }

        ret = mbedtls_sm4_crypt_ofb( &ctx, 64, &offset, iv, buf, buf );
        if( ret != 0 )
            goto exit;

        if( memcmp( buf, sm4_tests, 64 ) != 0 )
        {
            ret = 1;
            goto exit;
        }

        if( verbose != 0 )
            mbedtls_printf( "passed\n" );
    }

    if( verbose != 0 )
        mbedtls_printf( "\n" );
#endif /* MBEDTLS_CIPHER_MODE_OFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
    /*
     * CTR mode
     */
    for( i = 0; i < 6; i++ )
    {
        u = i >> 1;
        mode = i & 1;

        if( verbose != 0 )
            mbedtls_printf( "  SM4-CTR-128 (%s): ",
                            ( mode == MBEDTLS_SM4_DECRYPT ) ? "dec" : "enc" );

        memcpy( nonce_counter, sm4_test_ctr_nonce_counter[u], 16 );
        memcpy( key, sm4_test_ctr_key[u], 16 );

        offset = 0;
        if( ( ret = mbedtls_sm4_setkey_enc( &ctx, key, 128 ) ) != 0 )
            goto exit;

        len = sm4_test_ctr_len[u];

        if( mode == MBEDTLS_SM4_DECRYPT )
        {
            memcpy( buf, sm4_test_ctr_ct[u], len );
            sm4_tests = sm4_test_ctr_pt[u];
        }
        else
        {
            memcpy( buf, sm4_test_ctr_pt[u], len );
            sm4_tests = sm4_test_ctr_ct[u];
        }

        ret = mbedtls_sm4_crypt_ctr( &ctx, len, &offset, nonce_counter,
                                     stream_block, buf, buf );
        if( ret != 0 )
            goto exit;

        if( memcmp( buf, sm4_tests, len ) != 0 )
        {
            ret = 1;
            goto exit;
        }

        if( verbose != 0 )
            mbedtls_printf( "passed\n" );
    }

    if( verbose != 0 )
        mbedtls_printf( "\n" );
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#if defined(MBEDTLS_CIPHER_MODE_XTS)
    {
    static const int num_tests =
        sizeof(sm4_test_xts_key) / sizeof(*sm4_test_xts_key);
    mbedtls_sm4_xts_context ctx_xts;

    /*
     * XTS mode
     */
    mbedtls_sm4_xts_init( &ctx_xts );

    for( i = 0; i < num_tests << 1; i++ )
    {
        unsigned char data_unit[16] = {0};
        u = i >> 1;
        mode = i & 1;

        if( verbose != 0 )
            mbedtls_printf( "  SM4-XTS-128 (%s): ",
                            ( mode == MBEDTLS_SM4_DECRYPT ) ? "dec" : "enc" );

        memset( key, 0, sizeof( key ) );
        memcpy( key, sm4_test_xts_key[u], 32 );
        memcpy( data_unit, &sm4_test_xts_data_unit[u], sizeof(data_unit) );

        len = sizeof( *sm4_test_xts_ct32 );

        if( mode == MBEDTLS_SM4_DECRYPT )
        {
            ret = mbedtls_sm4_xts_setkey_dec( &ctx_xts, key, 256 );
            if( ret != 0)
                goto exit;
            memcpy( buf, sm4_test_xts_ct32[u], len );
            sm4_tests = sm4_test_xts_pt32[u];
        }
        else
        {
            ret = mbedtls_sm4_xts_setkey_enc( &ctx_xts, key, 256 );
            if( ret != 0)
                goto exit;
            memcpy( buf, sm4_test_xts_pt32[u], len );
            sm4_tests = sm4_test_xts_ct32[u];
        }

        ret = mbedtls_sm4_crypt_xts( &ctx_xts, mode, len, data_unit,
                                     buf, buf );
        if( ret != 0 )
            goto exit;

        if( memcmp( buf, sm4_tests, len ) != 0 )
        {
            ret = 1;
            goto exit;
        }

        if( verbose != 0 )
            mbedtls_printf( "passed\n" );
    }

    if( verbose != 0 )
        mbedtls_printf( "\n" );

    mbedtls_sm4_xts_free( &ctx_xts );
    }
#endif /* MBEDTLS_CIPHER_MODE_XTS */

    ret = 0;

exit:
    if( ret != 0 && verbose != 0 )
        mbedtls_printf( "failed\n" );

    mbedtls_sm4_free( &ctx );

    return( ret );
}

/*
 * Checkup routine
 */
int mbedtls_sm4_self_test(int verbose)
{
    int ret = 0, i;
    mbedtls_sm4_context ctx;
    int mode;
    uint8_t dst[16] = {0};

    mode = MBEDTLS_SM4_ENCRYPT;
    if (verbose != 0)
        mbedtls_printf("  SM4-ECB (%s): ",
                       (mode == MBEDTLS_SM4_DECRYPT) ? "dec" : "enc");

    mbedtls_sm4_init(&ctx);

    ret = mbedtls_sm4_setkey_enc(&ctx, (const unsigned char *)sm4_key, 128);
    if (ret) {
        mbedtls_printf("mbedtls_sm4_setkey_enc failed!\n");
        goto exit;
    }

    ret = mbedtls_sm4_crypt_ecb(&ctx, mode,
                                (const unsigned char *)sm4_ecb_plaintext, dst);
    if (ret) {
        mbedtls_printf("mbedtls_sm4_crypt_ecb failed!\n");
        goto exit;
    }

    mbedtls_sm4_free(&ctx);
    if (0 != memcmp(sm4_ecb_ciphertext, dst, 16)) {
        mbedtls_printf("SM4-ECB ENC failed!\n");
        ret = 1;
        goto exit;
    } else {
        mbedtls_printf("SM4-ECB ENC passed\n");
    }

    /* test enc 1000000 times */
    mbedtls_sm4_init(&ctx);

    ret = mbedtls_sm4_setkey_enc(&ctx, (const unsigned char *)sm4_key, 128);
    if (ret) {
        mbedtls_printf("mbedtls_sm4_setkey_enc failed!\n");
        goto exit;
    }

    memcpy(dst, sm4_ecb_plaintext, 16);
    for (i = 0; i < 1000000; i++) {
        ret =
            mbedtls_sm4_crypt_ecb(&ctx, mode, (const unsigned char *)dst, dst);
        if (ret) {
            mbedtls_printf("mbedtls_sm4_crypt_ecb failed!\n");
            goto exit;
        }
    }
    mbedtls_sm4_free(&ctx);
    if (0 != memcmp(sm4_ecb_1000000_ciphertext, dst, 16)) {
        mbedtls_printf("SM4-ECB ENC 1000000 times failed!\n");
        ret = 1;
        goto exit;
    } else {
        mbedtls_printf("SM4-ECB ENC 1000000 times passed\n");
    }

    mode = MBEDTLS_SM4_DECRYPT;
    if (verbose != 0)
        mbedtls_printf("  SM4-ECB (%s): ",
                       (mode == MBEDTLS_SM4_DECRYPT) ? "dec" : "enc");

    mbedtls_sm4_init(&ctx);

    ret = mbedtls_sm4_setkey_dec(&ctx, (const unsigned char *)sm4_key, 128);
    if (ret) {
        mbedtls_printf("mbedtls_sm4_setkey_dec failed!\n");
        goto exit;
    }

    ret = mbedtls_sm4_crypt_ecb(&ctx, mode,
                                (const unsigned char *)sm4_ecb_ciphertext, dst);
    if (ret) {
        mbedtls_printf("mbedtls_sm4_crypt_ecb failed!\n");
        goto exit;
    }

    mbedtls_sm4_free(&ctx);
    if (0 != memcmp(sm4_ecb_plaintext, dst, 16)) {
        mbedtls_printf("SM4-ECB DEC failed!\n");
        ret = 1;
        goto exit;
    } else {
        mbedtls_printf("SM4-ECB DEC passed\n");
    }

    /* test dec 1000000 times */
    mbedtls_sm4_init(&ctx);

    ret = mbedtls_sm4_setkey_dec(&ctx, (const unsigned char *)sm4_key, 128);
    if (ret) {
        mbedtls_printf("mbedtls_sm4_setkey_dec failed!\n");
        goto exit;
    }

    memcpy(dst, sm4_ecb_1000000_ciphertext, 16);
    for (i = 0; i < 1000000; i++) {
        ret =
            mbedtls_sm4_crypt_ecb(&ctx, mode, (const unsigned char *)dst, dst);
        if (ret) {
            mbedtls_printf("mbedtls_sm4_crypt_ecb failed!\n");
            goto exit;
        }
    }
    mbedtls_sm4_free(&ctx);
    if (0 != memcmp(sm4_ecb_plaintext, dst, 16)) {
        mbedtls_printf("SM4-ECB DEC 1000000 times failed!\n");
        ret = 1;
        goto exit;
    } else {
        mbedtls_printf("SM4-ECB DEC 1000000 times passed\n");
    }

    /* Test Key ladder */
    {
#if defined(MBEDTLS_FS_IO)
        FILE *f;
        if ((f = fopen(MODEL_KEY_FILE, "wb+")) == NULL) {
            mbedtls_printf("create file %s failed\n", MODEL_KEY_FILE);
            ret = 2;
            goto exit;
        }
        if (fwrite(SEC_KEK, 1, 16, f) != 16) {
            mbedtls_printf("write file %s failed\n", MODEL_KEY_FILE);
            ret = 2;
            goto exit;
        }
        fclose(f);
#endif
        mbedtls_sm4_context refkey_ctx;
        mbedtls_sm4_context seckey_ctx;
        uint8_t src_data[16]  = {0};
        uint8_t dst_data1[16] = {0};
        uint8_t dst_data2[16] = {0};
        mbedtls_sm4_sec_key_t seckey;
        uint8_t *p = NULL;

        /* Test setseckey 128 bits */

        mbedtls_sm4_init(&seckey_ctx);

        p              = (uint8_t *)SEC_SM4_KEY_128;
        seckey.sel     = MBEDTLS_SM4_KL_KEY_MODEL;
        seckey.ek3bits = 128;
        memcpy(seckey.ek1, p, 16);
        memcpy(seckey.ek2, p + 16, 16);
        memcpy(seckey.ek3, p + 32, 16);
        ret = mbedtls_sm4_setseckey_enc(&seckey_ctx, &seckey);
        if (0 != ret) {
            mbedtls_printf("mbedtls_sm4_setseckey_enc failed\n");
            goto exit;
        }

        /* do enc */
        memset(src_data, 0x12, 16);
        ret = mbedtls_sm4_crypt_ecb(&seckey_ctx, MBEDTLS_SM4_ENCRYPT, src_data,
                                    dst_data1);
        if (0 != ret) {
            mbedtls_printf("mbedtls_sm4_crypt_ecb failed\n");
            goto exit;
        }
        mbedtls_sm4_free(&seckey_ctx);

        /* call ref to do enc */
        mbedtls_sm4_init(&refkey_ctx);
        ret = mbedtls_sm4_setkey_enc(&refkey_ctx,
                                     (const unsigned char *)KEY_128, 128);
        if (0 != ret) {
            mbedtls_printf("mbedtls_sm4_setkey_enc failed\n");
            goto exit;
        }

        ret = mbedtls_sm4_crypt_ecb(&refkey_ctx, MBEDTLS_SM4_ENCRYPT, src_data,
                                    dst_data2);
        if (0 != ret) {
            mbedtls_printf("mbedtls_sm4_crypt_ecb failed\n");
            goto exit;
        }
        mbedtls_sm4_free(&refkey_ctx);

        if (0 != memcmp(dst_data1, dst_data2, 16)) {
            mbedtls_printf("SM4 set_seckey 128 failed!\n");
            ret = 1;
            goto exit;
        } else {
            mbedtls_printf("SM4 set_seckey 128 passed\n");
        }

#if defined(MBEDTLS_FS_IO)
        remove(MODEL_KEY_FILE);
#endif
    }

    ret = mbedtls_sm4_self_test2( verbose );
    if( ret )
        goto exit;

exit:
    if (ret != 0 && verbose != 0)
        mbedtls_printf("failed\n");

    return (ret);
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_SM4_C */
