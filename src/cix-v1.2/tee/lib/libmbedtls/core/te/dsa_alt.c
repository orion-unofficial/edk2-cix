#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_DSA_C)

#if defined(MBEDTLS_DSA_GENKEY_ALT) || \
     defined(MBEDTLS_DSA_SIGN_ALT) || \
     defined(MBEDTLS_DSA_VERIFY_ALT)

#if !defined(MBEDTLS_BIGNUM_ALT)
#error "MBEDTLS_DSA_GENKEY_ALT, MBEDTLS_DSA_SIGN_ALT, MBEDTLS_DSA_VERIFY_ALT depend on MBEDTLS_BIGNUM_ALT"
#endif

#include "te_bn.h"
#include "te_dsa.h"
#include "mbedtls/dsa.h"
#include "mbedtls/asn1write.h"

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
#define DSA_VALIDATE_RET(cond)                                                 \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_DSA_BAD_INPUT_DATA)
#define DSA_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

extern int te_err_to_mbedtls_dsa_err(int te_err);

/*
 * Convert TE error code to mbedtls DSA error
 * Same as ECP error code
 */
int te_err_to_mbedtls_dsa_err(int te_err)
{
    switch (te_err) {
    case TE_SUCCESS:
        return 0;
    case TE_ERROR_BAD_PARAMS:
    case TE_ERROR_BAD_INPUT_DATA:
        return MBEDTLS_ERR_DSA_BAD_INPUT_DATA;
    case TE_ERROR_SHORT_BUFFER:
        return MBEDTLS_ERR_DSA_BUFFER_TOO_SMALL;
    case TE_ERROR_FEATURE_UNAVAIL:
        return MBEDTLS_ERR_DSA_FEATURE_UNAVAILABLE;
    case TE_ERROR_VERIFY_SIG:
        return MBEDTLS_ERR_DSA_VERIFY_FAILED;
    case TE_ERROR_OOM:
        return MBEDTLS_ERR_DSA_ALLOC_FAILED;
    case TE_ERROR_GEN_RANDOM:
        return MBEDTLS_ERR_DSA_RANDOM_FAILED;
    default:
        return MBEDTLS_ERR_DSA_HW_FAILED;
    }
}

#define MBEDTLS_CALL_TE_DSA(func) te_err_to_mbedtls_dsa_err(func)
#define MBEDTLS_TE_DSA_CHK(f)                                                  \
    do {                                                                       \
        if ((ret = MBEDTLS_CALL_TE_DSA(f)) != 0) {                             \
            goto cleanup;                                                      \
        }                                                                      \
    } while (0)

#endif /* MBEDTLS_DSA_GENKEY_ALT || MBEDTLS_DSA_SIGN_ALT || MBEDTLS_DSA_VERIFY_ALT */

#if defined(MBEDTLS_DSA_SIGN_ALT)
/*
 * Compute DSA signature of a hashed message
 */
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
    DSA_VALIDATE_RET(p != NULL);
    DSA_VALIDATE_RET(q != NULL);
    DSA_VALIDATE_RET(g != NULL);
    DSA_VALIDATE_RET(r != NULL);
    DSA_VALIDATE_RET(s != NULL);
    DSA_VALIDATE_RET(x != NULL);
    DSA_VALIDATE_RET(buf != NULL || blen == 0);
    DSA_VALIDATE_RET(f_rng != NULL);

    return MBEDTLS_CALL_TE_DSA(
        te_dsa_sign(MPI2BN(p), MPI2BN(q), MPI2BN(g),
                    MPI2BN(x), (const uint8_t *)buf, (size_t)blen,
                    MPI2BN(r), MPI2BN(s), f_rng, p_rng));
}

#endif /* MBEDTLS_DSA_SIGN_ALT */

#if defined(MBEDTLS_DSA_VERIFY_ALT)
/*
 * Verify DSA signature of hashed message
 */
int mbedtls_dsa_verify(const mbedtls_mpi *p,
                       const mbedtls_mpi *q,
                       const mbedtls_mpi *g,
                       const unsigned char *buf,
                       size_t blen,
                       const mbedtls_mpi *y,
                       const mbedtls_mpi *r,
                       const mbedtls_mpi *s)
{
    DSA_VALIDATE_RET(p != NULL);
    DSA_VALIDATE_RET(q != NULL);
    DSA_VALIDATE_RET(g != NULL);
    DSA_VALIDATE_RET(y != NULL);
    DSA_VALIDATE_RET(r != NULL);
    DSA_VALIDATE_RET(s != NULL);
    DSA_VALIDATE_RET(buf != NULL || blen == 0);

    return MBEDTLS_CALL_TE_DSA(te_dsa_verify(MPI2BN(p),
                                             MPI2BN(q),
                                             MPI2BN(g),
                                             MPI2BN(y),
                                             (const uint8_t *)buf,
                                             (size_t)blen,
                                             MPI2BN(r),
                                             MPI2BN(s)));
}

#endif /* MBEDTLS_DSA_VERIFY_ALT */

#if defined(MBEDTLS_DSA_GENKEY_ALT)
/*
 * Generate key pair
 */
int mbedtls_dsa_genkey(mbedtls_dsa_context *ctx,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng)
{
    DSA_VALIDATE_RET(ctx != NULL);
    DSA_VALIDATE_RET(f_rng != NULL);

    return MBEDTLS_CALL_TE_DSA(te_dsa_gen_keypair(MPI2BN(&ctx->p),
                                                  MPI2BN(&ctx->q),
                                                  MPI2BN(&ctx->g),
                                                  MPI2BN(&ctx->x),
                                                  MPI2BN(&ctx->y),
                                                  f_rng,
                                                  p_rng));
}
#endif /* MBEDTLS_DSA_GENKEY_ALT */
#endif /* MBEDTLS_DSA_C */
