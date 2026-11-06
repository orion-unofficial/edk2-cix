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

#include "bl31/interrupt_mgmt.h"
#include "plat/common/platform.h"
#include <plat_cix.h>
#include <lib/extensions/ras.h>
#include "../include/platform_def.h"
#include "sky1_ras_private.h"

#define RAS_INTR_DEF(name, info, cookie) { SKY1_##name##_INTR, info, cookie }
#define RAS_EV_MAP_DEF(name, trigger_type, mpidr)                      \
	{                                                              \
		SDEI_SKY1_##name##_EVENT,                              \
		{                                                      \
			#name, SKY1_##name##_INTR, mpidr, trigger_type \
		}                                                      \
	}

#define RAS_INTR_ID_DEF(name, info, cookie, id) \
	{ SKY1_##name##_INTR(id), info, cookie }
#define RAS_EV_MAP_ID_DEF(name, trigger_type, mpidr, id)                  \
	{                                                                 \
		SDEI_SKY1_##name##_EVENT(id),                             \
		{                                                         \
			#name "[" #id "]", SKY1_##name##_INTR(id), mpidr, \
				trigger_type, id                          \
		}                                                         \
	}

#undef RAS_ARGS_DEF
#define RAS_ARGS_DEF(name, records, cookie, trigger_type, mpidr) \
	RAS_INTR_DEF(name, records, cookie)
#undef RAS_ARGSID_DEF
#define RAS_ARGSID_DEF(name, records, cookie, trigger_type, mpidr, id) \
	RAS_INTR_ID_DEF(name, records, cookie, id)
struct ras_interrupt sky1_ras_interrupts[] = {
#if SKY1_CPU_RAS_SUPPORT
	CPU_RAS_INTR_LIST,
#endif
#if SKY1_IDM_RAS_SUPPORT
	IDM_INTR_LIST,
#endif
#if SKY1_TZC400_IDM_SUPPORT
	TZC400_INTR_LIST
#endif
};

struct err_record_info sky1_err_records[] = {
	[0] = ERR_RECORD_SYSREG_V1(0, 0, sky1_esb_ras_err_record_probe,
				   sky1_esb_ras_err_record_handler, NULL),
};

REGISTER_ERR_RECORD_INFO(sky1_err_records);
REGISTER_RAS_INTERRUPTS(sky1_ras_interrupts);

#undef RAS_ARGS_DEF
#define RAS_ARGS_DEF(name, records, cookie, trigger_type, mpidr) \
	RAS_EV_MAP_DEF(name, trigger_type, mpidr)
#undef RAS_ARGSID_DEF
#define RAS_ARGSID_DEF(name, records, cookie, trigger_type, mpidr, id) \
	RAS_EV_MAP_ID_DEF(name, trigger_type, mpidr, id)
const struct sky1_ras_ev_map sky1_ras_map[] = {
#if SKY1_CPU_RAS_SUPPORT
	CPU_RAS_INTR_LIST,
#endif
#if SKY1_IDM_RAS_SUPPORT
	IDM_INTR_LIST,
#endif
#if SKY1_TZC400_IDM_SUPPORT
	TZC400_INTR_LIST
#endif
};

const struct sky1_ras_ev_map *find_ras_event_map_by_intr(uint32_t intr_num)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sky1_ras_map); i++) {
		if (sky1_ras_map[i].intr.intr == intr_num)
			return &sky1_ras_map[i];
	}

	return NULL;
}

extern void gicd_set_icfgr(uintptr_t base, unsigned int id, unsigned int cfg);
static void sky1_ras_intr_configure(int intr)
{
	const struct sky1_ras_ev_map *map = find_ras_event_map_by_intr(intr);
	const struct intr_info *info = NULL;

	assert(map);
	info = &map->intr;

	plat_ic_set_interrupt_type(intr, INTR_TYPE_EL3);
	plat_ic_set_interrupt_priority(intr, PLAT_RAS_PRI);
	plat_ic_clear_interrupt_pending(intr);
	plat_ic_set_spi_routing(intr, INTR_ROUTING_MODE_PE,
				info->mpidr != INVALID_HWID ? info->mpidr :
							      read_mpidr_el1());
	gicd_set_icfgr(PLAT_SKY1_GICD_BASE, info->intr, info->trigger_type);
	plat_ic_enable_interrupt(intr);
}

static void sky1_ras_intr_setup(void)
{
	for (int i = 0; i < ARRAY_SIZE(sky1_ras_interrupts); i++)
		sky1_ras_intr_configure(sky1_ras_interrupts[i].intr_number);
}

void sky1_ras_setup_resume(void)
{
	plat_cix_security_setup();
#if SKY1_IDM_RAS_SUPPORT
	sky1_idm_init();
#endif
}

void sky1_ras_setup(void)
{
	sky1_ras_intr_setup();
	sky1_ras_setup_resume();
#ifdef SKY1_CPU_RAS_SUPPORT
	sky1_ras_cpu_cache_resume();
	sky1_ras_dsu_cache_resume();
#endif
}
