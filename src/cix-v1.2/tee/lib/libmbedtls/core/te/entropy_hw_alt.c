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
/*
 *  The AES block cipher was designed by Vincent Rijmen and Joan Daemen.
 *
 *  http://csrc.nist.gov/encryption/aes/rijndael/Rijndael.pdf
 *  http://csrc.nist.gov/publications/fips/fips197/fips-197.pdf
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_ENTROPY_C)

#include "mbedtls/entropy.h"
#include "mbedtls/entropy_poll.h"

#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)

#include "mbedtls/platform.h"
#include "driver/te_drv_trng.h"

int mbedtls_hardware_poll(void *data,
                          unsigned char *output,
                          size_t len,
                          size_t *olen)
{
    int ret = TE_SUCCESS;
    te_trng_drv_t *drv = NULL;
    (void)data;

    /* TODO: call Merak driver to generate random */
    drv = (te_trng_drv_t *)te_drv_get(te_platform_get_drvhandle(),
                                      TE_DRV_TYPE_TRNG);
    ret = te_trng_read(drv, output, len);
    if (TE_SUCCESS == ret) {
        *olen = len;
    } else {
        *olen = 0;
    }

    te_drv_put(te_platform_get_drvhandle(),
               TE_DRV_TYPE_TRNG);
    return ret;
}

#endif  /* MBEDTLS_ENTROPY_HARDWARE_ALT */

#endif  /* MBEDTLS_ENTROPY_C */
