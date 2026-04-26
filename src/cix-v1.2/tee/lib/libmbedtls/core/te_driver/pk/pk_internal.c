
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
#include "pk_internal.h"

void te_pk_lock(const te_drv_handle hdl)
{
    int ret                   = TE_SUCCESS;
    const te_crypt_drv_t *drv = NULL;

    TE_ASSERT(hdl);

    drv = (te_crypt_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_ACA);
    TE_ASSERT(drv);
    ret = te_drv_put(hdl, TE_DRV_TYPE_ACA);
    TE_ASSERT(TE_SUCCESS == ret);

    ret = te_aca_lock(drv);
    TE_ASSERT(TE_SUCCESS == ret);
}

void te_pk_unlock(const te_drv_handle hdl)
{
    int ret                   = TE_SUCCESS;
    const te_crypt_drv_t *drv = NULL;

    TE_ASSERT(hdl);

    drv = (te_crypt_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_ACA);
    TE_ASSERT(drv);
    ret = te_drv_put(hdl, TE_DRV_TYPE_ACA);
    TE_ASSERT(TE_SUCCESS == ret);

    ret = te_aca_unlock(drv);
    TE_ASSERT(TE_SUCCESS == ret);
}

int te_pk_submit_req(void *req)
{
    TE_ASSERT(req);

    return te_aca_submit_req(req);
}
