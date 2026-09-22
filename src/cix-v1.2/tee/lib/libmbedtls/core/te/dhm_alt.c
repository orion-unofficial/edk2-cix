/*
 *  Diffie-Hellman-Merkle key exchange
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */
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

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_DHM_C) && defined(MBEDTLS_DHM_ALT)

#include "te_bn.h"
#include "te_dhm.h"
#include "mbedtls/dhm.h"
#include "mbedtls/platform_util.h"

#include <string.h>

#if defined(MBEDTLS_PEM_PARSE_C)
#include "mbedtls/pem.h"
#endif

#if defined(MBEDTLS_ASN1_PARSE_C)
#include "mbedtls/asn1.h"
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdlib.h>
#include <stdio.h>
#define mbedtls_printf printf
#define mbedtls_calloc calloc
#define mbedtls_free free
#endif

#pragma GCC diagnostic ignored "-Wstrict-aliasing"

#define DHM_VALIDATE_RET(cond)                                                 \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_DHM_BAD_INPUT_DATA)
#define DHM_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

extern int te_err_to_mbedtls_dhm_err(int te_err);

/*
 * Convert TE error code to mbedtls DHM error
 */
int te_err_to_mbedtls_dhm_err(int te_err)
{
    switch (te_err) {
    case TE_SUCCESS:
        return 0;
    case TE_ERROR_BAD_PARAMS:
    case TE_ERROR_BAD_INPUT_DATA:
        return MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    case TE_ERROR_READ_PARAMS:
        return MBEDTLS_ERR_DHM_READ_PARAMS_FAILED;
    case TE_ERROR_MAKE_PARAMS:
        return MBEDTLS_ERR_DHM_MAKE_PARAMS_FAILED;
    case TE_ERROR_READ_PUBLIC:
        return MBEDTLS_ERR_DHM_READ_PUBLIC_FAILED;
    case TE_ERROR_MAKE_PUBLIC:
        return MBEDTLS_ERR_DHM_MAKE_PUBLIC_FAILED;
    case TE_ERROR_CALC_SECRET:
        return MBEDTLS_ERR_DHM_CALC_SECRET_FAILED;
    case TE_ERROR_OOM:
        return MBEDTLS_ERR_DHM_ALLOC_FAILED;
    case TE_ERROR_SET_GROUP:
        return MBEDTLS_ERR_DHM_SET_GROUP_FAILED;

    default:
        return MBEDTLS_ERR_MPI_HW_FAILED;
    }
}

#define MBEDTLS_CALL_TE_DHM(func) te_err_to_mbedtls_dhm_err(func)
#define MBEDTLS_TE_DHM_CHK(f)                                                  \
    do {                                                                       \
        if ((ret = MBEDTLS_CALL_TE_DHM(f)) != 0) {                             \
            goto cleanup;                                                      \
        }                                                                      \
    } while (0)

/*
 * helper to validate the mbedtls_mpi size and import it
 */
static int dhm_read_bignum(mbedtls_mpi *X,
                           unsigned char **p,
                           const unsigned char *end)
{
    int ret, n;

    if (end - *p < 2)
        return (MBEDTLS_ERR_DHM_BAD_INPUT_DATA);

    n = ((*p)[0] << 8) | (*p)[1];
    (*p) += 2;

    if ((int)(end - *p) < n)
        return (MBEDTLS_ERR_DHM_BAD_INPUT_DATA);

    if ((ret = mbedtls_mpi_read_binary(X, *p, n)) != 0)
        return (MBEDTLS_ERR_DHM_READ_PARAMS_FAILED + ret);

    (*p) += n;

    return (0);
}

/*
 * Verify sanity of parameter with regards to P
 *
 * Parameter should be: 2 <= public_param <= P - 2
 *
 * This means that we need to return an error if
 *              public_param < 2 or public_param > P-2
 *
 * For more information on the attack, see:
 *  http://www.cl.cam.ac.uk/~rja14/Papers/psandqs.pdf
 *  http://web.nvd.nist.gov/view/vuln/detail?vulnId=CVE-2005-2643
 */
static int dhm_check_range(const mbedtls_mpi *param, const mbedtls_mpi *P)
{
    mbedtls_mpi L, U;
    int ret = 0;

    mbedtls_mpi_init(&L);
    mbedtls_mpi_init(&U);

    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&L, 2));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&U, P, 2));

    if (mbedtls_mpi_cmp_mpi(param, &L) < 0 ||
        mbedtls_mpi_cmp_mpi(param, &U) > 0) {
        ret = MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    }

cleanup:
    mbedtls_mpi_free(&L);
    mbedtls_mpi_free(&U);
    return (ret);
}

void mbedtls_dhm_init(mbedtls_dhm_context *ctx)
{
    DHM_VALIDATE(ctx != NULL);
    memset(ctx, 0, sizeof(mbedtls_dhm_context));

    mbedtls_mpi_init(&ctx->P);
    mbedtls_mpi_init(&ctx->G);
    mbedtls_mpi_init(&ctx->X);
    mbedtls_mpi_init(&ctx->GX);
    mbedtls_mpi_init(&ctx->GY);
    mbedtls_mpi_init(&ctx->K);
    mbedtls_mpi_init(&ctx->RP);
    mbedtls_mpi_init(&ctx->Vi);
    mbedtls_mpi_init(&ctx->Vf);
    mbedtls_mpi_init(&ctx->pX);

    /* zerorize X as required by te_dhm_make_public() */
    mbedtls_mpi_lset(&ctx->X, 0);

    /* zerorize pX as required by mbedtls_dhm_calc_secret() */
    mbedtls_mpi_lset(&ctx->pX, 0);
}

/*
 * Parse the ServerKeyExchange parameters
 */
int mbedtls_dhm_read_params(mbedtls_dhm_context *ctx,
                            unsigned char **p,
                            const unsigned char *end)
{
    int ret;
    DHM_VALIDATE_RET(ctx != NULL);
    DHM_VALIDATE_RET(p != NULL && *p != NULL);
    DHM_VALIDATE_RET(end != NULL);

    if ((ret = dhm_read_bignum(&ctx->P, p, end)) != 0 ||
        (ret = dhm_read_bignum(&ctx->G, p, end)) != 0 ||
        (ret = dhm_read_bignum(&ctx->GY, p, end)) != 0)
        return (ret);

    if ((ret = dhm_check_range(&ctx->GY, &ctx->P)) != 0)
        return (ret);

    ctx->len = mbedtls_mpi_size(&ctx->P);

    return (0);
}

/*
 * Setup and write the ServerKeyExchange parameters
 */
int mbedtls_dhm_make_params(mbedtls_dhm_context *ctx,
                            int x_size,
                            unsigned char *output,
                            size_t *olen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret, count = 0;
    size_t n1, n2, n3;
    unsigned char *p;
    DHM_VALIDATE_RET(ctx != NULL);
    DHM_VALIDATE_RET(output != NULL);
    DHM_VALIDATE_RET(olen != NULL);
    DHM_VALIDATE_RET(f_rng != NULL);

    if (mbedtls_mpi_cmp_int(&ctx->P, 0) == 0)
        return (MBEDTLS_ERR_DHM_BAD_INPUT_DATA);

    /*
     * Generate X as large as possible ( < P )
     */
    do {
        MBEDTLS_MPI_CHK(mbedtls_mpi_fill_random(&ctx->X, x_size, f_rng, p_rng));

        while (mbedtls_mpi_cmp_mpi(&ctx->X, &ctx->P) >= 0)
            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&ctx->X, 1));

        if (count++ > 10)
            return (MBEDTLS_ERR_DHM_MAKE_PARAMS_FAILED);
    } while (dhm_check_range(&ctx->X, &ctx->P) != 0);

    /*
     * Calculate GX = G^X mod P
     */
    MBEDTLS_MPI_CHK(
        mbedtls_mpi_exp_mod(&ctx->GX, &ctx->G, &ctx->X, &ctx->P, &ctx->RP));

    if ((ret = dhm_check_range(&ctx->GX, &ctx->P)) != 0)
        return (ret);

        /*
         * export P, G, GX
         */
#define DHM_MPI_EXPORT(X, n)                                                   \
    do {                                                                       \
        MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary((X), p + 2, (n)));            \
        *p++ = (unsigned char)((n) >> 8);                                      \
        *p++ = (unsigned char)((n));                                           \
        p += (n);                                                              \
    } while (0)

    n1 = mbedtls_mpi_size(&ctx->P);
    n2 = mbedtls_mpi_size(&ctx->G);
    n3 = mbedtls_mpi_size(&ctx->GX);

    p = output;
    DHM_MPI_EXPORT(&ctx->P, n1);
    DHM_MPI_EXPORT(&ctx->G, n2);
    DHM_MPI_EXPORT(&ctx->GX, n3);

    *olen = p - output;

    ctx->len = n1;

cleanup:

    if (ret != 0)
        return (MBEDTLS_ERR_DHM_MAKE_PARAMS_FAILED + ret);

    return (0);
}

/*
 * Set prime modulus and generator
 */
int mbedtls_dhm_set_group(mbedtls_dhm_context *ctx,
                          const mbedtls_mpi *P,
                          const mbedtls_mpi *G)
{
    int ret;
    DHM_VALIDATE_RET(ctx != NULL);
    DHM_VALIDATE_RET(P != NULL);
    DHM_VALIDATE_RET(G != NULL);

    if ((ret = mbedtls_mpi_copy(&ctx->P, P)) != 0 ||
        (ret = mbedtls_mpi_copy(&ctx->G, G)) != 0) {
        return (MBEDTLS_ERR_DHM_SET_GROUP_FAILED + ret);
    }

    ctx->len = mbedtls_mpi_size(&ctx->P);
    return (0);
}

/*
 * Import the peer's public value G^Y
 */
int mbedtls_dhm_read_public(mbedtls_dhm_context *ctx,
                            const unsigned char *input,
                            size_t ilen)
{
    int ret;
    DHM_VALIDATE_RET(ctx != NULL);
    DHM_VALIDATE_RET(input != NULL);

    if (ilen < 1 || ilen > ctx->len)
        return (MBEDTLS_ERR_DHM_BAD_INPUT_DATA);

    if ((ret = mbedtls_mpi_read_binary(&ctx->GY, input, ilen)) != 0)
        return (MBEDTLS_ERR_DHM_READ_PUBLIC_FAILED + ret);

    return (0);
}

/*
 * Create own private value X and export G^X
 */
int mbedtls_dhm_make_public(mbedtls_dhm_context *ctx,
                            int x_size,
                            unsigned char *output,
                            size_t olen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret = 0;
    DHM_VALIDATE_RET(ctx != NULL);
    DHM_VALIDATE_RET(output != NULL);
    DHM_VALIDATE_RET(f_rng != NULL);

    if (olen < 1 || olen > ctx->len)
        return (MBEDTLS_ERR_DHM_BAD_INPUT_DATA);

    MBEDTLS_TE_DHM_CHK(te_dhm_make_public(MPI2BN(&ctx->P),
                                          MPI2BN(&ctx->G),
                                          x_size,
                                          MPI2BN(&ctx->X),
                                          MPI2BN(&ctx->GX),
                                          f_rng,
                                          p_rng));
    MBEDTLS_TE_DHM_CHK(te_bn_export(MPI2BN(&ctx->GX), output, olen));

cleanup:
    return (ret);
}

/*
 * Derive and export the shared secret (G^Y)^X mod P
 */
int mbedtls_dhm_calc_secret(mbedtls_dhm_context *ctx,
                            unsigned char *output,
                            size_t output_size,
                            size_t *olen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret = 0;
    DHM_VALIDATE_RET(ctx != NULL);
    DHM_VALIDATE_RET(output != NULL);
    DHM_VALIDATE_RET(olen != NULL);

    if (output_size < ctx->len)
        return (MBEDTLS_ERR_DHM_BAD_INPUT_DATA);

    MBEDTLS_TE_DHM_CHK(te_dhm_compute_shared(MPI2BN(&ctx->P),
                                             MPI2BN(&ctx->G),
                                             MPI2BN(&ctx->X),
                                             MPI2BN(&ctx->GY),
                                             MPI2BN(&ctx->pX),
                                             MPI2BN(&ctx->Vi),
                                             MPI2BN(&ctx->Vf),
                                             MPI2BN(&ctx->K),
                                             f_rng,
                                             p_rng));

    ret = te_bn_bytelen(MPI2BN(&ctx->K));
    if (ret < 0) {
        ret = MBEDTLS_CALL_TE_DHM(ret);
        goto cleanup;
    }

    *olen = ret;

    MBEDTLS_TE_DHM_CHK(te_bn_export(MPI2BN(&ctx->K), output, *olen));

cleanup:
    return (ret);
}

/*
 * Free the components of a DHM key
 */
void mbedtls_dhm_free(mbedtls_dhm_context *ctx)
{
    if (ctx == NULL)
        return;

    mbedtls_mpi_free(&ctx->pX);
    mbedtls_mpi_free(&ctx->Vf);
    mbedtls_mpi_free(&ctx->Vi);
    mbedtls_mpi_free(&ctx->RP);
    mbedtls_mpi_free(&ctx->K);
    mbedtls_mpi_free(&ctx->GY);
    mbedtls_mpi_free(&ctx->GX);
    mbedtls_mpi_free(&ctx->X);
    mbedtls_mpi_free(&ctx->G);
    mbedtls_mpi_free(&ctx->P);

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_dhm_context));
}

#if defined(MBEDTLS_ASN1_PARSE_C)
/*
 * Parse DHM parameters
 */
int mbedtls_dhm_parse_dhm(mbedtls_dhm_context *dhm,
                          const unsigned char *dhmin,
                          size_t dhminlen)
{
    int ret;
    size_t len;
    unsigned char *p, *end;
#if defined(MBEDTLS_PEM_PARSE_C)
    mbedtls_pem_context pem;
#endif /* MBEDTLS_PEM_PARSE_C */

    DHM_VALIDATE_RET(dhm != NULL);
    DHM_VALIDATE_RET(dhmin != NULL);

#if defined(MBEDTLS_PEM_PARSE_C)
    mbedtls_pem_init(&pem);

    /* Avoid calling mbedtls_pem_read_buffer() on non-null-terminated string */
    if (dhminlen == 0 || dhmin[dhminlen - 1] != '\0')
        ret = MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT;
    else
        ret = mbedtls_pem_read_buffer(&pem, "-----BEGIN DH PARAMETERS-----",
                                      "-----END DH PARAMETERS-----", dhmin,
                                      NULL, 0, &dhminlen);

    if (ret == 0) {
        /*
         * Was PEM encoded
         */
        dhminlen = pem.buflen;
    } else if (ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT)
        goto exit;

    p = (ret == 0) ? pem.buf : (unsigned char *)dhmin;
#else
    p = (unsigned char *)dhmin;
#endif /* MBEDTLS_PEM_PARSE_C */
    end = p + dhminlen;

    /*
     *  DHParams ::= SEQUENCE {
     *      prime              INTEGER,  -- P
     *      generator          INTEGER,  -- g
     *      privateValueLength INTEGER OPTIONAL
     *  }
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED |
                                        MBEDTLS_ASN1_SEQUENCE)) != 0) {
        ret = MBEDTLS_ERR_DHM_INVALID_FORMAT + ret;
        goto exit;
    }

    end = p + len;

    if ((ret = mbedtls_asn1_get_mpi(&p, end, &dhm->P)) != 0 ||
        (ret = mbedtls_asn1_get_mpi(&p, end, &dhm->G)) != 0) {
        ret = MBEDTLS_ERR_DHM_INVALID_FORMAT + ret;
        goto exit;
    }

    if (p != end) {
        /* This might be the optional privateValueLength.
         * If so, we can cleanly discard it */
        mbedtls_mpi rec;
        mbedtls_mpi_init(&rec);
        ret = mbedtls_asn1_get_mpi(&p, end, &rec);
        mbedtls_mpi_free(&rec);
        if (ret != 0) {
            ret = MBEDTLS_ERR_DHM_INVALID_FORMAT + ret;
            goto exit;
        }
        if (p != end) {
            ret = MBEDTLS_ERR_DHM_INVALID_FORMAT +
                  MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;
            goto exit;
        }
    }

    ret = 0;

    dhm->len = mbedtls_mpi_size(&dhm->P);

exit:
#if defined(MBEDTLS_PEM_PARSE_C)
    mbedtls_pem_free(&pem);
#endif
    if (ret != 0)
        mbedtls_dhm_free(dhm);

    return (ret);
}

#if defined(MBEDTLS_FS_IO)
/*
 * Load and parse DHM parameters
 */
int mbedtls_dhm_parse_dhmfile(mbedtls_dhm_context *dhm, const char *path)
{
    return MBEDTLS_ERR_DHM_FILE_IO_ERROR;
}
#endif /* MBEDTLS_FS_IO */
#endif /* MBEDTLS_ASN1_PARSE_C */

#endif /* MBEDTLS_DHM_C && MBEDTLS_DHM_ALT */
