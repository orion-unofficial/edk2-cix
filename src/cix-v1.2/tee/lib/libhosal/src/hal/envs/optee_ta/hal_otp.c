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
#include "hal_otp.h"
#include "osal_log.h"
#include "utils_ext.h"
#include <tee_api_ext.h>

static int hal_otp_id(hal_otp_type_t type, tee_otp_ident_t *id, uint32_t *size)
{
    tee_otp_ident_t toid = -1;
    uint32_t osize       = 0;
    switch (type) {
    case HAL_OTP_TYPE_MODEL_ID:
        toid  = TEE_OTP_MODEL_ID;
        osize = HAL_OTP_SIZE_MODEL_ID;
        break;
    case HAL_OTP_TYPE_DEVICE_ID:
        toid  = TEE_OTP_DEV_ID;
        osize = HAL_OTP_SIZE_DEVICE_ID;
        break;
    case HAL_OTP_TYPE_SECURE_BOOT_PUBKEY_HASH:
        toid  = TEE_OTP_ROTPK_HASH;
        osize = HAL_OTP_SIZE_SECURE_BOOT_PUBKEY_HASH;
        break;
    case HAL_OTP_TYPE_LCS:
        toid  = TEE_OTP_LCS;
        osize = HAL_OTP_SIZE_LCS;
        break;
    case HAL_OTP_TYPE_LOCK_CTRL:
        toid  = TEE_OTP_LOCK_CTRL;
        osize = HAL_OTP_SIZE_LOCK_CTRL;
        break;
    case HAL_OTP_TYPE_USER_NON_SECURE_REGION:
        toid  = TEE_OTP_USR_NS_RGN;
        osize = HAL_OTP_SIZE_USER_NON_SECURE_REGION;
        break;
    case HAL_OTP_TYPE_USER_SECURE_REGION:
        toid  = TEE_OTP_USR_SEC_RGN;
        osize = HAL_OTP_SIZE_USER_SECURE_REGION;
        break;
    case HAL_OTP_TYPE_TEST_REGION:
        toid  = TEE_OTP_TST_RGN;
        osize = HAL_OTP_SIZE_TEST_REGION;
        break;
    default:
        return -1; /* bad otp type */
    }

    *id   = toid;
    *size = osize;
    return 0;
}

osal_err_t hal_otp_init(void)
{
    return OSAL_SUCCESS;
}

osal_err_t hal_otp_write(hal_otp_type_t type, const uint8_t data[])
{
    TEE_Result tres     = TEE_SUCCESS;
    osal_err_t ret      = OSAL_SUCCESS;
    tee_otp_ident_t oid = -1;
    uint32_t osize      = 0;

    if (!data) {
        return OSAL_ERROR_BAD_PARAMETERS;
    }

    /*
     * write the entire region in one shoot
     */
    UTILS_CHECK_CONDITION(hal_otp_id(type, &oid, &osize) == 0,
                         OSAL_ERROR_BAD_PARAMETERS, "Bad otp type %d\n", type);
    tres = TEE_OTP_Write(oid, 0, data, osize);
    UTILS_CHECK_CONDITION(TEE_SUCCESS == tres, tres,
                         "TEE_OTP_Write failed: 0x%x!\n", tres);

finish:
    return ret;
}

osal_err_t hal_otp_read(hal_otp_type_t type, uint8_t data[])
{
    TEE_Result tres     = TEE_SUCCESS;
    osal_err_t ret      = OSAL_SUCCESS;
    tee_otp_ident_t oid = -1;
    uint32_t osize      = 0;

    if (!data) {
        return OSAL_ERROR_BAD_PARAMETERS;
    }

    /*
     * read the entire region in one shoot
     */
    UTILS_CHECK_CONDITION(hal_otp_id(type, &oid, &osize) == 0,
                         OSAL_ERROR_BAD_PARAMETERS, "Bad otp type %d\n", type);
    tres = TEE_OTP_Read(oid, 0, data, osize);
    UTILS_CHECK_CONDITION(TEE_SUCCESS == tres, tres,
                         "TEE_OTP_Read failed: 0x%x!\n", tres);

finish:
    return ret;
}

void hal_otp_cleanup(void)
{
}
