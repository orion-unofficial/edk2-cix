#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_ECDSA_C)

#if defined(MBEDTLS_ECDSA_SIGN_ALT) || defined(MBEDTLS_ECDSA_VERIFY_ALT) || \
    defined(MBEDTLS_ECDSA_GENKEY_ALT)

#if !defined(MBEDTLS_BIGNUM_ALT)
#error "MBEDTLS_ECDSA_SIGN_ALT, MBEDTLS_ECDSA_VERIFY_ALT, MBEDTLS_ECDSA_GENKEY_ALT depend on MBEDTLS_BIGNUM_ALT"
#endif

#if !defined(MBEDTLS_ECP_ALT)
#error "MBEDTLS_ECDSA_SIGN_ALT, MBEDTLS_ECDSA_VERIFY_ALT, MBEDTLS_ECDSA_GENKEY_ALT depend on MBEDTLS_ECP_ALT"
#endif

#include "te_bn.h"
#include "te_ecdsa.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/asn1write.h"

#include <string.h>

#if defined(MBEDTLS_ECDSA_DETERMINISTIC)
#include "mbedtls/hmac_drbg.h"
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdlib.h>
#define mbedtls_calloc calloc
#define mbedtls_free free
#endif

#include "mbedtls/platform_util.h"

/* Parameter validation macros based on platform_util.h */
#define ECDSA_VALIDATE_RET(cond)                                               \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
#define ECDSA_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

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

#endif /* MBEDTLS_ECDSA_SIGN_ALT || ECDSA_VERIFY_ALT || ECDSA_GENKEY_ALT */

#if defined(MBEDTLS_ECDSA_SIGN_ALT)
/*
 * Compute ECDSA signature of a hashed message
 */
int mbedtls_ecdsa_sign(mbedtls_ecp_group *grp,
                       mbedtls_mpi *r,
                       mbedtls_mpi *s,
                       const mbedtls_mpi *d,
                       const unsigned char *buf,
                       size_t blen,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng)
{
    ECDSA_VALIDATE_RET(grp != NULL);
    ECDSA_VALIDATE_RET(r != NULL);
    ECDSA_VALIDATE_RET(s != NULL);
    ECDSA_VALIDATE_RET(d != NULL);
    ECDSA_VALIDATE_RET(f_rng != NULL);
    ECDSA_VALIDATE_RET(buf != NULL || blen == 0);

    return MBEDTLS_CALL_TE_ECP(te_ecdsa_sign((const te_ecp_group_t *)grp,
                                             MPI2BN(d),
                                             (const uint8_t *)buf,
                                             blen,
                                             MPI2BN(r),
                                             MPI2BN(s),
                                             f_rng,
                                             p_rng));
}
#endif /* MBEDTLS_ECDSA_SIGN_ALT */

#if defined(MBEDTLS_ECDSA_VERIFY_ALT)
/*
 * Verify ECDSA signature of hashed message
 */
int mbedtls_ecdsa_verify( mbedtls_ecp_group *grp,
                          const unsigned char *buf, size_t blen,
                          const mbedtls_ecp_point *Q,
                          const mbedtls_mpi *r,
                          const mbedtls_mpi *s)
{
    ECDSA_VALIDATE_RET(grp != NULL);
    ECDSA_VALIDATE_RET( Q   != NULL );
    ECDSA_VALIDATE_RET( r   != NULL );
    ECDSA_VALIDATE_RET( s   != NULL );
    ECDSA_VALIDATE_RET( buf != NULL || blen == 0 );

    return MBEDTLS_CALL_TE_ECP(te_ecdsa_verify((const te_ecp_group_t *)grp,
                                               (const uint8_t *)buf,
                                               blen,
                                               (const te_ecp_point_t *)Q,
                                               MPI2BN(r),
                                               MPI2BN(s)));
}
#endif /* MBEDTLS_ECDSA_VERIFY_ALT */

#if defined(MBEDTLS_ECDSA_GENKEY_ALT)
/*
 * Generate key pair
 */
int mbedtls_ecdsa_genkey( mbedtls_ecdsa_context *ctx, mbedtls_ecp_group_id gid,
                  int (*f_rng)(void *, unsigned char *, size_t), void *p_rng )
{
    int ret = 0;
    ECDSA_VALIDATE_RET(ctx != NULL);
    ECDSA_VALIDATE_RET( f_rng != NULL );

    ret = mbedtls_ecp_group_load( &ctx->grp, gid );
    if (ret) {
        return ret;
    }

    ret = mbedtls_ecp_gen_keypair( &ctx->grp, &ctx->d, &ctx->Q, f_rng, p_rng );
    if (ret ) {
        return ret;
    }
    return ( 0 );
}
#endif /* MBEDTLS_ECDSA_GENKEY_ALT */

#endif /* MBEDTLS_ECDSA_C */
