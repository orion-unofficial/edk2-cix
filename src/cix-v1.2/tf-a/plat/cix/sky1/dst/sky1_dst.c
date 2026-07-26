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
#include <cix_dst.h>
#include <plat_cix.h>
#include "sky1_dst.h"

static void memory_check_init(void);

static uint64_t sky1_dram1_end = 0;

static dst_cmd_t sky1_dst_cmds[] = {
	DST_CMD_DEF(DST_SET_IDM_MEMORY, set_idm_data_address),
	DST_CMD_DEF(DST_SET_TEE_MEMORY, set_tee_dump_data_address),
	DST_CMD_DEF(DST_SET_TZC400_MEMORY, set_tzc400_data_address),
	DST_CMD_DEF(DST_SET_OS_MEM_SIZE, set_os_mem_size),
	DST_CMD_DEF(DST_SET_TFA_TRACE_MEMORY, set_fiq_data_address),
	DST_CMD_DEF(DST_SET_LAST_STACK_MEMORY, set_last_stack_address),
#if DEBUG
	DST_CMD_DEF(DST_EXCEPTION_DEBUG, sky1_exception_test),
#endif
};
REGISTER_DST_CMD(sky1_dst_cmds);

static dst_init_t sky1_dst_inits[] = {
	DST_INIT_DEF(MEMORY_CHECK, memory_check_init),
	DST_INIT_DEF(BACKUP_LAST_STACK, backup_last_stack),
};
REGISTER_DST_INIT(sky1_dst_inits);

static void memory_check_init(void)
{
	uint64_t dram1_size = 0;
	MEM_INIT_OUTPUT_BUFFER *MemOutputBuffer;

	MemOutputBuffer = GetMemOutputBuffer();
	dram1_size = MemOutputBuffer->TotalSize;
	dram1_size *= 0x100000; // size unit is MB
	sky1_dram1_end = SKY1_DRAM1_BASE + dram1_size - 1;
	sky1_dram1_end =
		MIN((unsigned long long)sky1_dram1_end, SKY1_DRAM1_END);
}

bool check_memory_valid(uint64_t addr, uint64_t size)
{
	uint64_t end_addr = addr + size;

	if (addr < SKY1_DRAM1_BASE || addr > sky1_dram1_end ||
	    end_addr > sky1_dram1_end || end_addr < SKY1_DRAM1_BASE) {
		WARN("0x%lx, %lx, is invalid\n", addr, size);
		return false;
	}

	return true;
}
