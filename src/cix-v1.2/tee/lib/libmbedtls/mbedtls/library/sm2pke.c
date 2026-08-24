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

#if defined(MBEDTLS_SM2PKE_C)

#include "mbedtls/sm2pke.h"
#include "mbedtls/sm2_internal.h"

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
#define SM2PKE_VALIDATE_RET(cond)                                              \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
#define SM2PKE_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

/* Implementation that should never be optimized out by the compiler */
static void mbedtls_zeroize(void *v, size_t n)
{
    volatile unsigned char *p = v;
    while (n--)
        *p++ = 0;
}

#if !defined(MBEDTLS_SM2PKE_GEN_PUBLIC_ALT)
/*
 * Generate public key: simple wrapper around mbedtls_ecp_gen_keypair
 */
int mbedtls_sm2pke_gen_public(mbedtls_mpi *d,
                              mbedtls_ecp_point *Q,
                              int (*f_rng)(void *, unsigned char *, size_t),
                              void *p_rng)
{
    int ret;
    mbedtls_ecp_group grp;
    SM2PKE_VALIDATE_RET(d != NULL);
    SM2PKE_VALIDATE_RET(Q != NULL);
    SM2PKE_VALIDATE_RET(f_rng != NULL);

    mbedtls_ecp_group_init(&grp);

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SM2P256V1);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_ecp_gen_keypair(&grp, d, Q, f_rng, p_rng);
    if (ret != 0) {
        goto cleanup;
    }
cleanup:
    mbedtls_ecp_group_free( &grp );
    return ret;
}
#endif /* !MBEDTLS_SM2PKE_GEN_PUBLIC_ALT */

/*
 * Initialize context
 */
void mbedtls_sm2pke_init(mbedtls_sm2pke_context *ctx)
{
    if (ctx == NULL)
        return;
    memset(ctx, 0, sizeof(mbedtls_sm2pke_context));
    mbedtls_ecp_group_init(&ctx->grp);
    mbedtls_mpi_init(&ctx->d);
    mbedtls_ecp_point_init(&ctx->Q);
    mbedtls_ecp_point_init(&ctx->QP);
}

/*
 * Free context
 */
void mbedtls_sm2pke_free(mbedtls_sm2pke_context *ctx)
{
    if (ctx == NULL)
        return;

    mbedtls_ecp_group_free(&ctx->grp);
    mbedtls_ecp_point_free(&ctx->Q);
    mbedtls_ecp_point_free(&ctx->QP);
    mbedtls_mpi_free(&ctx->d);
}

/*
 * Setup and write the ServerKeyExhange parameters (RFC 4492)
 *      struct {
 *          ECParameters    curve_params;
 *          ECPoint         public;
 *      } ServerECDHParams;
 */
int mbedtls_sm2pke_make_params(mbedtls_sm2pke_context *ctx,
                               size_t *olen,
                               unsigned char *buf,
                               size_t blen,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng)
{
    int ret;
    size_t grp_len, pt_len;
    SM2PKE_VALIDATE_RET(ctx != NULL);
    SM2PKE_VALIDATE_RET(ctx->grp.id == MBEDTLS_ECP_DP_SM2P256V1);
    SM2PKE_VALIDATE_RET(olen != NULL);
    SM2PKE_VALIDATE_RET(buf != NULL);
    SM2PKE_VALIDATE_RET(f_rng != NULL);
    SM2PKE_VALIDATE_RET(blen != 0);

    if ((ret = mbedtls_sm2pke_gen_public(
             &ctx->d, &ctx->Q, f_rng, p_rng)) != 0)
        return (ret);

    if ((ret = mbedtls_ecp_tls_write_group(&ctx->grp, &grp_len, buf, blen)) !=
        0)
        return (ret);

    buf += grp_len;
    blen -= grp_len;

    if ((ret = mbedtls_ecp_tls_write_point(
             &ctx->grp, &ctx->Q, ctx->point_format, &pt_len, buf, blen)) != 0)
        return (ret);

    *olen = grp_len + pt_len;
    return (0);
}

/*
 * Read the ServerKeyExhange parameters (RFC 4492)
 *      struct {
 *          ECParameters    curve_params;
 *          ECPoint         public;
 *      } ServerECDHParams;
 */
int mbedtls_sm2pke_read_params(mbedtls_sm2pke_context *ctx,
                               const unsigned char **buf,
                               const unsigned char *end)
{
    int ret;
    SM2PKE_VALIDATE_RET(ctx != NULL);
    SM2PKE_VALIDATE_RET(ctx->grp.id == MBEDTLS_ECP_DP_SM2P256V1);
    SM2PKE_VALIDATE_RET(buf != NULL);
    SM2PKE_VALIDATE_RET(*buf != NULL);
    SM2PKE_VALIDATE_RET(end != NULL);

    if ((ret = mbedtls_ecp_tls_read_group(&ctx->grp, buf, end - *buf)) != 0)
        return (ret);
    if ((ret = mbedtls_ecp_tls_read_point(
             &ctx->grp, &ctx->QP, buf, end - *buf)) != 0)
        return (ret);

    return (0);
}

#if !defined(MBEDTLS_SM2PKE_ENCRYPT)
int mbedtls_sm2pke_encrypt(mbedtls_sm2pke_context *ctx,
                           mbedtls_md_type_t md_alg,
                           unsigned char *output,
                           size_t *olen,
                           const unsigned char *input, /*in*/
                           size_t ilen,                /*in*/
                           int (*f_rng)(void *, unsigned char *, size_t),
                           void *p_rng)
{
    int ret;
    const mbedtls_md_info_t *md_info;
    unsigned char *tmpbuf1 = NULL, *tmpbuf2 = NULL;
    size_t plen = 0, mlen = 0;
    size_t pt_len, i;
    mbedtls_mpi k;
    mbedtls_ecp_point C1, R;
    unsigned char *c2, *c3;

    SM2PKE_VALIDATE_RET(ctx != NULL);
    SM2PKE_VALIDATE_RET(ctx->grp.id == MBEDTLS_ECP_DP_SM2P256V1);
    SM2PKE_VALIDATE_RET(output != NULL);
    SM2PKE_VALIDATE_RET(olen != NULL);
    SM2PKE_VALIDATE_RET(input != NULL);
    SM2PKE_VALIDATE_RET(ilen != 0);
    SM2PKE_VALIDATE_RET(f_rng != NULL);

    if ((md_info = mbedtls_md_info_from_type(md_alg)) == NULL)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    plen = ctx->grp.pbits / 8 + ((ctx->grp.pbits % 8) != 0);
    mlen = mbedtls_md_get_size(md_info);

    if (ctx->point_format == MBEDTLS_ECP_PF_UNCOMPRESSED) {
        if (*olen < 2 * plen + ilen + mlen + 2) {
            *olen = 2 * plen + ilen + mlen + 2;
            return (MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL);
        }
    } else if (ctx->point_format == MBEDTLS_ECP_PF_COMPRESSED) {
        if (*olen < plen + ilen + mlen + 2) {
            *olen = plen + ilen + mlen + 2;
            return (MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL);
        }
    }

    tmpbuf1 =
        (unsigned char *)mbedtls_calloc(2 * plen + ilen, sizeof(unsigned char));
    if (tmpbuf1 == NULL) {
        mbedtls_printf("%s ERROR mbedtls_calloc failed !!!\n", __func__);
        return MBEDTLS_ERR_ECP_ALLOC_FAILED;
    }

    tmpbuf2 = (unsigned char *)mbedtls_calloc(2 * plen, sizeof(unsigned char));
    if (tmpbuf2 == NULL) {
        mbedtls_printf("%s ERROR mbedtls_calloc failed !!!\n", __func__);
        ret = MBEDTLS_ERR_ECP_ALLOC_FAILED;
        goto cleanup1;
    }

    mbedtls_mpi_init(&k);
    mbedtls_ecp_point_init(&C1);
    mbedtls_ecp_point_init(&R);

    if ((ret = mbedtls_sm2pke_gen_public(&k, &C1, f_rng, p_rng)) !=
        0)
        goto cleanup;

    if ((ret = mbedtls_ecp_point_write_binary(
             &ctx->grp, &C1, ctx->point_format, &pt_len, output, *olen)) != 0)
        goto cleanup;

    c2 = output + pt_len;
    if ((ret = mbedtls_ecp_mul(&ctx->grp, &R, &k, &ctx->QP, f_rng, p_rng)) != 0)
        goto cleanup;
    if ((ret = mbedtls_mpi_write_binary(&R.X, tmpbuf2, plen)) != 0)
        goto cleanup;

    if ((ret = mbedtls_mpi_write_binary(&R.Y, tmpbuf2 + plen, plen)) != 0)
        goto cleanup;

    if ((ret = mbedtls_sm2_kdf(md_info, tmpbuf2, 2 * plen, ilen, tmpbuf1)) != 0)
        goto cleanup;

    /*need to check if tmpbuf1 is all 0*/

    for (i = 0; i < ilen; i++) {
        c2[i] = input[i] ^ tmpbuf1[i];
    }
    mbedtls_zeroize(tmpbuf1, ilen);

    if ((ret = mbedtls_mpi_write_binary(&R.X, tmpbuf1, plen)) != 0)
        goto cleanup;

    memcpy(tmpbuf1 + plen, input, ilen);

    if ((ret = mbedtls_mpi_write_binary(&R.Y, tmpbuf1 + plen + ilen, plen)) !=
        0)
        goto cleanup;

    c3 = c2 + ilen;

    if ((ret = mbedtls_md(md_info, tmpbuf1, 2 * plen + ilen, c3)) != 0)
        goto cleanup;

    *olen = pt_len + ilen + mlen;
cleanup:
    mbedtls_mpi_free(&k);
    mbedtls_ecp_point_free(&C1);
    mbedtls_ecp_point_free(&R);
cleanup1:
    if (ret) {
        mbedtls_zeroize(output, *olen);
    }
    if (tmpbuf2 != NULL) {
        mbedtls_zeroize(tmpbuf2, 2 * plen);
        mbedtls_free(tmpbuf2);
    }
    if (tmpbuf1 != NULL) {
        mbedtls_zeroize(tmpbuf1, 2 * plen + ilen);
        mbedtls_free(tmpbuf1);
    }
    return ret;
}
#endif /* !MBEDTLS_SM2PKE_ENCRYPT */

#if !defined(MBEDTLS_SM2PKE_DECRYPT)
int mbedtls_sm2pke_decrypt(mbedtls_sm2pke_context *ctx,
                           mbedtls_md_type_t md_alg,
                           unsigned char *output,
                           size_t *olen,
                           const unsigned char *input, /*in*/
                           size_t ilen,                /*in*/
                           int (*f_rng)(void *, unsigned char *, size_t),
                           void *p_rng)
{
    int ret;
    const mbedtls_md_info_t *md_info;
    unsigned char *tmpbuf1 = NULL, *tmpbuf2 = NULL;
    size_t plen = 0, mlen = 0;
    size_t pt_len, t_olen, i;
    mbedtls_ecp_point C1, R;
    const unsigned char *c1, *c2, *c3;

    SM2PKE_VALIDATE_RET(ctx != NULL);
    SM2PKE_VALIDATE_RET(ctx->grp.id == MBEDTLS_ECP_DP_SM2P256V1);
    SM2PKE_VALIDATE_RET(output != NULL);
    SM2PKE_VALIDATE_RET(olen != NULL);
    SM2PKE_VALIDATE_RET(input != NULL);
    SM2PKE_VALIDATE_RET(ilen != 0);

    if ((md_info = mbedtls_md_info_from_type(md_alg)) == NULL)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    plen   = ctx->grp.pbits / 8 + ((ctx->grp.pbits % 8) != 0);
    mlen   = mbedtls_md_get_size(md_info);
    pt_len = 2 * plen + 1;

    if (ilen <= mlen + pt_len) {
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    }

    tmpbuf1 = (unsigned char *)mbedtls_calloc(ilen, sizeof(unsigned char));
    if (tmpbuf1 == NULL) {
        mbedtls_printf("%s ERROR mbedtls_calloc failed !!!\n", __func__);
        return MBEDTLS_ERR_ECP_ALLOC_FAILED;
    }
    tmpbuf2 = (unsigned char *)mbedtls_calloc(ilen, sizeof(unsigned char));
    if (tmpbuf2 == NULL) {
        mbedtls_printf("%s ERROR mbedtls_calloc failed !!!\n", __func__);
        ret = MBEDTLS_ERR_ECP_ALLOC_FAILED;
        goto cleanup1;
    }
    mbedtls_ecp_point_init(&C1);
    mbedtls_ecp_point_init(&R);

    c1 = input;
    if ((ret = mbedtls_ecp_point_read_binary(&ctx->grp, &C1, c1, pt_len)) != 0)
        goto cleanup;

    mbedtls_zeroize(tmpbuf1, ilen);

    if ((ret = mbedtls_ecp_check_pubkey(&ctx->grp, &C1)) != 0)
        goto cleanup;

    c2 = input + pt_len;

    if (*olen + pt_len + mlen < ilen) {
        *olen = ilen - pt_len - mlen;
        ret   = MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    t_olen = ilen - pt_len - mlen;

    if ((ret = mbedtls_ecp_mul(&ctx->grp, &R, &ctx->d, &C1, f_rng, p_rng)) != 0)
        goto cleanup;
    if ((ret = mbedtls_mpi_write_binary(&R.X, tmpbuf2, plen)) != 0)
        goto cleanup;

    if ((ret = mbedtls_mpi_write_binary(&R.Y, tmpbuf2 + plen, plen)) != 0)
        goto cleanup;

    if ((ret = mbedtls_sm2_kdf(md_info, tmpbuf2, 2 * plen, t_olen, tmpbuf1)) !=
        0)
        goto cleanup;

    /*need to check if tmpbuf1 is all 0*/

    for (i = 0; i < t_olen; i++) {
        output[i] = c2[i] ^ tmpbuf1[i];
    }
    mbedtls_zeroize(tmpbuf1, ilen);
    mbedtls_zeroize(tmpbuf2, ilen);
    if ((ret = mbedtls_mpi_write_binary(&R.X, tmpbuf2, plen)) != 0)
        goto cleanup;

    memcpy(tmpbuf2 + plen, output, t_olen);

    if ((ret = mbedtls_mpi_write_binary(&R.Y, tmpbuf2 + plen + t_olen, plen)) !=
        0)
        goto cleanup;

    if ((ret = mbedtls_md(md_info, tmpbuf2, 2 * plen + t_olen, tmpbuf1)) != 0)
        goto cleanup;

    c3 = c2 + t_olen;
    if ((ret = memcmp(c3, tmpbuf1, mlen)) != 0) {
        ret = MBEDTLS_ERR_ECP_SM_DECRYPT_FAILED;
        goto cleanup;
    }

    *olen = t_olen;
cleanup:
    mbedtls_ecp_point_free(&C1);
    mbedtls_ecp_point_free(&R);
cleanup1:
    if (ret) {
        mbedtls_zeroize(output, *olen);
    }
    if (tmpbuf2 != NULL) {
        mbedtls_zeroize(tmpbuf2, ilen);
        mbedtls_free(tmpbuf2);
    }
    if (tmpbuf1 != NULL) {
        mbedtls_zeroize(tmpbuf1, ilen);
        mbedtls_free(tmpbuf1);
    }
    return ret;
}
#endif /* !MBEDTLS_SM2PKE_DECRYPT */

#if defined(MBEDTLS_SELF_TEST)

#include <mbedtls/sm3.h>

/*
 * G/MT 0003 (SM2) Part 5 Annex C.2 - encryption/decryption
 */

#define SM2PKE_D   "3945208F7B2144B13F36E38AC6D39F95" \
                   "889393692860B51A42FB81EF4DF7C5B8"

#define SM2PKE_QX  "09F9DF311E5421A150DD7D161E4BC5C6" \
                   "72179FAD1833FC076BB08FF356F35020"

#define SM2PKE_QY  "CCEA490CE26775A52DC6EA718CC1AA60" \
                   "0AED05FBF35E084A6632F6072DA9AD13"

static const uint8_t sm2pke_ptx[19] =
/* M */
	"encryption standard";

static const uint8_t sm2pke_ctx[] = {
/* C */
	/* C1 */
	0x04,
	0x04, 0xEB, 0xFC, 0x71, 0x8E, 0x8D, 0x17, 0x98, 0x62, 0x04, 0x32, 0x26,
	0x8E, 0x77, 0xFE, 0xB6, 0x41, 0x5E, 0x2E, 0xDE, 0x0E, 0x07, 0x3C, 0x0F,
	0x4F, 0x64, 0x0E, 0xCD, 0x2E, 0x14, 0x9A, 0x73, 0xE8, 0x58, 0xF9, 0xD8,
	0x1E, 0x54, 0x30, 0xA5, 0x7B, 0x36, 0xDA, 0xAB, 0x8F, 0x95, 0x0A, 0x3C,
	0x64, 0xE6, 0xEE, 0x6A, 0x63, 0x09, 0x4D, 0x99, 0x28, 0x3A, 0xFF, 0x76,
	0x7E, 0x12, 0x4D, 0xF0,
	/* C2 */
	0x21, 0x88, 0x6C, 0xA9, 0x89, 0xCA, 0x9C, 0x7D, 0x58, 0x08, 0x73, 0x07,
	0xCA, 0x93, 0x09, 0x2D, 0x65, 0x1E, 0xFA,
	/* C3 */
	0x59, 0x98, 0x3C, 0x18, 0xF8, 0x09, 0xE2, 0x62, 0x92, 0x3C, 0x53, 0xAE,
	0xC2, 0x95, 0xD3, 0x03, 0x83, 0xB5, 0x4E, 0x39, 0xD6, 0x09, 0xD1, 0x60,
	0xAF, 0xCB, 0x19, 0x08, 0xD0, 0xBD, 0x87, 0x66
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
int mbedtls_sm2pke_self_test( int verbose )
{
    int ret = 0, i = 0;
    mbedtls_sm2pke_context sm2pke;
    mbedtls_ecp_group_id gid = MBEDTLS_ECP_DP_SM2P256V1;
    unsigned char out[512];
    size_t olen = 0;
    unsigned char oute[512];
    size_t oelen;

    mbedtls_sm2pke_init( &sm2pke );
    if( mbedtls_ecp_group_load( &sm2pke.grp, gid ) != 0 )
    {
        ret = 1;
        goto fail;
    }

    for ( i = 0; i < 2; i++ )
    {
        if( verbose != 0 )
            mbedtls_printf( "  SM2PKE key validation #%d: ", i + 1 );

        if ( i & 1 )
        {
            /* load keypair */

            MBEDTLS_MPI_CHK( mbedtls_mpi_read_string( &sm2pke.d, 16, SM2PKE_D ) );
            if( mbedtls_ecp_point_read_string( &sm2pke.Q,
                                               16,
                                               SM2PKE_QX,
                                               SM2PKE_QY ) != 0 )
            {
                ret = 1;
                goto fail;
            }
        }
        else
        {
            /* gen keypair */
            if( mbedtls_sm2pke_gen_public( &sm2pke.d,
                                           &sm2pke.Q,
                                           myrand,
                                           NULL ) != 0 )
            {
                ret = 1;
                goto fail;
            }
        }

        /*
         * We are using the same keypair in between decryption and encryption.
         * So, QP = Q.
         */
        if( mbedtls_ecp_copy( &sm2pke.QP, &sm2pke.Q ) != 0 )
        {
            ret = 1;
            goto fail;
        }

        /*
         * validate keypair
         */
        if( mbedtls_ecp_check_pubkey( &sm2pke.grp, &sm2pke.Q ) ||
            mbedtls_ecp_check_privkey( &sm2pke.grp, &sm2pke.d ) )
        {
            ret = 1;
            goto fail;
        }

        if( verbose != 0 )
            mbedtls_printf( "passed\n" );

        /* decrypt */
        if( verbose != 0 )
            mbedtls_printf( "  SM2PKE decrypt        #%d: ", i + 1 );

        if ( 0 == i )
        {
            /*
             * the ctx for the generated key is not available yet so skip it.
             */
            if( verbose != 0 )
                mbedtls_printf( "skipped\n" );
        }
        else
        {
            olen = sizeof(out);
            memset(out, 0, olen);
            if( mbedtls_sm2pke_decrypt( &sm2pke,
                                        MBEDTLS_MD_SM3,
                                        out,
                                        &olen,
                                        sm2pke_ctx,
                                        sizeof(sm2pke_ctx),
                                        myrand,
                                        NULL ) != 0 )
            {
                ret = 1;
                goto fail;
            }

            if( olen != sizeof(sm2pke_ptx) || memcmp(sm2pke_ptx, out, olen) )
            {
                ret = 1;
                goto fail;
            }

            if( verbose != 0 )
                mbedtls_printf( "passed\n" );
        }

        /* encrypt */
        if( verbose != 0 )
            mbedtls_printf( "  SM2PKE encrypt        #%d: ", i + 1 );

        oelen = sizeof(oute);
        memset(oute, 0, oelen);
        if( mbedtls_sm2pke_encrypt( &sm2pke,
                                    MBEDTLS_MD_SM3,
                                    oute,
                                    &oelen,
                                    sm2pke_ptx,
                                    sizeof(sm2pke_ptx),
                                    myrand,
                                    NULL ) != 0 )
        {
            ret = 1;
            goto fail;
        }

        /*
         * Randomness (k) is added when encrypting so we can't
         * verify against the precomputed values. For instead,
         * we use the decryption operation to see the result is
         * correct.
         */
        olen = sizeof(out);
        memset(out, 0, olen);
        if( mbedtls_sm2pke_decrypt( &sm2pke,
                                    MBEDTLS_MD_SM3,
                                    out,
                                    &olen,
                                    oute,
                                    oelen,
                                    myrand,
                                    NULL ) != 0 )
        {
            ret = 1;
            goto fail;
        }

        if( olen != sizeof(sm2pke_ptx) || memcmp(sm2pke_ptx, out, olen) )
        {
            ret = 1;
            goto fail;
        }

        if( verbose != 0 )
            mbedtls_printf( "passed\n" );
    }

    goto cleanup;

fail:
    if ( verbose != 0 )
        mbedtls_printf( "failed\n" );

cleanup:
    mbedtls_sm2pke_free( &sm2pke );

    return( ret );
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_SM2PKE_C */
