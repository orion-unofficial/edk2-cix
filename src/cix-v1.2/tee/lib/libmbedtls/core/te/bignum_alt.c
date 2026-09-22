/*
 *  Multi-precision integer library
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
 *  The following sources were referenced in the design of this Multi-precision
 *  Integer library:
 *
 *  [1] Handbook of Applied Cryptography - 1997
 *      Menezes, van Oorschot and Vanstone
 *
 *  [2] Multi-Precision Math
 *      Tom St Denis
 *      https://github.com/libtom/libtommath/blob/develop/tommath.pdf
 *
 *  [3] GNU Multi-Precision Arithmetic Library
 *      https://gmplib.org/manual/index.html
 *
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

#if defined(MBEDTLS_BIGNUM_C) && defined(MBEDTLS_BIGNUM_ALT)

#include "te_bn.h"
#include "mbedtls/bignum.h"
#include "mbedtls/bn_mul.h"
#include "mbedtls/platform_util.h"

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

#pragma GCC diagnostic ignored "-Wstrict-aliasing"

#define MPI_VALIDATE_RET(cond)                                                 \
    MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_MPI_BAD_INPUT_DATA)
#define MPI_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

#define ciL (sizeof(mbedtls_mpi_uint)) /* chars in limb  */
#define biL (ciL << 3)                 /* bits  in limb  */
#define biH (ciL << 2)                 /* half limb size */

#define MPI_SIZE_T_MAX ((size_t)-1) /* SIZE_T_MAX is not standard */

/*
 * Convert between bits/chars and number of limbs
 * Divide first in order to avoid potential overflows
 */
#define BITS_TO_LIMBS(i) ((i) / biL + ((i) % biL != 0))
#define CHARS_TO_LIMBS(i) ((i) / ciL + ((i) % ciL != 0))

extern int te_err_to_mbedtls_mpi_err(int te_err);

/*
 * Convert TE error code to mbedtls MPI error
 */
int te_err_to_mbedtls_mpi_err(int te_err)
{
    switch (te_err) {
    case TE_SUCCESS:
        return 0;
    case TE_ERROR_BAD_PARAMS:
    case TE_ERROR_BAD_INPUT_DATA:
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    case TE_ERROR_SHORT_BUFFER:
        return MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL;
    case TE_ERROR_NEGATIVE_VALUE:
        return MBEDTLS_ERR_MPI_NEGATIVE_VALUE;
    case TE_ERROR_DIV_BY_ZERO:
        return MBEDTLS_ERR_MPI_DIVISION_BY_ZERO;
    case TE_ERROR_NOT_ACCEPTABLE:
        return MBEDTLS_ERR_MPI_NOT_ACCEPTABLE;
    case TE_ERROR_OOM:
        return MBEDTLS_ERR_MPI_ALLOC_FAILED;
    default:
        return MBEDTLS_ERR_MPI_HW_FAILED;
    }
}

#define MBEDTLS_CALL_TE_BN(func) te_err_to_mbedtls_mpi_err(func)

#define MBEDTLS_TE_BN_CHK(f)                                                   \
    do {                                                                       \
        if ((ret = MBEDTLS_CALL_TE_BN(f)) != 0) {                              \
            goto cleanup;                                                      \
        }                                                                      \
    } while (0)

/* For mbedtls with TE driver, mpi zeroize does nothing, because the mpi will be
 * freed immediately after zeroize, and the mpi data will be cleared in free.
 */
static void mbedtls_mpi_zeroize(te_bn_t *p)
{
    (void)(p);
    return;
}

/*
 * Initialize one MPI
 */
void mbedtls_mpi_init(mbedtls_mpi *X)
{
    int ret = 0;
    MPI_VALIDATE(X != NULL);

    ret = te_bn_alloc(te_platform_get_drvhandle(), 0, &MPI2BN(X));
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("TE alloc BN failed!\n");
    }
}

/*
 * Unallocate one MPI
 */
void mbedtls_mpi_free(mbedtls_mpi *X)
{
    if (X == NULL)
        return;

    if (X != NULL) {
        mbedtls_mpi_zeroize(MPI2BN(X));
        te_bn_free(MPI2BN(X));
    }

    MPI2BN(X) = NULL;
}

/*
 * Enlarge to the specified number of limbs
 */
int mbedtls_mpi_grow(mbedtls_mpi *X, size_t nblimbs)
{
    MPI_VALIDATE_RET(X != NULL);

    return MBEDTLS_CALL_TE_BN(te_bn_grow(MPI2BN(X), nblimbs));
}

/*
 * Resize down as much as possible,
 * while keeping at least the specified number of limbs
 */
int mbedtls_mpi_shrink(mbedtls_mpi *X, size_t nblimbs)
{
    MPI_VALIDATE_RET(X != NULL);

    return MBEDTLS_CALL_TE_BN(te_bn_shrink(MPI2BN(X), nblimbs));
}

/*
 * Copy the contents of Y into X
 */
int mbedtls_mpi_copy(mbedtls_mpi *X, const mbedtls_mpi *Y)
{
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(Y != NULL);

    if (X == Y)
        return (0);

    if (Y == NULL) {
        mbedtls_mpi_free(X);
        return (0);
    }

    return MBEDTLS_CALL_TE_BN(te_bn_copy(MPI2BN(X), MPI2BN(Y)));
}

/*
 * Swap the contents of X and Y
 */
void mbedtls_mpi_swap(mbedtls_mpi *X, mbedtls_mpi *Y)
{
    int ret;
    MPI_VALIDATE(X != NULL);
    MPI_VALIDATE(Y != NULL);

    ret = MBEDTLS_CALL_TE_BN(te_bn_swap(MPI2BN(X), MPI2BN(Y)));
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("TE SWAP BN failed!\n");
    }
}

/*
 * Conditionally assign X = Y, without leaking information
 * about whether the assignment was made or not.
 * (Leaking information about the respective sizes of X and Y is ok however.)
 */
int mbedtls_mpi_safe_cond_assign(mbedtls_mpi *X,
                                 const mbedtls_mpi *Y,
                                 unsigned char assign)
{
    int ret       = 0;
    te_bn_t *tmp1 = NULL;
    te_bn_t *tmp2 = NULL;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(Y != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp1));
    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp2));

    /* make sure assign is 0 or 1 in a time-constant manner */
    assign = (assign | (unsigned char)-assign) >> 7;

    /* tmp1 = Y * assign */
    MBEDTLS_TE_BN_CHK(te_bn_mul_s32(tmp1, MPI2BN(Y), assign));

    /* tmp2 = X * (1 - assign) */
    MBEDTLS_TE_BN_CHK(te_bn_mul_s32(tmp2, MPI2BN(X), (1 - assign)));

    /* X = tmp1 + tmp2 */
    MBEDTLS_TE_BN_CHK(te_bn_add_bn(MPI2BN(X), tmp1, tmp2));

cleanup:
    te_bn_free(tmp1);
    te_bn_free(tmp2);
    return (ret);
}

/*
 * Conditionally swap X and Y, without leaking information
 * about whether the swap was made or not.
 * Here it is not ok to simply swap the pointers, which whould lead to
 * different memory access patterns when X and Y are used afterwards.
 */
int mbedtls_mpi_safe_cond_swap(mbedtls_mpi *X,
                               mbedtls_mpi *Y,
                               unsigned char swap)
{
    int ret        = 0;
    te_bn_t *tmp_X = NULL;
    te_bn_t *tmp_Y = NULL;
    te_bn_t *tmp1  = NULL;
    te_bn_t *tmp2  = NULL;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(Y != NULL);

    if (X == Y)
        return (0);

    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_X));
    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_Y));
    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp1));
    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp2));

    /* make sure swap is 0 or 1 in a time-constant manner */
    swap = (swap | (unsigned char)-swap) >> 7;

    MBEDTLS_TE_BN_CHK(te_bn_copy(tmp_X, MPI2BN(X)));
    MBEDTLS_TE_BN_CHK(te_bn_copy(tmp_Y, MPI2BN(Y)));

    /* X = X * (1 - swap) + Y * (swap) */
    MBEDTLS_TE_BN_CHK(te_bn_mul_s32(tmp1, tmp_X, 1 - swap));
    MBEDTLS_TE_BN_CHK(te_bn_mul_s32(tmp2, tmp_Y, swap));
    MBEDTLS_TE_BN_CHK(te_bn_add_bn(MPI2BN(X), tmp1, tmp2));

    /* Y = Y * (1 - swap) + X * (swap) */
    MBEDTLS_TE_BN_CHK(te_bn_mul_s32(tmp1, tmp_Y, 1 - swap));
    MBEDTLS_TE_BN_CHK(te_bn_mul_s32(tmp2, tmp_X, swap));
    MBEDTLS_TE_BN_CHK(te_bn_add_bn(MPI2BN(Y), tmp1, tmp2));

cleanup:
    te_bn_free(tmp_X);
    te_bn_free(tmp_Y);
    te_bn_free(tmp1);
    te_bn_free(tmp2);
    return (ret);
}

/*
 * Set value from integer
 */
int mbedtls_mpi_lset(mbedtls_mpi *X, mbedtls_mpi_sint z)
{
    MPI_VALIDATE_RET(X != NULL);

    return MBEDTLS_CALL_TE_BN(te_bn_import_s32(MPI2BN(X), z));
}

/*
 * Get a specific bit
 */
int mbedtls_mpi_get_bit(const mbedtls_mpi *X, size_t pos)
{
    int ret = 0;

    MPI_VALIDATE_RET(X != NULL);

    ret = te_bn_get_bit(MPI2BN(X), (int)pos);
    if (ret < 0)
        return (ret);
    return ret;
}

/* Get a specific byte, without range checks. */
#define GET_BYTE(X, i) (((X)->p[(i) / ciL] >> (((i) % ciL) * 8)) & 0xff)

/*
 * Set a bit to a specific value of 0 or 1
 */
int mbedtls_mpi_set_bit(mbedtls_mpi *X, size_t pos, unsigned char val)
{
    MPI_VALIDATE_RET(X != NULL);

    if (val != 0 && val != 1)
        return (MBEDTLS_ERR_MPI_BAD_INPUT_DATA);

    return MBEDTLS_CALL_TE_BN(te_bn_set_bit(MPI2BN(X), (int)pos, val));
}

/*
 * Return the number of less significant zero-bits
 */
size_t mbedtls_mpi_lsb(const mbedtls_mpi *X)
{
    int ret = 0;
    MBEDTLS_INTERNAL_VALIDATE_RET(X != NULL, 0);

    ret = te_bn_0bits_before_lsb1(MPI2BN(X));
    if (ret < 0) {
        return 0;
    }
    return ret;
}

/*
 * Return the number of bits
 */
size_t mbedtls_mpi_bitlen(const mbedtls_mpi *X)
{
    int ret = 0;
    if (X == NULL)
        return (0);

    ret = te_bn_bitlen(MPI2BN(X));
    if (ret < 0) {
        return 0;
    }
    return ret;
}

/*
 * Return the total size in bytes
 */
size_t mbedtls_mpi_size(const mbedtls_mpi *X)
{
    int ret = 0;
    if (X == NULL)
        return (0);

    ret = te_bn_bytelen(MPI2BN(X));
    if (ret < 0) {
        return 0;
    }
    return ret;
}

/*
 * Convert an ASCII character to digit value
 */
static int mpi_get_digit(mbedtls_mpi_uint *d, int radix, char c)
{
    *d = 255;

    if (c >= 0x30 && c <= 0x39)
        *d = c - 0x30;
    if (c >= 0x41 && c <= 0x46)
        *d = c - 0x37;
    if (c >= 0x61 && c <= 0x66)
        *d = c - 0x57;

    if (*d >= (mbedtls_mpi_uint)radix)
        return (MBEDTLS_ERR_MPI_INVALID_CHARACTER);

    return (0);
}

/*
 * Import from an ASCII string
 */
int mbedtls_mpi_read_string(mbedtls_mpi *X, int radix, const char *s)
{
    int ret;
    size_t i, slen;
    mbedtls_mpi_uint d;
    int sign = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(s != NULL);

    if (radix < 2 || radix > 16)
        return (MBEDTLS_ERR_MPI_BAD_INPUT_DATA);

    MBEDTLS_TE_BN_CHK(te_bn_import_s32(MPI2BN(X), 0));
    slen = strlen(s);

    if (s[0] == '-') {
        sign = -1;
        i    = 1;
    } else {
        sign = 1;
        i    = 0;
    }

    for (; i < slen; i++) {
        d = 0;
        MBEDTLS_MPI_CHK(mpi_get_digit(&d, radix, s[i]));
        /* X = X * radix */
        MBEDTLS_TE_BN_CHK(te_bn_mul_s32(MPI2BN(X), MPI2BN(X), radix));
        /* X = X + d */
        MBEDTLS_TE_BN_CHK(te_bn_add_s32(MPI2BN(X), MPI2BN(X), (int32_t)d));
    }

    MBEDTLS_TE_BN_CHK(te_bn_set_sign(MPI2BN(X), sign));

cleanup:

    return (ret);
}

/*
 * Helper to write the digits high-order first
 */
static int mpi_write_hlp(te_bn_t *bn, int radix, char *buf, size_t *size)
{
    int ret            = 0;
    mbedtls_mpi_uint r = 0;
    te_bn_t *radix_bn  = NULL;
    te_bn_t *tmp       = NULL;
    char *p            = NULL;
    size_t str_len     = 0;
    int result         = 0;
    size_t i           = 0;

    OSAL_ASSERT(radix >= 2 && radix <= 16);

    /* change bn sign to positive */
    MBEDTLS_TE_BN_CHK(te_bn_set_sign(bn, 1));

    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &radix_bn));
    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp));
    MBEDTLS_TE_BN_CHK(te_bn_import_s32(radix_bn, radix));

    p       = buf;
    str_len = 0;

    do {
        MBEDTLS_TE_BN_CHK(te_bn_div_bn(bn, tmp, bn, radix_bn));
        /* export tmp to u32 */
        MBEDTLS_TE_BN_CHK(te_bn_export_s32(tmp, (int32_t *)(&r)));

        OSAL_ASSERT(r <= 15);
        if (r < 10)
            *p = (char)(r + 0x30);
        else
            *p = (char)(r + 0x37);

        p++;
        str_len++;
        MBEDTLS_TE_BN_CHK(te_bn_cmp_s32(bn, 0, &result));
        if (result == 0) {
            break;
        }
    } while (true);

    /* adational 0 for radix == 16 */
    if (16 == radix) {
        if (str_len & 1) {
            *p = '0';
            p++;
            str_len++;
        }
    }

    OSAL_ASSERT(*size >= str_len + 1);

    p = buf;
    /* revert string */
    for (i = 0; i < str_len / 2; i++) {
        r                    = buf[str_len - 1 - i];
        buf[str_len - 1 - i] = buf[i];
        buf[i]               = (char)r;
    }

    /* add string ender */
    buf[str_len] = '\0';
    *size        = str_len + 1;

cleanup:
    te_bn_free(radix_bn);
    te_bn_free(tmp);
    return (ret);
}

/*
 * Export into an ASCII string
 */
int mbedtls_mpi_write_string(
    const mbedtls_mpi *X, int radix, char *buf, size_t buflen, size_t *olen)
{
    int ret  = 0;
    size_t n = 0, total_size = 0;
    char *p      = NULL;
    te_bn_t *tmp = NULL;
    int32_t sign = 0;

    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(olen != NULL);
    MPI_VALIDATE_RET(buflen == 0 || buf != NULL);

    if (radix < 2 || radix > 16) {
        return (MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
    }

    n = mbedtls_mpi_bitlen(X);
    if (radix >= 4)
        n >>= 1;
    if (radix >= 16)
        n >>= 1;
    /*
     * Round up the buffer length to an even value to ensure that there is
     * enough room for hexadecimal values that can be represented in an odd
     * number of digits.
     */
    n += 3 + ((n + 1) & 1);

    if (buflen < n) {
        *olen = n;
        return (MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL);
    }

    total_size = 0;
    p          = buf;
    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp));

    MBEDTLS_TE_BN_CHK(te_bn_get_sign(MPI2BN(X), &sign));
    if (sign == -1) {
        *p++ = '-';
        total_size++;
    }

    /* copy X to t0 */
    MBEDTLS_TE_BN_CHK(te_bn_copy(tmp, MPI2BN(X)));

    MBEDTLS_MPI_CHK(mpi_write_hlp(tmp, radix, p, &n));
    total_size += n;

    *olen = (total_size);

cleanup:
    te_bn_free(tmp);
    return (ret);
}

#if defined(MBEDTLS_FS_IO)
/*
 * Read X from an opened file
 */
int mbedtls_mpi_read_file(mbedtls_mpi *X, int radix, FILE *fin)
{
    return MBEDTLS_ERR_MPI_FILE_IO_ERROR;
}

/*
 * Write X into an opened file (or stdout if fout == NULL)
 */
int mbedtls_mpi_write_file(const char *p,
                           const mbedtls_mpi *X,
                           int radix,
                           FILE *fout)
{
    return MBEDTLS_ERR_MPI_FILE_IO_ERROR;
}
#endif /* MBEDTLS_FS_IO */

/*
 * Import X from unsigned binary data, big endian
 */
int mbedtls_mpi_read_binary(mbedtls_mpi *X,
                            const unsigned char *buf,
                            size_t buflen)
{
    int ret;
    int32_t sign = 0;

    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(buflen == 0 || buf != NULL);

    /* read old sign */
    MBEDTLS_TE_BN_CHK(te_bn_get_sign(MPI2BN(X), &sign));
    MBEDTLS_TE_BN_CHK(te_bn_import(MPI2BN(X), buf, buflen, sign));

cleanup:
    return (ret);
}

/*
 * Export X into unsigned binary data, big endian
 */
int mbedtls_mpi_write_binary(const mbedtls_mpi *X,
                             unsigned char *buf,
                             size_t buflen)
{
    int ret = 0;

    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(buflen == 0 || buf != NULL);

    ret = te_bn_bytelen(MPI2BN(X));
    if (ret < 0) {
        return MBEDTLS_CALL_TE_BN(ret);
    }

    if (buflen < (size_t)ret) {
        return (MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL);
    }

    return MBEDTLS_CALL_TE_BN(te_bn_export(MPI2BN(X), buf, buflen));
}

/*
 * Left-shift: X <<= count
 */
int mbedtls_mpi_shift_l(mbedtls_mpi *X, size_t count)
{
    MPI_VALIDATE_RET(X != NULL);
    return MBEDTLS_CALL_TE_BN(
        te_bn_shift_l(MPI2BN(X), MPI2BN(X), (int)(count)));
}

/*
 * Right-shift: X >>= count
 */
int mbedtls_mpi_shift_r(mbedtls_mpi *X, size_t count)
{
    MPI_VALIDATE_RET(X != NULL);
    return MBEDTLS_CALL_TE_BN(
        te_bn_shift_r(MPI2BN(X), MPI2BN(X), (int)(count)));
}

/*
 * Compare unsigned values
 */
int mbedtls_mpi_cmp_abs(const mbedtls_mpi *X, const mbedtls_mpi *Y)
{
    int ret        = 0;
    int result     = 0;
    te_bn_t *tmp_x = NULL, *tmp_y = NULL;

    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(Y != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_x));
    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_y));

    MBEDTLS_TE_BN_CHK(te_bn_abs(tmp_x, MPI2BN(X)));
    MBEDTLS_TE_BN_CHK(te_bn_abs(tmp_y, MPI2BN(Y)));

    MBEDTLS_TE_BN_CHK(te_bn_cmp_bn(tmp_x, tmp_y, &result));

    ret = result;

cleanup:
    te_bn_free(tmp_x);
    te_bn_free(tmp_y);
    return (ret);
}

/*
 * Compare signed values
 */
int mbedtls_mpi_cmp_mpi(const mbedtls_mpi *X, const mbedtls_mpi *Y)
{
    int ret    = 0;
    int result = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(Y != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_cmp_bn(MPI2BN(X), MPI2BN(Y), &result));

    ret = result;

cleanup:
    return (ret);
}

/*
 * Compare signed values
 */
int mbedtls_mpi_cmp_int(const mbedtls_mpi *X, mbedtls_mpi_sint z)
{
    int ret    = 0;
    int result = 0;
    MPI_VALIDATE_RET(X != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_cmp_s32(MPI2BN(X), (int32_t)z, &result));
    ret = result;

cleanup:
    return (ret);
}

/*
 * Unsigned addition: X = |A| + |B|  (HAC 14.7)
 */
int mbedtls_mpi_add_abs(mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *B)
{
    int ret = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(A != NULL);
    MPI_VALIDATE_RET(B != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_add_abs(MPI2BN(X), MPI2BN(A), MPI2BN(B)));

cleanup:
    return (ret);
}

/*
 * Unsigned subtraction: X = |A| - |B|  (HAC 14.9)
 */
int mbedtls_mpi_sub_abs(mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *B)
{
    int ret = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(A != NULL);
    MPI_VALIDATE_RET(B != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_sub_abs(MPI2BN(X), MPI2BN(A), MPI2BN(B)));
cleanup:
    return (ret);
}

/*
 * Signed addition: X = A + B
 */
int mbedtls_mpi_add_mpi(mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *B)
{
    int ret = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(A != NULL);
    MPI_VALIDATE_RET(B != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_add_bn(MPI2BN(X), MPI2BN(A), MPI2BN(B)));
cleanup:
    return (ret);
}

/*
 * Signed subtraction: X = A - B
 */
int mbedtls_mpi_sub_mpi(mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *B)
{
    int ret = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(A != NULL);
    MPI_VALIDATE_RET(B != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_sub_bn(MPI2BN(X), MPI2BN(A), MPI2BN(B)));
cleanup:
    return (ret);
}

/*
 * Signed addition: X = A + b
 */
int mbedtls_mpi_add_int(mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        mbedtls_mpi_sint b)
{
    int ret = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(A != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_add_s32(MPI2BN(X), MPI2BN(A), (int32_t)b));
cleanup:
    return (ret);
}

/*
 * Signed subtraction: X = A - b
 */
int mbedtls_mpi_sub_int(mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        mbedtls_mpi_sint b)
{
    int ret = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(A != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_sub_s32(MPI2BN(X), MPI2BN(A), (int32_t)b));
cleanup:
    return (ret);
}

/*
 * Baseline multiplication: X = A * B  (HAC 14.12)
 */
int mbedtls_mpi_mul_mpi(mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *B)
{
    int ret = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(A != NULL);
    MPI_VALIDATE_RET(B != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_mul_bn(MPI2BN(X), MPI2BN(A), MPI2BN(B)));

cleanup:
    return (ret);
}

/*
 * Baseline multiplication: X = A * b
 */
int mbedtls_mpi_mul_int(mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        mbedtls_mpi_uint b)
{
    int ret        = 0;
    te_bn_t *tmp_b = NULL;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(A != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_b));
    MBEDTLS_TE_BN_CHK(te_bn_import_u32(tmp_b, (uint32_t)b));
    MBEDTLS_TE_BN_CHK(te_bn_mul_bn(MPI2BN(X), MPI2BN(A), tmp_b));

cleanup:
    te_bn_free(tmp_b);
    return (ret);
}

/*
 * Division by mbedtls_mpi: A = Q * B + R  (HAC 14.20)
 */
int mbedtls_mpi_div_mpi(mbedtls_mpi *Q,
                        mbedtls_mpi *R,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *B)
{
    int ret = 0;
    MPI_VALIDATE_RET(A != NULL);
    MPI_VALIDATE_RET(B != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_div_bn((Q ? (MPI2BN(Q)) : (NULL)),
                                   (R ? (MPI2BN(R)) : (NULL)), MPI2BN(A),
                                   MPI2BN(B)));

cleanup:
    return (ret);
}

/*
 * Division by int: A = Q * b + R
 */
int mbedtls_mpi_div_int(mbedtls_mpi *Q,
                        mbedtls_mpi *R,
                        const mbedtls_mpi *A,
                        mbedtls_mpi_sint b)
{
    int ret        = 0;
    te_bn_t *tmp_b = NULL;
    MPI_VALIDATE_RET(A != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_b));
    MBEDTLS_TE_BN_CHK(te_bn_import_s32(tmp_b, (int32_t)b));

    MBEDTLS_TE_BN_CHK(te_bn_div_bn((Q ? (MPI2BN(Q)) : (NULL)),
                                   (R ? (MPI2BN(R)) : (NULL)), MPI2BN(A),
                                   tmp_b));

cleanup:
    te_bn_free(tmp_b);
    return (ret);
}

/*
 * Modulo: R = A mod B
 */
int mbedtls_mpi_mod_mpi(mbedtls_mpi *R,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *B)
{
    int ret    = 0;
    int result = 0;
    int sign   = 0;
    MPI_VALIDATE_RET(R != NULL);
    MPI_VALIDATE_RET(A != NULL);
    MPI_VALIDATE_RET(B != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_get_sign(MPI2BN(B), &sign));
    if (sign < 0) {
        return MBEDTLS_ERR_MPI_NEGATIVE_VALUE;
    }

    MBEDTLS_TE_BN_CHK(te_bn_div_bn(NULL, MPI2BN(R), MPI2BN(A), MPI2BN(B)));

    do {
        MBEDTLS_TE_BN_CHK(te_bn_cmp_s32(MPI2BN(R), 0, &result));
        if (result < 0) {
            MBEDTLS_TE_BN_CHK(te_bn_add_bn(MPI2BN(R), MPI2BN(R), MPI2BN(B)));
        } else {
            break;
        }
    } while (true);

    do {
        MBEDTLS_TE_BN_CHK(te_bn_cmp_bn(MPI2BN(R), MPI2BN(B), &result));
        if (result >= 0) {
            MBEDTLS_TE_BN_CHK(te_bn_sub_bn(MPI2BN(R), MPI2BN(R), MPI2BN(B)));
        } else {
            break;
        }
    } while (true);

cleanup:
    return (ret);
}

/*
 * Modulo: r = A mod b
 */
int mbedtls_mpi_mod_int(mbedtls_mpi_uint *r,
                        const mbedtls_mpi *A,
                        mbedtls_mpi_sint b)
{
    int ret        = 0;
    te_bn_t *tmp_A = NULL;
    int32_t a_sign = 0;
    uint32_t y     = 0;
    MPI_VALIDATE_RET(r != NULL);
    MPI_VALIDATE_RET(A != NULL);

    if (b == 0)
        return (MBEDTLS_ERR_MPI_DIVISION_BY_ZERO);

    if (b < 0)
        return (MBEDTLS_ERR_MPI_NEGATIVE_VALUE);

    /*
     * handle trivial cases
     */
    if (b == 1) {
        *r = 0;
        return (0);
    }

    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_A));
    MBEDTLS_TE_BN_CHK(te_bn_abs(tmp_A, MPI2BN(A)));
    MBEDTLS_TE_BN_CHK(te_bn_get_sign(MPI2BN(A), &a_sign));
    MBEDTLS_TE_BN_CHK(te_bn_mod_u32(&y, tmp_A, (uint32_t)b));

    /*
     * If A is negative, then the current y represents a negative value.
     * Flipping it to the positive side.
     */
    if (a_sign < 0 && y != 0)
        y = (uint32_t)b - y;

    *r = y;

cleanup:
    te_bn_free(tmp_A);
    return (ret);
}

/*
 * Sliding-window exponentiation: X = A^E mod N  (HAC 14.85)
 */
int mbedtls_mpi_exp_mod(mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *E,
                        const mbedtls_mpi *N,
                        mbedtls_mpi *_RR)
{
    int ret        = 0;
    int result     = 0;
    int e_result   = 0;
    te_bn_t *tmp_A = NULL;
    int32_t sign   = 0;
    int e_lsb      = 0;

    (void)(_RR);
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(A != NULL);
    MPI_VALIDATE_RET(E != NULL);
    MPI_VALIDATE_RET(N != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_cmp_s32(MPI2BN(N), 0, &result));
    if (result <= 0) {
        return (MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
    }
    ret = te_bn_get_bit(MPI2BN(N), 0);
    if ((ret != 0) && (ret != 1)) {
        return MBEDTLS_CALL_TE_BN(ret);
    }
    if (ret == 0) {
        return (MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
    }

    MBEDTLS_TE_BN_CHK(te_bn_cmp_s32(MPI2BN(E), 0, &e_result));
    if (e_result < 0) {
        return (MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
    }

    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_A));
    MBEDTLS_TE_BN_CHK(te_bn_copy(tmp_A, MPI2BN(A)));

    /*
     * Compensate for negative A (and correct at the end)
     */
    MBEDTLS_TE_BN_CHK(te_bn_get_sign(MPI2BN(A), &sign));
    if (sign < 0) {
        MBEDTLS_TE_BN_CHK(te_bn_set_sign(tmp_A, 1));
    }
    e_lsb = te_bn_get_bit(MPI2BN(E), 0);
    if ((e_lsb != 0) && (e_lsb != 1)) {
        return MBEDTLS_CALL_TE_BN(e_lsb);
    }

    MBEDTLS_TE_BN_CHK(te_bn_exp_mod(MPI2BN(X), tmp_A, MPI2BN(E), MPI2BN(N)));

    MBEDTLS_TE_BN_CHK(te_bn_cmp_s32(MPI2BN(X), 0, &result));
    if (result > 0) {
        if ((sign < 0) && (e_result != 0) && (e_lsb == 1)) {
            MBEDTLS_TE_BN_CHK(te_bn_set_sign(MPI2BN(X), -1));
            MBEDTLS_TE_BN_CHK(te_bn_add_bn(MPI2BN(X), MPI2BN(X), MPI2BN(N)));
        }

        /* double check X */
        MBEDTLS_TE_BN_CHK(te_bn_get_sign(MPI2BN(X), &sign));
        OSAL_ASSERT(sign == 1);
        MBEDTLS_TE_BN_CHK(te_bn_cmp_bn(MPI2BN(X), MPI2BN(N), &result));
        OSAL_ASSERT(result < 0);
    }

cleanup:
    te_bn_free(tmp_A);
    return (ret);
}

/*
 * Greatest common divisor: G = gcd(A, B)  (HAC 14.54)
 */
int mbedtls_mpi_gcd(mbedtls_mpi *G, const mbedtls_mpi *A, const mbedtls_mpi *B)
{
    int ret        = 0;
    te_bn_t *tmp_A = NULL;
    te_bn_t *tmp_B = NULL;
    MPI_VALIDATE_RET(G != NULL);
    MPI_VALIDATE_RET(A != NULL);
    MPI_VALIDATE_RET(B != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_A));
    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_B));

    MBEDTLS_TE_BN_CHK(te_bn_abs(tmp_A, MPI2BN(A)));
    MBEDTLS_TE_BN_CHK(te_bn_abs(tmp_B, MPI2BN(B)));

    MBEDTLS_TE_BN_CHK(te_bn_gcd(MPI2BN(G), tmp_A, tmp_B));

cleanup:
    te_bn_free(tmp_A);
    te_bn_free(tmp_B);
    return (ret);
}

/*
 * Fill X with size bytes of random.
 */
int mbedtls_mpi_fill_random(mbedtls_mpi *X,
                            size_t size,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret      = 0;
    int32_t sign = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(f_rng != NULL);

    if (size > MBEDTLS_MPI_MAX_SIZE)
        return (MBEDTLS_ERR_MPI_BAD_INPUT_DATA);

    MBEDTLS_TE_BN_CHK(te_bn_get_sign(MPI2BN(X), &sign));
    MBEDTLS_TE_BN_CHK(te_bn_import_random(MPI2BN(X), size, sign, f_rng, p_rng));

cleanup:
    return (ret);
}

/*
 * Modular inverse: X = A^-1 mod N  (HAC 14.61 / 14.64)
 */
int mbedtls_mpi_inv_mod(mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *N)
{
    int ret        = 0;
    int result     = 0;
    te_bn_t *gcd   = NULL;
    te_bn_t *tmp_A = NULL;
    int32_t sign   = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(A != NULL);
    MPI_VALIDATE_RET(N != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_cmp_s32(MPI2BN(N), 1, &result));
    if (result <= 0) {
        return (MBEDTLS_ERR_MPI_BAD_INPUT_DATA);
    }

    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &gcd));
    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_A));
    MBEDTLS_TE_BN_CHK(te_bn_div_bn(NULL, tmp_A, (const te_bn_t *)MPI2BN(A),
                                   (const te_bn_t *)MPI2BN(N)));

    MBEDTLS_TE_BN_CHK(te_bn_get_sign(tmp_A, &sign));
    if (-1 == sign) {
        MBEDTLS_TE_BN_CHK(te_bn_add_bn(tmp_A, (const te_bn_t *)tmp_A,
                                       (const te_bn_t *)MPI2BN(N)));
    }

    MBEDTLS_TE_BN_CHK(te_bn_gcd(gcd, tmp_A, MPI2BN(N)));

    MBEDTLS_TE_BN_CHK(te_bn_cmp_s32(gcd, 1, &result));
    if (result != 0) {
        ret = MBEDTLS_ERR_MPI_NOT_ACCEPTABLE;
        goto cleanup;
    }

    MBEDTLS_TE_BN_CHK(te_bn_inv_mod(MPI2BN(X), tmp_A, MPI2BN(N)));
cleanup:
    te_bn_free(gcd);
    te_bn_free(tmp_A);
    return (ret);
}

#if defined(MBEDTLS_GENPRIME)

/*
 * Pseudo-primality test: small factors, then Miller-Rabin
 */
int mbedtls_mpi_is_prime_ext(const mbedtls_mpi *X,
                             int rounds,
                             int (*f_rng)(void *, unsigned char *, size_t),
                             void *p_rng)
{
    int ret        = 0;
    te_bn_t *tmp_X = NULL;
    int result     = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(f_rng != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_alloc(te_platform_get_drvhandle(), 0, &tmp_X));
    MBEDTLS_TE_BN_CHK(te_bn_abs(tmp_X, MPI2BN(X)));

    MBEDTLS_TE_BN_CHK(te_bn_cmp_s32(tmp_X, 0, &result));
    if (result == 0) {
        ret = (MBEDTLS_ERR_MPI_NOT_ACCEPTABLE);
        goto cleanup;
    }

    MBEDTLS_TE_BN_CHK(te_bn_cmp_s32(tmp_X, 1, &result));
    if (result == 0) {
        ret = (MBEDTLS_ERR_MPI_NOT_ACCEPTABLE);
        goto cleanup;
    }

    MBEDTLS_TE_BN_CHK(te_bn_cmp_s32(tmp_X, 2, &result));
    if (result == 0) {
        ret = (0);
        goto cleanup;
    }

    MBEDTLS_TE_BN_CHK(
        te_bn_is_probale_prime(tmp_X, (uint32_t)rounds, f_rng, p_rng));

cleanup:
    te_bn_free(tmp_X);
    return ret;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
/*
 * Pseudo-primality test, error probability 2^-80
 */
int mbedtls_mpi_is_prime(const mbedtls_mpi *X,
                         int (*f_rng)(void *, unsigned char *, size_t),
                         void *p_rng)
{
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(f_rng != NULL);

    /*
     * In the past our key generation aimed for an error rate of at most
     * 2^-80. Since this function is deprecated, aim for the same certainty
     * here as well.
     */
    return (mbedtls_mpi_is_prime_ext(X, 40, f_rng, p_rng));
}
#endif /* MBEDTLS_DEPRECATED_REMOVED */

/*
 * Prime number generation
 *
 * To generate an RSA key in a way recommended by FIPS 186-4, both primes must
 * be either 1024 bits or 1536 bits long, and flags must contain
 * MBEDTLS_MPI_GEN_PRIME_FLAG_LOW_ERR.
 */
int mbedtls_mpi_gen_prime(mbedtls_mpi *X,
                          size_t nbits,
                          int flags,
                          int (*f_rng)(void *, unsigned char *, size_t),
                          void *p_rng)
{
    int ret         = 0;
    bool is_low_err = false;
    bool is_dh_prim = false;

    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET(f_rng != NULL);

    if (nbits < 3 || nbits > MBEDTLS_MPI_MAX_BITS)
        return (MBEDTLS_ERR_MPI_BAD_INPUT_DATA);

    if (flags & MBEDTLS_MPI_GEN_PRIME_FLAG_LOW_ERR) {
        is_low_err = true;
    } else {
        is_low_err = false;
    }

    if (flags & MBEDTLS_MPI_GEN_PRIME_FLAG_DH) {
        is_dh_prim = true;
    } else {
        is_dh_prim = false;
    }

    MBEDTLS_TE_BN_CHK(te_bn_gen_prime(MPI2BN(X), is_low_err, is_dh_prim, nbits,
                                      f_rng, p_rng));

cleanup:
    return (ret);
}

#endif /* MBEDTLS_GENPRIME */

int mbedtls_mpi_get_sign(const mbedtls_mpi *X)
{
    int ret      = 0;
    int32_t sign = 0;
    MPI_VALIDATE_RET(X != NULL);

    MBEDTLS_TE_BN_CHK(te_bn_get_sign(MPI2BN(X), &sign));

    return (sign);
cleanup:
    return ret;
}

int mbedtls_mpi_set_sign(mbedtls_mpi *X, int sign)
{
    int ret = 0;
    MPI_VALIDATE_RET(X != NULL);
    MPI_VALIDATE_RET((sign == 1) || (sign == -1));

    MBEDTLS_TE_BN_CHK(te_bn_set_sign(MPI2BN(X), sign));
cleanup:
    return ret;
}

/* For OP-TEE OS porting */
void mbedtls_mpi_init_mempool(mbedtls_mpi *X)
{
    return mbedtls_mpi_init(X);
}

#endif /* MBEDTLS_BIGNUM_C && MBEDTLS_BIGNUM_ALT */
