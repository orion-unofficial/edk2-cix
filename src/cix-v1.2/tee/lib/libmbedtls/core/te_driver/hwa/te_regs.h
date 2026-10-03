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

#ifndef __TRUST_ENGINE_REGS_H__
#define __TRUST_ENGINE_REGS_H__

#include "common_regs.h"
#include "host_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

static inline void te_write32(uint32_t val, void* addr)
{
    *(volatile uint32_t*)addr = val;
}

static inline uint32_t te_read32(void* addr)
{
    return *(volatile uint32_t*)addr;
}

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUST_ENGINE_REGS_H__ */
