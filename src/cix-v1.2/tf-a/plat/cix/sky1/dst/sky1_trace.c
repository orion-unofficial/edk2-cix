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

typedef struct FIQ_INFO {
	unsigned long tick; /* Timestamp */
	uint32_t intr_num; /* Interrupt number */
	uint32_t reserve; /* Reserved for future use */
} FIQ_INFO;

typedef struct FIQ_DATA {
	uintptr_t addr; /* Address of the FIQ data */
	int size; /* Size of the FIQ data */
	uint32_t max_cnt; /* Maximum number of FIQs to be handled */
	uint32_t cur_cnt; /* Current number of FIQs handled */
	uint32_t cur_write; /* Current write index */
	uint32_t reserve[2]; /* Reserved for future use */
	FIQ_INFO data[]; /* Array of FIQ_INFO structures */
} FIQ_DATA;

FIQ_DATA *g_fiq_data = NULL;

static int fiq_address_init(int32_t size, uintptr_t addr)
{
	if (size < sizeof(FIQ_DATA) + sizeof(FIQ_INFO) || !addr) {
		ERROR("Invalid size, too small\n");
		return -1;
	}

	g_fiq_data = (FIQ_DATA *)addr;
	g_fiq_data->size = size;
	g_fiq_data->addr = addr;
	g_fiq_data->cur_cnt = 0;
	g_fiq_data->max_cnt =
		(size - (int32_t)sizeof(FIQ_DATA)) / (int32_t)sizeof(FIQ_INFO);
	g_fiq_data->cur_write = 0;
	flush_dcache_range((uintptr_t)g_fiq_data, sizeof(FIQ_DATA));
	return 0;
}

void fiq_push(uint32_t intr)
{
	if (!g_fiq_data) {
		return;
	}

	g_fiq_data->cur_cnt++;
	g_fiq_data->data[g_fiq_data->cur_write].intr_num = intr;
	g_fiq_data->data[g_fiq_data->cur_write++].tick = read_cntpct_el0();

	if (g_fiq_data->cur_write >= g_fiq_data->max_cnt) {
		g_fiq_data->cur_write = 0;
	}
}

int set_fiq_data_address(uint64_t size, uintptr_t addr, uint64_t arg2)
{
	fiq_address_init(size, addr);
	return 0;
}
