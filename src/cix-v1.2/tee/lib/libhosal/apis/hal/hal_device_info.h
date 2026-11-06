/**
 * Copyright (C), 2020, Arm Technology (China) Co., Ltd.
 * All rights reserved
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#ifndef __HAL_DEVICE_INFO_H__
#define __HAL_DEVICE_INFO_H__

#include "osal_err.h"
#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_MODEL_ID_SIZE (4)
#define HAL_DEVICE_ID_SIZE (4)
#define HAL_CRK_SIZE (32)
#define HAL_HUK_SIZE (16)

HAL_API osal_err_t
hal_write_device_info(const uint8_t model_id[HAL_MODEL_ID_SIZE],
                      const uint8_t device_id[HAL_DEVICE_ID_SIZE]);
HAL_API osal_err_t hal_read_device_info(uint8_t model_id[HAL_MODEL_ID_SIZE],
                                        uint8_t device_id[HAL_DEVICE_ID_SIZE]);
HAL_API osal_err_t hal_derive_crk(uint8_t crk[HAL_CRK_SIZE]);
HAL_API osal_err_t hal_get_huk(uint8_t huk[HAL_HUK_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* __HAL_DEVICE_INFO_H__ */