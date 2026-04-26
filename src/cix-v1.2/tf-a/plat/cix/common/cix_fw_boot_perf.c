/*
 * Copyright 2024 Cix Technology Group Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "include/cix_fw_boot_perf.h"
#include "plat_pwrc.h"
#include <arch_helpers.h>
#include <assert.h>
#include <common/debug.h>
#include <drivers/arm/css/scmi.h>
#include <drivers/delay_timer.h>
#include <lib/cassert.h>
#include <plat/common/platform.h>
#include <plat_cix.h>
#include <platform_def.h>
#include <sky1_plat.h>

#define FW_BOOT_PERF_BASE (0x83E04000)

/* Sys counter register */
#define SYS_COUNTER_BASE 0x16002000
#define LOW_OFFSET 0x8
#define UP_OFFSET 0xC
#define SYS_COUNTER_ENABLE (1 << 0) /*Enable sys counter*/

struct fw_boot_phase_point *g_boot_perf_base;
/*
 * To get the value from the sys counter register proceed as follows:
 * 1. Read the upper 32-bit timer counter register
 * 2. Read the lower 32-bit timer counter register
 * 3. Read the upper 32-bit timer counter register again. If the value is
 *  different to the 32-bit upper value read previously, go back to step 2.
 *  Otherwise the 64-bit timer counter value is correct.
 */
static unsigned long sky1_syscounter_read(void)
{
	uint64_t counter;
	uint32_t lower;

	counter = mmio_read_32(SYS_COUNTER_BASE + UP_OFFSET);
	lower = mmio_read_32(SYS_COUNTER_BASE + LOW_OFFSET);

	counter <<= 32;
	counter |= lower;
	return counter;
}

static void set_fw_name(enum FW_BOOT_PHASE phase, char *fw_name)
{
	switch (phase) {
	case BROM_PHASE:
		memcpy(fw_name, "BOOTROM", sizeof("BOOTROM"));
		break;
	case SE_PHASE:
		memcpy(fw_name, "CSU_SE", sizeof("CSU_SE"));
		break;
	case PM_PHASE:
		memcpy(fw_name, "CSU_PM", sizeof("CSU_PM"));
		break;
	case PBL_PHASE:
		memcpy(fw_name, "PBL", sizeof("PBL"));
		break;
	case TFA_PHASE:
		memcpy(fw_name, "TFA", sizeof("TFA"));
		break;
	case TEE_PHASE:
		memcpy(fw_name, "TEE", sizeof("TEE"));
		break;
	case BLOADER_PHASE:
		memcpy(fw_name, "BLOADER", sizeof("BLOADER"));
		break;
	case GRUB_PHASE:
		memcpy(fw_name, "GRUB", sizeof("GRUB"));
		break;
	default:
		break;
	}
}

void cix_set_boot_phase(enum FW_BOOT_PHASE phase, enum RECORD_POINT point)
{
	if (!g_boot_perf_base) {
		return;
	}

	switch (point) {
	case RECORD_START:
		set_fw_name(phase, g_boot_perf_base[phase].fw_name);
		g_boot_perf_base[phase].start = sky1_syscounter_read() / 100000;
		break;
	case RECORD_END:
		g_boot_perf_base[phase].end = sky1_syscounter_read() / 100000;
		flush_dcache_range((uint64_t)g_boot_perf_base,sizeof(struct fw_boot_phase_point) * (phase + 1));
		break;
	default:
		break;
	}
}

void cix_set_boot_tfa_end(void)
{
    cix_set_boot_phase(TFA_PHASE, RECORD_END);
    cix_boot_perf_uninit(TFA_PHASE);
}

void cix_boot_perf_init(enum FW_BOOT_PHASE phase)
{
	g_boot_perf_base = (struct fw_boot_phase_point *)FW_BOOT_PERF_BASE;
	if (g_boot_perf_base) {
		memset(g_boot_perf_base + phase, 0,
		       sizeof(struct fw_boot_phase_point));
	}
}

void cix_boot_perf_uninit(enum FW_BOOT_PHASE phase)
{
	if (g_boot_perf_base) {
		flush_dcache_range((uintptr_t)(&g_boot_perf_base[phase]),
			   sizeof(struct fw_boot_phase_point));
	}
    g_boot_perf_base = NULL;
}
