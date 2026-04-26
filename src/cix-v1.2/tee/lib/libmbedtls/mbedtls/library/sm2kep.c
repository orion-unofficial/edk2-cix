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

#if defined(MBEDTLS_SM2KEP_C)

#include "mbedtls/sm2kep.h"
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
#define SM2KE_VALIDATE_RET(cond)                                               \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
#define SM2KE_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

#if defined(MBEDTLS_SM2KEP_LEGACY_CONTEXT)
typedef mbedtls_sm2kep_context mbedtls_sm2kep_context_mbed;
#endif

/* Implementation that should never be optimized out by the compiler */
static void mbedtls_zeroize(void *v, size_t n)
{
    volatile unsigned char *p = v;
    while (n--)
        *p++ = 0;
}

#if !defined(MBEDTLS_SM2KEP_GEN_PUBLIC_ALT)
/*
 * Generate public key
 */
int mbedtls_sm2kep_gen_public(mbedtls_mpi *d,
                              mbedtls_ecp_point *Q,
                              int (*f_rng)(void *, unsigned char *, size_t),
                              void *p_rng)
{
    int ret;
    mbedtls_ecp_group grp;

    SM2KE_VALIDATE_RET(d != NULL);
    SM2KE_VALIDATE_RET(Q != NULL);
    SM2KE_VALIDATE_RET(f_rng != NULL);

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
    mbedtls_ecp_group_free(&grp);
    return ret;
}
#endif /* !MBEDTLS_SM2KEP_GEN_PUBLIC_ALT */

#if !defined(MBEDTLS_SM2KEP_COMPUTE_SHARED_ALT)

static int _mbedtls_mpi_and(mbedtls_mpi *R,
                            const mbedtls_mpi *A,
                            const mbedtls_mpi *B)
{
    int ret;
    size_t i;
    size_t r_size = 0;
    mbedtls_mpi_uint tmp_a, tmp_b;

    r_size = (A->n > B->n) ? (A->n) : (B->n);

    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(R, r_size));

    for (i = 0; i < r_size; i++) {
        if (i >= A->n) {
            tmp_a = 0;
        } else {
            tmp_a = A->p[i];
        }
        if (i >= B->n) {
            tmp_b = 0;
        } else {
            tmp_b = B->p[i];
        }
        R->p[i] = tmp_a & tmp_b;
    }
    R->s = 1;
    ret  = 0;
cleanup:
    return ret;
}
/*
 * Compute shared secret
 */
int mbedtls_sm2kep_compute_shared(mbedtls_ecp_point *K,
                                  const mbedtls_ecp_point *R,
                                  const mbedtls_ecp_point *Rp,
                                  const mbedtls_ecp_point *Qp,
                                  const mbedtls_mpi *d,
                                  const mbedtls_mpi *r,
                                  int (*f_rng)(void *, unsigned char *, size_t),
                                  void *p_rng)
{
    int ret;
    mbedtls_ecp_point P1;
    mbedtls_mpi t1, t2, u1, u2;
    mbedtls_mpi_uint wT;
    mbedtls_ecp_group grp;

    SM2KE_VALIDATE_RET(K != NULL);
    SM2KE_VALIDATE_RET(R != NULL);
    SM2KE_VALIDATE_RET(Rp != NULL);
    SM2KE_VALIDATE_RET(Qp != NULL);
    SM2KE_VALIDATE_RET(d != NULL);
    SM2KE_VALIDATE_RET(r != NULL);

    mbedtls_ecp_group_init(&grp);

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SM2P256V1);
    if (ret != 0) {
        goto cleanup;
    }

    (void)f_rng;
    (void)p_rng;
    mbedtls_ecp_point_init(&P1);
    mbedtls_mpi_init(&t1);
    mbedtls_mpi_init(&t2);
    mbedtls_mpi_init(&u1);
    mbedtls_mpi_init(&u2);

    wT = (grp.nbits + 1) / 2 - 1;
    MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(&t1, wT, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&t2, &t1, 1));
    MBEDTLS_MPI_CHK(_mbedtls_mpi_and(&u1, &R->X, &t2));
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&u1, &u1, &t1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&u2, &u1, r));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&u2, &u2, &grp.N));
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&t2, &u2, d));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&t2, &t2, &grp.N));
    /*
     * Make sure Q is a valid pubkey before using it
     */

    MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(&grp, Qp));
    MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(&grp, Rp));

    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&u2, &t1, 1));
    MBEDTLS_MPI_CHK(_mbedtls_mpi_and(&u1, &Rp->X, &u2));
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&u2, &u1, &t1));
    /*P = h*t2(Q + u2*R1) = h*t2*Qp + h*t2*u2*Rp, the h == 1 here*/
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&u2, &u2, &t2));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&u2, &u2, &grp.N));
    MBEDTLS_MPI_CHK(mbedtls_ecp_muladd(&grp, K, &t2, Qp, &u2, Rp));

    if (mbedtls_ecp_is_zero(K)) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&P1);
    mbedtls_mpi_free(&t1);
    mbedtls_mpi_free(&t2);
    mbedtls_mpi_free(&u1);
    mbedtls_mpi_free(&u2);

    return (ret);
}
#endif /* !MBEDTLS_SM2KEP_COMPUTE_SHARED_ALT */

static void sm2kep_init_internal(mbedtls_sm2kep_context_mbed *ctx)
{
    mbedtls_ecp_group_init(&ctx->grp);
    mbedtls_mpi_init(&ctx->d);
    mbedtls_ecp_point_init(&ctx->Q);
    mbedtls_mpi_init(&ctx->r);
    mbedtls_ecp_point_init(&ctx->R);
    mbedtls_ecp_point_init(&ctx->Qp);
    mbedtls_ecp_point_init(&ctx->Rp);
    mbedtls_ecp_point_init(&ctx->Z);
}

/*
 * Initialize context
 */
void mbedtls_sm2kep_init(mbedtls_sm2kep_context *ctx)
{
    SM2KE_VALIDATE(ctx != NULL);

#if defined(MBEDTLS_SM2KEP_LEGACY_CONTEXT)
    sm2kep_init_internal( ctx );
#else
    memset(ctx, 0, sizeof(mbedtls_sm2kep_context));
    ctx->var = MBEDTLS_SM2KEP_VARIANT_NONE;
#endif
    ctx->point_format = MBEDTLS_ECP_PF_UNCOMPRESSED;
}

static int sm2kep_setup_internal(mbedtls_sm2kep_context_mbed *ctx,
                                 mbedtls_ecp_group_id grp_id)
{
    int ret;

    ret = mbedtls_ecp_group_load(&ctx->grp, grp_id);
    if (ret != 0) {
        return (MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE);
    }

    return (0);
}

/*
 * Setup context
 */
int mbedtls_sm2kep_setup(mbedtls_sm2kep_context *ctx)
{
    mbedtls_ecp_group_id grp_id = MBEDTLS_ECP_DP_SM2P256V1;
    SM2KE_VALIDATE_RET(ctx != NULL);

#if defined(MBEDTLS_SM2KEP_LEGACY_CONTEXT)
    return( sm2kep_setup_internal( ctx, grp_id ) );
#else
    switch (grp_id) {
    default:
        ctx->point_format = MBEDTLS_ECP_PF_UNCOMPRESSED;
        ctx->var          = MBEDTLS_SM2KEP_VARIANT_MBEDTLS_2_0;
        ctx->grp_id       = grp_id;
        sm2kep_init_internal(&ctx->ctx.mbed_sm2kep);
        return (sm2kep_setup_internal(&ctx->ctx.mbed_sm2kep, grp_id));
    }
#endif
}

static void sm2kep_free_internal(mbedtls_sm2kep_context_mbed *ctx)
{
    mbedtls_ecp_group_free(&ctx->grp);
    mbedtls_mpi_free(&ctx->d);
    mbedtls_ecp_point_free(&ctx->Q);
    mbedtls_mpi_free(&ctx->r);
    mbedtls_ecp_point_free(&ctx->R);
    mbedtls_ecp_point_free(&ctx->Qp);
    mbedtls_ecp_point_free(&ctx->Rp);
    mbedtls_ecp_point_free(&ctx->Z);
}

/*
 * Free context
 */
void mbedtls_sm2kep_free(mbedtls_sm2kep_context *ctx)
{
    if (ctx == NULL)
        return;

#if defined(MBEDTLS_SM2KEP_LEGACY_CONTEXT)
    sm2kep_free_internal( ctx );
#else
    switch (ctx->var) {
    case MBEDTLS_SM2KEP_VARIANT_MBEDTLS_2_0:
        sm2kep_free_internal(&ctx->ctx.mbed_sm2kep);
        break;
    default:
        break;
    }

    ctx->point_format = MBEDTLS_ECP_PF_UNCOMPRESSED;
    ctx->var          = MBEDTLS_SM2KEP_VARIANT_NONE;
    ctx->grp_id       = MBEDTLS_ECP_DP_NONE;
#endif
}

static int
sm2kep_make_params_internal(mbedtls_sm2kep_context_mbed *ctx,
                            size_t *olen,
                            int point_format,
                            unsigned char *buf,
                            size_t blen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret;
    size_t grp_len, pt1_len, pt2_len;

    if (ctx->grp.pbits == 0)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if (ctx->grp.id != MBEDTLS_ECP_DP_SM2P256V1)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if ((ret = mbedtls_sm2kep_gen_public(&ctx->d, &ctx->Q, f_rng, p_rng)) != 0)
        return (ret);

    /* generate tmp keypair */
    if ((ret = mbedtls_sm2kep_gen_public(&ctx->r, &ctx->R, f_rng, p_rng)) != 0)
        return (ret);

    if ((ret = mbedtls_ecp_tls_write_group(&ctx->grp, &grp_len, buf, blen)) !=
        0)
        return (ret);

    buf += grp_len;
    blen -= grp_len;

    if ((ret = mbedtls_ecp_tls_write_point(&ctx->grp, &ctx->Q, point_format,
                                           &pt1_len, buf, blen)) != 0)
        return (ret);

    buf += pt1_len;
    blen -= pt1_len;

    if ((ret = mbedtls_ecp_tls_write_point(&ctx->grp, &ctx->R, point_format,
                                           &pt2_len, buf, blen)) != 0)
        return (ret);

    *olen = grp_len + pt1_len + pt2_len;
    return (0);
}

/*
 * Setup and write the ServerKeyExhange parameters
 *      struct {
 *          ECParameters    curve_params;
 *          ECPoint         public;
 *          ECPoint         temporary public;
 *      } ServerECDHParams;
 */
int mbedtls_sm2kep_make_params(mbedtls_sm2kep_context *ctx,
                               size_t *olen,
                               unsigned char *buf,
                               size_t blen,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng)
{
    SM2KE_VALIDATE_RET(ctx != NULL);
    SM2KE_VALIDATE_RET(olen != NULL);
    SM2KE_VALIDATE_RET(buf != NULL);
    SM2KE_VALIDATE_RET(f_rng != NULL);

#if defined(MBEDTLS_SM2KEP_LEGACY_CONTEXT)
    return( sm2kep_make_params_internal( ctx, olen, ctx->point_format, buf, blen,
                                       f_rng, p_rng ) );
#else
    switch (ctx->var) {
    case MBEDTLS_SM2KEP_VARIANT_MBEDTLS_2_0:
        return (sm2kep_make_params_internal(&ctx->ctx.mbed_sm2kep, olen,
                                            ctx->point_format, buf, blen, f_rng,
                                            p_rng));
    default:
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
#endif
}

static int sm2kep_read_params_internal(mbedtls_sm2kep_context_mbed *ctx,
                                       const unsigned char **buf,
                                       const unsigned char *end)
{
    int ret;

    if ((ret = mbedtls_ecp_tls_read_point(&ctx->grp, &ctx->Qp, buf,
                                          end - *buf)) != 0)
        return (ret);

    if ((ret = mbedtls_ecp_tls_read_point(&ctx->grp, &ctx->Rp, buf,
                                          end - *buf)) != 0)
        return (ret);

    return (0);
}

/*
 * TODO: check SM2 TLS
 * Read the ServerKeyExhange parameters
 *      struct {
 *          ECParameters    curve_params;
 *          ECPoint         public;
 *          ECPoint         temporary public;
 *      } ServerECDHParams;
 */
int mbedtls_sm2kep_read_params(mbedtls_sm2kep_context *ctx,
                               const unsigned char **buf,
                               const unsigned char *end)
{
    int ret;
    mbedtls_ecp_group_id grp_id = MBEDTLS_ECP_DP_NONE;
    SM2KE_VALIDATE_RET(ctx != NULL);
    SM2KE_VALIDATE_RET(buf != NULL);
    SM2KE_VALIDATE_RET(*buf != NULL);
    SM2KE_VALIDATE_RET(end != NULL);

    if ((ret = mbedtls_ecp_tls_read_group_id(&grp_id, buf, end - *buf)) != 0)
        return (ret);

    if (grp_id != MBEDTLS_ECP_DP_SM2P256V1)
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

    if ((ret = mbedtls_sm2kep_setup(ctx)) != 0)
        return (ret);

#if defined(MBEDTLS_SM2KEP_LEGACY_CONTEXT)
    return( sm2kep_read_params_internal( ctx, buf, end ) );
#else
    switch (ctx->var) {
    case MBEDTLS_SM2KEP_VARIANT_MBEDTLS_2_0:
        return (sm2kep_read_params_internal(&ctx->ctx.mbed_sm2kep, buf, end));
    default:
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
#endif
}

static int sm2kep_get_params_internal(mbedtls_sm2kep_context_mbed *ctx,
                                      const mbedtls_ecp_keypair *key,
                                      const mbedtls_ecp_keypair *tmpkey,
                                      mbedtls_sm2kep_side side)
{
    int ret;

    /* If it's not our key, just import the public part as Qp */
    if (side == MBEDTLS_SM2KEP_RESPONDER) {
        if ((ret = mbedtls_ecp_copy(&ctx->Qp, &key->Q)) != 0 ||
            (ret = mbedtls_ecp_copy(&ctx->Rp, &tmpkey->Q)) != 0)
            return (ret);
    } else if (side == MBEDTLS_SM2KEP_INITIATOR) {
        if ((ret = mbedtls_ecp_copy(&ctx->Q, &key->Q)) != 0 ||
            (ret = mbedtls_mpi_copy(&ctx->d, &key->d)) != 0)
            return (ret);

        if ((ret = mbedtls_ecp_copy(&ctx->R, &tmpkey->Q)) != 0 ||
            (ret = mbedtls_mpi_copy(&ctx->r, &tmpkey->d)) != 0)
            return (ret);
    } else {
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    }

    return (0);
}

/*
 * Get parameters from a keypair
 */
int mbedtls_sm2kep_get_params(mbedtls_sm2kep_context *ctx,
                              const mbedtls_ecp_keypair *key,
                              const mbedtls_ecp_keypair *tmpkey,
                              mbedtls_sm2kep_side side)
{
    int ret;
    SM2KE_VALIDATE_RET(ctx != NULL);
    SM2KE_VALIDATE_RET(key != NULL);
    SM2KE_VALIDATE_RET(side == MBEDTLS_SM2KEP_INITIATOR ||
                       side == MBEDTLS_SM2KEP_RESPONDER);

    if ((ret = mbedtls_sm2kep_setup(ctx)) != 0)
        return (ret);

#if defined(MBEDTLS_SM2KEP_LEGACY_CONTEXT)
    return (sm2kep_get_params_internal(ctx, key, tmpkey, side));
#else
    switch (ctx->var) {
    case MBEDTLS_SM2KEP_VARIANT_MBEDTLS_2_0:
        return (sm2kep_get_params_internal(&ctx->ctx.mbed_sm2kep, key, tmpkey, side));
    default:
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
#endif
}

static int
sm2kep_make_public_internal(mbedtls_sm2kep_context_mbed *ctx,
                            size_t *olen,
                            int point_format,
                            unsigned char *buf,
                            size_t blen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret;
    size_t pt1_len, pt2_len;

    if (ctx->grp.pbits == 0)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if (ctx->grp.id != MBEDTLS_ECP_DP_SM2P256V1)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if ((ret = mbedtls_sm2kep_gen_public(&ctx->d, &ctx->Q, f_rng, p_rng)) != 0)
        return (ret);

    /* generate temporary keypair */
    if ((ret = mbedtls_sm2kep_gen_public(&ctx->r, &ctx->R, f_rng, p_rng)) != 0)
        return (ret);

    if ((ret = mbedtls_ecp_tls_write_point(&ctx->grp, &ctx->Q, point_format,
                                           &pt1_len, buf, blen)) != 0)
        return (ret);

    buf += pt1_len;
    blen -= pt1_len;

    if ((ret = mbedtls_ecp_tls_write_point(&ctx->grp, &ctx->R, point_format,
                                           &pt2_len, buf, blen)) != 0)
        return (ret);

    *olen = pt1_len + pt2_len;
    return (0);
}

/*
 * Setup and export the client public value
 */
int mbedtls_sm2kep_make_public(mbedtls_sm2kep_context *ctx,
                               size_t *olen,
                               unsigned char *buf,
                               size_t blen,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng)
{
    SM2KE_VALIDATE_RET(ctx != NULL);
    SM2KE_VALIDATE_RET(olen != NULL);
    SM2KE_VALIDATE_RET(buf != NULL);
    SM2KE_VALIDATE_RET(f_rng != NULL);

#if defined(MBEDTLS_SM2KEP_LEGACY_CONTEXT)
    return( sm2kep_make_public_internal( ctx, olen, ctx->point_format, buf, blen,
                                       f_rng, p_rng ) );
#else
    switch (ctx->var) {
    case MBEDTLS_SM2KEP_VARIANT_MBEDTLS_2_0:
        return (sm2kep_make_public_internal(&ctx->ctx.mbed_sm2kep, olen,
                                            ctx->point_format, buf, blen, f_rng,
                                            p_rng));
    default:
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
#endif
}

static int sm2kep_read_public_internal(mbedtls_sm2kep_context_mbed *ctx,
                                       const unsigned char *buf,
                                       size_t blen)
{
    int ret;
    const unsigned char *p = buf;

    if (ctx->grp.pbits == 0)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if (ctx->grp.id != MBEDTLS_ECP_DP_SM2P256V1)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if ((ret = mbedtls_ecp_tls_read_point(&ctx->grp, &ctx->Qp, &p, blen)) != 0)
        return (ret);

    if ((ret = mbedtls_ecp_check_pubkey(&ctx->grp, &ctx->Qp)) != 0)
        return (ret);

    if ((ret = mbedtls_ecp_tls_read_point(&ctx->grp, &ctx->Rp, &p, blen)) != 0)
        return (ret);

    if ((ret = mbedtls_ecp_check_pubkey(&ctx->grp, &ctx->Rp)) != 0)
        return (ret);

    if ((size_t)(p - buf) != blen)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    return (0);
}

/*
 * Parse and import the client's public value
 */
int mbedtls_sm2kep_read_public(mbedtls_sm2kep_context *ctx,
                               const unsigned char *buf,
                               size_t blen)
{
    SM2KE_VALIDATE_RET(ctx != NULL);
    SM2KE_VALIDATE_RET(buf != NULL);

#if defined(MBEDTLS_SM2KEP_LEGACY_CONTEXT)
    return( sm2kep_read_public_internal( ctx, buf, blen ) );
#else
    switch (ctx->var) {
    case MBEDTLS_SM2KEP_VARIANT_MBEDTLS_2_0:
        return (sm2kep_read_public_internal(&ctx->ctx.mbed_sm2kep, buf, blen));
    default:
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
#endif
}

static int mbedtls_sm2kep_compute_shared_key(const mbedtls_md_info_t *md_info,
                                             size_t modsize,             /*in*/
                                             const mbedtls_ecp_point *P, /*in*/
                                             unsigned char *ZA,          /*in*/
                                             size_t ZAlen,               /*in*/
                                             unsigned char *ZB,          /*in*/
                                             size_t ZBlen,               /*in*/
                                             size_t Kresultlen,          /*in*/
                                             unsigned char *Kresult /*out*/)
{
    int ret;
    unsigned char *dgst_ctx = NULL;

    dgst_ctx = (unsigned char *)mbedtls_calloc(modsize * 2 + ZAlen + ZBlen,
                                               sizeof(unsigned char));
    if (dgst_ctx == NULL) {
        return MBEDTLS_ERR_ECP_ALLOC_FAILED;
    }
    if ((ret = mbedtls_mpi_write_binary(&P->X, dgst_ctx, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_mpi_write_binary(&P->Y, dgst_ctx + modsize, modsize)) !=
        0)
        goto exit;

    memcpy(dgst_ctx + 2 * modsize, ZA, ZAlen);
    memcpy(dgst_ctx + 2 * modsize + ZAlen, ZB, ZBlen);
    ret = mbedtls_sm2_kdf(md_info, dgst_ctx, 2 * modsize + ZBlen + ZAlen,
                          Kresultlen, Kresult);
    mbedtls_zeroize(dgst_ctx, 2 * modsize + ZBlen + ZAlen);
exit:
    mbedtls_free(dgst_ctx);
    return ret;
}

static int
sm2kep_calc_secret_internal(mbedtls_sm2kep_context_mbed *ctx,
                            mbedtls_md_type_t md_alg,
                            unsigned char *ZA,
                            size_t ZAlen,
                            unsigned char *ZB,
                            size_t ZBlen,
                            unsigned char *buf,
                            size_t blen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret;
    const mbedtls_md_info_t *md_info;
    size_t modsize;

    if (ctx == NULL || ctx->grp.pbits == 0)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if (ctx->grp.id != MBEDTLS_ECP_DP_SM2P256V1)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if ((md_info = mbedtls_md_info_from_type(md_alg)) == NULL)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if ((ret = mbedtls_sm2kep_compute_shared(&ctx->Z, &ctx->R, &ctx->Rp,
                                             &ctx->Qp, &ctx->d, &ctx->r, f_rng,
                                             p_rng)) != 0) {
        goto exit;
    }

    modsize = ctx->grp.pbits / 8 + ((ctx->grp.pbits % 8) != 0);

    if ((ret = mbedtls_sm2kep_compute_shared_key(md_info, modsize, &ctx->Z, ZA,
                                                 ZAlen, ZB, ZBlen, blen,
                                                 buf)) != 0) {
        goto exit;
    }
exit:
    return ret;
}

/*
 * Derive and export the shared secret
 */
int mbedtls_sm2kep_calc_secret(mbedtls_sm2kep_context *ctx,
                               mbedtls_md_type_t md_alg,
                               unsigned char *buf,
                               size_t blen,
                               unsigned char *ZA,
                               size_t ZAlen,
                               unsigned char *ZB,
                               size_t ZBlen,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng)
{
    SM2KE_VALIDATE_RET(ctx != NULL);
    SM2KE_VALIDATE_RET(ZA != NULL);
    SM2KE_VALIDATE_RET(ZAlen != 0);
    SM2KE_VALIDATE_RET(ZB != NULL);
    SM2KE_VALIDATE_RET(ZBlen != 0);
    SM2KE_VALIDATE_RET(buf != NULL);

#if defined(MBEDTLS_SM2KEP_LEGACY_CONTEXT)
    return (sm2kep_calc_secret_internal(ctx, md_alg, ZA, ZAlen, ZB, ZBlen, buf,
                                        blen, f_rng, p_rng));
#else
    switch (ctx->var) {
    case MBEDTLS_SM2KEP_VARIANT_MBEDTLS_2_0:
        return (sm2kep_calc_secret_internal(&ctx->ctx.mbed_sm2kep, md_alg, ZA,
                                            ZAlen, ZB, ZBlen, buf, blen, f_rng,
                                            p_rng));
    default:
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    }
#endif
}

static int mbedtls_sm2kep_calc_hash(const mbedtls_md_info_t *md_info,
                                    size_t modsize,              /*in*/
                                    const mbedtls_ecp_point *P,  /*in*/
                                    unsigned char *ZA,           /*in*/
                                    size_t ZAlen,                /*in*/
                                    unsigned char *ZB,           /*in*/
                                    size_t ZBlen,                /*in*/
                                    const mbedtls_ecp_point *RA, /*in*/
                                    const mbedtls_ecp_point *RB, /*in*/
                                    unsigned char *Sresult /*out*/)
{
    int ret;
    mbedtls_md_context_t md_ctx;
    unsigned char buf[MBEDTLS_ECP_MAX_PT_LEN];

    mbedtls_md_init(&md_ctx);

    if ((ret = mbedtls_md_setup(&md_ctx, md_info, 0)) != 0)
        return (ret);

    if ((ret = mbedtls_md_starts(&md_ctx)) != 0)
        goto exit;

    if ((ret = mbedtls_mpi_write_binary(&P->X, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_md_update(&md_ctx, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_md_update(&md_ctx, ZA, ZAlen)) != 0)
        goto exit;

    if ((ret = mbedtls_md_update(&md_ctx, ZB, ZBlen)) != 0)
        goto exit;

    if ((ret = mbedtls_mpi_write_binary(&RA->X, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_md_update(&md_ctx, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_mpi_write_binary(&RA->Y, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_md_update(&md_ctx, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_mpi_write_binary(&RB->X, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_md_update(&md_ctx, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_mpi_write_binary(&RB->Y, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_md_update(&md_ctx, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_md_finish(&md_ctx, Sresult)) != 0)
        goto exit;

exit:
    mbedtls_md_free(&md_ctx);
    return ret;
}

static int
mbedtls_sm2kep_compute_optional_sum(const mbedtls_md_info_t *md_info,
                                    unsigned char sm2kep_flag,  /*in*/
                                    size_t modsize,             /*in*/
                                    const mbedtls_ecp_point *P, /*in*/
                                    unsigned char *hash,        /*in*/
                                    size_t len,                 /*in*/
                                    unsigned char *Sresult /*out*/)
{
    int ret;
    mbedtls_md_context_t md_ctx;
    unsigned char buf[MBEDTLS_ECP_MAX_PT_LEN];

    mbedtls_md_init(&md_ctx);

    if ((ret = mbedtls_md_setup(&md_ctx, md_info, 0)) != 0)
        return (ret);

    if ((ret = mbedtls_md_starts(&md_ctx)) != 0)
        goto exit;

    if ((ret = mbedtls_md_update(&md_ctx, &sm2kep_flag,
                                 sizeof(unsigned char))) != 0)
        goto exit;

    if ((ret = mbedtls_mpi_write_binary(&P->Y, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_md_update(&md_ctx, buf, modsize)) != 0)
        goto exit;

    if ((ret = mbedtls_md_update(&md_ctx, hash, len)) != 0)
        goto exit;

    if ((ret = mbedtls_md_finish(&md_ctx, Sresult)) != 0)
        goto exit;

exit:
    mbedtls_md_free(&md_ctx);
    return ret;
}

static int sm2kep_calc_checksum_internal(mbedtls_sm2kep_context_mbed *ctx,
                                         mbedtls_md_type_t md_alg,
                                         mbedtls_sm2kep_side side,
                                         unsigned char *ZA,
                                         size_t ZAlen,
                                         unsigned char *ZB,
                                         size_t ZBlen,
                                         unsigned char *SI,
                                         unsigned char *SR)
{
    int ret;
    const mbedtls_md_info_t *md_info;
    unsigned char hash[MBEDTLS_MD_MAX_SIZE] = {0};
    size_t mlen = 0, mod_size = 0;

    if (ctx == NULL || ctx->grp.pbits == 0)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if (ctx->grp.id != MBEDTLS_ECP_DP_SM2P256V1)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if (side != MBEDTLS_SM2KEP_INITIATOR && side != MBEDTLS_SM2KEP_RESPONDER)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if ((md_info = mbedtls_md_info_from_type(md_alg)) == NULL)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    mod_size = ctx->grp.pbits / 8 + ((ctx->grp.pbits % 8) != 0);
    mlen     = mbedtls_md_get_size(md_info);
    if (side == MBEDTLS_SM2KEP_INITIATOR) {
        if ((ret = mbedtls_sm2kep_calc_hash(md_info,
                                            mod_size,
                                            &ctx->Z,
                                            ZA,
                                            ZAlen,
                                            ZB,
                                            ZBlen,
                                            &ctx->R,
                                            &ctx->Rp,
                                            hash)) != 0)
            return (ret);
    } else {
        if ((ret = mbedtls_sm2kep_calc_hash(md_info,
                                            mod_size,
                                            &ctx->Z,
                                            ZA,
                                            ZAlen,
                                            ZB,
                                            ZBlen,
                                            &ctx->Rp,
                                            &ctx->R,
                                            hash)) != 0)
            return (ret);
    }

    if ((ret = mbedtls_sm2kep_compute_optional_sum(md_info,
                                                   MBEDTLS_SM2KEP_INITIATOR,
                                                   mod_size,
                                                   &ctx->Z,
                                                   hash,
                                                   mlen,
                                                   SI)) != 0)
        return (ret);

    if ((ret = mbedtls_sm2kep_compute_optional_sum(md_info,
                                                   MBEDTLS_SM2KEP_RESPONDER,
                                                   mod_size,
                                                   &ctx->Z,
                                                   hash,
                                                   mlen,
                                                   SR)) != 0)
        return (ret);

    return ret;
}

int mbedtls_sm2kep_calc_checksum(mbedtls_sm2kep_context *ctx,
                                 mbedtls_md_type_t md_alg,
                                 mbedtls_sm2kep_side side,
                                 unsigned char *SI,
                                 unsigned char *SR,
                                 unsigned char *ZA,
                                 size_t ZAlen,
                                 unsigned char *ZB,
                                 size_t ZBlen)

{
    SM2KE_VALIDATE_RET(ctx != NULL);
    SM2KE_VALIDATE_RET(side == MBEDTLS_SM2KEP_INITIATOR ||
                       side == MBEDTLS_SM2KEP_RESPONDER);
    SM2KE_VALIDATE_RET(ZA != NULL);
    SM2KE_VALIDATE_RET(ZAlen != 0);
    SM2KE_VALIDATE_RET(ZB != NULL);
    SM2KE_VALIDATE_RET(ZBlen != 0);
    SM2KE_VALIDATE_RET(SI != NULL);
    SM2KE_VALIDATE_RET(SR != NULL);

#if defined(MBEDTLS_SM2KEP_LEGACY_CONTEXT)
    return (sm2kep_calc_checksum_internal(ctx, md_alg, side, ZA, ZAlen, ZB,
                                          ZBlen, SI, SR));
#else
    switch (ctx->var) {
    case MBEDTLS_SM2KEP_VARIANT_MBEDTLS_2_0:
        return (sm2kep_calc_checksum_internal(
            &ctx->ctx.mbed_sm2kep, md_alg, side, ZA, ZAlen, ZB, ZBlen, SI, SR));
    default:
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    }
#endif
}

#if defined(MBEDTLS_SELF_TEST)

#define TMP_BUF_SIZE    (1024)
#define ZA_STR  "client000"
#define ZB_STR  "server000"
#include <mbedtls/sm3.h>

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
int mbedtls_sm2kep_self_test(int verbose)
{
    int ret = 0;
    mbedtls_sm2kep_context client;
    mbedtls_sm2kep_context server;
    mbedtls_ecp_keypair key;
    mbedtls_ecp_keypair tmp_key;
    uint8_t *public = NULL;
    size_t public_size = 0;
    uint8_t *params = NULL;
    size_t params_size = 0;
    uint8_t *p                   = NULL;
    uint8_t client_secret[64]    = {0};
    uint8_t server_secret[64] = {0};
    uint8_t client_checksum[32] = {0};
    uint8_t server_checksum[32] = {0};
    uint8_t client_checksum2[32] = {0};
    uint8_t server_checksum2[32] = {0};
    /**
     * Flow1:
     * 1. server: mbedtls_sm2kep_get_params (MBEDTLS_SM2KEP_INITIATOR).
     * 2. client: mbedtls_sm2kep_get_params (MBEDTLS_SM2KEP_RESPONDER).
     * 3. client: mbedtls_sm2kep_make_public
     * 4. server: mbedtls_sm2kep_read_public
     * 5. client: mbedtls_sm2kep_calc_secret
     * 6. server: mbedtls_sm2kep_calc_secret
     * 7. calc checksum
     */
    mbedtls_ecp_keypair_init(&key);
    mbedtls_ecp_keypair_init(&tmp_key);

    if ( ( ret = mbedtls_ecp_gen_key( MBEDTLS_ECP_DP_SM2P256V1, &key,
                myrand, NULL ) ) != 0 ) {
        goto fail;
    }

    if ( ( ret = mbedtls_ecp_gen_key( MBEDTLS_ECP_DP_SM2P256V1, &tmp_key,
                myrand, NULL ) ) != 0 ) {
        goto fail;
    }

    mbedtls_sm2kep_init(&client);
    mbedtls_sm2kep_init(&server);

    if ( ( ret = mbedtls_sm2kep_get_params(&server,
                        &key, &tmp_key, MBEDTLS_SM2KEP_INITIATOR)) != 0 ) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if ( ( ret = mbedtls_sm2kep_get_params(&client,
                        &key, &tmp_key, MBEDTLS_SM2KEP_RESPONDER)) != 0 ) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    public = mbedtls_calloc(1, TMP_BUF_SIZE);
    if ( !public ) {
        ret = 1;
        goto fail;
    }

    if ( ( ret = mbedtls_sm2kep_make_public(&client,
                    &public_size, public, TMP_BUF_SIZE, myrand, NULL)) != 0 ) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if ( ( ret = mbedtls_sm2kep_read_public(&server, (const unsigned char *)public,
                               public_size) ) != 0 ) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if ( ( ret = mbedtls_sm2kep_calc_secret(&client,
                               MBEDTLS_MD_SM3,
                               client_secret,
                               64,
                               (unsigned char *)ZA_STR,
                               strlen(ZA_STR),
                               (unsigned char *)ZB_STR,
                               strlen(ZB_STR),
                               myrand, NULL) ) != 0 ) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if ( ( ret = mbedtls_sm2kep_calc_secret(&server,
                               MBEDTLS_MD_SM3,
                               server_secret,
                               64,
                               (unsigned char *)ZA_STR,
                               strlen(ZA_STR),
                               (unsigned char *)ZB_STR,
                               strlen(ZB_STR),
                               myrand, NULL) ) != 0 ) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if (0 != memcmp(client_secret, server_secret, 64)) {
        mbedtls_printf("secret not match!\n");
        ret = 1;
        goto fail;
    }

    if ( (ret = mbedtls_sm2kep_calc_checksum(&client,
                                       MBEDTLS_MD_SM3,
                                       MBEDTLS_SM2KEP_RESPONDER,
                                       server_checksum,
                                       client_checksum,
                                       (unsigned char *)ZA_STR,
                                       strlen(ZA_STR),
                                       (unsigned char *)ZB_STR,
                                       strlen(ZB_STR)) ) != 0) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if ( (ret = mbedtls_sm2kep_calc_checksum(&server,
                                       MBEDTLS_MD_SM3,
                                       MBEDTLS_SM2KEP_INITIATOR,
                                       server_checksum2,
                                       client_checksum2,
                                       (unsigned char *)ZA_STR,
                                       strlen(ZA_STR),
                                       (unsigned char *)ZB_STR,
                                       strlen(ZB_STR)) ) != 0) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if (0 != memcmp(server_checksum, server_checksum2, 32)) {
        mbedtls_printf("server checksum not match!\n");
        ret = 1;
        goto fail;
    }

    if (0 != memcmp(client_checksum, client_checksum2, 32)) {
        mbedtls_printf("client checksum not match!\n");
        ret = 1;
        goto fail;
    }

    if (verbose != 0)
        mbedtls_printf("passed\n  SM2KEP Usage 1\n");

    mbedtls_sm2kep_free(&client);
    mbedtls_sm2kep_free(&server);
    mbedtls_free(public);
    public = NULL;
    public_size = 0;
    memset(client_secret, 0, sizeof(client_secret));
    memset(server_secret, 0, sizeof(server_secret));
    memset(client_checksum, 0, sizeof(client_checksum));
    memset(server_checksum, 0, sizeof(server_checksum));
    memset(client_checksum2, 0, sizeof(client_checksum2));
    memset(server_checksum2, 0, sizeof(server_checksum2));
    mbedtls_ecp_keypair_free(&key);
    mbedtls_ecp_keypair_free(&tmp_key);


    /**
     * Flow2:
     * 1. server: mbedtls_sm2kep_setup + mbedtls_sm2kep_make_params
     * 2. client: mbedtls_sm2kep_read_params
     * 3. client: mbedtls_sm2kep_make_public
     * 4. server: mbedtls_sm2kep_read_public
     * 5. client: mbedtls_sm2kep_calc_secret
     * 6. server: mbedtls_sm2kep_calc_secret
     * 7. calc checksum
     */
    mbedtls_sm2kep_init(&client);
    mbedtls_sm2kep_init(&server);

    if ( ( ret = mbedtls_sm2kep_setup(&server)) != 0 ) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    params = mbedtls_calloc(1, TMP_BUF_SIZE);
    if (!params) {
        ret = 1;
        goto fail;
    }
    if ( (ret = mbedtls_sm2kep_make_params(&server,
                               &params_size,
                               params,
                               TMP_BUF_SIZE,
                               myrand,
                               NULL)) != 0) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    p = params;
    if ((ret = mbedtls_sm2kep_read_params(&client, (const unsigned char **)(&p),
                                                   (const unsigned char *)(p + params_size))) != 0) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    public = mbedtls_calloc(1, TMP_BUF_SIZE);
    if ( !public ) {
        ret = 1;
        goto fail;
    }

    if ( ( ret = mbedtls_sm2kep_make_public(&client,
                    &public_size, public, TMP_BUF_SIZE, myrand, NULL)) != 0 ) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if ( ( ret = mbedtls_sm2kep_read_public(&server, (const unsigned char *)public,
                               public_size) ) != 0 ) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if ( ( ret = mbedtls_sm2kep_calc_secret(&client,
                               MBEDTLS_MD_SM3,
                               client_secret,
                               64,
                               (unsigned char *)ZA_STR,
                               strlen(ZA_STR),
                               (unsigned char *)ZB_STR,
                               strlen(ZB_STR),
                               myrand, NULL) ) != 0 ) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if ( ( ret = mbedtls_sm2kep_calc_secret(&server,
                               MBEDTLS_MD_SM3,
                               server_secret,
                               64,
                               (unsigned char *)ZA_STR,
                               strlen(ZA_STR),
                               (unsigned char *)ZB_STR,
                               strlen(ZB_STR),
                               myrand, NULL) ) != 0 ) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if (0 != memcmp(client_secret, server_secret, 64)) {
        mbedtls_printf("secret not match!\n");
        ret = 1;
        goto fail;
    }

    if ( (ret = mbedtls_sm2kep_calc_checksum(&client,
                                       MBEDTLS_MD_SM3,
                                       MBEDTLS_SM2KEP_RESPONDER,
                                       server_checksum,
                                       client_checksum,
                                       (unsigned char *)ZA_STR,
                                       strlen(ZA_STR),
                                       (unsigned char *)ZB_STR,
                                       strlen(ZB_STR)) ) != 0) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if ( (ret = mbedtls_sm2kep_calc_checksum(&server,
                                       MBEDTLS_MD_SM3,
                                       MBEDTLS_SM2KEP_INITIATOR,
                                       server_checksum2,
                                       client_checksum2,
                                       (unsigned char *)ZA_STR,
                                       strlen(ZA_STR),
                                       (unsigned char *)ZB_STR,
                                       strlen(ZB_STR)) ) != 0) {
        mbedtls_printf("%s %d failed! \n", __func__, __LINE__);
        goto fail;
    }

    if (0 != memcmp(server_checksum, server_checksum2, 32)) {
        mbedtls_printf("server checksum not match!\n");
        ret = 1;
        goto fail;
    }

    if (0 != memcmp(client_checksum, client_checksum2, 32)) {
        mbedtls_printf("client checksum not match!\n");
        ret = 1;
        goto fail;
    }

    if (verbose != 0)
        mbedtls_printf("passed\n  SM2KEP Usage 2\n");

    ret = 0;
    mbedtls_printf("passed\n");
    goto cleanup;
fail:
    mbedtls_printf("failed\n");
    mbedtls_printf("ret: -0x%x\n", -ret);

cleanup:
    mbedtls_sm2kep_free(&client);
    mbedtls_sm2kep_free(&server);
    if (public) {
        mbedtls_free(public);
    }
    memset(client_secret, 0, sizeof(client_secret));
    memset(server_secret, 0, sizeof(server_secret));
    memset(client_checksum, 0, sizeof(client_checksum));
    memset(server_checksum, 0, sizeof(server_checksum));
    memset(client_checksum2, 0, sizeof(client_checksum2));
    memset(server_checksum2, 0, sizeof(server_checksum2));
    mbedtls_ecp_keypair_free(&key);
    mbedtls_ecp_keypair_free(&tmp_key);
    if (params) {
        mbedtls_free(params);
    }
    return ret;
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_SM2KEP_C */
