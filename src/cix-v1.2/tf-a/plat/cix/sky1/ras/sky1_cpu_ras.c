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

#include "lib/extensions/ras.h"
#include "plat/common/platform.h"
#include "sky1_ras_private.h"
#include <lib/el3_runtime/pubsub_events.h>
#include "arch_helpers.h"
#include "common/debug.h"
#include "lib/extensions/ras_arch.h"
#ifdef RAM_LOG_SUPPORT
#include <lib/rlog.h>
#endif

#define SELECT_DSU_RAM 0
#define SELECT_CPU_RAM 1
#define SELECT_L2_RAM 2

#define MAX_CPU_NAME (12)

#define CPU_RAS_RECORD_DEF(idx, size, aux)                             \
	ERR_RECORD_SYSREG_V1(idx, size, sky1_cpu_ras_err_record_probe, \
			     sky1_cpu_ras_err_record_handler, aux)

#define GET_CPU_ERR_VALUE(value, err) ((value >> (err)->shift) & (err)->mask)

#define ERR_CTRL_ENABLE(fr, ctrl, field) \
	if (fr.field)                    \
		ctrl.field = 1;

struct ERX_CTRL_REG {
	union {
		struct {
			u_register_t ED : 1;
			u_register_t RAZ_WI__01 : 1;
			u_register_t UI : 1;
			u_register_t FI : 1;
			u_register_t RAZ_WI__02 : 1;
			u_register_t RES0__01 : 3;
			u_register_t CFI : 1;
			u_register_t RES0__02 : 1;
			u_register_t DUI : 1;
			u_register_t RES0__03 : 2;
			u_register_t CI : 1;
			u_register_t RES0__04 : 50;
		};
		u_register_t value;
	};
};

struct ERX_FR_REG {
	union {
		struct {
			u_register_t ED : 2;
			u_register_t DE : 2;
			u_register_t UI : 2;
			u_register_t FI : 2;
			u_register_t UE : 2;
			u_register_t CFI : 2;
			u_register_t CEC : 3;
			u_register_t RP : 1;
			u_register_t DUI : 2;
			u_register_t CEO : 2;
			u_register_t INJ : 2;
			u_register_t CI : 2;
			u_register_t TS : 2;
			u_register_t RES0 : 38;
		};
		u_register_t value;
	};
};

struct CPU_ERR_MSG {
	uint64_t mask;
	uint32_t shift;
	char *msg;
};

struct CPU_AUX_DATA {
	uint32_t valid_msg_num;
	struct CPU_ERR_MSG *valid_msg;
	uint32_t msg_num;
	struct CPU_ERR_MSG *msg;
};

static int
sky1_cpu_ras_err_record_handler(const struct err_record_info *info,
				int probe_data,
				const struct err_handler_data *const data);
static int sky1_cpu_ras_err_record_probe(const struct err_record_info *info,
					 int *probe_data);

const static struct CPU_ERR_MSG err_valid_msg[] = {
	{ 0x1, 31, "Address Valid" },
	{ 0x1, 30, "Status Register Valid" },
};

const static struct CPU_ERR_MSG err_msg[] = {
	{ 0xff, 0, "Architecturally-defined primary error code" },
	{ 0xff, 8, "IMPLEMENTATION DEFINED error code" },
	{ 0x1, 19, "Critical Error" },
	{ 0x3, 20, "Uncorrected Error Type" },
	{ 0x1, 22, "Poison" },
	{ 0x1, 23, "Deferred Error" },
	{ 0x3, 24, "Corrected Error" },
	{ 0x1, 26, "Miscellaneous Registers Valid" },
	{ 0x1, 27, "Overflow" },
	{ 0x1, 28, "Error Reported" },
	{ 0x1, 29, "Uncorrected Error" },
};

static struct CPU_AUX_DATA cpu_aux_data = {
	.valid_msg_num = ARRAY_SIZE(err_valid_msg),
	.valid_msg = (struct CPU_ERR_MSG *)err_valid_msg,
	.msg_num = ARRAY_SIZE(err_msg),
	.msg = (struct CPU_ERR_MSG *)err_msg,
};

struct err_record_info cpu_ras_records[] = {
	CPU_RAS_RECORD_DEF(SELECT_DSU_RAM, 1, &cpu_aux_data),
	CPU_RAS_RECORD_DEF(SELECT_CPU_RAM, 1, &cpu_aux_data),
	CPU_RAS_RECORD_DEF(SELECT_CPU_RAM, 2, &cpu_aux_data)
};

static int sky1_cpu_ras_err_record_probe(const struct err_record_info *info,
					 int *probe_data)
{
	u_register_t err_status;
	struct CPU_AUX_DATA *aux = info->aux_data;
	uint32_t val = 0, find = 0;

	assert(aux);

	for (uint32_t i = 0; i < info->sysreg.num_idx; i++) {
		ser_sys_select_record(i + info->sysreg.idx_start);
		err_status = read_erxstatus_el1();
		for (uint32_t j = 0; j < aux->valid_msg_num; j++) {
			val = GET_CPU_ERR_VALUE(err_status, &aux->valid_msg[j]);
			if (!val)
				continue;
			find++;
			ERROR("%s\n", aux->valid_msg[j].msg);
			if (find > 1)
				continue;
			*probe_data = (int)err_status;
		}
		if (find) {
			err_status&=~(0xffffUL);
			write_erxstatus_el1(err_status);
			return 1;
		}
	}

	return 0;
}

static int
sky1_cpu_ras_err_record_handler(const struct err_record_info *info,
				int probe_data,
				const struct err_handler_data *const data)
{
	struct CPU_AUX_DATA *aux = info->aux_data;
	uint32_t valid[8], val;
	const struct sky1_ras_ev_map *ev;

	assert(aux);

	for (uint32_t i = 0; i < aux->valid_msg_num; i++) {
		valid[i] = GET_CPU_ERR_VALUE(probe_data, &aux->valid_msg[i]);
	}

	ev = find_ras_event_map_by_intr(data->interrupt);
	ERROR("core[%d], %s-> \n", plat_my_core_pos(), ev->intr.name);
	for (uint32_t i = 0; i < aux->msg_num; i++) {
		val = GET_CPU_ERR_VALUE(probe_data, &aux->msg[i]);
		if (!val)
			continue;
		ERROR("\t%s: 0x%x\n", aux->msg[i].msg, val);
	}
	ERROR("\tmisc0: 0x%lx\n", read_erxmisc0_el1());

	if (valid[0]) {
		ERROR("\taddress: 0x%lx\n", read_erxaddr_el1());
	}
	plat_ic_end_of_interrupt(data->interrupt);
#ifdef RAM_LOG_SUPPORT
	rlog_flush_data();
#endif

	return 0;
}
static void sky1_ras_cache_resume(uint32_t select)
{
	struct ERX_CTRL_REG ctrl;
	struct ERX_FR_REG fr;

	ser_sys_select_record(select);
	fr.value = read_erxfr_el1();
	ERR_CTRL_ENABLE(fr, ctrl, ED);
	ERR_CTRL_ENABLE(fr, ctrl, UI);
	ERR_CTRL_ENABLE(fr, ctrl, FI);
	// ERR_CTRL_ENABLE(fr, ctrl, CFI);
	ERR_CTRL_ENABLE(fr, ctrl, DUI);
	ERR_CTRL_ENABLE(fr, ctrl, CI);
	write_erxctlr_el1(ctrl.value);
}

void sky1_ras_cpu_cache_resume(void)
{
	uint32_t core = plat_my_core_pos();
	sky1_ras_cache_resume(SELECT_CPU_RAM);
	if (core < 4)
		sky1_ras_cache_resume(SELECT_L2_RAM);
}

void sky1_ras_dsu_cache_resume(void)
{
	sky1_ras_cache_resume(SELECT_DSU_RAM);
}

static void *sky1_cpu_ras_cpu_on_init(const void *arg)
{
	sky1_ras_cpu_cache_resume();
	return NULL;
}

/* Subscribe to PSCI CPU on to initialize per-CPU configuration */
SUBSCRIBE_TO_EVENT(psci_cpu_on_finish, sky1_cpu_ras_cpu_on_init);

/* Subscribe to PSCI CPU suspend finisher for per-CPU configuration */
SUBSCRIBE_TO_EVENT(psci_suspend_pwrdown_finish, sky1_cpu_ras_cpu_on_init);
