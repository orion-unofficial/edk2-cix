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

#ifndef MBEDTLS_KASUMI_ALT_H
#define MBEDTLS_KASUMI_ALT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  f8 context structure
 */
#define MBEDTLS_F8_MAGIC     (0x6B616638U) /* kaf8 */
struct te_f8_ctx;
typedef struct {
    uint32_t magic;
    struct te_f8_ctx *f8;    /**<  f8 context of te driver */
} mbedtls_f8_context;

/**
 *  f9 context structure
 */
#define MBEDTLS_F9_MAGIC     (0x6B616639U) /* kaf9 */
struct te_f9_ctx;
typedef struct {
    uint32_t magic;
    struct te_f9_ctx *f9;    /**<  f9 context of te driver */
} mbedtls_f9_context;

#ifdef __cplusplus
}
#endif

#endif  /* MBEDTLS_KASUMI_ALT_H */
