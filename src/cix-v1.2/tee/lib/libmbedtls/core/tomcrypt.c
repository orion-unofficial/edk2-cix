// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2018, ARM Limited
 * Copyright (C) 2019, Linaro Limited
 */

#include <crypto/crypto.h>
#ifndef CFG_MBEDTLS_TE
#include <tomcrypt_init.h>
#endif
#include <trace.h>
#include <mbedtls/platform.h>
#include <mm/generic_ram_layout.h>
#include <mm/core_memprot.h>
#include "drivers/ipc.h"


static mbedtls_platform_context g_mbed_ctx;

#if defined(MBEDTLS_PLATFORM_C)
extern void mbedtls_platform_custom(void);
#endif
extern void init_mp_mbedtls(void);

TEE_Result crypto_init(void)
{
#if (defined(CFG_MBEDTLS_TE) || defined(CFG_TE_TRNG))
    int err = 0;
#if defined(CFG_TEE_KEY_META_SHM)
    void *point_shm = NULL;
    csec_km_meta_t *tee_key_meta;

    point_shm = phys_to_virt(TEE_KEY_META_SHM_BASE, MEM_AREA_RAM_SEC, 1);
    tee_key_meta = (csec_km_meta_t *)point_shm;

    if (tee_key_meta->config[KM_CFG_ENC_ENABLE] == 1) {
        if ((err = mbedtls_platform_setup(&g_mbed_ctx))) {
            EMSG("mbedtls plat setup error %d\n", err);
            return TEE_ERROR_BAD_STATE;
        }
    }
#else
    if ((err = mbedtls_platform_setup(&g_mbed_ctx))) {
        EMSG("mbedtls plat setup error %d\n", err);
        return TEE_ERROR_BAD_STATE;
    }
#endif
#endif

#if defined(CFG_MBEDTLS_TE)
	init_mp_mbedtls();
#else  /*CFG_MBEDTLS_TE*/
	tomcrypt_init();
#endif /*!CFG_MBEDTLS_TE*/

#if defined(MBEDTLS_PLATFORM_C)
	mbedtls_platform_custom();
#endif
	return TEE_SUCCESS;
}
