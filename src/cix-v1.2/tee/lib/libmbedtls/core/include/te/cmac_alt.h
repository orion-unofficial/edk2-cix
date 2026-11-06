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

#ifndef __CMAC_ALT_H__
#define __CMAC_ALT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef __ASSEMBLY__

#define MBEDTLS_CMAC_MAGIC          (0x434D4143U) /* CMAC */

struct te_cipher_ctx;

struct mbedtls_cmac_context_t {
    uint32_t magic;
    struct te_cipher_ctx *cmac;
};

/**
 * \brief          This function releases and clears the specified cmac context.
 *
 * \param ctx      The CMAC context to clear.
 *                 If this is \c NULL, this function does nothing.
 *                 Otherwise, the context must have been at least initialized.
 */
void mbedtls_cmac_free( mbedtls_cmac_context_t *ctx );

#endif

#ifdef __cplusplus
}
#endif
#endif
