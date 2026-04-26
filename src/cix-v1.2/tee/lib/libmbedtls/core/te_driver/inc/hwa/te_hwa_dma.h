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

#ifndef __TRUSTENGINE_HWA_DMA_H__
#define __TRUSTENGINE_HWA_DMA_H__

#include "te_hwa_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_dma_regs;
struct te_hwa_host;

/**
 * Trust engine AXI DMA HWA structure
 */
typedef struct te_hwa_dma {
    te_hwa_crypt_t base;
    //TODO
} te_hwa_dma_t;

int te_hwa_dma_alloc( struct te_dma_regs *regs,
                      struct te_hwa_host *host,
                      te_hwa_dma_t **hwa );

int te_hwa_dma_free( te_hwa_dma_t *hwa );

int te_hwa_dma_init( struct te_dma_regs *regs,
                     struct te_hwa_host *host,
                     te_hwa_dma_t *hwa );

int te_hwa_dma_exit( te_hwa_dma_t *hwa );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_DMA_H__ */
