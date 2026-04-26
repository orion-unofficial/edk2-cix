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

#if defined(MBEDTLS_SM2DSA_SIGN_ALT) || defined(MBEDTLS_SM2DSA_VERIFY_ALT) || \
    defined(MBEDTLS_SM2DSA_GENKEY_ALT)

#if !defined(MBEDTLS_BIGNUM_ALT)
#error "MBEDTLS_SM2DSA_SIGN_ALT, MBEDTLS_SM2DSA_VERIFY_ALT, MBEDTLS_SM2DSA_GENKEY_ALT depend on MBEDTLS_BIGNUM_ALT"
#endif

#if !defined(MBEDTLS_ECP_ALT)
#error "MBEDTLS_SM2DSA_SIGN_ALT, MBEDTLS_SM2DSA_VERIFY_ALT, MBEDTLS_SM2DSA_GENKEY_ALT depend on MBEDTLS_ECP_ALT"
#endif

#include "te_bn.h"
#include "te_sm2.h"
#include "mbedtls/sm2dsa.h"
#include <string.h>

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdlib.h>
#define mbedtls_calloc    calloc
#define mbedtls_free       free
#endif

#include "mbedtls/platform_util.h"

/* Parameter validation macros based on platform_util.h */
#define SM2DSA_VALIDATE_RET( cond )    \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_ECP_BAD_INPUT_DATA )
#define SM2DSA_VALIDATE( cond )        \
    MBEDTLS_INTERNAL_VALIDATE( cond )

/*
 * Convert TE error code to mbedtls ECDSA error
 */
extern int te_err_to_mbedtls_ecp_err(int te_err);

#define MBEDTLS_CALL_TE_ECP(func) te_err_to_mbedtls_ecp_err(func)
#define MBEDTLS_TE_ECP_CHK(f)                                                  \
    do {                                                                       \
        if ((ret = MBEDTLS_CALL_TE_ECP(f)) != 0) {                             \
            goto cleanup;                                                      \
        }                                                                      \
    } while (0)

#endif /* MBEDTLS_SM2DSA_SIGN_ALT || SM2DSA_VERIFY_ALT || SM2DSA_GENKEY_ALT */

#if defined(MBEDTLS_SM2DSA_SIGN_ALT)
int mbedtls_sm2dsa_sign( mbedtls_mpi *r,
                         mbedtls_mpi *s,
                         const mbedtls_mpi *d,
                         const unsigned char *buf,
                         size_t blen,
                         int ( *f_rng )( void *, unsigned char *, size_t ),
                         void *p_rng )
{
    SM2DSA_VALIDATE_RET( r     != NULL );
    SM2DSA_VALIDATE_RET( s     != NULL );
    SM2DSA_VALIDATE_RET( d     != NULL );
    SM2DSA_VALIDATE_RET( f_rng != NULL );
    SM2DSA_VALIDATE_RET( buf   != NULL || blen == 0 );

    return MBEDTLS_CALL_TE_ECP(te_sm2dsa_sign(MPI2BN(d),
                                              (const uint8_t *)buf,
                                              blen,
                                              MPI2BN(r),
                                              MPI2BN(s),
                                              f_rng,
                                              p_rng));
}
#endif /* MBEDTLS_SM2DSA_SIGN_ALT */

#if defined(MBEDTLS_SM2DSA_VERIFY_ALT)
int mbedtls_sm2dsa_verify( const unsigned char *buf,
                           size_t blen,
                           const mbedtls_ecp_point *Q,
                           const mbedtls_mpi *r,
                           const mbedtls_mpi *s )
{
    SM2DSA_VALIDATE_RET( Q   != NULL );
    SM2DSA_VALIDATE_RET( r   != NULL );
    SM2DSA_VALIDATE_RET( s   != NULL );
    SM2DSA_VALIDATE_RET( buf != NULL || blen == 0 );

    return MBEDTLS_CALL_TE_ECP(te_sm2dsa_verify((const uint8_t *)buf,
                                                blen,
                                                (const te_ecp_point_t *)Q,
                                                MPI2BN(r),
                                                MPI2BN(s)));
}
#endif /* MBEDTLS_SM2DSA_VERIFY_ALT */

#if defined(MBEDTLS_SM2DSA_GENKEY_ALT)
/*
 * Generate key pair
 */
int mbedtls_sm2dsa_genkey( mbedtls_sm2dsa_context *ctx, int (*f_rng)(void *, unsigned char *, size_t), void *p_rng )
{
    int ret;
    SM2DSA_VALIDATE_RET( ctx   != NULL );
    SM2DSA_VALIDATE_RET( f_rng != NULL );

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
#endif /* MBEDTLS_SM2DSA_GENKEY_ALT */

#endif /* MBEDTLS_SM2DSA_C */
