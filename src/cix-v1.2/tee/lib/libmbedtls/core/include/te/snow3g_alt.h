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

#ifndef MBEDTLS_SNOW3G_ALT_H
#define MBEDTLS_SNOW3G_ALT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  uea2 context structure
 */
#define MBEDTLS_UEA2_MAGIC     (0x75656132U) /* uea2 */
struct te_uea2_ctx;
typedef struct {
    uint32_t magic;
    struct te_uea2_ctx *uea2;    /**<  uea2 context of te driver */
} mbedtls_uea2_context;

/**
 *  uia2 context structure
 */
#define MBEDTLS_UIA2_MAGIC     (0x75696132U) /* uia2 */
struct te_uia2_ctx;
typedef struct {
    uint32_t magic;
    struct te_uia2_ctx *uia2;    /**<  uia2 context of te driver */
} mbedtls_uia2_context;

#ifdef __cplusplus
}
#endif

#endif  /* MBEDTLS_SNOW3G_ALT_H */
