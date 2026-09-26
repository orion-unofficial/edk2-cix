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

#ifndef __TRUSTENGINE_DRV_INTERNAL_H__
#define __TRUSTENGINE_DRV_INTERNAL_H__

#include <te_common.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * \brief           This function increases the reference count on the supplied
 *                  crypto driver instance \p drv.
 * \param[in] drv   The crypto driver instance.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static int te_crypt_drv_get(te_crypt_drv_t *drv)
{
    osal_atomic_inc(&drv->refcnt);
    return TE_SUCCESS;
}

/**
 * \brief           This function decreases the reference count on the supplied
 *                  crypto driver instance \p drv.
 *
 *                  This function destroys the driver instance when reference
 *                  count reaches zero after this put operation.
 *
 * \param[in] drv   The crypto driver instance.te_crypt_drv_put
 * \return          void.
 */
static void te_crypt_drv_put(te_crypt_drv_t *drv)
{
    if (osal_atomic_dec(&drv->refcnt) == 0 && drv->destroy)
        drv->destroy(drv);
}

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_DRV_INTERNAL_H__ */
