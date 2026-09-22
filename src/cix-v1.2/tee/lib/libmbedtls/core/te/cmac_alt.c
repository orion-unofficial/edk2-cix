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
 * References:
 *
 * - NIST SP 800-38B Recommendation for Block Cipher Modes of Operation: The
 *      CMAC Mode for Authentication
 *   http://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-38b.pdf
 *
 * - RFC 4493 - The AES-CMAC Algorithm
 *   https://tools.ietf.org/html/rfc4493
 *
 * - RFC 4615 - The Advanced Encryption Standard-Cipher-based Message
 *      Authentication Code-Pseudo-Random Function-128 (AES-CMAC-PRF-128)
 *      Algorithm for the Internet Key Exchange Protocol (IKE)
 *   https://tools.ietf.org/html/rfc4615
 *
 *   Additional test vectors: ISO/IEC 9797-1
 *
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_CMAC_C)

#include "te_cmac.h"
#include "mbedtls/cmac.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/platform.h"
#include <string.h>


#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdlib.h>
#define mbedtls_calloc     calloc
#define mbedtls_free       free
#if defined(MBEDTLS_SELF_TEST)
#include <stdio.h>
#define mbedtls_printf     printf
#endif /* MBEDTLS_SELF_TEST */
#endif /* MBEDTLS_PLATFORM_C */

#if defined(MBEDTLS_CMAC_ALT) || defined(MBEDTLS_SELF_TEST)

/*
 * Multiplication by u in the Galois field of GF(2^n)
 *
 * As explained in NIST SP 800-38B, this can be computed:
 *
 *   If MSB(p) = 0, then p = (p << 1)
 *   If MSB(p) = 1, then p = (p << 1) ^ R_n
 *   with R_64 = 0x1B and  R_128 = 0x87
 *
 * Input and output MUST NOT point to the same buffer
 * Block size must be 8 bytes or 16 bytes - the block sizes for DES and AES.
 */
static int cmac_multiply_by_u( unsigned char *output,
                               const unsigned char *input,
                               size_t blocksize )
{
    const unsigned char R_128 = 0x87;
    const unsigned char R_64 = 0x1B;
    unsigned char R_n, mask;
    unsigned char overflow = 0x00;
    int i;

    if( blocksize == MBEDTLS_AES_BLOCK_SIZE )
    {
        R_n = R_128;
    }
    else if( blocksize == MBEDTLS_DES3_BLOCK_SIZE )
    {
        R_n = R_64;
    }
    else
    {
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );
    }

    for( i = (int)blocksize - 1; i >= 0; i-- )
    {
        output[i] = input[i] << 1 | overflow;
        overflow = input[i] >> 7;
    }

    /* mask = ( input[0] >> 7 ) ? 0xff : 0x00
     * using bit operations to avoid branches */

    /* MSVC has a warning about unary minus on unsigned, but this is
     * well-defined and precisely what we want to do here */
#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4146 )
#endif
    mask = - ( input[0] >> 7 );
#if defined(_MSC_VER)
#pragma warning( pop )
#endif

    output[ blocksize - 1 ] ^= R_n & mask;

    return( 0 );
}

/*
 * Generate subkeys
 *
 * - as specified by RFC 4493, section 2.3 Subkey Generation Algorithm
 */
__attribute__((unused))
static int cmac_generate_subkeys( mbedtls_cipher_context_t *ctx,
                                  unsigned char* K1, unsigned char* K2 )
{
    int ret;
    unsigned char L[MBEDTLS_CIPHER_BLKSIZE_MAX];
    size_t olen, block_size;

    mbedtls_platform_zeroize( L, sizeof( L ) );

    block_size = ctx->cipher_info->block_size;

    /* Calculate Ek(0) */
    if( ( ret = mbedtls_cipher_update( ctx, L, block_size, L, &olen ) ) != 0 )
        goto exit;

    /*
     * Generate K1 and K2
     */
    if( ( ret = cmac_multiply_by_u( K1, L , block_size ) ) != 0 )
        goto exit;

    if( ( ret = cmac_multiply_by_u( K2, K1 , block_size ) ) != 0 )
        goto exit;

exit:
    mbedtls_platform_zeroize( L, sizeof( L ) );

    return( ret );
}
#endif /* defined(MBEDTLS_CMAC_ALT) || defined(MBEDTLS_SELF_TEST) */

#if defined(MBEDTLS_CMAC_ALT)
static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_CIPHER_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_STATE:
            errno = MBEDTLS_ERR_CIPHER_INVALID_CONTEXT;
            break;
        case TE_ERROR_NOT_SUPPORTED:
            errno = MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED;
            break;
        case TE_ERROR_BAD_KEY_LENGTH:
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
            break;
        default:
            errno = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
            break;
    }

    return errno;
}

int mbedtls_cipher_cmac_starts( mbedtls_cipher_context_t *ctx,
                                const unsigned char *key, size_t keybits )
{
#define DES_EDE3_KEY_SIZE (MBEDTLS_KEY_LENGTH_DES_EDE3 / 8)
#define DES_EDE_KEY_SIZE  (MBEDTLS_KEY_LENGTH_DES_EDE / 8)
    mbedtls_cipher_type_t type;
    mbedtls_cmac_context_t *cmac_ctx = NULL;
    int retval = 0;
    int malg = 0;
    uint8_t tmp_key[DES_EDE3_KEY_SIZE] = {0}; /*special handle for des_ede3 with 128bits key*/
    const uint8_t *p_key = key;

    if( (ctx == NULL) || (ctx->cipher_info == NULL) || (key == NULL) )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    if( (( ctx->cipher_info->flags & MBEDTLS_CIPHER_VARIABLE_KEY_LEN ) == 0) &&
        (ctx->cipher_info->key_bitlen != keybits) )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    cmac_ctx = ctx->cmac_ctx;
    /*
     * xtest_4002 has sequence:
     * .alloc() -> .init() -> .update() -> .clone() -> .init() -> .finup()
     * where the same cmac ctx will start twice (by the .init() call).
     */
    if (cmac_ctx == NULL ||
        cmac_ctx->magic != MBEDTLS_CMAC_MAGIC ||
        cmac_ctx->cmac == NULL) {
        type = ctx->cipher_info->type;
        switch( type )
        {
            case MBEDTLS_CIPHER_AES_128_ECB:
            case MBEDTLS_CIPHER_AES_192_ECB:
            case MBEDTLS_CIPHER_AES_256_ECB:
                malg = TE_MAIN_ALGO_AES;
                break;
            case MBEDTLS_CIPHER_DES_EDE_ECB:
                malg = TE_MAIN_ALGO_TDES;
                break;
            case MBEDTLS_CIPHER_DES_EDE3_ECB:
                malg = TE_MAIN_ALGO_TDES;
                break;
            case MBEDTLS_CIPHER_DES_ECB:
                malg = TE_MAIN_ALGO_DES;
                break;
            case MBEDTLS_CIPHER_SM4_128_ECB:
                malg = TE_MAIN_ALGO_SM4;
                break;
            default:
                return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );
        }

        /* Allocated and initialise in the cipher context memory for the CMAC
        * context */
        cmac_ctx = (mbedtls_cmac_context_t *)mbedtls_calloc(1,
                                sizeof(*cmac_ctx) + sizeof(te_cmac_ctx_t));
        if( cmac_ctx == NULL )
            return( MBEDTLS_ERR_CIPHER_ALLOC_FAILED );
        cmac_ctx->cmac = (te_cmac_ctx_t*)(cmac_ctx + 1);
        retval = te_cmac_init(cmac_ctx->cmac,
                              te_platform_get_drvhandle(), malg);
        if (TE_SUCCESS != retval) {
            mbedtls_free(cmac_ctx);
            goto _out_;
        }
        cmac_ctx->magic = MBEDTLS_CMAC_MAGIC;
        ctx->cmac_ctx = cmac_ctx;
    }
    /** special handle for MBEDTLS_CIPHER_DES_EDE_ECB, cause driver only supports
     *  tdes-192 so in this case should extend key from 128 to 192 */
    if (ctx->cipher_info->type == MBEDTLS_CIPHER_DES_EDE_ECB) {
        if (keybits != MBEDTLS_KEY_LENGTH_DES_EDE) {
            retval = ( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );
            goto err;
        }
        memcpy(tmp_key, key, DES_EDE_KEY_SIZE);
        memcpy(tmp_key + DES_EDE_KEY_SIZE, key,
               DES_EDE3_KEY_SIZE - DES_EDE_KEY_SIZE);
        p_key = tmp_key;
        keybits = MBEDTLS_KEY_LENGTH_DES_EDE3;
    }

    retval = te_cmac_setkey(cmac_ctx->cmac, p_key, keybits);
    if (TE_SUCCESS != retval) {
        goto err;
    }

    retval = te_cmac_start(cmac_ctx->cmac);
    if(TE_SUCCESS == retval) {
        goto _out_;
    }
err:
    te_cmac_free(cmac_ctx->cmac);
    mbedtls_platform_zeroize(cmac_ctx->cmac, sizeof(*cmac_ctx->cmac));
    mbedtls_platform_zeroize(cmac_ctx, sizeof(*cmac_ctx));
    mbedtls_free(cmac_ctx);
    ctx->cmac_ctx = NULL;
_out_:

    return _convert_retval_to_mbedtls(retval);
}

static void _mbedtls_sec_key_to_te_sec_key( te_sec_key_t *sec_key,
                                            mbedtls_cmac_sec_key_t *key )
{
    sec_key->sel = (key->sel == MBEDTLS_CMAC_KL_KEY_MODEL) ?
                   TE_KL_KEY_MODEL : TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    osal_memcpy(sec_key->eks, key->eks,
              sizeof(sec_key->eks) > sizeof(key->eks)
                ? sizeof(key->eks) : sizeof(sec_key->eks));
}

int mbedtls_cipher_cmac_starts_with_seckey( mbedtls_cipher_context_t *ctx,
                                            mbedtls_cmac_sec_key_t *key )
{
    mbedtls_cipher_type_t type;
    mbedtls_cmac_context_t *cmac_ctx ;
    te_sec_key_t sec_key = {0};
    int retval = 0;
    int malg = 0;

    if( (ctx == NULL) || (ctx->cipher_info == NULL) || (key == NULL) ||
        ((key->sel != MBEDTLS_CMAC_KL_KEY_MODEL) &&
         (key->sel != MBEDTLS_CMAC_KL_KEY_ROOT)) )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    if( (( ctx->cipher_info->flags & MBEDTLS_CIPHER_VARIABLE_KEY_LEN ) == 0) &&
        (ctx->cipher_info->key_bitlen != key->ek3bits) )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    cmac_ctx = ctx->cmac_ctx;
    /*
     * xtest_4002: ctx could start twice.
     */
    if (cmac_ctx == NULL ||
        cmac_ctx->magic != MBEDTLS_CMAC_MAGIC ||
        cmac_ctx->cmac == NULL) {
        type = ctx->cipher_info->type;

        switch( type )
        {
            case MBEDTLS_CIPHER_AES_128_ECB:
            case MBEDTLS_CIPHER_AES_256_ECB:
                malg = TE_MAIN_ALGO_AES;
                break;
            case MBEDTLS_CIPHER_SM4_128_ECB:
                malg = TE_MAIN_ALGO_SM4;
                break;
            default:
                return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );
        }

        /* Allocated and initialise in the cipher context memory for the CMAC
        * context */
        cmac_ctx = (mbedtls_cmac_context_t *)mbedtls_calloc(1,
                        sizeof(mbedtls_cmac_context_t) + sizeof(te_cmac_ctx_t));
        if( cmac_ctx == NULL )
            return( MBEDTLS_ERR_CIPHER_ALLOC_FAILED );
        cmac_ctx->cmac = (te_cmac_ctx_t*)(cmac_ctx + 1);
        retval = te_cmac_init(cmac_ctx->cmac,
                              te_platform_get_drvhandle(), malg);
        if (TE_SUCCESS != retval) {
            mbedtls_free(cmac_ctx);
            goto _out_;
        }
        cmac_ctx->magic = MBEDTLS_CMAC_MAGIC;
        ctx->cmac_ctx = cmac_ctx;
    }

    _mbedtls_sec_key_to_te_sec_key(&sec_key, key);
    retval = te_cmac_setseckey(cmac_ctx->cmac, &sec_key);
    if (TE_SUCCESS != retval) {
        goto err;
    }

    retval = te_cmac_start(cmac_ctx->cmac);
    if(retval != TE_SUCCESS) {
        goto err;
    }

    goto _out_;
err:
    te_cmac_free(cmac_ctx->cmac);
    mbedtls_platform_zeroize(cmac_ctx->cmac, sizeof(*cmac_ctx->cmac));
    mbedtls_platform_zeroize(cmac_ctx, sizeof(*cmac_ctx));
    mbedtls_free(cmac_ctx);
    ctx->cmac_ctx = NULL;
_out_:

    return _convert_retval_to_mbedtls(retval);
}

static void _mbedtls_sec_key_to_te_sec_key_v2( te_sec_key_v2_t *sec_key,
                                               mbedtls_cmac_sec_key_v2_t *key )
{
    sec_key->sel = (key->sel == MBEDTLS_CMAC_KL_KEY_MODEL) ?
                   TE_KL_KEY_MODEL : TE_KL_KEY_ROOT;
    sec_key->ek3bits = key->ek3bits;
    osal_memcpy(sec_key->eks, key->eks,
              sizeof(sec_key->eks) > sizeof(key->eks)
                ? sizeof(key->eks) : sizeof(sec_key->eks));
}

int mbedtls_cipher_cmac_starts_with_seckey_v2( mbedtls_cipher_context_t *ctx,
                                               mbedtls_cmac_sec_key_v2_t *key )
{
    mbedtls_cipher_type_t type;
    mbedtls_cmac_context_t *cmac_ctx ;
    te_sec_key_v2_t sec_key = {0};
    int retval = 0;
    int malg = 0;

    if( (ctx == NULL) || (ctx->cipher_info == NULL) || (key == NULL) ||
        ((key->sel != MBEDTLS_CMAC_KL_KEY_MODEL) &&
         (key->sel != MBEDTLS_CMAC_KL_KEY_ROOT)) )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    if( (( ctx->cipher_info->flags & MBEDTLS_CIPHER_VARIABLE_KEY_LEN ) == 0) &&
        (ctx->cipher_info->key_bitlen != key->ek3bits) )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    cmac_ctx = ctx->cmac_ctx;
    /*
     * xtest_4002: ctx could start twice.
     */
    if (cmac_ctx == NULL ||
        cmac_ctx->magic != MBEDTLS_CMAC_MAGIC ||
        cmac_ctx->cmac == NULL) {
        type = ctx->cipher_info->type;

        switch( type )
        {
            case MBEDTLS_CIPHER_AES_256_ECB:
                malg = TE_MAIN_ALGO_AES;
                break;
            default:
                return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );
        }

        /* Allocated and initialise in the cipher context memory for the CMAC
        * context */
        cmac_ctx = (mbedtls_cmac_context_t *)mbedtls_calloc(1,
                    sizeof(mbedtls_cmac_context_t) + sizeof(te_cmac_ctx_t));
        if( cmac_ctx == NULL )
            return( MBEDTLS_ERR_CIPHER_ALLOC_FAILED );
        cmac_ctx->cmac = (te_cmac_ctx_t*)(cmac_ctx + 1);
        retval = te_cmac_init(cmac_ctx->cmac,
                              te_platform_get_drvhandle(), malg);
        if (TE_SUCCESS != retval) {
            mbedtls_free(cmac_ctx);
            goto _out_;
        }
        cmac_ctx->magic = MBEDTLS_CMAC_MAGIC;
        ctx->cmac_ctx = cmac_ctx;
    }

    _mbedtls_sec_key_to_te_sec_key_v2(&sec_key, key);
    retval = te_cmac_setseckey_v2(cmac_ctx->cmac, &sec_key);
    if (TE_SUCCESS != retval) {
        goto err;
    }

    retval = te_cmac_start(cmac_ctx->cmac);
    if(retval != TE_SUCCESS) {
        goto err;
    }

    goto _out_;
err:
    te_cmac_free(cmac_ctx->cmac);
    mbedtls_platform_zeroize(cmac_ctx->cmac, sizeof(*cmac_ctx->cmac));
    mbedtls_platform_zeroize(cmac_ctx, sizeof(*cmac_ctx));
    mbedtls_free(cmac_ctx);
    ctx->cmac_ctx = NULL;
_out_:

    return _convert_retval_to_mbedtls(retval);
}

int mbedtls_cipher_cmac_update( mbedtls_cipher_context_t *ctx,
                                const unsigned char *input, size_t ilen )
{
    mbedtls_cmac_context_t *cmac_ctx;

    if( ctx == NULL || ctx->cipher_info == NULL || input == NULL ||
        ctx->cmac_ctx == NULL || ctx->cmac_ctx->magic != MBEDTLS_CMAC_MAGIC)
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    cmac_ctx = ctx->cmac_ctx;
    return _convert_retval_to_mbedtls(
            te_cmac_update((te_cmac_ctx_t *)cmac_ctx->cmac, ilen, input));
}

int mbedtls_cipher_cmac_finish( mbedtls_cipher_context_t *ctx,
                                unsigned char *output )
{
    mbedtls_cmac_context_t *cmac_ctx;
    int ret = 0;

    if( ctx == NULL || ctx->cipher_info == NULL || ctx->cmac_ctx == NULL ||
        output == NULL || ctx->cmac_ctx->magic != MBEDTLS_CMAC_MAGIC)
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    cmac_ctx = ctx->cmac_ctx;
    ret = te_cmac_finish((te_cmac_ctx_t *)cmac_ctx->cmac, output,
                         mbedtls_cipher_get_block_size(ctx));
    return _convert_retval_to_mbedtls(ret);
}

int mbedtls_cipher_cmac_reset( mbedtls_cipher_context_t *ctx )
{
    mbedtls_cmac_context_t *cmac_ctx = NULL;

    if( ctx == NULL || ctx->cipher_info == NULL || ctx->cmac_ctx == NULL
        || ctx->cmac_ctx->magic != MBEDTLS_CMAC_MAGIC )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    cmac_ctx = ctx->cmac_ctx;
    return _convert_retval_to_mbedtls(
                te_cmac_reset((te_cmac_ctx_t *)cmac_ctx->cmac));
}

void mbedtls_cmac_free( mbedtls_cmac_context_t *ctx )
{
    if( ctx == NULL || ctx->magic != MBEDTLS_CMAC_MAGIC )
        return;

    OSAL_ASSERT(TE_SUCCESS == te_cmac_free(ctx->cmac));
    mbedtls_platform_zeroize( ctx->cmac, sizeof( *ctx->cmac ) );
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );
}

int mbedtls_cipher_cmac( const mbedtls_cipher_info_t *cipher_info,
                         const unsigned char *key, size_t keylen,
                         const unsigned char *input, size_t ilen,
                         unsigned char *output )
{
    mbedtls_cipher_context_t ctx;
    int ret;

    if( cipher_info == NULL || key == NULL || input == NULL || output == NULL )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    mbedtls_cipher_init( &ctx );

    if( ( ret = mbedtls_cipher_setup( &ctx, cipher_info ) ) != 0 )
        goto exit;

    ret = mbedtls_cipher_cmac_starts( &ctx, key, keylen );
    if( ret != 0 )
        goto exit;

    ret = mbedtls_cipher_cmac_update( &ctx, input, ilen );
    if( ret != 0 )
        goto exit;

    ret = mbedtls_cipher_cmac_finish( &ctx, output );

exit:
    mbedtls_cipher_free( &ctx );

    return( ret );
}

/*
 * prerequisites:
 * The 'dst' shall be initialized by mbedtls_cipher_init() at least.
 */
int mbedtls_cipher_cmac_clone( mbedtls_cipher_context_t *dst,
                               const mbedtls_cipher_context_t *src )
{
    int ret = 0;
    mbedtls_cmac_context_t *cmac_ctx = NULL;
    mbedtls_cipher_context_t tmp = {0}; /** tmp context for swapping */

    if( (dst == NULL) || (src == NULL) || ((src->cmac_ctx != NULL) &&
        src->cmac_ctx->magic != MBEDTLS_CMAC_MAGIC)) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    mbedtls_cipher_init( &tmp );
    if( ( ret = mbedtls_cipher_setup( &tmp,
                                        src->cipher_info ) ) != 0 )
        goto out;

    /**< when clone right after setup cmac_ctx is NULL ,so if this case just do
     the setup stuff and return */
     if ( src->cmac_ctx == NULL ) {
         goto suc;
     }

    /* Allocated and initialise in the cipher context memory for the CMAC
     * context */
    cmac_ctx = (mbedtls_cmac_context_t *)mbedtls_calloc(1,
                            sizeof(*cmac_ctx) + sizeof(te_cmac_ctx_t));
    if( cmac_ctx == NULL ) {
        ret = MBEDTLS_ERR_CIPHER_ALLOC_FAILED;
        goto err;
    }

    cmac_ctx->cmac = (te_cmac_ctx_t*)(cmac_ctx + 1);
    cmac_ctx->magic = MBEDTLS_CMAC_MAGIC;
    tmp.cmac_ctx = cmac_ctx;

    ret = te_cmac_clone( src->cmac_ctx->cmac, tmp.cmac_ctx->cmac );
    ret = _convert_retval_to_mbedtls( ret );
    if ( ret == 0 ) {
        goto suc;
    }
err:
    /** if failed free tmp */
    mbedtls_cipher_free( &tmp );
    goto out;
suc:
    /** clone success, free original cipher context and copy the new one */
    mbedtls_cipher_free( dst );
    memcpy( dst, &tmp, sizeof(*dst) );
out:
    return ret;
}

#if defined(MBEDTLS_AES_C)
/*
 * Implementation of AES-CMAC-PRF-128 defined in RFC 4615
 */
int mbedtls_aes_cmac_prf_128( const unsigned char *key, size_t key_length,
                              const unsigned char *input, size_t in_len,
                              unsigned char *output )
{
    int ret;
    const mbedtls_cipher_info_t *cipher_info;
    unsigned char zero_key[MBEDTLS_AES_BLOCK_SIZE];
    unsigned char int_key[MBEDTLS_AES_BLOCK_SIZE];

    if( key == NULL || input == NULL || output == NULL )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    cipher_info = mbedtls_cipher_info_from_type( MBEDTLS_CIPHER_AES_128_ECB );
    if( cipher_info == NULL )
    {
        /* Failing at this point must be due to a build issue */
        ret = MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE;
        goto exit;
    }

    if( key_length == MBEDTLS_AES_BLOCK_SIZE )
    {
        /* Use key as is */
        memcpy( int_key, key, MBEDTLS_AES_BLOCK_SIZE );
    }
    else
    {
        memset( zero_key, 0, MBEDTLS_AES_BLOCK_SIZE );

        ret = mbedtls_cipher_cmac( cipher_info, zero_key, 128, key,
                                   key_length, int_key );
        if( ret != 0 )
            goto exit;
    }

    ret = mbedtls_cipher_cmac( cipher_info, int_key, 128, input, in_len,
                               output );

exit:
    mbedtls_platform_zeroize( int_key, sizeof( int_key ) );

    return( ret );
}
#endif /* MBEDTLS_AES_C */

#endif /* !MBEDTLS_CMAC_ALT */

#endif /* MBEDTLS_CMAC_C */
