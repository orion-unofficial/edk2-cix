/*
 * Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DSU_PD_H
#define _DSU_PD_H

#define DSU_HW_CTRL_ENABLE		0x1
#define DSU_SW_CTRL_SET_PD		0x2
#define DSU_SW_CTRL_GET_PD		0x3
#define DSU_HW_CTRL_SET			0x4
#define DSU_ENABLE_MASK			(0x7 << 12)

#define DSU_SW_CTRL_READ_CLEAR_HITCNT	0x5
#define DSU_SW_CTRL_READ_CLEAR_MISSCNT	0x6

uint32_t dsu_pd_setting(uint64_t arg0, uint64_t arg1,
			 uint64_t arg2, uint64_t arg3,
			 uint64_t arg4, uint64_t arg5);
uint32_t dsu_configs_save(void);
uint32_t dsu_configs_restore(void);
#endif
