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
 *
 * Auto generated file (Fri Oct 21 20:27:02 2022).
 * DO NOT EDIT!
 *
 */

#ifndef __CTL_REGS_H__
#define __CTL_REGS_H__

#include "ctl/clock_ctrl.h"
#include "ctl/reset_ctrl.h"
#include "ctl/top_cfg.h"
#include "ctl/cfg_host.h"

#define CTL_OFS_CLOCK_CTRL       0x0000
#define CTL_OFS_RESET_CTRL       0x0004
#define CTL_OFS_TOP_CFG          0x0008
#define CTL_OFS_HOST             0x0010

#define CTL_REGS_SIZE            0x0050

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_ctl module register file definition.
 */
typedef struct te_ctl_regs {
    volatile ctl_clock_ctrlReg_t clock_ctrl; /**< +0x000 sub-block clock control */
    volatile ctl_reset_ctrlReg_t reset_ctrl; /**< +0x004 sub-block reset control */
    volatile ctl_top_cfgReg_t top_cfg; /**< +0x008  */
    volatile uint32_t hole3;
    struct {
        volatile ctl_cfg_hostReg_t cfg_host;
    } host[16];                      /**< +0x010  */
} te_ctl_regs_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __CTL_REGS_H__ */
