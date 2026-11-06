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

#ifndef __OSAL_COMPLETION_H__
#define __OSAL_COMPLETION_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _osal_completion_t {
    void *ctx;
} osal_completion_t;

OSAL_API osal_err_t osal_completion_init(osal_completion_t *comp);
OSAL_API void osal_completion_destroy(osal_completion_t *comp);
OSAL_API void osal_completion_wait(osal_completion_t *comp);
OSAL_API void osal_completion_signal(osal_completion_t *comp);
OSAL_API void osal_completion_broadcast(osal_completion_t *comp);
OSAL_API void osal_completion_reset(osal_completion_t *comp);

#define OSAL_COMPLETION_COND_WAIT(__cond_expr__, __comp_ptr__)  \
    do {                                                        \
        if ((__cond_expr__)) {                                  \
            break;                                              \
        }                                                       \
        osal_completion_wait((__comp_ptr__));                   \
    } while (1);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_COMPLETION_H__ */
