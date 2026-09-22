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

#ifndef __OTP_ALT_H__
#define __OTP_ALT_H__
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define MBEDTLS_ERR_OTP_ALLOC_FAILED                   -0x0010  /**< Failed to allocate memory. */
#define MBEDTLS_ERR_OTP_INVALID_INPUT_LENGTH           -0x0032  /**< The data input has an invalid length. */
#define MBEDTLS_ERR_OTP_BAD_INPUT_DATA                 -0x0073  /**< OTP input data was malformed. */
#define MBEDTLS_ERR_OTP_OVERFLOW                       -0x0030  /**< OTP overflow. */
#define MBEDTLS_ERR_OTP_ACCESS_DENIED                  -0x0031  /**< OTP access denied. */


#define MBEDTLS_OTP_MAGIC       (0x4F5450U)   /* OTP */

struct te_otp_drv ;
struct te_otp_conf ;

typedef struct mbedtls_otp_conf {
    bool otp_exist;
    uint16_t otp_tst_sz;         /**< test region size in byte */
    uint16_t otp_s_sz;           /**< sec region size in byte */
    uint16_t otp_ns_sz;          /**< ns region size in byte */
    uint8_t otp_skey_sz;         /**< model key or device root key size in byte */
}mbedtls_otp_conf;

typedef struct mbedtls_otp_context {
    uint32_t magic;
    struct te_otp_drv * otp_drv;
} mbedtls_otp_context;

/**
 * OTP LAYOUT
 */
/**
 * size of each region
 */
#define MBEDTLS_OTP_MODEL_ID_SIZE           (0x04U)
#define MBEDTLS_OTP_MODEL_KEY_SIZE __extension__({  \
    mbedtls_otp_conf _cfg = {0};                    \
    mbedtls_otp_get_conf(NULL, &_cfg);              \
    _cfg.otp_skey_sz;                               \
})
#define MBEDTLS_OTP_DEVICE_ID_SIZE          (0x04U)
#define MBEDTLS_OTP_DEVICE_RK_SIZE __extension__({  \
    mbedtls_otp_conf _cfg = {0};                    \
    mbedtls_otp_get_conf(NULL, &_cfg);              \
    _cfg.otp_skey_sz;                               \
})
#define MBEDTLS_OTP_SEC_BOOT_HASH_SIZE      (0x20U)
#define MBEDTLS_OTP_LCS_SIZE                (0x04U)
#define MBEDTLS_OTP_LOCK_CTRL_SIZE          (0x04U)
#define MBEDTLS_OTP_NSEC_SIZE __extension__({           \
    mbedtls_otp_conf _cfg = {0};                        \
    mbedtls_otp_get_conf(NULL, &_cfg);                  \
    _cfg.otp_ns_sz;                                     \
})
#define MBEDTLS_OTP_SEC_SIZE __extension__({            \
    mbedtls_otp_conf _cfg = {0};                        \
    mbedtls_otp_get_conf(NULL, &_cfg);                  \
    _cfg.otp_s_sz;                                      \
})
#define MBEDTLS_OTP_TST_SIZE __extension__({            \
    mbedtls_otp_conf _cfg = {0};                        \
    mbedtls_otp_get_conf(NULL, &_cfg);                  \
    _cfg.otp_tst_sz;                                    \
})

/**
 *  offset of each region
 */
#define MBEDTLS_OTP_MODEL_ID_OFFSET         (0x00U)
#define MBEDTLS_OTP_MODEL_KEY_OFFSET                                            \
            (MBEDTLS_OTP_MODEL_ID_OFFSET + MBEDTLS_OTP_MODEL_ID_SIZE)
#define MBEDTLS_OTP_DEVICE_ID_OFFSET                                            \
            (MBEDTLS_OTP_MODEL_KEY_OFFSET + MBEDTLS_OTP_MODEL_KEY_SIZE)
#define MBEDTLS_OTP_DEVICE_RK_OFFSET                                            \
            (MBEDTLS_OTP_DEVICE_ID_OFFSET + MBEDTLS_OTP_DEVICE_ID_SIZE)
#define MBEDTLS_OTP_SEC_BOOT_HASH_OFFSET                                        \
    (MBEDTLS_OTP_DEVICE_RK_OFFSET + MBEDTLS_OTP_DEVICE_RK_SIZE)
#define MBEDTLS_OTP_LCS_OFFSET                                                  \
    (MBEDTLS_OTP_SEC_BOOT_HASH_OFFSET + MBEDTLS_OTP_SEC_BOOT_HASH_SIZE)
#define MBEDTLS_OTP_LOCK_CTRL_OFFSET                                            \
    (MBEDTLS_OTP_LCS_OFFSET + MBEDTLS_OTP_LCS_SIZE)
#define MBEDTLS_OTP_NSEC_REGION_OFFSET                                          \
    (MBEDTLS_OTP_LOCK_CTRL_OFFSET + MBEDTLS_OTP_LOCK_CTRL_SIZE)
#define MBEDTLS_OTP_SEC_REGION_OFFSET                                           \
    (MBEDTLS_OTP_NSEC_REGION_OFFSET + MBEDTLS_OTP_NSEC_SIZE)
#define MBEDTLS_OTP_TST_REGION_OFFSET                                           \
    (MBEDTLS_OTP_SEC_REGION_OFFSET + MBEDTLS_OTP_SEC_SIZE)

/**
 * \brief          otp checkup routine.
 *
 * \return         \c 0 on success.
 * \return         \c 1 on failure.
 */
int mbedtls_otp_self_test( int verbose );

#endif

#ifdef __cplusplus
}
#endif
#endif
