/*
 * Copyright (c) 2020-2021, Arm Technology (China) Co., Ltd.
 * All rights reserved.
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#ifndef MBEDTLS_ZUC_ALT_H
#define MBEDTLS_ZUC_ALT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  eea3 context structure
 */
#define MBEDTLS_EEA3_MAGIC     (0x65656133U) /* eea3 */
struct te_eea3_ctx;
typedef struct {
    uint32_t magic;
    struct te_eea3_ctx *eea3;    /**<  eea3 context of te driver */
} mbedtls_eea3_context;

/**
 *  eia3 context structure
 */
#define MBEDTLS_EIA3_MAGIC     (0x65696133U) /* eia3 */
struct te_eia3_ctx;
typedef struct {
    uint32_t magic;
    struct te_eia3_ctx *eia3;    /**<  eia3 context of te driver */
} mbedtls_eia3_context;

#ifdef __cplusplus
}
#endif

#endif  /* MBEDTLS_ZUC_ALT_H */
