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

#ifndef __MD5_ALT_H__
#define __MD5_ALT_H__
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define MBEDTLS_ERR_MD5_ALLOC_FAILED                      -0x0010  /**< Failed to allocate memory. */
#define MBEDTLS_ERR_MD5_BAD_INPUT_DATA                    -0x0073  /**< MD5 input data was malformed. */
#define MBEDTLS_ERR_MD5_INVALID_INPUT_LENGTH              -0x0032  /**< The data input has an invalid length. */

#define MBEDTLS_MD5_MAGIC       (0x4D4435U)         /* MD5 */

struct te_dgst_ctx;

typedef struct mbedtls_md5_context {
    uint32_t magic;
    bool is_dgst_init;
    struct te_dgst_ctx * dgst;
} mbedtls_md5_context;

#endif

#ifdef __cplusplus
}
#endif
#endif