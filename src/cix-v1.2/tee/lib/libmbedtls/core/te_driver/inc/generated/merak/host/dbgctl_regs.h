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
 * Auto generated file (Fri Oct 21 20:26:59 2022).
 * DO NOT EDIT!
 *
 */

#ifndef __DBGCTL_REGS_H__
#define __DBGCTL_REGS_H__


#define DBGCTL_OFS_CTRL             0x0000
#define DBGCTL_OFS_LOCK             0x0004

#define DBGCTL_REGS_SIZE            0x0008

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_dbgctl module register file definition.
 */
typedef struct te_dbgctl_regs {
    volatile uint32_t ctrl;          /**< +0x000 debug ctrl */
    volatile uint32_t lock;          /**< +0x004 debug ctlr lock */
} te_dbgctl_regs_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __DBGCTL_REGS_H__ */
