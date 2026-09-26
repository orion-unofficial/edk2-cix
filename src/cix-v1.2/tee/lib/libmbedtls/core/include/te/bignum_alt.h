/**
 * \file bignum.h
 *
 * \brief Multi-precision integer library
 */
/*
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
#ifndef MBEDTLS_BIGNUM_ALT_H
#define MBEDTLS_BIGNUM_ALT_H

#ifdef __cplusplus
extern "C" {
#endif

/* For bignumber implementation with TE driver, we doesn't support FS IO */
#if defined(MBEDTLS_FS_IO)
#error "Bad mbedtls config: MBEDTLS_FS_IO couldn't be supported with TE driver!"
#endif

/*
 * Define the base integer type.
 *
 * For bignumber implementation based on TE driver, the base integer type is fixed
 * to 32-bit, and the following configs MUST NOT be defined:
 *
 * 1. MBEDTLS_HAVE_ASM.
 * 2. MBEDTLS_HAVE_INT64.
 * 3. MBEDTLS_NO_UDBL_DIVISION
 *
 */

#if defined(MBEDTLS_HAVE_ASM)
#error "Bad mbedtls config: MBEDTLS_HAVE_ASM couldn't be supported with TE driver!"
#endif
#if defined(MBEDTLS_HAVE_INT64)
#error "Bad mbedtls config: MBEDTLS_HAVE_INT64 couldn't be supported with TE driver!"
#endif
#if defined(MBEDTLS_NO_UDBL_DIVISION)
#error "Bad mbedtls config: MBEDTLS_NO_UDBL_DIVISION couldn't be supported with TE driver!"
#endif
typedef  int32_t mbedtls_mpi_sint;
typedef uint32_t mbedtls_mpi_uint;

/*
 * mbedtls_mpi ptr to te_bn ptr
 */
#define MPI2BN(x) ((x)->p)

/**
 * \brief          MPI structure
 *
 * \note           For MPI based on TE driver, the mbedtls_mpi structure is one
 *                 te_bn_t pointer.
 */
typedef struct mbedtls_mpi
{
    struct _te_bn_t *p; /*!<  pointer to TE bignumber anonymous structure  */
}
mbedtls_mpi;

#ifdef __cplusplus
}
#endif

#endif /* bignum.h */
