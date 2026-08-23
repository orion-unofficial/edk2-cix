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

#include <types_ext.h>
#include "osal_assert.h"
#include "hal_device_info.h"
#include "hal_otp.h"
#include "utils_ext.h"
#include <tee_api.h>
#include <tee_api_ext.h>

/* MUST be aligned with DEV-ID TA's UUID */
#define GEN_CRK_HINT                                                           \
    {                                                                          \
        0x4f, 0xfa, 0x82, 0x88, 0x19, 0x38, 0x8e, 0x45, 0x8f, 0xdb, 0x5e,      \
            0x09, 0x26, 0x2b, 0x18, 0xe6                                       \
    }
#define GEN_CRK_USER_SEED_STR "CRK"

osal_err_t hal_write_device_info(const uint8_t model_id[HAL_MODEL_ID_SIZE],
                                 const uint8_t device_id[HAL_DEVICE_ID_SIZE])
{
    UTILS_UNUSED(model_id);
    UTILS_UNUSED(device_id);
    return OSAL_ERROR_NOT_SUPPORTED;
}

osal_err_t hal_read_device_info(uint8_t model_id[HAL_MODEL_ID_SIZE],
                                uint8_t device_id[HAL_DEVICE_ID_SIZE])
{
    osal_err_t ret = OSAL_SUCCESS;

    UTILS_CHECK_CONDITION(model_id, OSAL_ERROR_BAD_PARAMETERS,
                         "model_id pointer is NULL!\n");
    UTILS_CHECK_CONDITION(device_id, OSAL_ERROR_BAD_PARAMETERS,
                         "device_id pointer is NULL!\n");

    ret = hal_otp_init();
    OSAL_ASSERT_MSG(OSAL_SUCCESS == ret, "HAL Init OTP failed!\n");

    ret = hal_otp_read(HAL_OTP_TYPE_MODEL_ID, model_id);
    UTILS_CHECK_RET_QUIET;
    ret = hal_otp_read(HAL_OTP_TYPE_DEVICE_ID, device_id);
    UTILS_CHECK_RET_QUIET;
    ret = OSAL_SUCCESS;

finish:
    hal_otp_cleanup();
    return ret;
}

osal_err_t hal_derive_crk(uint8_t crk[HAL_CRK_SIZE])
{
    TEE_Result tee_res    = TEE_SUCCESS;
    osal_err_t ret        = OSAL_SUCCESS;
    TEE_UUID caller_uuid  = {0};
    uint8_t hint[]        = GEN_CRK_HINT;
    const char *seed      = GEN_CRK_USER_SEED_STR;
    TEE_ObjectHandle hkey = TEE_HANDLE_NULL;
    TEE_Attribute attr    = {0};
    uint32_t crk_size     = HAL_CRK_SIZE;

    OSAL_ASSERT(sizeof(hint) == sizeof(caller_uuid));
    UTILS_CHECK_CONDITION(crk, OSAL_ERROR_BAD_PARAMETERS,
                         "CRK pointer is NULL!\n");

    /* check caller UUID */
    tee_res = TEE_GetPropertyAsUUID(TEE_PROPSET_CURRENT_TA, "gpd.ta.appID",
                                    &(caller_uuid));
    UTILS_CHECK_CONDITION(TEE_SUCCESS == tee_res, tee_res,
                         "TEE_GetPropertyAsUUID failed: 0x%x!\n", tee_res);
    UTILS_CHECK_CONDITION(0 == memcmp(hint, &(caller_uuid), sizeof(caller_uuid)),
                         OSAL_ERROR_ACCESS_DENIED,
                         "Bad caller UUID! Couldn't derive CRK!\n");

    tee_res = TEE_AllocateTransientObject(TEE_TYPE_AES, 256, &hkey);
    UTILS_CHECK_CONDITION(TEE_SUCCESS == tee_res, (osal_err_t)(tee_res),
                         "TEE_AllocateTransientObject failed: 0x%x!\n",
                         tee_res);

    /* generak key */
    memset(&attr, 0, sizeof(attr));
    attr.attributeID        = TEE_ATTR_SECRET_VALUE;
    attr.content.ref.buffer = (void *)seed;
    attr.content.ref.length = strlen(seed);
    tee_res = TEE_KLGenerateKey(hkey, TEE_KLAD_RKSEL_ROOT, HAL_CRK_SIZE << 3,
                                &attr, 1);

    UTILS_CHECK_CONDITION(TEE_SUCCESS == tee_res, (osal_err_t)(tee_res),
                         "TEE_KLGenerateKey failed: 0x%x!\n", tee_res);

    /* export key */
    tee_res = TEE_GetObjectBufferAttribute(hkey, TEE_ATTR_SECRET_VALUE, crk,
                                           &crk_size);
    UTILS_CHECK_CONDITION(TEE_SUCCESS == tee_res, (osal_err_t)(tee_res),
                         "TEE_GetObjectBufferAttribute failed: 0x%x!\n",
                         tee_res);
    ret = OSAL_SUCCESS;

finish:
    if (hkey != TEE_HANDLE_NULL) {
        TEE_FreeTransientObject(hkey);
    }
    return ret;
}
