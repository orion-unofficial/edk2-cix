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

#ifndef __PLATFORM_ALT_H__
#define __PLATFORM_ALT_H__
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define MBEDTLS_ERR_PLTF_ALLOC_FAILED         -0x6180  /**< Failed to allocate memory. */
#define MBEDTLS_ERR_PLTF_BAD_INPUT       -0x000D /**< Bad input parameters to the function. */

#define MBEDTLS_PLATFORM_MAGIC      (0x504C5446U) /* PLTF */

struct te_hwa_host;
struct __te_drv_handle;

typedef struct mbedtls_platform_context
{
    uint32_t magic;
    int usr;                        /**< user counter */
    struct te_hwa_host *hwa;        /**< hwa host handle*/
    struct __te_drv_handle *hdrv;   /**< merak driver handle */
} mbedtls_platform_context;

/**
 * \brief           This function gets the private ctx of TE driver.
 * \return          The private context pointer.
 */
struct __te_drv_handle * te_platform_get_drvhandle(void);

/*
 * \brief            This function collects the configurations for the trust
 *                   engine, including base address, irq number and host id.
 * \param[out] base  Filled with the base address on success.
 * \param[out] irq   Filled with the irq number on success.
 * \param[out] host  Filled with the host number on success.
 */
void platform_get_te_configs(void **base, int *irq, int *host);

/**
 * \brief           This function attempts to suspend the TE driver.
 *
 *
 * \return          \c 0 on success.
 * \return          \c <0 on failure.
 */

int platform_te_suspend(void);

/**
 * \brief           This function resumes the TE driver.
 *                  It is the opposite side of the suspend call.
 *
 *                  This function will not return until done resuming well.
 *
 * \return          \c 0 always.
 */
int platform_te_resume(void);

#endif

#ifdef __cplusplus
}
#endif
#endif
