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

#if defined(MBEDTLS_SM2DSA_C)

#include "mbedtls/sm2dsa.h"
#include "mbedtls/asn1write.h"

#include <string.h>

#if defined(MBEDTLS_SM2DSA_DETERMINISTIC)
#include "mbedtls/hmac_drbg.h"
#endif

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
#define SM2DSA_VALIDATE_RET(cond)                                              \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
#define SM2DSA_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

#if defined(MBEDTLS_SM2DSA_DETERMINISTIC)
/*
 * Derive a suitable integer for group grp from a buffer of length len
 * SEC1 4.1.3 step 5 aka SEC1 4.1.4 step 3
 */
static int derive_mpi(const mbedtls_ecp_group *grp,
                      mbedtls_mpi *x,
                      const unsigned char *buf,
                      size_t blen)
{
    int ret;
    size_t n_size   = (grp->nbits + 7) / 8;
    size_t use_size = blen > n_size ? n_size : blen;

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(x, buf, use_size));
    if (use_size * 8 > grp->nbits)
        MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(x, use_size * 8 - grp->nbits));

    /* While at it, reduce modulo N */
    if (mbedtls_mpi_cmp_mpi(x, &grp->N) >= 0)
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(x, x, &grp->N));

cleanup:
    return (ret);
}
#endif /* MBEDTLS_SM2DSA_DETERMINISTIC */

#if !defined(MBEDTLS_SM2DSA_SIGN_ALT)
int mbedtls_sm2dsa_sign(mbedtls_mpi *r,
                        mbedtls_mpi *s,
                        const mbedtls_mpi *d,
                        const unsigned char *buf,
                        size_t blen,
                        int (*f_rng)(void *, unsigned char *, size_t),
                        void *p_rng)
{
    int ret, key_tries, sign_tries;
    int blind_tries;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point R;
    mbedtls_mpi k, e, t;

    SM2DSA_VALIDATE_RET(r != NULL);
    SM2DSA_VALIDATE_RET(s != NULL);
    SM2DSA_VALIDATE_RET(d != NULL);
    SM2DSA_VALIDATE_RET(f_rng != NULL);
    SM2DSA_VALIDATE_RET(buf != NULL || blen == 0);

    /* Load SM2 group */
    mbedtls_ecp_group_init(&grp);

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SM2P256V1);
    if (ret != 0) {
        goto cleanup;
    }

    /* Make sure d is in range 1..n-1 */
    if (mbedtls_mpi_cmp_int(d, 1) < 0 || mbedtls_mpi_cmp_mpi(d, &grp.N) >= 0) {
        ret = (MBEDTLS_ERR_ECP_INVALID_KEY);
        goto cleanup;
    }

    mbedtls_ecp_point_init(&R);
    mbedtls_mpi_init(&k);
    mbedtls_mpi_init(&e);
    mbedtls_mpi_init(&t);

    /* import digest to e */
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&e, buf, blen));

    sign_tries = 0;
    do {
        /*
         * Steps 1-5: generate a suitable ephemeral keypair
         * and set r = e+xR mod n
         */
        key_tries = 0;
        do {
            MBEDTLS_MPI_CHK(
                mbedtls_ecp_gen_keypair(&grp, &k, &R, f_rng, p_rng));
            MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(r, &e, &R.X));
            MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(r, r, &grp.N));
            MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&t, r, &k));

            if (key_tries++ > 10) {
                ret = MBEDTLS_ERR_ECP_RANDOM_FAILED;
                goto cleanup;
            }
        } while ((mbedtls_mpi_cmp_int(r, 0) == 0) ||
                 (mbedtls_mpi_cmp_mpi(&t, &grp.N) == 0));

        /*
         * Generate a random value to blind inv_mod in next step,
         * avoiding a potential timing leak.
         */
        blind_tries = 0;
        do {
            size_t n_size = (grp.nbits + 7) / 8;
            MBEDTLS_MPI_CHK(mbedtls_mpi_fill_random(&t, n_size, f_rng, p_rng));
            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&t, 8 * n_size - grp.nbits));

            /* See mbedtls_ecp_gen_keypair() */
            if (++blind_tries > 30)
                return (MBEDTLS_ERR_ECP_RANDOM_FAILED);
        } while (mbedtls_mpi_cmp_int(&t, 1) < 0 ||
                 mbedtls_mpi_cmp_mpi(&t, &grp.N) >= 0);
        /*
         * Step 6: compute s = (k-r*d) / (1 + d) = t*(k-r*d) / ((1 + d)*t) mod n
         */
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&e, r, d));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&e, &e, &grp.N));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&k, &k, &grp.N));
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&e, &k, &e));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&e, &e, &grp.N));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&e, &e, &t));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&e, &e, &grp.N));
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&k, d, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&k, &k, &t));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&k, &k, &grp.N));
        MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod(s, &k, &grp.N));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(s, s, &e));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(s, s, &grp.N));

        if (sign_tries++ > 10) {
            ret = MBEDTLS_ERR_ECP_RANDOM_FAILED;
            goto cleanup;
        }
    } while (mbedtls_mpi_cmp_int(s, 0) == 0);

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&R);
    mbedtls_mpi_free(&k);
    mbedtls_mpi_free(&e);
    mbedtls_mpi_free(&t);

    return (ret);
}
#endif /* !MBEDTLS_SM2DSA_SIGN_ALT */

#if defined(MBEDTLS_SM2DSA_DETERMINISTIC)

int mbedtls_sm2dsa_sign_det(mbedtls_mpi *r,
                            mbedtls_mpi *s,
                            const mbedtls_mpi *d,
                            const unsigned char *buf,
                            size_t blen,
                            mbedtls_md_type_t md_alg)
{
    int ret;
    mbedtls_hmac_drbg_context rng_ctx;
    mbedtls_hmac_drbg_context *p_rng = &rng_ctx;
    unsigned char data[2 * MBEDTLS_ECP_MAX_BYTES];
    mbedtls_ecp_group grp;
    size_t grp_len;
    const mbedtls_md_info_t *md_info;
    mbedtls_mpi h;

    SM2DSA_VALIDATE_RET(r != NULL);
    SM2DSA_VALIDATE_RET(s != NULL);
    SM2DSA_VALIDATE_RET(d != NULL);
    SM2DSA_VALIDATE_RET(buf != NULL || blen == 0);

    if ((md_info = mbedtls_md_info_from_type(md_alg)) == NULL)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&h);
    mbedtls_hmac_drbg_init(&rng_ctx);

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SM2P256V1);
    if (ret != 0) {
        goto cleanup;
    }

    grp_len = (grp.nbits + 7) / 8;

    /* Use private key and message hash (reduced) to initialize HMAC_DRBG */
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(d, data, grp_len));
    MBEDTLS_MPI_CHK(derive_mpi(&grp, &h, buf, blen));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&h, data + grp_len, grp_len));
    mbedtls_hmac_drbg_seed_buf(p_rng, md_info, data, 2 * grp_len);

    MBEDTLS_MPI_CHK(mbedtls_sm2dsa_sign(r, s, d, buf, blen,
                                        mbedtls_hmac_drbg_random, p_rng));

cleanup:
    mbedtls_hmac_drbg_free(&rng_ctx);
    mbedtls_mpi_free(&h);
    mbedtls_ecp_group_free(&grp);

    return (ret);
}
#endif /* MBEDTLS_SM2DSA_DETERMINISTIC */

#if !defined(MBEDTLS_SM2DSA_VERIFY_ALT)
int mbedtls_sm2dsa_verify(const unsigned char *buf,
                          size_t blen,
                          const mbedtls_ecp_point *Q,
                          const mbedtls_mpi *r,
                          const mbedtls_mpi *s)
{
    int ret;
    mbedtls_mpi e, u1, u2;
    mbedtls_ecp_point R1;
    mbedtls_ecp_group grp;

    SM2DSA_VALIDATE_RET(Q != NULL);
    SM2DSA_VALIDATE_RET(r != NULL);
    SM2DSA_VALIDATE_RET(s != NULL);
    SM2DSA_VALIDATE_RET(buf != NULL || blen == 0);

    /* Load SM2 group */
    mbedtls_ecp_group_init(&grp);

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SM2P256V1);
    if (ret != 0) {
        goto cleanup;
    }

    mbedtls_ecp_point_init(&R1);
    mbedtls_mpi_init(&e);
    mbedtls_mpi_init(&u1);
    mbedtls_mpi_init(&u2);

    /*
     * Step 1: make sure r and s are in range 1..n-1
     */
    if (mbedtls_mpi_cmp_int(r, 1) < 0 || mbedtls_mpi_cmp_mpi(r, &grp.N) >= 0 ||
        mbedtls_mpi_cmp_int(s, 1) < 0 || mbedtls_mpi_cmp_mpi(s, &grp.N) >= 0) {
        ret = MBEDTLS_ERR_ECP_VERIFY_FAILED;
        goto cleanup;
    }

    /*
     * Additional precaution: make sure Q is valid
     */
    MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(&grp, Q));

    /*
     * Step 3: Import MPI from hashed message
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&e, buf, blen));

    /*
     * Step 4: t = (r+s)mod n
     */

    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&u2, r, s));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&u2, &u2, &grp.N));

    if (mbedtls_mpi_cmp_int(&u2, 0) == 0) {
        ret = MBEDTLS_ERR_ECP_VERIFY_FAILED;
        goto cleanup;
    }

    /*
     * Step 5: R = s G + u2 Q
     *
     */
    MBEDTLS_MPI_CHK(mbedtls_ecp_muladd(&grp, &R1, s, &grp.G, &u2, Q));
    /*
     * Step 6: ( xR + e ) mod n
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&u1, &R1.X, &e));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&u1, &u1, &grp.N));

    /*
     * Step 7: check if v (that is, u1) is equal to r
     */
    if (mbedtls_mpi_cmp_mpi(&u1, r) != 0) {
        ret = MBEDTLS_ERR_ECP_VERIFY_FAILED;
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&R1);
    mbedtls_mpi_free(&e);
    mbedtls_mpi_free(&u1);
    mbedtls_mpi_free(&u2);

    return (ret);
}
#endif /* !MBEDTLS_SM2DSA_VERIFY_ALT */
/*
 * Convert a signature (given by context) to ASN.1
 */
static int ecdsa_signature_to_asn1(const mbedtls_mpi *r,
                                   const mbedtls_mpi *s,
                                   unsigned char *sig,
                                   size_t *slen)
{
    int ret;
    unsigned char buf[MBEDTLS_SM2DSA_MAX_LEN];
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
int mbedtls_sm2dsa_write_signature(mbedtls_sm2dsa_context *ctx,
                                   mbedtls_md_type_t md_alg,
                                   const unsigned char *hash,
                                   size_t hlen,
                                   unsigned char *sig,
                                   size_t *slen,
                                   int (*f_rng)(void *,
                                                unsigned char *,
                                                size_t),
                                   void *p_rng)
{
    int ret;
    mbedtls_mpi r, s;
    SM2DSA_VALIDATE_RET(ctx != NULL);
    SM2DSA_VALIDATE_RET(ctx->grp.id == MBEDTLS_ECP_DP_SM2P256V1);
    SM2DSA_VALIDATE_RET(hash != NULL);
    SM2DSA_VALIDATE_RET(hlen != 0);
    SM2DSA_VALIDATE_RET(sig != NULL);
    SM2DSA_VALIDATE_RET(slen != NULL);
    SM2DSA_VALIDATE_RET(f_rng != NULL);

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

#if defined(MBEDTLS_SM2DSA_DETERMINISTIC)
    (void)f_rng;
    (void)p_rng;

    MBEDTLS_MPI_CHK(
        mbedtls_sm2dsa_sign_det(&r, &s, &ctx->d, hash, hlen, md_alg));
#else
    (void)md_alg;
    MBEDTLS_MPI_CHK(
        mbedtls_sm2dsa_sign(&r, &s, &ctx->d, hash, hlen, f_rng, p_rng));
#endif /* MBEDTLS_SM2DSA_DETERMINISTIC */

    MBEDTLS_MPI_CHK(ecdsa_signature_to_asn1(&r, &s, sig, slen));

cleanup:
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);

    return (ret);
}

/*
 * Restartable read and check signature
 */
int mbedtls_sm2dsa_read_signature(mbedtls_sm2dsa_context *ctx,
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
    SM2DSA_VALIDATE_RET(ctx != NULL);
    SM2DSA_VALIDATE_RET(ctx->grp.id == MBEDTLS_ECP_DP_SM2P256V1);
    SM2DSA_VALIDATE_RET(hash != NULL);
    SM2DSA_VALIDATE_RET(hlen != 0);
    SM2DSA_VALIDATE_RET(sig != NULL);
    SM2DSA_VALIDATE_RET(slen != 0);

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED |
                                        MBEDTLS_ASN1_SEQUENCE)) != 0) {
        ret += MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

    if (p + len != end) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA + MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;
        goto cleanup;
    }

    if ((ret = mbedtls_asn1_get_mpi(&p, end, &r)) != 0 ||
        (ret = mbedtls_asn1_get_mpi(&p, end, &s)) != 0) {
        ret += MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

    if ((ret = mbedtls_sm2dsa_verify(hash, hlen, &ctx->Q, &r, &s)) != 0)
        goto cleanup;

    /* At this point we know that the buffer starts with a valid signature.
     * Return 0 if the buffer just contains the signature, and a specific
     * error code if the valid signature is followed by more data. */
    if (p != end)
        ret = MBEDTLS_ERR_ECP_SIG_LEN_MISMATCH;

cleanup:
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);

    return (ret);
}

#if !defined(MBEDTLS_SM2DSA_GENKEY_ALT)

/*
 * Generate key pair
 */
int mbedtls_sm2dsa_genkey(mbedtls_sm2dsa_context *ctx,
                          int (*f_rng)(void *, unsigned char *, size_t),
                          void *p_rng)
{
    int ret = 0;
    SM2DSA_VALIDATE_RET(ctx != NULL);
    SM2DSA_VALIDATE_RET(f_rng != NULL);

    ret = mbedtls_ecp_group_load(&ctx->grp, MBEDTLS_ECP_DP_SM2P256V1);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_ecp_gen_keypair(&ctx->grp, &ctx->d, &ctx->Q, f_rng, p_rng);
    if (ret != 0) {
        goto cleanup;
    }

cleanup:
    return ret;
}
#endif /* !MBEDTLS_SM2DSA_GENKEY_ALT */
/*
 * Set context from an mbedtls_ecp_keypair
 */
int mbedtls_sm2dsa_from_keypair(mbedtls_sm2dsa_context *ctx,
                                const mbedtls_ecp_keypair *key)
{
    int ret;
    SM2DSA_VALIDATE_RET(ctx != NULL);
    SM2DSA_VALIDATE_RET(key != NULL);
    SM2DSA_VALIDATE_RET(key->grp.id == MBEDTLS_ECP_DP_SM2P256V1);

    if ((ret = mbedtls_ecp_group_copy(&ctx->grp, &key->grp)) != 0 ||
        (ret = mbedtls_mpi_copy(&ctx->d, &key->d)) != 0 ||
        (ret = mbedtls_ecp_copy(&ctx->Q, &key->Q)) != 0) {
        mbedtls_sm2dsa_free(ctx);
    }

    return (ret);
}

/*
 * Initialize context
 */
void mbedtls_sm2dsa_init(mbedtls_sm2dsa_context *ctx)
{
    SM2DSA_VALIDATE(ctx != NULL);

    mbedtls_mpi_init(&ctx->d);
    mbedtls_ecp_point_init(&ctx->Q);
    mbedtls_ecp_group_init(&ctx->grp);
}

/*
 * Free context
 */
void mbedtls_sm2dsa_free(mbedtls_sm2dsa_context *ctx)
{
    if (ctx == NULL)
        return;

    mbedtls_mpi_free(&ctx->d);
    mbedtls_ecp_point_free(&ctx->Q);
    mbedtls_ecp_group_free(&ctx->grp);
}

/**
 * Compute ZA
 */
int mbedtls_sm2_compute_id_digest( mbedtls_md_type_t md_type,
                                   const mbedtls_ecp_point *Q,
                                   const char *id,
                                   size_t idlen,
                                   unsigned char *za )
{
    int ret;
    unsigned char entl[2];
    unsigned char *buf = NULL;
    size_t p_size;
    mbedtls_md_context_t ctx;
    mbedtls_ecp_group grp;
    mbedtls_md_info_t *md_info;

    SM2DSA_VALIDATE_RET(md_type != MBEDTLS_MD_NONE);
    SM2DSA_VALIDATE_RET(Q != NULL);
    SM2DSA_VALIDATE_RET(id != NULL);
    SM2DSA_VALIDATE_RET(za != NULL);
    SM2DSA_VALIDATE_RET(idlen <= 0xFFFF / 8);

    /* init group */
    mbedtls_ecp_group_init(&grp);

    /* init md ctx */
    mbedtls_md_init( &ctx );

    md_info = (mbedtls_md_info_t *)mbedtls_md_info_from_type( md_type );
    if (md_info == NULL) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto exit;
    }

    if ((mbedtls_md_get_size(md_info)) != 256 / 8) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto exit;
    }

    /* load sm2 group */
    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SM2P256V1);
    if (ret != 0) {
        goto exit;
    }

    /* check user pubkey */
    ret = mbedtls_ecp_check_pubkey((const mbedtls_ecp_group *)(&grp), Q);
    if (ret != 0) {
        mbedtls_printf("111 %s %d : ret = - 0x%x\n", __func__, __LINE__, -ret);
        goto exit;
    }

    /* prepare buffer */
    p_size = (grp.pbits + 7) / 8;
    buf = mbedtls_calloc(1, p_size);
    if (buf == NULL) {
        return MBEDTLS_ERR_ECP_ALLOC_FAILED;
    }

    /* start md context */
    ret = mbedtls_md_setup(&ctx, md_info, 0);
    if (ret != 0) {
        goto exit;
    }
    ret = mbedtls_md_starts(&ctx);
    if (ret != 0) {
        goto exit;
    }

    /* update ENTL */
    entl[0] = ((idlen * 8) >> 8) & 0xFF;
    entl[1] = (idlen * 8) & 0xFF;
    ret = mbedtls_md_update(&ctx, (const unsigned char *)entl, sizeof(entl));
    if (ret != 0) {
        goto exit;
    }

    /* update ID */
    ret = mbedtls_md_update(&ctx, (const unsigned char *)id, idlen);
    if (ret != 0) {
        goto exit;
    }

    /* update A */
    ret = mbedtls_mpi_write_binary((const mbedtls_mpi *)(&grp.A), buf, p_size);
    if (ret != 0) {
        goto exit;
    }
    ret = mbedtls_md_update(&ctx, (const unsigned char *)buf, p_size);
    if (ret != 0) {
        goto exit;
    }

    /* update B */
    ret = mbedtls_mpi_write_binary((const mbedtls_mpi *)(&grp.B), buf, p_size);
    if (ret != 0) {
        goto exit;
    }
    ret = mbedtls_md_update(&ctx, (const unsigned char *)buf, p_size);
    if (ret != 0) {
        goto exit;
    }

    /* update GX */
    ret = mbedtls_mpi_write_binary((const mbedtls_mpi *)(&grp.G.X), buf, p_size);
    if (ret != 0) {
        goto exit;
    }
    ret = mbedtls_md_update(&ctx, (const unsigned char *)buf, p_size);
    if (ret != 0) {
        goto exit;
    }

    /* update GY */
    ret = mbedtls_mpi_write_binary((const mbedtls_mpi *)(&grp.G.Y), buf, p_size);
    if (ret != 0) {
        goto exit;
    }
    ret = mbedtls_md_update(&ctx, (const unsigned char *)buf, p_size);
    if (ret != 0) {
        goto exit;
    }

    /* update QX */
    ret = mbedtls_mpi_write_binary((const mbedtls_mpi *)(&Q->X), buf, p_size);
    if (ret != 0) {
        goto exit;
    }
    ret = mbedtls_md_update(&ctx, (const unsigned char *)buf, p_size);
    if (ret != 0) {
        goto exit;
    }

    /* update QY */
    ret = mbedtls_mpi_write_binary((const mbedtls_mpi *)(&Q->Y), buf, p_size);
    if (ret != 0) {
        goto exit;
    }
    ret = mbedtls_md_update(&ctx, (const unsigned char *)buf, p_size);
    if (ret != 0) {
        goto exit;
    }

    /* Calculate result */
    ret = mbedtls_md_finish( &ctx, za );
    if (ret != 0) {
        goto exit;
    }

exit:
    if (buf) {
        mbedtls_free(buf);
    }
    mbedtls_ecp_group_free(&grp);
    mbedtls_md_free(&ctx);
    return ret;
}

#if defined(MBEDTLS_SELF_TEST)

#include <mbedtls/sm3.h>

/*
 * G/MT 0003 (SM2) Part 5 Annex A.2 - SM2 digital signature based on
 * elliptic curves
 */

#define SM2DSA_D                                                               \
    "3945208F7B2144B13F36E38AC6D39F95"                                         \
    "889393692860B51A42FB81EF4DF7C5B8"

#define SM2DSA_QX                                                              \
    "09F9DF311E5421A150DD7D161E4BC5C6"                                         \
    "72179FAD1833FC076BB08FF356F35020"

#define SM2DSA_QY                                                              \
    "CCEA490CE26775A52DC6EA718CC1AA60"                                         \
    "0AED05FBF35E084A6632F6072DA9AD13"

#define SM2DSA_MSG "sm2dsa-sm3 test message"
#define SM2DSA_ID  "1234567890"
static int myrand(void *rng_state, unsigned char *output, size_t len)
{
#if !defined(__OpenBSD__)
    size_t i;

    if (rng_state != NULL)
        rng_state = NULL;

    for (i = 0; i < len; ++i)
        output[i] = rand();
#else
    if (rng_state != NULL)
        rng_state = NULL;

    arc4random_buf(output, len);
#endif /* !OpenBSD */

    return (0);
}

/*
 * Checkup routine
 */
int mbedtls_sm2dsa_self_test(int verbose)
{
    int ret = 0, i = 0;
    mbedtls_sm2dsa_context sm2dsa;
    mbedtls_ecp_group grp;
    mbedtls_ecp_group_id gid = MBEDTLS_ECP_DP_SM2P256V1;
#if defined(MBEDTLS_SM3_C)
    mbedtls_sm3_context ctx;
    unsigned char sm3sum[32];
    unsigned char sig[MBEDTLS_SM2DSA_MAX_LEN];
    size_t siglen = sizeof(sig);
    unsigned char za[32];
#endif

    mbedtls_sm2dsa_init(&sm2dsa);
    mbedtls_ecp_group_init(&grp);

    for (i = 0; i < 2; i++) {
        if (verbose != 0)
            mbedtls_printf("  SM2DSA key validation #%d: ", i + 1);

        if (i & 1) {
            /* load keypair */
            MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&sm2dsa.d, 16, SM2DSA_D));
            if (mbedtls_ecp_point_read_string(
                    &sm2dsa.Q, 16, SM2DSA_QX, SM2DSA_QY) != 0) {
                ret = 1;
                goto fail;
            }
        } else {
            /* generate keypair */
            if (mbedtls_sm2dsa_genkey(&sm2dsa, myrand, NULL) != 0) {
                ret = 1;
                goto fail;
            }
        }

        /*
         * validate keypair
         */
        if (mbedtls_ecp_group_load(&grp, gid) ||
            mbedtls_ecp_check_pubkey(&grp, &sm2dsa.Q) ||
            mbedtls_ecp_check_privkey(&grp, &sm2dsa.d)) {
            ret = 1;
            goto fail;
        }

        if (verbose != 0)
            mbedtls_printf("passed\n");

#if defined(MBEDTLS_SM3_C)
        if (verbose != 0)
            mbedtls_printf("  SM2DSA data sign      #%d: ", i + 1);

        if ( ( ret = mbedtls_sm2_compute_id_digest( MBEDTLS_MD_SM3,
                                   &sm2dsa.Q,
                                   (const char *)SM2DSA_ID,
                                   strlen(SM2DSA_ID),
                                   za) ) != 0 ) {
            goto fail;
        }

        mbedtls_sm3_init( &ctx );

        if ( ( ret = mbedtls_sm3_starts_ret( &ctx ) ) != 0 ) {
            mbedtls_sm3_free( &ctx );
            goto fail;
        }

        if ( ( ret = mbedtls_sm3_update_ret( &ctx, za, 32 ) ) != 0 ) {
            mbedtls_sm3_free( &ctx );
            goto fail;
        }

        if ( ( ret = mbedtls_sm3_update_ret( &ctx, (const unsigned char *)SM2DSA_MSG, strlen(SM2DSA_MSG) ) ) != 0 ) {
            mbedtls_sm3_free( &ctx );
            goto fail;
        }

        if ( ( ret = mbedtls_sm3_finish_ret( &ctx, sm3sum ) ) != 0 ) {
            mbedtls_sm3_free( &ctx );
            goto fail;
        }

        mbedtls_sm3_free( &ctx );

        siglen = sizeof(sig);
        if (mbedtls_sm2dsa_write_signature(&sm2dsa,
                                           MBEDTLS_MD_SM3,
                                           sm3sum,
                                           sizeof(sm3sum),
                                           sig,
                                           &siglen,
                                           myrand,
                                           NULL) != 0) {
            ret = 1;
            goto fail;
        }

        if (verbose != 0)
            mbedtls_printf("passed\n  SM2DSA sig. verify    #%d: ", i + 1);

        if (mbedtls_sm2dsa_read_signature(
                &sm2dsa, sm3sum, sizeof(sm3sum), sig, siglen) != 0) {
            ret = 1;
            goto fail;
        }

        if (verbose != 0)
            mbedtls_printf("passed\n");
#endif /* MBEDTLS_SM3_C */
    }

    goto cleanup;

fail:
    if (verbose != 0)
        mbedtls_printf("failed\n");

cleanup:
    mbedtls_sm2dsa_free(&sm2dsa);
    mbedtls_ecp_group_free(&grp);
    return (ret);
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_SM2DSA_C */
