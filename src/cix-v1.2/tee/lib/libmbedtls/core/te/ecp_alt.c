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

#if defined(MBEDTLS_ECP_C) && defined(MBEDTLS_ECP_ALT)

#if !defined(MBEDTLS_BIGNUM_ALT)
#error "MBEDTLS_ECP_ALT depend on MBEDTLS_BIGNUM_ALT"
#endif

#include "te_bn.h"
#include "te_ecp.h"
#include "mbedtls/ecp.h"
#include "mbedtls/platform_util.h"

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdlib.h>
#include <stdio.h>
#define mbedtls_printf printf
#define mbedtls_calloc calloc
#define mbedtls_free free
#endif

/* Parameter validation macros based on platform_util.h */
#define ECP_VALIDATE_RET(cond)                                                 \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
#define ECP_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

extern int te_err_to_mbedtls_ecp_err(int te_err);

/*
 * Convert TE error code to mbedtls ECP error
 */
int te_err_to_mbedtls_ecp_err(int te_err)
{
    switch (te_err) {
    case TE_SUCCESS:
        return 0;
    case TE_ERROR_BAD_PARAMS:
    case TE_ERROR_BAD_INPUT_DATA:
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    case TE_ERROR_SHORT_BUFFER:
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    case TE_ERROR_FEATURE_UNAVAIL:
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    case TE_ERROR_VERIFY_SIG:
        return MBEDTLS_ERR_ECP_VERIFY_FAILED;
    case TE_ERROR_OOM:
        return MBEDTLS_ERR_ECP_ALLOC_FAILED;
    case TE_ERROR_GEN_RANDOM:
        return MBEDTLS_ERR_ECP_RANDOM_FAILED;
    case TE_ERROR_INVAL_KEY:
        return MBEDTLS_ERR_ECP_INVALID_KEY;
    default:
        return MBEDTLS_ERR_MPI_HW_FAILED;
    }
}

#define MBEDTLS_CALL_TE_ECP(func) te_err_to_mbedtls_ecp_err(func)
#define MBEDTLS_TE_ECP_CHK(f)                                                  \
    do {                                                                       \
        if ((ret = MBEDTLS_CALL_TE_ECP(f)) != 0) {                             \
            goto cleanup;                                                      \
        }                                                                      \
    } while (0)

#define _TO_TE_BN(__mbedtls_mpi_ptr__) (__mbedtls_mpi_ptr__->p)

#define _TO_TE_ECP_POINT(__mbedtls_ecp_point_ptr__)                            \
    {                                                                          \
        .X = _TO_TE_BN(&(__mbedtls_ecp_point_ptr__->X));                       \
        .Y = _TO_TE_BN(&(__mbedtls_ecp_point_ptr__->Y));                       \
        .Z = _TO_TE_BN(&(__mbedtls_ecp_point_ptr__->Z));                       \
    }

#define _TO_TE_ECP_GROUP(__mbedtls_ecp_group_ptr__)                            \
    {                                                                          \
        .id    = __mbedtls_ecp_group_ptr__->id;                                \
        .P     = _TO_TE_BN(&(__mbedtls_ecp_group_ptr__->P));                   \
        .A     = _TO_TE_BN(&(__mbedtls_ecp_group_ptr__->A));                   \
        .B     = _TO_TE_BN(&(__mbedtls_ecp_group_ptr__->B));                   \
        .G     = _TO_TE_ECP_POINT(&(__mbedtls_ecp_group_ptr__->G));            \
        .N     = _TO_TE_BN(&(__mbedtls_ecp_group_ptr__->N));                   \
        .pbits = __mbedtls_ecp_group_ptr__->pbits;                             \
        .nbits = __mbedtls_ecp_group_ptr__->nbits;                             \
    }

#if defined(MBEDTLS_ECP_RESTARTABLE)
int mbedtls_ecp_check_budget(const mbedtls_ecp_group *grp,
                             mbedtls_ecp_restart_ctx *rs_ctx,
                             unsigned ops)
{
    OSAL_ASSERT(0);
    return MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED;
}

void mbedtls_ecp_set_max_ops(unsigned max_ops)
{
    OSAL_ASSERT(0);
}
int mbedtls_ecp_restart_is_enabled(void)
{
    OSAL_ASSERT(0);
    return MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED;
}

void mbedtls_ecp_restart_init(mbedtls_ecp_restart_ctx *ctx)
{
    OSAL_ASSERT(0);
}
void mbedtls_ecp_restart_free(mbedtls_ecp_restart_ctx *ctx)
{
    OSAL_ASSERT(0);
}
#endif

#if defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED) ||                               \
    defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED) ||                               \
    defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED) ||                               \
    defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED) ||                               \
    defined(MBEDTLS_ECP_DP_SECP521R1_ENABLED) ||                               \
    defined(MBEDTLS_ECP_DP_BP256R1_ENABLED) ||                                 \
    defined(MBEDTLS_ECP_DP_BP384R1_ENABLED) ||                                 \
    defined(MBEDTLS_ECP_DP_BP512R1_ENABLED) ||                                 \
    defined(MBEDTLS_ECP_DP_SECP192K1_ENABLED) ||                               \
    defined(MBEDTLS_ECP_DP_SECP224K1_ENABLED) ||                               \
    defined(MBEDTLS_ECP_DP_SECP256K1_ENABLED)
#define ECP_SHORTWEIERSTRASS
#endif

#if defined(MBEDTLS_ECP_DP_CURVE25519_ENABLED) ||                              \
    defined(MBEDTLS_ECP_DP_CURVE448_ENABLED)
#define ECP_MONTGOMERY
#endif

/*
 * Curve types: internal for now, might be exposed later
 */
typedef enum {
    ECP_TYPE_NONE = 0,
    ECP_TYPE_SHORT_WEIERSTRASS, /* y^2 = x^3 + a x + b      */
    ECP_TYPE_MONTGOMERY,        /* y^2 = x^3 + a x^2 + x    */
} ecp_curve_type;

/*
 * List of supported curves:
 *  - internal ID
 *  - TLS NamedCurve ID (RFC 4492 sec. 5.1.1, RFC 7071 sec. 2)
 *  - size in bits
 *  - readable name
 *
 * Curves are listed in order: largest curves first, and for a given size,
 * fastest curves first. This provides the default order for the SSL module.
 *
 * Reminder: update profiles in x509_crt.c when adding a new curves!
 */
static const mbedtls_ecp_curve_info ecp_supported_curves[] = {
#if defined(MBEDTLS_ECP_DP_SECP521R1_ENABLED)
    {MBEDTLS_ECP_DP_SECP521R1, 25, 521, "secp521r1"},
#endif
#if defined(MBEDTLS_ECP_DP_BP512R1_ENABLED)
    {MBEDTLS_ECP_DP_BP512R1, 28, 512, "brainpoolP512r1"},
#endif
#if defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED)
    {MBEDTLS_ECP_DP_SECP384R1, 24, 384, "secp384r1"},
#endif
#if defined(MBEDTLS_ECP_DP_BP384R1_ENABLED)
    {MBEDTLS_ECP_DP_BP384R1, 27, 384, "brainpoolP384r1"},
#endif
#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
    {MBEDTLS_ECP_DP_SECP256R1, 23, 256, "secp256r1"},
#endif
#if defined(MBEDTLS_ECP_DP_SECP256K1_ENABLED)
    {MBEDTLS_ECP_DP_SECP256K1, 22, 256, "secp256k1"},
#endif
#if defined(MBEDTLS_ECP_DP_BP256R1_ENABLED)
    {MBEDTLS_ECP_DP_BP256R1, 26, 256, "brainpoolP256r1"},
#endif
#if defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED)
    {MBEDTLS_ECP_DP_SECP224R1, 21, 224, "secp224r1"},
#endif
#if defined(MBEDTLS_ECP_DP_SECP224K1_ENABLED)
    {MBEDTLS_ECP_DP_SECP224K1, 20, 224, "secp224k1"},
#endif
#if defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED)
    {MBEDTLS_ECP_DP_SECP192R1, 19, 192, "secp192r1"},
#endif
#if defined(MBEDTLS_ECP_DP_SECP192K1_ENABLED)
    {MBEDTLS_ECP_DP_SECP192K1, 18, 192, "secp192k1"},
#endif
/*SM2 curve id, we refer to GMSSL ssl_curve_tbl in t1_trce.c*/
#if defined(MBEDTLS_ECP_DP_SM2P256V1_ENABLED)
    { MBEDTLS_ECP_DP_SM2P256V1,    30,     256,    "sm2p256v1"         },
#endif
    {MBEDTLS_ECP_DP_NONE, 0, 0, NULL},
};

#define ECP_NB_CURVES                                                          \
    sizeof(ecp_supported_curves) / sizeof(ecp_supported_curves[0])

static mbedtls_ecp_group_id ecp_supported_grp_id[ECP_NB_CURVES];

/*
 * List of supported curves and associated info
 */
const mbedtls_ecp_curve_info *mbedtls_ecp_curve_list(void)
{
    return (ecp_supported_curves);
}

/*
 * List of supported curves, group ID only
 */
const mbedtls_ecp_group_id *mbedtls_ecp_grp_id_list(void)
{
    static int init_done = 0;

    if (!init_done) {
        size_t i = 0;
        const mbedtls_ecp_curve_info *curve_info;

        for (curve_info = mbedtls_ecp_curve_list();
             curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
             curve_info++) {
            ecp_supported_grp_id[i++] = curve_info->grp_id;
        }
        ecp_supported_grp_id[i] = MBEDTLS_ECP_DP_NONE;

        init_done = 1;
    }

    return (ecp_supported_grp_id);
}

/*
 * Get the curve info for the internal identifier
 */
const mbedtls_ecp_curve_info *
mbedtls_ecp_curve_info_from_grp_id(mbedtls_ecp_group_id grp_id)
{
    const mbedtls_ecp_curve_info *curve_info;

    for (curve_info = mbedtls_ecp_curve_list();
         curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
         curve_info++) {
        if (curve_info->grp_id == grp_id)
            return (curve_info);
    }

    return (NULL);
}

/*
 * Get the curve info from the TLS identifier
 */
const mbedtls_ecp_curve_info *
mbedtls_ecp_curve_info_from_tls_id(uint16_t tls_id)
{
    const mbedtls_ecp_curve_info *curve_info;

    for (curve_info = mbedtls_ecp_curve_list();
         curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
         curve_info++) {
        if (curve_info->tls_id == tls_id)
            return (curve_info);
    }

    return (NULL);
}

/*
 * Get the curve info from the name
 */
const mbedtls_ecp_curve_info *mbedtls_ecp_curve_info_from_name(const char *name)
{
    const mbedtls_ecp_curve_info *curve_info;

    if (name == NULL)
        return (NULL);

    for (curve_info = mbedtls_ecp_curve_list();
         curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
         curve_info++) {
        if (strcmp(curve_info->name, name) == 0)
            return (curve_info);
    }

    return (NULL);
}

/*
 * Get the type of a curve
 */
__attribute__((unused))
static inline ecp_curve_type ecp_get_type(const mbedtls_ecp_group *grp)
{
    if (grp->id == MBEDTLS_ECP_DP_SECP192R1 ||
        grp->id == MBEDTLS_ECP_DP_SECP224R1 ||
        grp->id == MBEDTLS_ECP_DP_SECP256R1 ||
        grp->id == MBEDTLS_ECP_DP_SECP384R1 ||
        grp->id == MBEDTLS_ECP_DP_SECP521R1 ||
        grp->id == MBEDTLS_ECP_DP_BP256R1 ||
        grp->id == MBEDTLS_ECP_DP_BP384R1 ||
        grp->id == MBEDTLS_ECP_DP_BP512R1 ||
        grp->id == MBEDTLS_ECP_DP_SECP192K1 ||
        grp->id == MBEDTLS_ECP_DP_SECP224K1 ||
        grp->id == MBEDTLS_ECP_DP_SECP256K1) {
        return ECP_TYPE_SHORT_WEIERSTRASS;
    } else if (grp->id == MBEDTLS_ECP_DP_CURVE25519 ||
               grp->id == MBEDTLS_ECP_DP_CURVE448) {
        return ECP_TYPE_MONTGOMERY;
    } else {
        return ECP_TYPE_NONE;
    }
}

/*
 * Initialize (the components of) a point
 */
void mbedtls_ecp_point_init(mbedtls_ecp_point *pt)
{
    int ret = 0;
    ECP_VALIDATE(pt != NULL);

    OSAL_ASSERT(sizeof(te_ecp_point_t) == sizeof(mbedtls_ecp_point));

    ret = te_ecp_point_init(te_platform_get_drvhandle(), (te_ecp_point_t *)pt);
    OSAL_ASSERT(TE_SUCCESS == ret);
}

/*
 * Initialize (the components of) a group
 */
void mbedtls_ecp_group_init(mbedtls_ecp_group *grp)
{
    int ret = 0;
    ECP_VALIDATE(grp != NULL);

    OSAL_ASSERT(sizeof(te_ecp_group_t) == sizeof(mbedtls_ecp_group));

    ret = te_ecp_group_init(te_platform_get_drvhandle(), (te_ecp_group_t *)grp);
    OSAL_ASSERT(TE_SUCCESS == ret);
}

/*
 * Initialize (the components of) a key pair
 */
void mbedtls_ecp_keypair_init(mbedtls_ecp_keypair *key)
{
    ECP_VALIDATE(key != NULL);

    mbedtls_ecp_group_init(&key->grp);
    mbedtls_mpi_init(&key->d);
    mbedtls_ecp_point_init(&key->Q);
}

/*
 * Unallocate (the components of) a point
 */
void mbedtls_ecp_point_free(mbedtls_ecp_point *pt)
{
    if (pt == NULL)
        return;

    te_ecp_point_free((te_ecp_point_t *)pt);
}

/*
 * Unallocate (the components of) a group
 */
void mbedtls_ecp_group_free(mbedtls_ecp_group *grp)
{
    if (grp == NULL)
        return;

    te_ecp_group_free((te_ecp_group_t *)grp);
    mbedtls_platform_zeroize(grp, sizeof(mbedtls_ecp_group));
}

/*
 * Unallocate (the components of) a key pair
 */
void mbedtls_ecp_keypair_free(mbedtls_ecp_keypair *key)
{
    if (key == NULL)
        return;

    mbedtls_ecp_group_free(&key->grp);
    mbedtls_mpi_free(&key->d);
    mbedtls_ecp_point_free(&key->Q);
}

/*
 * Copy the contents of a point
 */
int mbedtls_ecp_copy(mbedtls_ecp_point *P, const mbedtls_ecp_point *Q)
{
    ECP_VALIDATE_RET(P != NULL);
    ECP_VALIDATE_RET(Q != NULL);

    return MBEDTLS_CALL_TE_ECP(
        te_ecp_point_copy((te_ecp_point_t *)P, (te_ecp_point_t *)Q));
}

/*
 * Copy the contents of a group object
 */
int mbedtls_ecp_group_copy(mbedtls_ecp_group *dst, const mbedtls_ecp_group *src)
{
    ECP_VALIDATE_RET(dst != NULL);
    ECP_VALIDATE_RET(src != NULL);

    return MBEDTLS_CALL_TE_ECP(
        te_ecp_group_copy((te_ecp_group_t *)dst, (te_ecp_group_t *)src));
}

/*
 * Set point to zero
 */
int mbedtls_ecp_set_zero(mbedtls_ecp_point *pt)
{
    ECP_VALIDATE_RET(pt != NULL);

    return MBEDTLS_CALL_TE_ECP(te_ecp_set_zero((te_ecp_point_t *)pt));
}

/*
 * Tell if a point is zero
 */
int mbedtls_ecp_is_zero(mbedtls_ecp_point *pt)
{
    int ret = 0;
    ECP_VALIDATE_RET(pt != NULL);

    ret = te_ecp_is_zero((te_ecp_point_t *)pt);
    if ((ret != 0) && (ret != 1)) {
        return MBEDTLS_CALL_TE_ECP(ret);
    }
    return ret;
}

/*
 * Compare two points lazily
 */
int mbedtls_ecp_point_cmp(const mbedtls_ecp_point *P,
                          const mbedtls_ecp_point *Q)
{
    int ret = 0;
    ECP_VALIDATE_RET(P != NULL);
    ECP_VALIDATE_RET(Q != NULL);

    ret =
        te_ecp_point_cmp((const te_ecp_point_t *)P, (const te_ecp_point_t *)Q);
    if ((ret != 0) && (ret != 1)) {
        return MBEDTLS_CALL_TE_ECP(ret);
    }
    return (ret == 0) ? (0) : (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
}

/*
 * Import a non-zero point from ASCII strings
 */
int mbedtls_ecp_point_read_string(mbedtls_ecp_point *P,
                                  int radix,
                                  const char *x,
                                  const char *y)
{
    int ret;
    ECP_VALIDATE_RET(P != NULL);
    ECP_VALIDATE_RET(x != NULL);
    ECP_VALIDATE_RET(y != NULL);

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&P->X, radix, x));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&P->Y, radix, y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&P->Z, 1));

cleanup:
    return (ret);
}

/*
 * Export a point into unsigned binary data (SEC1 2.3.3)
 */
int mbedtls_ecp_point_write_binary(const mbedtls_ecp_group *grp,
                                   const mbedtls_ecp_point *P,
                                   int format,
                                   size_t *olen,
                                   unsigned char *buf,
                                   size_t buflen)
{
    int ret            = 0;
    size_t tmp_size    = 0;
    bool is_compressed = false;

    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(P != NULL);
    ECP_VALIDATE_RET(olen != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(format == MBEDTLS_ECP_PF_UNCOMPRESSED ||
                     format == MBEDTLS_ECP_PF_COMPRESSED);

    is_compressed = (format == MBEDTLS_ECP_PF_COMPRESSED) ? (true) : (false);

    tmp_size = buflen;
    ret = te_ecp_point_export((const te_ecp_group_t *)grp, (te_ecp_point_t *)P,
                              is_compressed, buf, &tmp_size);
    if ((int)TE_ERROR_SHORT_BUFFER == ret) {
        *olen = tmp_size;
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    } else if (TE_SUCCESS == ret) {
        *olen = tmp_size;
        return 0;
    } else {
        return MBEDTLS_CALL_TE_ECP(ret);
    }
}

/*
 * Import a point from unsigned binary data (SEC1 2.3.4)
 */
int mbedtls_ecp_point_read_binary(const mbedtls_ecp_group *grp,
                                  mbedtls_ecp_point *pt,
                                  const unsigned char *buf,
                                  size_t ilen)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(pt != NULL);
    ECP_VALIDATE_RET(buf != NULL);

    if (ilen < 1) {
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    }

    return MBEDTLS_CALL_TE_ECP(te_ecp_point_import(
        (const te_ecp_group_t *)grp, (te_ecp_point_t *)pt, false, buf, ilen));
}

/*
 * Import a point from a TLS ECPoint record (RFC 4492)
 *      struct {
 *          opaque point <1..2^8-1>;
 *      } ECPoint;
 */
int mbedtls_ecp_tls_read_point(const mbedtls_ecp_group *grp,
                               mbedtls_ecp_point *pt,
                               const unsigned char **buf,
                               size_t buf_len)
{
    unsigned char data_len         = 0;
    const unsigned char *buf_start = NULL;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(pt != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(*buf != NULL);

    /*
     * We must have at least two bytes (1 for length, at least one for data)
     */
    if (buf_len < 2)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    data_len = *(*buf)++;
    if (data_len < 1 || data_len > buf_len - 1)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    /*
     * Save buffer start for read_binary and update buf
     */
    buf_start = *buf;
    *buf += data_len;

    return (mbedtls_ecp_point_read_binary(grp, pt, buf_start, data_len));
}

/*
 * Export a point as a TLS ECPoint record (RFC 4492)
 *      struct {
 *          opaque point <1..2^8-1>;
 *      } ECPoint;
 */
int mbedtls_ecp_tls_write_point(const mbedtls_ecp_group *grp,
                                const mbedtls_ecp_point *pt,
                                int format,
                                size_t *olen,
                                unsigned char *buf,
                                size_t blen)
{
    int ret = 0;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(pt != NULL);
    ECP_VALIDATE_RET(olen != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(format == MBEDTLS_ECP_PF_UNCOMPRESSED ||
                     format == MBEDTLS_ECP_PF_COMPRESSED);

    /*
     * buffer length must be at least one, for our length byte
     */
    if (blen < 1)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    if ((ret = mbedtls_ecp_point_write_binary(grp, pt, format, olen, buf + 1,
                                              blen - 1)) != 0)
        return (ret);

    /*
     * write length to the first byte and update total length
     */
    buf[0] = (unsigned char)*olen;
    ++*olen;

    return (0);
}

/*
 * Set a group from an ECParameters record (RFC 4492)
 */
int mbedtls_ecp_tls_read_group(mbedtls_ecp_group *grp,
                               const unsigned char **buf,
                               size_t len)
{
    int ret = 0;
    mbedtls_ecp_group_id grp_id;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(*buf != NULL);

    if ((ret = mbedtls_ecp_tls_read_group_id(&grp_id, buf, len)) != 0)
        return (ret);

    return (mbedtls_ecp_group_load(grp, grp_id));
}

/*
 * Read a group id from an ECParameters record (RFC 4492) and convert it to
 * mbedtls_ecp_group_id.
 */
int mbedtls_ecp_tls_read_group_id(mbedtls_ecp_group_id *grp,
                                  const unsigned char **buf,
                                  size_t len)
{
    uint16_t tls_id                          = 0;
    const mbedtls_ecp_curve_info *curve_info = NULL;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(*buf != NULL);

    /*
     * We expect at least three bytes (see below)
     */
    if (len < 3)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    /*
     * First byte is curve_type; only named_curve is handled
     */
    if (*(*buf)++ != MBEDTLS_ECP_TLS_NAMED_CURVE)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    /*
     * Next two bytes are the namedcurve value
     */
    tls_id = *(*buf)++;
    tls_id <<= 8;
    tls_id |= *(*buf)++;

    if ((curve_info = mbedtls_ecp_curve_info_from_tls_id(tls_id)) == NULL)
        return (MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE);

    *grp = curve_info->grp_id;

    return (0);
}

/*
 * Write the ECParameters record corresponding to a group (RFC 4492)
 */
int mbedtls_ecp_tls_write_group(const mbedtls_ecp_group *grp,
                                size_t *olen,
                                unsigned char *buf,
                                size_t blen)
{
    const mbedtls_ecp_curve_info *curve_info = NULL;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(olen != NULL);

    if ((curve_info = mbedtls_ecp_curve_info_from_grp_id(grp->id)) == NULL)
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);

    /*
     * We are going to write 3 bytes (see below)
     */
    *olen = 3;
    if (blen < *olen)
        return (MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL);

    /*
     * First byte is curve_type, always named_curve
     */
    *buf++ = MBEDTLS_ECP_TLS_NAMED_CURVE;

    /*
     * Next two bytes are the namedcurve value
     */
    buf[0] = curve_info->tls_id >> 8;
    buf[1] = curve_info->tls_id & 0xFF;

    return (0);
}

/*
 * Multiplication R = m * P
 */
int mbedtls_ecp_mul(mbedtls_ecp_group *grp,
                    mbedtls_ecp_point *R,
                    const mbedtls_mpi *m,
                    const mbedtls_ecp_point *P,
                    int (*f_rng)(void *, unsigned char *, size_t),
                    void *p_rng)
{
    int ret = 0;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(R != NULL);
    ECP_VALIDATE_RET(m != NULL);
    ECP_VALIDATE_RET(P != NULL);

    /* Common sanity checks */
    MBEDTLS_TE_ECP_CHK(
        te_ecp_check_privkey((const te_ecp_group_t *)grp, MPI2BN(m)));
    MBEDTLS_TE_ECP_CHK(te_ecp_check_pubkey((const te_ecp_group_t *)grp,
                                           (const te_ecp_point_t *)P));

    MBEDTLS_TE_ECP_CHK(te_ecp_mul((const te_ecp_group_t *)grp,
                                  (te_ecp_point_t *)R,
                                  MPI2BN(m),
                                  (const te_ecp_point_t *)P,
                                  f_rng,
                                  p_rng));

cleanup:
    return ret;
}

/*
 * Restartable multiplication R = m * P
 */
int mbedtls_ecp_mul_restartable(mbedtls_ecp_group *grp,
                                mbedtls_ecp_point *R,
                                const mbedtls_mpi *m,
                                const mbedtls_ecp_point *P,
                                int (*f_rng)(void *, unsigned char *, size_t),
                                void *p_rng,
                                mbedtls_ecp_restart_ctx *rs_ctx)
{
    (void)(rs_ctx);
    return mbedtls_ecp_mul(grp, R, m, P, f_rng, p_rng);
}

/*
 * Linear combination
 * NOT constant-time
 */
int mbedtls_ecp_muladd(mbedtls_ecp_group *grp,
                       mbedtls_ecp_point *R,
                       const mbedtls_mpi *m,
                       const mbedtls_ecp_point *P,
                       const mbedtls_mpi *n,
                       const mbedtls_ecp_point *Q)
{
    int ret = 0;

    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(R != NULL);
    ECP_VALIDATE_RET(m != NULL);
    ECP_VALIDATE_RET(P != NULL);
    ECP_VALIDATE_RET(n != NULL);
    ECP_VALIDATE_RET(Q != NULL);

    if ((0 != mbedtls_mpi_cmp_int(m, 1)) && (0 != mbedtls_mpi_cmp_int(m, -1))) {
        MBEDTLS_TE_ECP_CHK(
            te_ecp_check_privkey((const te_ecp_group_t *)grp, MPI2BN(m)));
    }
    if ((0 != mbedtls_mpi_cmp_int(n, 1)) && (0 != mbedtls_mpi_cmp_int(n, -1))) {
        MBEDTLS_TE_ECP_CHK(
            te_ecp_check_privkey((const te_ecp_group_t *)grp, MPI2BN(n)));
    }
    MBEDTLS_TE_ECP_CHK(te_ecp_check_pubkey((const te_ecp_group_t *)grp,
                                           (const te_ecp_point_t *)P));
    MBEDTLS_TE_ECP_CHK(te_ecp_check_pubkey((const te_ecp_group_t *)grp,
                                           (const te_ecp_point_t *)Q));

    MBEDTLS_TE_ECP_CHK(te_ecp_muladd((const te_ecp_group_t *)grp,
                                     (te_ecp_point_t *)R,
                                     MPI2BN(m),
                                     (const te_ecp_point_t *)P,
                                     MPI2BN(n),
                                     (const te_ecp_point_t *)Q));

cleanup:
    return ret;
}

/*
 * Restartable linear combination
 * NOT constant-time
 */
int mbedtls_ecp_muladd_restartable(mbedtls_ecp_group *grp,
                                   mbedtls_ecp_point *R,
                                   const mbedtls_mpi *m,
                                   const mbedtls_ecp_point *P,
                                   const mbedtls_mpi *n,
                                   const mbedtls_ecp_point *Q,
                                   mbedtls_ecp_restart_ctx *rs_ctx)
{
    (void)(rs_ctx);
    return mbedtls_ecp_muladd(grp, R, m, P, n, Q);
}

/*
 * Check that a point is valid as a public key
 */
int mbedtls_ecp_check_pubkey(const mbedtls_ecp_group *grp,
                             const mbedtls_ecp_point *pt)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(pt != NULL);

    return MBEDTLS_CALL_TE_ECP(te_ecp_check_pubkey((const te_ecp_group_t *)grp,
                                                  (const te_ecp_point_t *)pt));
}

/*
 * Check that an mbedtls_mpi is valid as a private key
 */
int mbedtls_ecp_check_privkey(const mbedtls_ecp_group *grp,
                              const mbedtls_mpi *d)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(d != NULL);

    return MBEDTLS_CALL_TE_ECP(
        te_ecp_check_privkey((const te_ecp_group_t *)grp, MPI2BN(d)));
}

/*
 * Generate a private key
 */
int mbedtls_ecp_gen_privkey(const mbedtls_ecp_group *grp,
                            mbedtls_mpi *d,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(d != NULL);
    ECP_VALIDATE_RET(f_rng != NULL);

    return MBEDTLS_CALL_TE_ECP(te_ecp_gen_privkey((const te_ecp_group_t *)grp,
                                                  MPI2BN(d), f_rng, p_rng));
}

/*
 * Generate a keypair with configurable base point
 */
int mbedtls_ecp_gen_keypair_base(mbedtls_ecp_group *grp,
                                 const mbedtls_ecp_point *G,
                                 mbedtls_mpi *d,
                                 mbedtls_ecp_point *Q,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(d != NULL);
    ECP_VALIDATE_RET(G != NULL);
    ECP_VALIDATE_RET(Q != NULL);
    ECP_VALIDATE_RET(f_rng != NULL);

    return MBEDTLS_CALL_TE_ECP(
        te_ecp_gen_keypair_base((const te_ecp_group_t *)grp,
                                (const te_ecp_point_t *)G,
                                MPI2BN(d),
                                (te_ecp_point_t *)Q,
                                f_rng,
                                (void *)p_rng));
}

/*
 * Generate key pair, wrapper for conventional base point
 */
int mbedtls_ecp_gen_keypair(mbedtls_ecp_group *grp,
                            mbedtls_mpi *d,
                            mbedtls_ecp_point *Q,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(d != NULL);
    ECP_VALIDATE_RET(Q != NULL);
    ECP_VALIDATE_RET(f_rng != NULL);

    return MBEDTLS_CALL_TE_ECP(te_ecp_gen_keypair((const te_ecp_group_t *)grp,
                                                 MPI2BN(d),
                                                 (te_ecp_point_t *)Q,
                                                 f_rng,
                                                 (void *)p_rng));
}

/*
 * Generate a keypair, prettier wrapper
 */
int mbedtls_ecp_gen_key(mbedtls_ecp_group_id grp_id,
                        mbedtls_ecp_keypair *key,
                        int (*f_rng)(void *, unsigned char *, size_t),
                        void *p_rng)
{
    int ret;
    ECP_VALIDATE_RET(key != NULL);
    ECP_VALIDATE_RET(f_rng != NULL);

    if ((ret = mbedtls_ecp_group_load(&key->grp, grp_id)) != 0)
        return (ret);

    return (mbedtls_ecp_gen_keypair(&key->grp, &key->d, &key->Q, f_rng, p_rng));
}

/*
 * Check a public-private key pair
 */
int mbedtls_ecp_check_pub_priv(const mbedtls_ecp_keypair *pub,
                               const mbedtls_ecp_keypair *prv)
{
    int ret               = 0;
    mbedtls_ecp_point Q   = {0};
    mbedtls_ecp_group grp = {0};
    ECP_VALIDATE_RET(pub != NULL);
    ECP_VALIDATE_RET(prv != NULL);

    if (pub->grp.id == MBEDTLS_ECP_DP_NONE || pub->grp.id != prv->grp.id ||
        mbedtls_mpi_cmp_mpi(&pub->Q.X, &prv->Q.X) ||
        mbedtls_mpi_cmp_mpi(&pub->Q.Y, &prv->Q.Y) ||
        mbedtls_mpi_cmp_mpi(&pub->Q.Z, &prv->Q.Z)) {
        return (MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    }

    mbedtls_ecp_point_init(&Q);
    mbedtls_ecp_group_init(&grp);

    /* mbedtls_ecp_mul() needs a non-const group... */
    mbedtls_ecp_group_copy(&grp, &prv->grp);

    /* Also checks d is valid */
    MBEDTLS_MPI_CHK(
        mbedtls_ecp_mul(&grp, &Q, &prv->d, &prv->grp.G, NULL, NULL));

    if (mbedtls_mpi_cmp_mpi(&Q.X, &prv->Q.X) ||
        mbedtls_mpi_cmp_mpi(&Q.Y, &prv->Q.Y) ||
        mbedtls_mpi_cmp_mpi(&Q.Z, &prv->Q.Z)) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_point_free(&Q);
    mbedtls_ecp_group_free(&grp);

    return (ret);
}

int mbedtls_ecp_group_load(mbedtls_ecp_group *grp, mbedtls_ecp_group_id id)
{
    ECP_VALIDATE_RET(grp != NULL);

    return MBEDTLS_CALL_TE_ECP(te_ecp_group_load((te_ecp_group_t * )grp,
                                                 (te_ecp_group_id_t)id));
}

#if defined(MBEDTLS_SELF_TEST)

/*
 * Counts of point addition and doubling, and field multiplications.
 * Used to test resistance of point multiplication to simple timing attacks.
 */
static unsigned long add_count, dbl_count, mul_count;

/*
 * Checkup routine
 */
int mbedtls_ecp_self_test(int verbose)
{
    int ret;
    size_t i;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point R, P;
    mbedtls_mpi m;
    unsigned long add_c_prev, dbl_c_prev, mul_c_prev;
    /* exponents especially adapted for secp192r1 */
    const char *exponents[] = {
        "000000000000000000000000000000000000000000000001", /* one */
        "FFFFFFFFFFFFFFFFFFFFFFFF99DEF836146BC9B1B4D22830", /* N - 1 */
        "5EA6F389A38B8BC81E767753B15AA5569E1782E30ABE7D25", /* random */
        "400000000000000000000000000000000000000000000000", /* one and zeros */
        "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", /* all ones */
        "555555555555555555555555555555555555555555555555", /* 101010... */
    };

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&R);
    mbedtls_ecp_point_init(&P);
    mbedtls_mpi_init(&m);

    /* Use secp192r1 if available, or any available curve */
#if defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED)
    MBEDTLS_MPI_CHK(mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP192R1));
#else
    MBEDTLS_MPI_CHK(
        mbedtls_ecp_group_load(&grp, mbedtls_ecp_curve_list()->grp_id));
#endif

    if (verbose != 0)
        mbedtls_printf("  ECP test #1 (constant op_count, base point G): ");

    /* Do a dummy multiplication first to trigger precomputation */
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&m, 2));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&grp, &P, &m, &grp.G, NULL, NULL));

    add_count = 0;
    dbl_count = 0;
    mul_count = 0;
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&m, 16, exponents[0]));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&grp, &R, &m, &grp.G, NULL, NULL));

    for (i = 1; i < sizeof(exponents) / sizeof(exponents[0]); i++) {
        add_c_prev = add_count;
        dbl_c_prev = dbl_count;
        mul_c_prev = mul_count;
        add_count  = 0;
        dbl_count  = 0;
        mul_count  = 0;

        MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&m, 16, exponents[i]));
        MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&grp, &R, &m, &grp.G, NULL, NULL));

        if (add_count != add_c_prev || dbl_count != dbl_c_prev ||
            mul_count != mul_c_prev) {
            if (verbose != 0)
                mbedtls_printf("failed (%u)\n", (unsigned int)i);

            ret = 1;
            goto cleanup;
        }
    }

    if (verbose != 0)
        mbedtls_printf("passed\n");

    if (verbose != 0)
        mbedtls_printf("  ECP test #2 (constant op_count, other point): ");
    /* We computed P = 2G last time, use it */

    add_count = 0;
    dbl_count = 0;
    mul_count = 0;
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&m, 16, exponents[0]));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&grp, &R, &m, &P, NULL, NULL));

    for (i = 1; i < sizeof(exponents) / sizeof(exponents[0]); i++) {
        add_c_prev = add_count;
        dbl_c_prev = dbl_count;
        mul_c_prev = mul_count;
        add_count  = 0;
        dbl_count  = 0;
        mul_count  = 0;

        MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&m, 16, exponents[i]));
        MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&grp, &R, &m, &P, NULL, NULL));

        if (add_count != add_c_prev || dbl_count != dbl_c_prev ||
            mul_count != mul_c_prev) {
            if (verbose != 0)
                mbedtls_printf("failed (%u)\n", (unsigned int)i);

            ret = 1;
            goto cleanup;
        }
    }

    if (verbose != 0)
        mbedtls_printf("passed\n");

cleanup:

    if (ret < 0 && verbose != 0)
        mbedtls_printf("Unexpected error, return code = %08X\n", ret);

    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&R);
    mbedtls_ecp_point_free(&P);
    mbedtls_mpi_free(&m);

    if (verbose != 0)
        mbedtls_printf("\n");

    return (ret);
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_ECP_C && MBEDTLS_ECP_ALT */
