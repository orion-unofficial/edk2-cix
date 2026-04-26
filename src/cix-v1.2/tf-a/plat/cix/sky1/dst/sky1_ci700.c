/*
 * Copyright (C) 2025 Cixcomputing, Inc. All rights reserved.
 *
 * All information contained herein is Cix confidential.
 *
 * This software is provided to you pursuant to Software License
 * Agreement (SLA) with Cix Inc ("Cix"). This software may be
 * used only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission
 * from Cix.
 *
*/
#include "../include/platform_def.h"
#include <common/debug.h>
#include <drivers/delay_timer.h>

/*HN-F ID*/
#define POR_HNF_NODE_INFO_U_HNF_NID2 \
	(0x10380000UL) ///< hnf0 nodeid2 tgt sn nodeid = 4
#define POR_HNF_NODE_INFO_U_HNF_NID3 \
	(0x103c0000UL) ///< hnf0 nodeid2 tgt sn nodeid = 4
#define POR_HNF_NODE_INFO_U_HNF_NID10 \
	(0x10780000UL) ///< hnf0 nodeid10 tgt sn nodeid = 12
#define POR_HNF_NODE_INFO_U_HNF_NID11 \
	(0x107c0000UL) ///< hnf0 nodeid10 tgt sn nodeid = 12
#define POR_HNF_NODE_INFO_U_HNF_NID34 \
	(0x11380000UL) ///< hnf0 nodeid34 tgt sn nodeid = 36
#define POR_HNF_NODE_INFO_U_HNF_NID35 \
	(0x113c0000UL) ///< hnf0 nodeid34 tgt sn nodeid = 36
#define POR_HNF_NODE_INFO_U_HNF_NID42 \
	(0x11780000UL) ///< hnf0 nodeid42 tgt sn nodeid = 44
#define POR_HNF_NODE_INFO_U_HNF_NID43 \
	(0x117c0000UL) ///< hnf0 nodeid42 tgt sn nodeid = 44
#define POR_HNF_AUX_CTL_OFFSET (0xA08)
#define POR_HNF_ABF_LO_ADDR_OFFSET (0xF50)
#define POR_HNF_ABF_HI_ADDR_OFFSET (0xF58)
#define POR_HNF_ABF_PR_OFFSET (0xf60)
#define POR_HNF_ABF_SR_OFFSET (0xf68)
#define CI700_SLC_FLUSH_TMR (1000000)
#define CI700_RD(addr) mmio_read_64(addr + 0x10000)
#define CI700_WR(v, addr) mmio_write_64(addr + 0x10000, v)
#define SIZE_32GB 0x800000000ull

uint64_t g_os_mem_size = 0;
uint64_t g_os_mem_base = 0;

static void __drv_ci700_slc_flush(uint32_t hnf_base, uint64_t start_addr,
				  uint64_t size)
{
	uint64_t val;
	/*slc disabled then flush irgnore*/
	val = CI700_RD(hnf_base + POR_HNF_AUX_CTL_OFFSET);
	if ((val & (UINT64_C(1) << 11)) || (val & (UINT64_C(1) << 13)))
		return;
	/*slc flush*/
	CI700_WR(start_addr, hnf_base + POR_HNF_ABF_LO_ADDR_OFFSET);
	CI700_WR(start_addr + size, hnf_base + POR_HNF_ABF_HI_ADDR_OFFSET);
	val = CI700_RD(hnf_base + POR_HNF_ABF_PR_OFFSET);
	val |= UINT64_C(1) << 0;
	CI700_WR(val, hnf_base + POR_HNF_ABF_PR_OFFSET);
	uint32_t count = CI700_SLC_FLUSH_TMR;
	do {
		val = UINT64_C(1) & CI700_RD(hnf_base + POR_HNF_ABF_SR_OFFSET);
		udelay(1);
	} while (count-- && (!val));
	if (count == 0)
		INFO("slc timeout!!!\n");
}

static void _drv_ci700_slc_flush(uint32_t hnf_base, uint64_t start_addr, uint64_t size)
{
	uint64_t fbase, fsize;

	fbase = start_addr;
	fsize = size;

	if (fbase + size > SIZE_32GB)
		fsize = SIZE_32GB - fbase;
	/*flush dram1*/
	__drv_ci700_slc_flush(hnf_base, fbase, fsize);

	if (fbase + size > SIZE_32GB) {
		/*flush dram2*/
		fsize = size - fsize;
		__drv_ci700_slc_flush(hnf_base, SKY1_DRAM2_BASE, fsize);
	}
}

int set_os_mem_size(uint64_t base, uint64_t size, uint64_t arg2)
{
	g_os_mem_size = size;
	g_os_mem_base = base & 0xffffffffUL;
	INFO("os mem base[0x%lx], size[0x%lx]\n", g_os_mem_base, g_os_mem_size);
	return 0;
}

void drv_ci700_slc_flush(void)
{
	INFO("flush all slc cache, base[0x%lx], size[0x%lx]\n", g_os_mem_base,
	     g_os_mem_size);
	if (!g_os_mem_size || !g_os_mem_base)
		return;

	_drv_ci700_slc_flush(POR_HNF_NODE_INFO_U_HNF_NID2, g_os_mem_base,
			     g_os_mem_size);
	_drv_ci700_slc_flush(POR_HNF_NODE_INFO_U_HNF_NID10, g_os_mem_base,
			     g_os_mem_size);
	_drv_ci700_slc_flush(POR_HNF_NODE_INFO_U_HNF_NID34, g_os_mem_base,
			     g_os_mem_size);
	_drv_ci700_slc_flush(POR_HNF_NODE_INFO_U_HNF_NID42, g_os_mem_base,
			     g_os_mem_size);
}
