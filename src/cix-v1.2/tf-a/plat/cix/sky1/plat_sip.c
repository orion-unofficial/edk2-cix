/*
 * Copyright (C) 2022 Cixcomputing, Inc. All rights reserved.
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
#include <assert.h>
#include <cix_sip_svc.h>
#include <lib/mmio.h>

/***************************************************
* export platform specific soc service ops
***************************************************/
void cix_set_reboot_reason(uint32_t reason)

{
	INFO("%s: set reboot reason: 0x%x \n", __func__, reason);
	mmio_write_32(SKY1_REBOOT_REASON_ADDR, reason);
}

uint32_t cix_get_reboot_reason(void)
{
	return mmio_read_32(SKY1_REBOOT_REASON_ADDR);
}

void cix_reboot_reason_init(void)
{
	uint32_t reason = mmio_read_32(SKY1_REBOOT_REASON_ADDR);

	INFO("last reboot reason:0x%x \n", reason);
	mmio_write_32(SKY1_REBOOT_REASON_UEFI, reason);

	return;
}

static const struct plat_sip_svc_ops_t plat_sip_ops = {
	.set_reboot_reason = cix_set_reboot_reason,
	.set_clr_pd_cnt = set_clr_power_domain_cnt,
	.board_id_check= cix_board_id_check,
	.dst_cmd = cix_dst_cmd,
};

int platform_setup_sip_ops(const struct plat_sip_svc_ops_t **plat_ops)

{
	cix_reboot_reason_init();

	*plat_ops = &plat_sip_ops;

	return 0;

}