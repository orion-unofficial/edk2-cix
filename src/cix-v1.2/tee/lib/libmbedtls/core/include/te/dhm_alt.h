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

#ifndef __DHM_ALT_H__
#define __DHM_ALT_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * \brief          The DHM context structure.
 */
typedef struct mbedtls_dhm_context
{
    size_t len;         /*!<  The size of \p P in Bytes. */
    mbedtls_mpi P;      /*!<  The prime modulus. */
    mbedtls_mpi G;      /*!<  The generator. */
    mbedtls_mpi X;      /*!<  Our secret value. */
    mbedtls_mpi GX;     /*!<  Our public key = \c G^X mod \c P. */
    mbedtls_mpi GY;     /*!<  The public key of the peer = \c G^Y mod \c P. */
    mbedtls_mpi K;      /*!<  The shared secret = \c G^(XY) mod \c P. */
    mbedtls_mpi RP;     /*!<  The cached value = \c R^2 mod \c P. */
    mbedtls_mpi Vi;     /*!<  The blinding value. */
    mbedtls_mpi Vf;     /*!<  The unblinding value. */
    mbedtls_mpi pX;     /*!<  The previous \c X. */
} mbedtls_dhm_context;

#endif

#ifdef __cplusplus
}
#endif
#endif