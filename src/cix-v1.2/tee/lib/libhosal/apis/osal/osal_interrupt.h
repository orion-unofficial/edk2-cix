/**
 * Copyright (C), 2018-2020, Arm Technology (China) Co., Ltd.
 * All rights reserved
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#ifndef __OSAL_INTERRUPT_H__
#define __OSAL_INTERRUPT_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long osal_intr_flag_t;
typedef osal_err_t (*osal_intr_handler_t)(void *para);
typedef struct _osal_intr_ctx_t {
    void *ctx;
} osal_intr_ctx_t;

OSAL_API osal_intr_flag_t osal_intr_lock(void);
OSAL_API void osal_intr_unlock(osal_intr_flag_t flag);

/* Request IRQ and register handler */
OSAL_API osal_err_t osal_irq_request(osal_intr_ctx_t *intr_ctx,
                                     int32_t intr_num,
                                     osal_intr_handler_t intr_handler,
                                     void *para);
/* Free IRQ */
OSAL_API void osal_irq_free(osal_intr_ctx_t *intr_ctx);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_INTERRUPT_H__ */
