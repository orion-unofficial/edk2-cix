/*
 * Copyright (c) 2023, CIX Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <arch_helpers.h>
#include <common/debug.h>
#include <common/runtime_svc.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>
#include <lib/psci/psci.h>
#include <lib/spinlock.h>
#include <platform_def.h>
#include <plat/common/platform.h>
#include <sky1_dp_gop.h>

int sky1_dp_gop_get(void)
{
	int value;

	value = mmio_read_32(DP_GOP_RESERVED);

	return value;
}

int sky1_dp_gop_set(unsigned int dp_gop_bit)
{
	mmio_setbits_32(DP_GOP_RESERVED, dp_gop_bit & DP_GOP_MASK);

	return 0;
}

int sky1_dp_gop_handler(u_register_t x1, u_register_t x2)
{
	int value = 0;

	switch(x1) {
        case SKY1_SIP_DP_GOP_GET:
                value = sky1_dp_gop_get();
                break;
        case SKY1_SIP_DP_GOP_SET:
                sky1_dp_gop_set(x2);
                break;
        default:
                return SMC_UNK;
        }

	return value;
}
