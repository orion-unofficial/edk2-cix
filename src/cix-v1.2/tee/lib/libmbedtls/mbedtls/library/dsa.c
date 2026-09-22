/*
 * Copyright (c) 2018-2020, Arm Technology (China) Co., Ltd.
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

#if defined(MBEDTLS_DSA_C)

#include "mbedtls/dsa.h"
#include "mbedtls/asn1write.h"

#include <string.h>

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_printf printf
#define mbedtls_calloc calloc
#define mbedtls_free free
#endif

#include "mbedtls/platform_util.h"

/* Parameter validation macros based on platform_util.h */
#define DSA_VALIDATE_RET(cond)                                                 \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_DSA_BAD_INPUT_DATA)
#define DSA_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

#if !defined(MBEDTLS_DSA_SIGN_ALT)
int mbedtls_dsa_sign(const mbedtls_mpi *p,
                     const mbedtls_mpi *q,
                     const mbedtls_mpi *g,
                     mbedtls_mpi *r,
                     mbedtls_mpi *s,
                     const mbedtls_mpi *x,
                     const unsigned char *buf,
                     size_t blen,
                     int (*f_rng)(void *, unsigned char *, size_t),
                     void *p_rng)
{
    return MBEDTLS_ERR_DSA_FEATURE_UNAVAILABLE;
}
#endif /* !MBEDTLS_DSA_SIGN_ALT */

#if !defined(MBEDTLS_DSA_VERIFY_ALT)
int mbedtls_dsa_verify(const mbedtls_mpi *p,
                       const mbedtls_mpi *q,
                       const mbedtls_mpi *g,
                       const unsigned char *buf,
                       size_t blen,
                       const mbedtls_mpi *y,
                       const mbedtls_mpi *r,
                       const mbedtls_mpi *s)
{
    return MBEDTLS_ERR_DSA_FEATURE_UNAVAILABLE;
}
#endif /* !MBEDTLS_DSA_VERIFY_ALT */
/*
 * Convert a signature (given by context) to ASN.1
 */
static int dsa_signature_to_asn1(const mbedtls_mpi *r,
                                 const mbedtls_mpi *s,
                                 unsigned char *sig,
                                 size_t *slen)
{
    int ret;
    unsigned char buf[MBEDTLS_DSA_MAX_LEN];
    unsigned char *p = buf + sizeof(buf);
    size_t len       = 0;

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_mpi(&p, buf, s));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_mpi(&p, buf, r));

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&p, buf, len));
    MBEDTLS_ASN1_CHK_ADD(
        len, mbedtls_asn1_write_tag(
                 &p, buf, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    memcpy(sig, p, len);
    *slen = len;

    return (0);
}

/*
 * Compute and write signature
 */

int mbedtls_dsa_write_signature(mbedtls_dsa_context *ctx,
                                mbedtls_md_type_t md_alg,
                                const unsigned char *hash,
                                size_t hlen,
                                unsigned char *sig,
                                size_t *slen,
                                int (*f_rng)(void *, unsigned char *, size_t),
                                void *p_rng)
{
    int ret;
    mbedtls_mpi r, s;
    DSA_VALIDATE_RET(ctx != NULL);
    DSA_VALIDATE_RET(hash != NULL);
    DSA_VALIDATE_RET(sig != NULL);
    DSA_VALIDATE_RET(slen != NULL);

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    (void)md_alg;
    MBEDTLS_MPI_CHK(mbedtls_dsa_sign(&ctx->p, &ctx->q, &ctx->g, &r, &s, &ctx->x,
                                     hash, hlen, f_rng, p_rng));

    MBEDTLS_MPI_CHK(dsa_signature_to_asn1(&r, &s, sig, slen));

cleanup:
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);

    return (ret);
}

/*
 * Restartable read and check signature
 */
int mbedtls_dsa_read_signature(mbedtls_dsa_context *ctx,
                               const unsigned char *hash,
                               size_t hlen,
                               const unsigned char *sig,
                               size_t slen)
{
    int ret;
    unsigned char *p         = (unsigned char *)sig;
    const unsigned char *end = sig + slen;
    size_t len;
    mbedtls_mpi r, s;
    DSA_VALIDATE_RET(ctx != NULL);
    DSA_VALIDATE_RET(hash != NULL);
    DSA_VALIDATE_RET(sig != NULL);

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED |
                                        MBEDTLS_ASN1_SEQUENCE)) != 0) {
        ret += MBEDTLS_ERR_DSA_BAD_INPUT_DATA;
        goto cleanup;
    }

    if (p + len != end) {
        ret = MBEDTLS_ERR_DSA_BAD_INPUT_DATA + MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;
        goto cleanup;
    }

    if ((ret = mbedtls_asn1_get_mpi(&p, end, &r)) != 0 ||
        (ret = mbedtls_asn1_get_mpi(&p, end, &s)) != 0) {
        ret += MBEDTLS_ERR_DSA_BAD_INPUT_DATA;
        goto cleanup;
    }

    if ((ret = mbedtls_dsa_verify(&ctx->p, &ctx->q, &ctx->g, hash, hlen,
                                  &ctx->y, &r, &s)) != 0)
        goto cleanup;

    /* At this point we know that the buffer starts with a valid signature.
     * Return 0 if the buffer just contains the signature, and a specific
     * error code if the valid signature is followed by more data. */
    if (p != end)
        ret = MBEDTLS_ERR_DSA_SIG_LEN_MISMATCH;

cleanup:
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);

    return (ret);
}

int mbedtls_dsa_write_parameter(mbedtls_dsa_context *ctx,
                                unsigned char *param,
                                size_t *plen)
{
    int ret;
    unsigned char *p = NULL;
    size_t len       = 0;
    size_t i         = 0;

    DSA_VALIDATE_RET(ctx != NULL);
    DSA_VALIDATE_RET(param != NULL);
    DSA_VALIDATE_RET(plen != NULL);

    p = param + *plen;

    MBEDTLS_ASN1_CHK_ADD(
        len, mbedtls_asn1_write_mpi(&p, param, (const mbedtls_mpi *)(&ctx->p)));
    MBEDTLS_ASN1_CHK_ADD(
        len, mbedtls_asn1_write_mpi(&p, param, (const mbedtls_mpi *)(&ctx->q)));
    MBEDTLS_ASN1_CHK_ADD(
        len, mbedtls_asn1_write_mpi(&p, param, (const mbedtls_mpi *)(&ctx->g)));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&p, param, len));
    MBEDTLS_ASN1_CHK_ADD(
        len, mbedtls_asn1_write_tag(
                 &p, param, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    for (i = 0; i < len; i++) {
        param[i] = p[i];
    }
    *plen = len;

    return (0);
}

int mbedtls_dsa_read_parameter(mbedtls_dsa_context *ctx,
                               const unsigned char *param,
                               size_t plen)
{
    int ret;
    unsigned char *p         = (unsigned char *)param;
    const unsigned char *end = param + plen;
    size_t len;

    DSA_VALIDATE_RET(ctx != NULL);
    DSA_VALIDATE_RET(param != NULL);
    DSA_VALIDATE_RET(plen != 0);

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED |
                                        MBEDTLS_ASN1_SEQUENCE)) != 0) {
        ret += MBEDTLS_ERR_DSA_BAD_INPUT_DATA;
        goto cleanup;
    }

    if (p + len != end) {
        ret = MBEDTLS_ERR_DSA_BAD_INPUT_DATA + MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;
        goto cleanup;
    }

    if ((ret = mbedtls_asn1_get_mpi(&p, end, &ctx->p)) != 0 ||
        (ret = mbedtls_asn1_get_mpi(&p, end, &ctx->q)) != 0 ||
        (ret = mbedtls_asn1_get_mpi(&p, end, &ctx->g)) != 0) {
        ret += MBEDTLS_ERR_DSA_BAD_INPUT_DATA;
        goto cleanup;
    }

cleanup:
    return (ret);
}

#if !defined(MBEDTLS_DSA_GENKEY_ALT)
/*
 * Generate key pair
 */
int mbedtls_dsa_genkey(mbedtls_dsa_context *ctx,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng)
{
    return MBEDTLS_ERR_DSA_FEATURE_UNAVAILABLE;
}
#endif /* !MBEDTLS_DSA_GENKEY_ALT */

/*
 * Initialize context
 */
void mbedtls_dsa_init(mbedtls_dsa_context *ctx)
{
    DSA_VALIDATE(ctx != NULL);

    mbedtls_mpi_init(&ctx->p);
    mbedtls_mpi_init(&ctx->q);
    mbedtls_mpi_init(&ctx->g);
    mbedtls_mpi_init(&ctx->x);
    mbedtls_mpi_init(&ctx->y);
}

/*
 * Free context
 */
void mbedtls_dsa_free(mbedtls_dsa_context *ctx)
{
    if (ctx == NULL)
        return;

    mbedtls_mpi_free(&ctx->p);
    mbedtls_mpi_free(&ctx->q);
    mbedtls_mpi_free(&ctx->g);
    mbedtls_mpi_free(&ctx->x);
    mbedtls_mpi_free(&ctx->y);
}

#if defined(MBEDTLS_SELF_TEST)

#include "mbedtls/sha1.h"

/*
 * DSA Test data automatically generated from
 * from http://csrc.nist.gov/groups/STM/cavp/documents/dss/186-3dsatestvectors.zip
 * SigGen.txt
 */

/* [mod = L=1024, N=160, SHA-1] */
static const uint8_t dsa_prime[] = {
/* P */
    0xa8, 0xf9, 0xcd, 0x20, 0x1e, 0x5e, 0x35, 0xd8, 0x92, 0xf8, 0x5f, 0x80,
    0xe4, 0xdb, 0x25, 0x99, 0xa5, 0x67, 0x6a, 0x3b, 0x1d, 0x4f, 0x19, 0x03,
    0x30, 0xed, 0x32, 0x56, 0xb2, 0x6d, 0x0e, 0x80, 0xa0, 0xe4, 0x9a, 0x8f,
    0xff, 0xaa, 0xad, 0x2a, 0x24, 0xf4, 0x72, 0xd2, 0x57, 0x32, 0x41, 0xd4,
    0xd6, 0xd6, 0xc7, 0x48, 0x0c, 0x80, 0xb4, 0xc6, 0x7b, 0xb4, 0x47, 0x9c,
    0x15, 0xad, 0xa7, 0xea, 0x84, 0x24, 0xd2, 0x50, 0x2f, 0xa0, 0x14, 0x72,
    0xe7, 0x60, 0x24, 0x17, 0x13, 0xda, 0xb0, 0x25, 0xae, 0x1b, 0x02, 0xe1,
    0x70, 0x3a, 0x14, 0x35, 0xf6, 0x2d, 0xdf, 0x4e, 0xe4, 0xc1, 0xb6, 0x64,
    0x06, 0x6e, 0xb2, 0x2f, 0x2e, 0x3b, 0xf2, 0x8b, 0xb7, 0x0a, 0x2a, 0x76,
    0xe4, 0xfd, 0x5e, 0xbe, 0x2d, 0x12, 0x29, 0x68, 0x1b, 0x5b, 0x06, 0x43,
    0x9a, 0xc9, 0xc7, 0xe9, 0xd8, 0xbd, 0xe2, 0x83
};
static const uint8_t dsa_sub_prime[] = {
/* Q */
    0xf8, 0x5f, 0x0f, 0x83, 0xac, 0x4d, 0xf7, 0xea, 0x0c, 0xdf, 0x8f, 0x46,
    0x9b, 0xfe, 0xea, 0xea, 0x14, 0x15, 0x64, 0x95
};
static const uint8_t dsa_base[] = {
/* G */
    0x2b, 0x31, 0x52, 0xff, 0x6c, 0x62, 0xf1, 0x46, 0x22, 0xb8, 0xf4, 0x8e,
    0x59, 0xf8, 0xaf, 0x46, 0x88, 0x3b, 0x38, 0xe7, 0x9b, 0x8c, 0x74, 0xde,
    0xea, 0xe9, 0xdf, 0x13, 0x1f, 0x8b, 0x85, 0x6e, 0x3a, 0xd6, 0xc8, 0x45,
    0x5d, 0xab, 0x87, 0xcc, 0x0d, 0xa8, 0xac, 0x97, 0x34, 0x17, 0xce, 0x4f,
    0x78, 0x78, 0x55, 0x7d, 0x6c, 0xdf, 0x40, 0xb3, 0x5b, 0x4a, 0x0c, 0xa3,
    0xeb, 0x31, 0x0c, 0x6a, 0x95, 0xd6, 0x8c, 0xe2, 0x84, 0xad, 0x4e, 0x25,
    0xea, 0x28, 0x59, 0x16, 0x11, 0xee, 0x08, 0xb8, 0x44, 0x4b, 0xd6, 0x4b,
    0x25, 0xf3, 0xf7, 0xc5, 0x72, 0x41, 0x0d, 0xdf, 0xb3, 0x9c, 0xc7, 0x28,
    0xb9, 0xc9, 0x36, 0xf8, 0x5f, 0x41, 0x91, 0x29, 0x86, 0x99, 0x29, 0xcd,
    0xb9, 0x09, 0xa6, 0xa3, 0xa9, 0x9b, 0xbe, 0x08, 0x92, 0x16, 0x36, 0x81,
    0x71, 0xbd, 0x0b, 0xa8, 0x1d, 0xe4, 0xfe, 0x33
};
static const uint8_t dsa_ptx[] = {
/* Msg */
    0x3b, 0x46, 0x73, 0x6d, 0x55, 0x9b, 0xd4, 0xe0, 0xc2, 0xc1, 0xb2, 0x55,
    0x3a, 0x33, 0xad, 0x3c, 0x6c, 0xf2, 0x3c, 0xac, 0x99, 0x8d, 0x3d, 0x0c,
    0x0e, 0x8f, 0xa4, 0xb1, 0x9b, 0xca, 0x06, 0xf2, 0xf3, 0x86, 0xdb, 0x2d,
    0xcf, 0xf9, 0xdc, 0xa4, 0xf4, 0x0a, 0xd8, 0xf5, 0x61, 0xff, 0xc3, 0x08,
    0xb4, 0x6c, 0x5f, 0x31, 0xa7, 0x73, 0x5b, 0x5f, 0xa7, 0xe0, 0xf9, 0xe6,
    0xcb, 0x51, 0x2e, 0x63, 0xd7, 0xee, 0xa0, 0x55, 0x38, 0xd6, 0x6a, 0x75,
    0xcd, 0x0d, 0x42, 0x34, 0xb5, 0xcc, 0xf6, 0xc1, 0x71, 0x5c, 0xca, 0xaf,
    0x9c, 0xdc, 0x0a, 0x22, 0x28, 0x13, 0x5f, 0x71, 0x6e, 0xe9, 0xbd, 0xee,
    0x7f, 0xc1, 0x3e, 0xc2, 0x7a, 0x03, 0xa6, 0xd1, 0x1c, 0x5c, 0x5b, 0x36,
    0x85, 0xf5, 0x19, 0x00, 0xb1, 0x33, 0x71, 0x53, 0xbc, 0x6c, 0x4e, 0x8f,
    0x52, 0x92, 0x0c, 0x33, 0xfa, 0x37, 0xf4, 0xe7
};

static int myrand( void *rng_state, unsigned char *output, size_t len )
{
#if !defined(__OpenBSD__)
    size_t i;

    if( rng_state != NULL )
        rng_state  = NULL;

    for( i = 0; i < len; ++i )
        output[i] = rand();
#else
    if( rng_state != NULL )
        rng_state = NULL;

    arc4random_buf( output, len );
#endif /* !OpenBSD */

    return( 0 );
}

/*
 * Checkup routine
 */
int mbedtls_dsa_self_test( int verbose )
{
#define DSA_ARRAY(nm)  (nm), sizeof(nm)
    int ret = 0;
    mbedtls_mpi T;
    mbedtls_dsa_context dsa;
#if defined(MBEDTLS_SHA1_C)
    unsigned char sha1sum[20];
    unsigned char sig[MBEDTLS_DSA_MAX_LEN];
    size_t siglen = sizeof(sig);
#endif

    mbedtls_mpi_init( &T );
    mbedtls_dsa_init( &dsa );

    MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &dsa.p,
                                              DSA_ARRAY(dsa_prime) ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &dsa.q,
                                              DSA_ARRAY(dsa_sub_prime ) ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &dsa.g,
                                              DSA_ARRAY(dsa_base) ) );

    if( verbose != 0 )
        mbedtls_printf( "  DSA key validation: " );

    if( mbedtls_dsa_genkey( &dsa, myrand, NULL ) != 0 )
    {
        ret = 1;
        goto fail;
    }

    /* X randomly from {1...Q-1} */
    if( mbedtls_mpi_cmp_int( &dsa.x, 1 ) <= 0 )
    {
        ret = 1;
        goto fail;
    }

    if( mbedtls_mpi_cmp_mpi( &dsa.x, &dsa.q ) >= 0 )
    {
        ret = 1;
        goto fail;
    }

    /* Y := G^X mod P */
    MBEDTLS_MPI_CHK( mbedtls_mpi_exp_mod( &T, &dsa.g, &dsa.x, &dsa.p, NULL ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_cmp_mpi( &T, &dsa.y ) );

    if( verbose != 0 )
        mbedtls_printf( "passed\n" );

#if defined(MBEDTLS_SHA1_C)
    if( verbose != 0 )
        mbedtls_printf( "  DSA data sign     : " );

    if( mbedtls_sha1_ret( DSA_ARRAY(dsa_ptx), sha1sum ) != 0 )
    {
        ret = 1;
        goto fail;
    }

    siglen = sizeof(sig);
    if( mbedtls_dsa_write_signature( &dsa,
                                     MBEDTLS_MD_SHA1,
                                     sha1sum,
                                     sizeof(sha1sum),
                                     sig,
                                     &siglen,
                                     myrand,
                                     NULL ) != 0 )
    {
        ret = 1;
        goto fail;
    }

    if( verbose != 0 )
        mbedtls_printf( "passed\n  DSA sig. verify   : " );

    if( mbedtls_dsa_read_signature( &dsa,
                                    sha1sum,
                                    sizeof(sha1sum),
                                    sig,
                                    siglen ) != 0 )
    {
        ret = 1;
        goto fail;
    }

    if( verbose != 0 )
        mbedtls_printf( "passed\n" );
#endif /* MBEDTLS_SHA1_C */

    goto cleanup;

fail:
    if ( verbose != 0 )
        mbedtls_printf( "failed\n" );

cleanup:
    mbedtls_dsa_free( &dsa );
    mbedtls_mpi_free( &T );
    return( ret );
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_DSA_C */
