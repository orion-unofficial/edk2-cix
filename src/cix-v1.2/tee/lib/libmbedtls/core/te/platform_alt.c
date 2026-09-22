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
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"
#include "driver/te_drv.h"
#include "hwa/te_hwa.h"

#define MERAK_BASE_ADDRESS          (0x05050000UL)    // 0xA0040000 - 0xA0000000 + 0x05010000
#define MERAK_IRQ                   (473U)   // dts config: <GIC_SPI 473 IRQ_TYPE_LEVEL_HIGH>
#define MERAK_HOST                  (1U)    // TF-A, OPTEE use HOST1

/*
 * The following platform functions are weakly defined. They
 * are default implementations that allow mbedtls to compile in
 * absence of real definitions. The Platforms may override
 * with more specific definitions.
 */
#pragma weak platform_get_te_configs


static mbedtls_platform_context *g_plt_ctx = NULL;

#define __PLATFORM_ALERT__(_ret_, _msg_)                                       \
        do {                                                                   \
            if( TE_SUCCESS != ret ){                                           \
                OSAL_LOG_ERR("%s +%d ret->%X %s\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                (_ret_),                                       \
                                (_msg_));                                      \
            }                                                                  \
        } while (0);

static int _convert_retval_to_mbedtls(int errno)
{
    switch (errno) {
        case TE_SUCCESS:
            break;
        case TE_ERROR_OOM:
            errno = MBEDTLS_ERR_PLTF_ALLOC_FAILED;
            break;
        case TE_ERROR_BAD_PARAMS:
        case TE_ERROR_BAD_FORMAT:
            errno = MBEDTLS_ERR_PLTF_BAD_INPUT;
            break;
        default:
            break;
    }

    return errno;
}

int mbedtls_platform_setup( mbedtls_platform_context *ctx )
{
    int ret = TE_SUCCESS;
    mbedtls_platform_context *tctx = NULL;
    void *base = NULL;
    int irq = 0, host = -1;

    if (!ctx) {
        return MBEDTLS_ERR_PLTF_BAD_INPUT;
    }
    mbedtls_platform_zeroize( ctx, sizeof( *ctx ) );

    if ( (NULL != g_plt_ctx )
        && (MBEDTLS_PLATFORM_MAGIC == g_plt_ctx->magic)
        && (NULL != g_plt_ctx->hwa)
        && (NULL != g_plt_ctx->hdrv) ) {
        goto _setup_;
    }

    tctx = ( mbedtls_platform_context * )mbedtls_calloc( 1, sizeof(*tctx) );
    if ( NULL == tctx ) {
        ret = TE_ERROR_OOM;
        goto _out_;
    }
    /* instantiate the hwa instance */
    platform_get_te_configs(&base, &irq, &host);
    ret = te_hwa_alloc( &tctx->hwa,
                        base,
                        irq,
                        host );
    if ( TE_SUCCESS != ret ) {
        mbedtls_free( tctx );
        goto _out_;
    }
    /* instantiate the drv instance and bind it to the hwa */
    ret = te_drv_alloc(tctx->hwa, &tctx->hdrv);
    if(TE_SUCCESS != ret){
        te_hwa_free(tctx->hwa);
        mbedtls_free( tctx );
        goto _out_;
    }
    tctx->magic = MBEDTLS_PLATFORM_MAGIC;
    g_plt_ctx = tctx;
_setup_:
    g_plt_ctx->usr++;
    ctx->hdrv = g_plt_ctx->hdrv;
    ctx->hwa = g_plt_ctx->hwa;
    ctx->magic = g_plt_ctx->magic;
    ctx->usr = 1;
_out_:
    return _convert_retval_to_mbedtls(ret);
}

void mbedtls_platform_teardown( mbedtls_platform_context *ctx ){
    int ret = TE_SUCCESS;

    if ( NULL == ctx || ctx->usr != 1 )
        return;

    if ( (NULL == g_plt_ctx )
        || (NULL == g_plt_ctx->hwa)
        || (NULL == g_plt_ctx->hdrv) ){
        return;
    }

    ctx->usr--;
    if (--g_plt_ctx->usr > 0)
        return;

    /* instantiate the hwa instance */
    ret = te_drv_free(g_plt_ctx->hdrv);
    __PLATFORM_ALERT__(ret, "te_drv_free Failed");
    ret = te_hwa_free(g_plt_ctx->hwa);
    __PLATFORM_ALERT__(ret, "te_hwa_free Failed");
    mbedtls_platform_zeroize( g_plt_ctx, sizeof( *g_plt_ctx ) );
    mbedtls_free( g_plt_ctx );
    g_plt_ctx = NULL;
}

te_drv_handle te_platform_get_drvhandle(void)
{
    return g_plt_ctx ? g_plt_ctx->hdrv : TE_HANDLE_NULL;
}

/*
 * Collect the configurations for the trust engine, including
 * base address, irq line number, and host id.
 */
void platform_get_te_configs(void **base, int *irq, int *host)
{
    if (base)
        *base = (void*)(uintptr_t)MERAK_BASE_ADDRESS;

    if (irq)
        *irq  = MERAK_IRQ;

    if (host)
        *host = MERAK_HOST;
}

int platform_te_suspend(void)
{
    int ret = TE_SUCCESS;

    if (!g_plt_ctx) {
        ret = TE_ERROR_BAD_STATE;
        goto out;
    }

    ret = te_drv_suspend(g_plt_ctx->hdrv, TE_SUSPEND_MEM);

out:
    return _convert_retval_to_mbedtls(ret);
}

int platform_te_resume(void)
{
    int ret = TE_SUCCESS;

    OSAL_ASSERT(g_plt_ctx && g_plt_ctx->hdrv);
    ret = te_drv_resume(g_plt_ctx->hdrv);
    OSAL_ASSERT(TE_SUCCESS == ret);
    return 0;
}
