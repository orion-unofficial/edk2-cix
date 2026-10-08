/*
 * Copyright (c) 2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <common/runtime_svc.h>
#include <lib/mmio.h>

#include <drivers/dsu_portion_pd.h>
#include <arch/aarch64/arch_helpers.h>

unsigned long reg[8] = {0};

uint32_t dsu_pd_setting(uint64_t arg0, uint64_t arg1,
			 uint64_t arg2, uint64_t arg3,
			 uint64_t arg4, uint64_t arg5)
{
	unsigned long v = 0;

	switch (arg0) {
	case DSU_HW_CTRL_SET:
		if (arg1)
			write_clusterl3dnth0(arg1);
		if (arg2)
			write_clusterl3dnth1(arg2);
		if (arg3)
			write_clusterl3upth0(arg3);
		if (arg4)
			write_clusterl3upth1(arg4);
		if (arg5)
			write_clusterl3upth2(arg5);
		break;
	case DSU_HW_CTRL_ENABLE:
		v = read_clusterpwrctlr();
		v &= ~DSU_ENABLE_MASK;
		v |= ((arg1) << 12);
		write_clusterpwrctlr(v);
		break;
	case DSU_SW_CTRL_SET_PD:
		write_clusterpwrctlr(arg1);
		break;
	case DSU_SW_CTRL_GET_PD:
		v = read_clusterpwrctlr();
		return v;
	case DSU_SW_CTRL_READ_CLEAR_HITCNT:
		v = read_clusterl3hit();
		write_clusterl3hit(0);
		return v;
	case DSU_SW_CTRL_READ_CLEAR_MISSCNT:
		v = read_clusterl3miss();
		write_clusterl3miss(0);
		return v;
	default:
		break;
	}

	return 0;
}

uint32_t dsu_configs_save(void)
{
	int i;

	/* dump registers */
	reg[0] = read_clusterpwrctlr();
	reg[1] = read_clusterl3dnth0();
	reg[2] = read_clusterl3dnth1();
	reg[3] = read_clusterl3upth0();
	reg[4] = read_clusterl3upth1();
	reg[5] = read_clusterl3upth2();
	reg[6] = read_clusterl3hit();
	reg[7] = read_clusterl3miss();

	for(i=0; i < ARRAY_SIZE(reg); i++)
		INFO("dsu threshold reg[%d] = 0x%lx\n", i, reg[i]);
	return 0;
}

uint32_t dsu_configs_restore(void)
{
	int i;

	/* restore registers */
	write_clusterl3dnth0(reg[1]);
	write_clusterl3dnth1(reg[2]);
	write_clusterl3upth0(reg[3]);
	write_clusterl3upth1(reg[4]);
	write_clusterl3upth2(reg[5]);
	write_clusterl3hit(reg[6]);
	write_clusterl3miss(reg[7]);
	write_clusterpwrctlr(reg[0]);

	reg[0] = read_clusterpwrctlr();
	reg[1] = read_clusterl3dnth0();
	reg[2] = read_clusterl3dnth1();
	reg[3] = read_clusterl3upth0();
	reg[4] = read_clusterl3upth1();
	reg[5] = read_clusterl3upth2();
	reg[6] = read_clusterl3hit();
	reg[7] = read_clusterl3miss();

	for(i=0; i < ARRAY_SIZE(reg); i++)
		INFO("restore dsu threshold reg[%d] = 0x%lx\n", i, reg[i]);
	return 0;
}
