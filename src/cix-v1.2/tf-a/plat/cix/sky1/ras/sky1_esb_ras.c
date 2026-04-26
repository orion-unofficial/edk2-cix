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

#include <context.h>
#include <bl31/interrupt_mgmt.h>
#include <lib/el3_runtime/context_mgmt.h>
#include <lib/extensions/ras.h>
#include "plat/common/platform.h"
#ifdef RAM_LOG_SUPPORT
#include <lib/rlog.h>
#endif

#define DISR_SERROR U(0x11)

DEFINE_RENAME_SYSREG_RW_FUNCS(disr_el1, DISR_EL1)

static const char *reason_str[] = { "EA_ASYNC", "EA_SYNC", "EA_ESB",
				    "INTERRUPT" };

static const char *get_reason_str(unsigned int reason)
{
	if (reason >= ARRAY_SIZE(reason_str))
		return "UNKNOWN";

	return reason_str[reason];
}

#if !DEBUG
 static const char *get_el_str(unsigned int el)
{
	if (el == 3U) {
		return "EL3";
	} else if (el == 2U) {
		return "EL2";
	} else {
		return "S-EL1";
	}
}
#endif

int sky1_esb_ras_err_record_probe(const struct err_record_info *info,
				  int *probe_data)
{
	u_register_t disr_el1;

	disr_el1 = read_disr_el1();
	if (disr_el1 == (BIT(DISR_A_BIT) | DISR_SERROR))
		return 1;

	return 0;
}

int sky1_esb_ras_err_record_handler(const struct err_record_info *info,
				    int probe_data,
				    const struct err_handler_data *const data)
{
	unsigned int level = (unsigned int)GET_EL(read_spsr_el3());
	unsigned int sec_state = get_interrupt_src_ss(data->flags);
	cpu_context_t *ctx;
	gp_regs_t *g_ctx;

	ERROR("core[%d], exception reason: %s, disr_el1: 0x%x, from: %s-%s\n",
	      plat_my_core_pos(), get_reason_str(data->ea_reason),
	      data->syndrome, get_el_str(level),
	      sec_state ? "Non-secure" : "Secure");
	write_disr_el1(0);

	ERROR("elr_el3: 0x%lx\n", read_elr_el3());
	ctx = cm_get_context(sec_state);

	g_ctx = get_gpregs_ctx(ctx);

	ERROR("far_el1: 0x%lx\n", read_far_el1());
	ERROR("esr_el1: 0x%lx\n", read_esr_el1());
	ERROR("far_el2: 0x%lx\n", read_far_el2());
	ERROR("esr_el2: 0x%lx\n", read_esr_el2());
	ERROR("x0: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X0));
	ERROR("x1: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X1));
	ERROR("x2: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X2));
	ERROR("x3: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X3));
	ERROR("x4: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X4));
	ERROR("x5: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X5));
	ERROR("x6: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X6));
	ERROR("x7: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X7));
	ERROR("x8: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X8));
	ERROR("x9: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X9));
	ERROR("x10: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X10));
	ERROR("x11: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X11));
	ERROR("x12: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X12));
	ERROR("x13: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X13));
	ERROR("x14: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X14));
	ERROR("x15: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X15));
	ERROR("x16: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X16));
	ERROR("x17: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X17));
	ERROR("x18: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X18));
	ERROR("x19: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X19));
	ERROR("x20: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X20));
	ERROR("x21: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X21));
	ERROR("x22: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X22));
	ERROR("x23: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X23));
	ERROR("x24: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X24));
	ERROR("x25: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X25));
	ERROR("x26: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X26));
	ERROR("x27: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X27));
	ERROR("x28: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X28));
	ERROR("x29: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_X29));
	ERROR("lr: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_LR));
	ERROR("sp_el0: 0x%lx\n", read_ctx_reg(g_ctx, CTX_GPREG_SP_EL0));
#ifdef RAM_LOG_SUPPORT
	rlog_flush_data();
#endif

	return 0;
}
