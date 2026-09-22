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
#include <common/debug.h>
#include <common/runtime_svc.h>
#include <cix_dst.h>
#include <lib/mmio.h>
#include "plat_cix.h"

#define REBOOT_REASON_LABEL1 0x40
#define REBOOT_REASON_LABEL4 0xa0

static uint32_t init_stage = STAGE_OK;

int cix_dst_cmd(char cmd, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
	int index = 0;
	dst_cmd_t *cmds = dst_cmd_mappings.cmds;

	for (index = 0; index < dst_cmd_mappings.num_cmds; index++) {
		if (cmd == cmds[index].cmd) {
			break;
		}
	}
	if (index == dst_cmd_mappings.num_cmds) {
		INFO("cmd[%d] is unknown\n", index);
		return -1;
	}
	if (!cmds[index].callback) {
		INFO("cmd[%d] callback is null\n", index);
		return -1;
	}

	return cmds[index].callback(arg0, arg1, arg2);
}

bool check_exception_boot(void)
{
	uint32_t reboot_reason = cix_get_reboot_reason();
	uint32_t reboot_type = reboot_reason & 0xff;

	if (reboot_type < REBOOT_REASON_LABEL1 ||
	    (reboot_type >= REBOOT_REASON_LABEL4)) {
		return false;
	}
	return true;
}

char *cix_get_cur_dst_init_name(void)
{
	int index = 0;

	if (init_stage == STAGE_OK || init_stage >= STAGE_MAX)
		return NULL;

	for (index = 0; index < dst_init_mappings.num_inits; index++) {
		if (dst_init_mappings.inits[index].stage == init_stage)
			break;
	}

	return dst_init_mappings.inits[index].name;
}

void cix_dst_init(void)
{
	int index = 0;
	dst_init_t *inits = dst_init_mappings.inits;

	for (index = 0; index < dst_init_mappings.num_inits; index++) {
		if (!inits[index].init || !inits[index].name)
			continue;
		init_stage = inits[index].stage;
		inits[index].init();
		init_stage = STAGE_OK;
	}
}
