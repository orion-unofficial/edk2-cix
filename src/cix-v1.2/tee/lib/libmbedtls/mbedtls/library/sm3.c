/*
 * Copyright (c) 2020, Arm Technology (China) Co., Ltd.
 * All rights reserved.
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,
 * any derivative work shall retain this copyright notice.
 */
/*
* SM3 is a cryptographic hash function used in the Chinese National
* Standard. It was published by the State Cryptography Administration
* (Chinese: 国家密码管理局) on 2010-12-17[1][2] as
* "GM/T 0004-2012: SM3 cryptographic hash algorithm".
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_SM3_C)

#include "mbedtls/sm3.h"
#include "mbedtls/platform_util.h"

#include <string.h>

#if defined(MBEDTLS_SELF_TEST)
#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#define mbedtls_printf printf
#endif /* MBEDTLS_PLATFORM_C */
#endif /* MBEDTLS_SELF_TEST */

#define SM3_VALIDATE_RET(cond)                             \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_SM3_BAD_INPUT_DATA )

#define SM3_VALIDATE(cond)  MBEDTLS_INTERNAL_VALIDATE( cond )

#if !defined(MBEDTLS_SM3_ALT)

/*
 * 32-bit integer manipulation macros (big endian)
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

void mbedtls_sm3_init( mbedtls_sm3_context *ctx )
{
    SM3_VALIDATE( ctx != NULL );

    memset( ctx, 0, sizeof( mbedtls_sm3_context ) );
}

void mbedtls_sm3_free( mbedtls_sm3_context *ctx )
{
    if( ctx == NULL )
        return;

    mbedtls_platform_zeroize( ctx, sizeof( mbedtls_sm3_context ) );
}

void mbedtls_sm3_clone( mbedtls_sm3_context *dst,
                         const mbedtls_sm3_context *src )
{
    SM3_VALIDATE( dst != NULL );
    SM3_VALIDATE( src != NULL );

    *dst = *src;
}

/* The IV */
#define IV0 0x7380166FU
#define IV1 0x4914B2B9U
#define IV2 0x172442D7U
#define IV3 0xDA8A0600U
#define IV4 0xA96F30BCU
#define IV5 0x163138AAU
#define IV6 0xE38DEE4DU
#define IV7 0xB0FB0E4EU

/*
 * SM3 context setup
 */
int mbedtls_sm3_starts_ret( mbedtls_sm3_context *ctx )
{
    SM3_VALIDATE_RET( ctx != NULL );

    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = IV0;
    ctx->state[1] = IV1;
    ctx->state[2] = IV2;
    ctx->state[3] = IV3;
    ctx->state[4] = IV4;
    ctx->state[5] = IV5;
    ctx->state[6] = IV6;
    ctx->state[7] = IV7;

    return( 0 );
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sm3_starts( mbedtls_sm3_context *ctx )
{
    mbedtls_sm3_starts_ret( ctx );
}
#endif

#if !defined(MBEDTLS_SM3_PROCESS_ALT)
/* The T(j) const variables */
#define T(j)   \
        ( ( ( j >= 0 ) && ( j <= 15 ) ) ?           \
          (0x79CC4519U) : (0x7A879D8AU) )
/* The FF(j) and GG(j) functions */
#define FF(x, y, z, j)                              \
        ( ( ( j >= 0 ) && ( j <= 15 ) ) ?           \
          ( x ^ y ^ z ) :                           \
          ( ( x & y ) | ( x & z ) | ( y & z ) ) )

#define GG(x, y, z, j)                              \
        ( ( ( j >= 0 ) && ( j <= 15 ) ) ?           \
          ( x ^ y ^ z ) :                           \
          ( (x & y) | ( (0xFFFFFFFFU ^ x) & z ) ) )

/* The P0(X) and P1(X) functions */
#define ROTL(x, n) (((x) << ((n)&31)) | (((x)&0xFFFFFFFFU) >> (32 - ((n)&31))))
#define P0(x) ((x) ^ ROTL((x), 9) ^ ROTL((x),17))
#define P1(x) ((x) ^ ROTL((x),15) ^ ROTL((x),23))

/* Method to get Wj' */
#define W_(j, W_ptr)  (W_ptr[j] ^ W_ptr[j + 4])
int mbedtls_internal_sm3_process( mbedtls_sm3_context *ctx,
                                  const unsigned char data[64] )
{
    uint32_t A, B, C, D, E, F, G, H;
    uint32_t SS1, SS2, TT1, TT2;
    /* Only define W, no W' for saving stack size */
    uint32_t W[68];
    int j;

    SM3_VALIDATE_RET( ctx != NULL );
    SM3_VALIDATE_RET( (const unsigned char *)data != NULL );

    /* 消息扩展 */
    for (j = 0; j < 68; j++) {
        if (j < 16) {
            /* init W for j = 0 to 15 */
            GET_UINT32_BE( W[j], data, 4 * j );
        } else {
            /* init W for j = 16 - 67. */
            W[j] = P1(W[j - 16] ^ W[j - 9] ^ ROTL(W[j - 3], 15)) ^ \
                   ROTL(W[j - 13], 7) ^ \
                   W[j - 6];
        }
    }

    /* 压缩函数 */
    /* ABCDEFGH <-- V(i) */
    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];
    E = ctx->state[4];
    F = ctx->state[5];
    G = ctx->state[6];
    H = ctx->state[7];

    for (j = 0; j < 64; j++) {
        SS1 = ROTL((ROTL(A, 12) + E + ROTL(T(j), j)), 7);
        SS2 = SS1 ^ ROTL(A, 12);
        TT1 = FF(A, B, C, j) + D + SS2 + W_(j, W);
        TT2 = GG(E, F, G, j) + H + SS1 + W[j];
        D   = C;
        C   = ROTL(B, 9);
        B   = A;
        A   = TT1;
        H   = G;
        G   = ROTL(F, 19);
        F   = E;
        E   = P0(TT2);
    }
    /* V(i+1) <--  ABCDEFGH ^ V(i) */
    ctx->state[0] ^= A;
    ctx->state[1] ^= B;
    ctx->state[2] ^= C;
    ctx->state[3] ^= D;
    ctx->state[4] ^= E;
    ctx->state[5] ^= F;
    ctx->state[6] ^= G;
    ctx->state[7] ^= H;

    return( 0 );
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sm3_process( mbedtls_sm3_context *ctx,
                           const unsigned char data[64] )
{
    mbedtls_internal_sm3_process( ctx, data );
}
#endif
#endif /* MBEDTLS_SM3_PROCESS_ALT */

/*
 * SM3 process buffer
 */
int mbedtls_sm3_update_ret( mbedtls_sm3_context *ctx,
                             const unsigned char *input,
                             size_t ilen )
{
    int ret;
    size_t fill;
    uint32_t left;

    SM3_VALIDATE_RET( ctx != NULL );
    SM3_VALIDATE_RET( ilen == 0 || input != NULL );

    if( ilen == 0 )
        return( 0 );

    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += (uint32_t) ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if( ctx->total[0] < (uint32_t) ilen )
        ctx->total[1]++;

    if( left && ilen >= fill )
    {
        memcpy( (void *) (ctx->buffer + left), input, fill );

        if( ( ret = mbedtls_internal_sm3_process( ctx, ctx->buffer ) ) != 0 )
            return( ret );

        input += fill;
        ilen  -= fill;
        left = 0;
    }

    while( ilen >= 64 )
    {
        if( ( ret = mbedtls_internal_sm3_process( ctx, input ) ) != 0 )
            return( ret );

        input += 64;
        ilen  -= 64;
    }

    if( ilen > 0 )
        memcpy( (void *) (ctx->buffer + left), input, ilen );

    return( 0 );
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sm3_update( mbedtls_sm3_context *ctx,
                          const unsigned char *input,
                          size_t ilen )
{
    mbedtls_sm3_update_ret( ctx, input, ilen );
}
#endif

/*
 * SM3 final digest
 */
int mbedtls_sm3_finish_ret( mbedtls_sm3_context *ctx,
                             unsigned char output[32] )
{
    int ret;
    uint32_t used;
    uint32_t high, low;

    SM3_VALIDATE_RET( ctx != NULL );
    SM3_VALIDATE_RET( (unsigned char *)output != NULL );

    /*
     * Add padding: 0x80 then 0x00 until 8 bytes remain for the length
     */
    used = ctx->total[0] & 0x3F;

    ctx->buffer[used++] = 0x80;

    if( used <= 56 )
    {
        /* Enough room for padding + length in current block */
        memset( ctx->buffer + used, 0, 56 - used );
    }
    else
    {
        /* We'll need an extra block */
        memset( ctx->buffer + used, 0, 64 - used );

        if( ( ret = mbedtls_internal_sm3_process( ctx, ctx->buffer ) ) != 0 )
            return( ret );

        memset( ctx->buffer, 0, 56 );
    }

    /*
     * Add message length
     */
    high = ( ctx->total[0] >> 29 )
         | ( ctx->total[1] <<  3 );
    low  = ( ctx->total[0] <<  3 );

    PUT_UINT32_BE( high, ctx->buffer, 56 );
    PUT_UINT32_BE( low,  ctx->buffer, 60 );

    if( ( ret = mbedtls_internal_sm3_process( ctx, ctx->buffer ) ) != 0 )
        return( ret );

    /*
     * Output final state
     */
    PUT_UINT32_BE( ctx->state[0], output,  0 );
    PUT_UINT32_BE( ctx->state[1], output,  4 );
    PUT_UINT32_BE( ctx->state[2], output,  8 );
    PUT_UINT32_BE( ctx->state[3], output, 12 );
    PUT_UINT32_BE( ctx->state[4], output, 16 );
    PUT_UINT32_BE( ctx->state[5], output, 20 );
    PUT_UINT32_BE( ctx->state[6], output, 24 );
    PUT_UINT32_BE( ctx->state[7], output, 28 );

    return( 0 );
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sm3_finish( mbedtls_sm3_context *ctx,
                          unsigned char output[32] )
{
    mbedtls_sm3_finish_ret( ctx, output );
}
#endif

/*
 * output = SM3( input buffer )
 */
int mbedtls_sm3_ret( const unsigned char *input,
                      size_t ilen,
                      unsigned char output[32] )
{
    int ret;
    mbedtls_sm3_context ctx;

    SM3_VALIDATE_RET( ilen == 0 || input != NULL );
    SM3_VALIDATE_RET( (unsigned char *)output != NULL );

    mbedtls_sm3_init( &ctx );

    if( ( ret = mbedtls_sm3_starts_ret( &ctx ) ) != 0 )
        goto exit;

    if( ( ret = mbedtls_sm3_update_ret( &ctx, input, ilen ) ) != 0 )
        goto exit;

    if( ( ret = mbedtls_sm3_finish_ret( &ctx, output ) ) != 0 )
        goto exit;

exit:
    mbedtls_sm3_free( &ctx );

    return( ret );
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_sm3( const unsigned char *input,
                   size_t ilen,
                   unsigned char output[32] )
{
    mbedtls_sm3_ret( input, ilen, output );
}
#endif

#endif /* MBEDTLS_SM3_ALT */

#if defined(MBEDTLS_SELF_TEST)
static unsigned char msg1[] = {0x61, 0x62, 0x63};
static unsigned char dgst1[32] = {
    0x66, 0xc7, 0xf0, 0xf4, 0x62, 0xee, 0xed, 0xd9, 0xd1, 0xf2, 0xd4,
    0x6b, 0xdc, 0x10, 0xe4, 0xe2, 0x41, 0x67, 0xc4, 0x87, 0x5c, 0xf2,
    0xf7, 0xa2, 0x29, 0x7d, 0xa0, 0x2b, 0x8f, 0x4b, 0xa8, 0xe0};
static unsigned char msg2[] = {
    0x61, 0x62, 0x63, 0x64, 0x61, 0x62, 0x63, 0x64, 0x61, 0x62, 0x63,
    0x64, 0x61, 0x62, 0x63, 0x64, 0x61, 0x62, 0x63, 0x64, 0x61, 0x62,
    0x63, 0x64, 0x61, 0x62, 0x63, 0x64, 0x61, 0x62, 0x63, 0x64, 0x61,
    0x62, 0x63, 0x64, 0x61, 0x62, 0x63, 0x64, 0x61, 0x62, 0x63, 0x64,
    0x61, 0x62, 0x63, 0x64, 0x61, 0x62, 0x63, 0x64, 0x61, 0x62, 0x63,
    0x64, 0x61, 0x62, 0x63, 0x64, 0x61, 0x62, 0x63, 0x64};

static unsigned char dgst2[32] = {
    0xde, 0xbe, 0x9f, 0xf9, 0x22, 0x75, 0xb8, 0xa1, 0x38, 0x60, 0x48,
    0x89, 0xc1, 0x8e, 0x5a, 0x4d, 0x6f, 0xdb, 0x70, 0xe5, 0x38, 0x7e,
    0x57, 0x65, 0x29, 0x3d, 0xcb, 0xa3, 0x9c, 0x0c, 0x57, 0x32};

#define ALL0_1M_SM3                                                            \
    (uint8_t                                                                   \
         *)"\xd5\xf3\x7b\x2e\xae\x2b\x48\xc2\x67\xe5\x95\x92\x78\xb9\x9d\xd3"  \
           "\xee\x83\xbe\xa4\xf5\x75\xf8\x22\x5a\x84\xea\x41\xb4\xd4\x32\x51"

#define BLOCKALIGNED_INPUT                                                     \
    (uint8_t                                                                   \
         *)"\x2F\xB2\xA6\x88\x07\x4F\x4C\x9C\x4B\xC9\xE8\x07\x16\xB3\x65\xFD"  \
           "\xAF\x3A\x01\xB0\xEA\xF8\x07\xE1\xFF\xCC\x3A\x3E\x74\xC6\x74\xCB"  \
           "\x91\x16\x7A\x72\x71\x2A\xED\xA4\x7F\xD7\xB2\x06\xFD\x3D\xBD\x8E"  \
           "\xB3\x30\x91\x47\xAE\xD7\xBA\x44\x8D\x70\x6D\xE0\x43\x30\xFA\x4B"  \
           "\xFE\xB3\x2E\x6F\xF6\xA8\xB0\xD2\xD2\xE9\x48\x46\x57\x6C\xB6\x14"  \
           "\xE9\xD5\xB0\x04\xEB\xA3\xAA\x66\xAC\xDF\xAD\xFA\x8D\x6D\x6A\x9A"  \
           "\xA9\xBB\xEC\x71\xEB\xE4\x90\xC9\xD9\x3D\x2B\x62\x54\x81\xFB\xE5"  \
           "\xD2\xDA\x73\xDF\xAB\x41\x89\x9D\x07\xC0\x37\x35\x6F\x4B\xA2\x93"
#define BLOCKALIGNED_LEN 128U
#define BLOCKALIGNED_SM3                                                       \
    (uint8_t                                                                   \
         *)"\x7f\xf1\x29\x33\xcc\x54\xa4\x49\xd6\x22\xca\xba\x0d\x47\x93\x7c"  \
           "\xb1\xf9\x48\x93\xe1\xf7\xf5\xe9\x9d\xce\x95\x32\x61\x07\x78\x33"

#define EMPTY_INPUT (uint8_t *)""
#define EMPTY_LEN 0U
#define EMPTY_SM3                                                              \
    (uint8_t                                                                   \
         *)"\x1a\xb2\x1d\x83\x55\xcf\xa1\x7f\x8e\x61\x19\x48\x31\xe8\x1a\x8f"  \
           "\x22\xbe\xc8\xc7\x28\xfe\xfb\x74\x7e\xd0\x35\xeb\x50\x82\xaa\x2b"

#define HASH127_INPUT                                                          \
    (uint8_t                                                                   \
         *)"\xA0\x5A\x2B\x9E\x4D\x7F\x43\x28\x70\x36\xEC\xFA\xC6\xCA\xB6\x98"  \
           "\x62\x8F\xF3\x6D\x04\x9C\x46\xEC\xF2\x4E\x06\x8C\xC0\x35\xD8\x4E"  \
           "\xFA\x64\x3C\xC4\x06\x38\xA3\xA5\x26\x1E\xF5\x0C\x78\x55\x09\x5B"  \
           "\x3A\x11\x84\x51\x0B\x78\xE7\x7D\xE1\x1B\x9F\x69\xDA\x26\x55\x06"  \
           "\x47\x0A\x5F\x4E\xF1\xB4\x01\x78\x3B\x79\x99\x40\xF2\x88\x02\xF0"  \
           "\xF3\xB7\xB9\x97\xF5\x8D\xBC\xEF\x12\x14\xE5\xF3\xD7\x11\xF0\xEF"  \
           "\xCC\xF5\x14\x2E\x6F\xC5\x97\x91\xA7\x6A\x97\x2B\xEA\xB2\x91\x4D"  \
           "\x78\x38\xB3\xD9\x9D\xBB\x91\xF1\x93\xCF\x94\x85\xCE\x77\x61"
#define HASH127_LEN 127U
#define HASH127_SM3                                                            \
    (uint8_t                                                                   \
         *)"\x7e\x8b\xb0\x6b\x58\xef\x15\xa6\x9c\x4c\xa6\x94\xaf\xfb\x3d\xf0"  \
           "\x97\x39\x69\xfd\xe7\x4d\x8f\xca\xf6\x40\x9d\x70\x5b\xf3\x32\x1a"

#define HASH1_INPUT (uint8_t *)"\xDA"
#define HASH1_LEN 1U
#define HASH1_SM3                                                              \
    (uint8_t                                                                   \
         *)"\x64\xf5\x48\xe1\xd0\x30\x50\xfc\x20\x47\xd3\x16\xb4\xa2\xd5\xde"  \
           "\xd3\x17\x51\x12\x8c\xaf\xae\x55\x41\x84\xbf\xbc\xfe\x05\xb2\x4d"

/*
 * FIPS-180-1 test vectors
 */
static const unsigned char sm3_test_buf[4][57] =
{
    { "abc" },
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" },
    { "" },
    { "" }
};

static const size_t sm3_test_buflen[4] =
{
    3, 56, 1000, 0
};

static const unsigned char sm3_test_sum[4][32] =
{
    { 0x66, 0xc7, 0xf0, 0xf4, 0x62, 0xee, 0xed, 0xd9,
      0xd1, 0xf2, 0xd4, 0x6b, 0xdc, 0x10, 0xe4, 0xe2,
      0x41, 0x67, 0xc4, 0x87, 0x5c, 0xf2, 0xf7, 0xa2,
      0x29, 0x7d, 0xa0, 0x2b, 0x8f, 0x4b, 0xa8, 0xe0 },
    { 0x63, 0x9b, 0x6c, 0xc5, 0xe6, 0x4d, 0x9e, 0x37,
      0xa3, 0x90, 0xb1, 0x92, 0xdf, 0x4f, 0xa1, 0xea,
      0x07, 0x20, 0xab, 0x74, 0x7f, 0xf6, 0x92, 0xb9,
      0xf3, 0x8c, 0x4e, 0x66, 0xad, 0x7b, 0x8c, 0x05 },
    { 0xc8, 0xaa, 0xf8, 0x94, 0x29, 0x55, 0x40, 0x29,
      0xe2, 0x31, 0x94, 0x1a, 0x2a, 0xcc, 0x0a, 0xd6,
      0x1f, 0xf2, 0xa5, 0xac, 0xd8, 0xfa, 0xdd, 0x25,
      0x84, 0x7a, 0x3a, 0x73, 0x2b, 0x3b, 0x02, 0xc3 },
    { 0x1a, 0xb2, 0x1d, 0x83, 0x55, 0xcf, 0xa1, 0x7f,
      0x8e, 0x61, 0x19, 0x48, 0x31, 0xe8, 0x1a, 0x8f,
      0x22, 0xbe, 0xc8, 0xc7, 0x28, 0xfe, 0xfb, 0x74,
      0x7e, 0xd0, 0x35, 0xeb, 0x50, 0x82, 0xaa, 0x2b }
};

static int mbedtls_sm3_self_test2( int verbose )
{
    int i, j, buflen, ret = 0;
    unsigned char buf[1024];
    unsigned char sm3sum[20];
    mbedtls_sm3_context ctx;

    mbedtls_sm3_init( &ctx );

    /*
     * SHA-1
     */
    for( i = 0; i < 4; i++ )
    {
        if( verbose != 0 )
            mbedtls_printf( "  SM3 test #%d: ", i + 1 );

        if( ( ret = mbedtls_sm3_starts_ret( &ctx ) ) != 0 )
            goto fail;

        if( i == 2 )
        {
            memset( buf, 'a', buflen = 1000 );

            for( j = 0; j < 1000; j++ )
            {
                ret = mbedtls_sm3_update_ret( &ctx, buf, buflen );
                if( ret != 0 )
                    goto fail;
            }
        }
        else
        {
            ret = mbedtls_sm3_update_ret( &ctx, sm3_test_buf[i],
                                           sm3_test_buflen[i] );
            if( ret != 0 )
                goto fail;
        }

        if( ( ret = mbedtls_sm3_finish_ret( &ctx, sm3sum ) ) != 0 )
            goto fail;

        if( memcmp( sm3sum, sm3_test_sum[i], 20 ) != 0 )
        {
            ret = 1;
            goto fail;
        }

        if( verbose != 0 )
            mbedtls_printf( "passed\n" );
    }

    if( verbose != 0 )
        mbedtls_printf( "\n" );

    goto exit;

fail:
    if( verbose != 0 )
        mbedtls_printf( "failed\n" );

exit:
    mbedtls_sm3_free( &ctx );

    return( ret );
}

int mbedtls_sm3_self_test(int verbose)
{
    int ret = 0;
    mbedtls_sm3_context ctx = {0};
    uint8_t dgst[32] = {0};

    mbedtls_sm3_init(&ctx);
    ret = mbedtls_sm3_starts_ret(&ctx);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_starts_ret failed!\n");
        goto exit;
    }
    ret = mbedtls_sm3_update_ret(&ctx, msg1, sizeof(msg1));
    if (ret) {
        mbedtls_printf("mbedtls_sm3_update_ret failed!\n");
        goto exit;
    }
    ret = mbedtls_sm3_finish_ret(&ctx, dgst);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_update_ret failed!\n");
        goto exit;
    }

    mbedtls_sm3_free(&ctx);
    if (0 != memcmp(dgst1, dgst, 32)) {
        mbedtls_printf("  SM3 32 bytes failed!\n");
        ret = 1;
        goto exit;
    } else {
        mbedtls_printf("  SM3 32 bytes passed\n");
    }


    mbedtls_sm3_init(&ctx);
    ret = mbedtls_sm3_starts_ret(&ctx);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_starts_ret failed!\n");
        goto exit;
    }
    ret = mbedtls_sm3_update_ret(&ctx, msg2, sizeof(msg2));
    if (ret) {
        mbedtls_printf("mbedtls_sm3_update_ret failed!\n");
        goto exit;
    }
    ret = mbedtls_sm3_finish_ret(&ctx, dgst);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_update_ret failed!\n");
        goto exit;
    }
    mbedtls_sm3_free(&ctx);
    if (0 != memcmp(dgst2, dgst, 32)) {
        mbedtls_printf("  SM3 64 bytes failed!\n");
        ret = 1;
        goto exit;
    } else {
        mbedtls_printf("  SM3 64 bytes passed\n");
    }


    mbedtls_sm3_init(&ctx);
    ret = mbedtls_sm3_starts_ret(&ctx);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_starts_ret failed!\n");
        goto exit;
    }
    ret = mbedtls_sm3_finish_ret(&ctx, dgst);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_update_ret failed!\n");
        goto exit;
    }
    mbedtls_sm3_free(&ctx);
    if (0 != memcmp(EMPTY_SM3, dgst, 32)) {
        mbedtls_printf("  SM3 empty data failed!\n");
        ret = 1;
        goto exit;
    } else {
        mbedtls_printf("  SM3 empty data passed\n");
    }


    mbedtls_sm3_init(&ctx);
    ret = mbedtls_sm3_starts_ret(&ctx);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_starts_ret failed!\n");
        goto exit;
    }
    ret = mbedtls_sm3_update_ret(&ctx, HASH127_INPUT, 127);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_update_ret failed!\n");
        goto exit;
    }
    ret = mbedtls_sm3_finish_ret(&ctx, dgst);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_update_ret failed!\n");
        goto exit;
    }
    mbedtls_sm3_free(&ctx);
    if (0 != memcmp(HASH127_SM3, dgst, 32)) {
        mbedtls_printf("  SM3 127 bytes data failed!\n");
        ret = 1;
        goto exit;
    } else {
        mbedtls_printf("  SM3 127 bytes data passed\n");
    }


    mbedtls_sm3_init(&ctx);
    ret = mbedtls_sm3_starts_ret(&ctx);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_starts_ret failed!\n");
        goto exit;
    }
    ret = mbedtls_sm3_update_ret(&ctx, HASH1_INPUT, 1);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_update_ret failed!\n");
        goto exit;
    }
    ret = mbedtls_sm3_finish_ret(&ctx, dgst);
    if (ret) {
        mbedtls_printf("mbedtls_sm3_update_ret failed!\n");
        goto exit;
    }
    mbedtls_sm3_free(&ctx);
    if (0 != memcmp(HASH1_SM3, dgst, 32)) {
        mbedtls_printf("  SM3 1 byte data failed!\n");
        ret = 1;
        goto exit;
    } else {
        mbedtls_printf("  SM3 1 byte data passed\n");
    }

    ret = mbedtls_sm3_self_test2( verbose );
    if( ret )
        goto exit;

exit:
    if (ret != 0 && verbose != 0)
        mbedtls_printf("failed\n");

    return (ret);
}
#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_SM3_C */
