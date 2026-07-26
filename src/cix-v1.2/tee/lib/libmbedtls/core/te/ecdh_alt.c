#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_ECDH_C)

#if defined(MBEDTLS_ECDH_GEN_PUBLIC_ALT) || defined(MBEDTLS_ECDH_COMPUTE_SHARED_ALT)

#if !defined(MBEDTLS_BIGNUM_ALT)
#error "MBEDTLS_ECDH_GEN_PUBLIC_ALT, MBEDTLS_ECDH_COMPUTE_SHARED_ALT depend on MBEDTLS_BIGNUM_ALT"
#endif

#if !defined(MBEDTLS_ECP_ALT)
#error "MBEDTLS_ECDH_GEN_PUBLIC_ALT, MBEDTLS_ECDH_COMPUTE_SHARED_ALT depend on MBEDTLS_ECP_ALT"
#endif

#include "te_bn.h"
#include "te_ecdh.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/platform_util.h"

#include <string.h>

#ifdef MBEDTLS_ECP_RESTARTABLE
#error                                                                         \
    "Bad mbedtls config: MBEDTLS_ECP_RESTARTABLE couldn't be supported with TE driver!"
#endif

/* Parameter validation macros based on platform_util.h */
#define ECDH_VALIDATE_RET(cond)                                                \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
#define ECDH_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

/*
 * Convert TE error code to mbedtls ECDH error
 */
extern int te_err_to_mbedtls_ecp_err(int te_err);

#define MBEDTLS_CALL_TE_ECP(func) te_err_to_mbedtls_ecp_err(func)
#define MBEDTLS_TE_ECP_CHK(f)                                                  \
    do {                                                                       \
        if ((ret = MBEDTLS_CALL_TE_ECP(f)) != 0) {                             \
            goto cleanup;                                                      \
        }                                                                      \
    } while (0)

#endif /* MBEDTLS_ECDH_GEN_PUBLIC_ALT || MBEDTLS_ECDH_COMPUTE_SHARED_ALT */

#if defined(MBEDTLS_ECDH_GEN_PUBLIC_ALT)
/*
 * Generate public key
 */
int mbedtls_ecdh_gen_public(mbedtls_ecp_group *grp,
                            mbedtls_mpi *d,
                            mbedtls_ecp_point *Q,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    ECDH_VALIDATE_RET(grp != NULL);
    ECDH_VALIDATE_RET(d != NULL);
    ECDH_VALIDATE_RET(Q != NULL);
    ECDH_VALIDATE_RET(f_rng != NULL);

    return MBEDTLS_CALL_TE_ECP(
        te_ecdh_gen_public((const te_ecp_group_t *)(grp), MPI2BN(d),
                           (te_ecp_point_t *)(Q), f_rng, p_rng));
}
#endif /* MBEDTLS_ECDH_GEN_PUBLIC_ALT */

#if defined(MBEDTLS_ECDH_COMPUTE_SHARED_ALT)
/*
 * Compute shared secret (SEC1 3.3.1)
 */
int mbedtls_ecdh_compute_shared(mbedtls_ecp_group *grp,
                                mbedtls_mpi *z,
                                const mbedtls_ecp_point *Q,
                                const mbedtls_mpi *d,
                                int (*f_rng)(void *, unsigned char *, size_t),
                                void *p_rng)

{
    ECDH_VALIDATE_RET(grp != NULL);
    ECDH_VALIDATE_RET(Q != NULL);
    ECDH_VALIDATE_RET(d != NULL);
    ECDH_VALIDATE_RET(z != NULL);

    return MBEDTLS_CALL_TE_ECP(
        te_ecdh_compute_shared((const te_ecp_group_t *)(grp),
                               MPI2BN(d),
                               (te_ecp_point_t *)Q,
                               MPI2BN(z),
                               f_rng,
                               p_rng));
}
#endif /* MBEDTLS_ECDH_COMPUTE_SHARED_ALT */

#endif /* MBEDTLS_ECDH_C */
