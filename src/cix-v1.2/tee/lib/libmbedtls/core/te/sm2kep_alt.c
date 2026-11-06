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

#if defined(MBEDTLS_SM2KEP_GEN_PUBLIC_ALT) || \
    defined(MBEDTLS_SM2KEP_COMPUTE_SHARED_ALT)

#if !defined(MBEDTLS_BIGNUM_ALT)
#error "MBEDTLS_SM2KEP_GEN_PUBLIC_ALT, MBEDTLS_SM2KEP_COMPUTE_SHARED_ALT depend on MBEDTLS_BIGNUM_ALT"
#endif

#if !defined(MBEDTLS_ECP_ALT)
#error "MBEDTLS_SM2KEP_GEN_PUBLIC_ALT, MBEDTLS_SM2KEP_COMPUTE_SHARED_ALT depend on MBEDTLS_ECP_ALT"
#endif

#include "te_bn.h"
#include "te_sm2.h"
#include "mbedtls/sm2kep.h"

#include <string.h>

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdlib.h>
#define mbedtls_calloc calloc
#define mbedtls_free free
#endif

#include "mbedtls/platform_util.h"

/* Parameter validation macros based on platform_util.h */
#define SM2KE_VALIDATE_RET(cond)                                               \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
#define SM2KE_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

/*
 * Convert TE error code to mbedtls ECDSA error
 * Same as ECP error code
 */
extern int te_err_to_mbedtls_ecp_err(int te_err);

#define MBEDTLS_CALL_TE_ECP(func) te_err_to_mbedtls_ecp_err(func)
#define MBEDTLS_TE_ECP_CHK(f)                                                  \
    do {                                                                       \
        if ((ret = MBEDTLS_CALL_TE_ECP(f)) != 0) {                             \
            goto cleanup;                                                      \
        }                                                                      \
    } while (0)

#endif /* MBEDTLS_SM2KEP_GEN_PUBLIC_ALT || SM2KEP_COMPUTE_SHARED_ALT */

#if defined(MBEDTLS_SM2KEP_GEN_PUBLIC_ALT)
/*
 * Generate public key
 */
int mbedtls_sm2kep_gen_public(mbedtls_mpi *d,
                              mbedtls_ecp_point *Q,
                              int (*f_rng)(void *, unsigned char *, size_t),
                              void *p_rng)
{
    SM2KE_VALIDATE_RET(d != NULL);
    SM2KE_VALIDATE_RET(Q != NULL);
    SM2KE_VALIDATE_RET(f_rng != NULL);

    return MBEDTLS_CALL_TE_ECP(te_sm2dh_gen_public(
        te_platform_get_drvhandle(), MPI2BN(d), (te_ecp_point_t *)(Q), f_rng, p_rng));
}
#endif /* MBEDTLS_SM2KEP_GEN_PUBLIC_ALT */

#if defined(MBEDTLS_SM2KEP_COMPUTE_SHARED_ALT)
/*
 * Compute shared secret
 */
int mbedtls_sm2kep_compute_shared(mbedtls_ecp_point *Z,
                                  const mbedtls_ecp_point *R,
                                  const mbedtls_ecp_point *Rp,
                                  const mbedtls_ecp_point *Qp,
                                  const mbedtls_mpi *d,
                                  const mbedtls_mpi *r,
                                  int (*f_rng)(void *, unsigned char *, size_t),
                                  void *p_rng)
{
    SM2KE_VALIDATE_RET(Z != NULL);
    SM2KE_VALIDATE_RET(R != NULL);
    SM2KE_VALIDATE_RET(Rp != NULL);
    SM2KE_VALIDATE_RET(Qp != NULL);
    SM2KE_VALIDATE_RET(d != NULL);
    SM2KE_VALIDATE_RET(r != NULL);
    return MBEDTLS_CALL_TE_ECP(
        te_sm2dh_compute_shared(MPI2BN(d),
                                MPI2BN(r),
                                (const te_ecp_point_t *)R,
                                (const te_ecp_point_t *)Rp,
                                (const te_ecp_point_t *)Qp,
                                (te_ecp_point_t *)Z,
                                f_rng,
                                p_rng));
}
#endif /* MBEDTLS_SM2KEP_COMPUTE_SHARED_ALT */

#endif /* MBEDTLS_SM2KEP_C */
