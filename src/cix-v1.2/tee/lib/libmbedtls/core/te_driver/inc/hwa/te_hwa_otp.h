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

#ifndef __TRUSTENGINE_HWA_OTP_H__
#define __TRUSTENGINE_HWA_OTP_H__

#include "te_hwa_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_otp_regs;
struct te_hwa_host;

/**
 * Trust engine OTP HWA structure
 */
typedef struct te_hwa_otp {
    te_hwa_crypt_t base;
    int (*read)(struct te_hwa_otp *h, uint32_t off,
                uint8_t *buf, uint32_t len);
} te_hwa_otp_t;

int te_hwa_otp_alloc( struct te_otp_regs *regs,
                      struct te_hwa_host *host,
                      te_hwa_otp_t **hwa );

int te_hwa_otp_free( te_hwa_otp_t *hwa );

int te_hwa_otp_init( struct te_otp_regs *regs,
                     struct te_hwa_host *host,
                     te_hwa_otp_t *hwa );

int te_hwa_otp_exit( te_hwa_otp_t *hwa );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_OTP_H__ */
