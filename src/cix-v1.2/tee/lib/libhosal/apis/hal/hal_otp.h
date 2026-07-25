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

#ifndef __HAL_OTP_H__
#define __HAL_OTP_H__

#include "osal_err.h"
#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * This file defines the customer implementation defined macros.
 */
#ifndef CFG_OTP_WITH_PUF
#define HAL_USER_DEFINE_OTP_SIZE_USER_NON_SECURE_REGION (16)
#define HAL_USER_DEFINE_OTP_SIZE_USER_SECURE_REGION (32)
#define HAL_USER_DEFINE_OTP_SIZE_TEST_REGION (0)
#else
#define HAL_USER_DEFINE_OTP_SIZE_USER_NON_SECURE_REGION (16)
#define HAL_USER_DEFINE_OTP_SIZE_USER_SECURE_REGION (48)
#define HAL_USER_DEFINE_OTP_SIZE_TEST_REGION (64)
#endif

/**
 * Currently the HAL OTP definition is alined with Merak OTP layout.
 **/

typedef enum _hal_otp_type_t {
    HAL_OTP_TYPE_MODEL_ID                = 0,
    HAL_OTP_TYPE_MODEL_KEY               = 1,
    HAL_OTP_TYPE_DEVICE_ID               = 2,
    HAL_OTP_TYPE_DEVICE_ROOT_KEY         = 3,
    HAL_OTP_TYPE_SECURE_BOOT_PUBKEY_HASH = 4,
    HAL_OTP_TYPE_LCS                     = 5,
    HAL_OTP_TYPE_LOCK_CTRL               = 6,
    HAL_OTP_TYPE_USER_NON_SECURE_REGION  = 7,
    HAL_OTP_TYPE_USER_SECURE_REGION      = 8,
    HAL_OTP_TYPE_TEST_REGION             = 9,
} hal_otp_type_t;

/**
 * Define the otp size.
 *
 *       The `CFG_EXTEND_OTP` is for security enhancement and customized
 *       improvement, it's disabled by default which means the default OTP
 *       layout is the `Merak Standard OTP Layout`, Users must revise them
 *       to fit the customized EXTEND OTP layout by defining `CFG_EXTEND_OTP`
 *       if required.
 *
 * \note For bare metal environment please make the definition of `CFG_EXTEND_OTP`
 *       aligned with the `CE_LITE_EXTEND_OTP` in the file ce_lite_config.h
 *
 */

#if !defined(CFG_EXTEND_OTP)
/* The `Merak Standard OTP Layout` by default */
enum {
    HAL_OTP_SIZE_MODEL_ID                = 4,
    HAL_OTP_SIZE_MODEL_KEY               = 16,
    HAL_OTP_SIZE_DEVICE_ID               = 4,
    HAL_OTP_SIZE_DEVICE_ROOT_KEY         = 16,
    HAL_OTP_SIZE_SECURE_BOOT_PUBKEY_HASH = 32,
    HAL_OTP_SIZE_LCS                     = 4,
    HAL_OTP_SIZE_LOCK_CTRL               = 4,
    HAL_OTP_SIZE_USER_NON_SECURE_REGION =
        HAL_USER_DEFINE_OTP_SIZE_USER_NON_SECURE_REGION,
    HAL_OTP_SIZE_USER_SECURE_REGION =
        HAL_USER_DEFINE_OTP_SIZE_USER_SECURE_REGION,
    HAL_OTP_SIZE_TEST_REGION = HAL_USER_DEFINE_OTP_SIZE_TEST_REGION,
};
#else
/* The `Merak Extend OTP Layout` for customization */
enum {
    HAL_OTP_SIZE_MODEL_ID                = 4,
    HAL_OTP_SIZE_MODEL_KEY               = 32,
    HAL_OTP_SIZE_DEVICE_ID               = 4,
    HAL_OTP_SIZE_DEVICE_ROOT_KEY         = 32,
    HAL_OTP_SIZE_SECURE_BOOT_PUBKEY_HASH = 32,
    HAL_OTP_SIZE_LCS                     = 4,
    HAL_OTP_SIZE_LOCK_CTRL               = 4,
    HAL_OTP_SIZE_USER_NON_SECURE_REGION =
        HAL_USER_DEFINE_OTP_SIZE_USER_NON_SECURE_REGION,
    HAL_OTP_SIZE_USER_SECURE_REGION =
        HAL_USER_DEFINE_OTP_SIZE_USER_SECURE_REGION,
    HAL_OTP_SIZE_TEST_REGION = HAL_USER_DEFINE_OTP_SIZE_TEST_REGION,
};
#endif

enum {
    HAL_OTP_LCS_CM = 0,
    HAL_OTP_LCS_DM = 1,
    HAL_OTP_LCS_DD = 3,
    HAL_OTP_LCS_DR = 7,
};

HAL_API osal_err_t hal_otp_init(void);
HAL_API osal_err_t hal_otp_write(hal_otp_type_t type, const uint8_t data[]);
HAL_API osal_err_t hal_otp_read(hal_otp_type_t type, uint8_t data[]);
HAL_API void hal_otp_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* __HAL_OTP_H__ */
