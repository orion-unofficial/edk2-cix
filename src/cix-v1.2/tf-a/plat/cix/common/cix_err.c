/*
 * Copyright (c) 2015-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <watchdog.h>
#include <plat_cix.h>
#include <plat/common/platform.h>
#include "include/plat_cix.h"
#include <common/debug.h>
#include <lib/rlog.h>
#include <services/sdei.h>
#include <cix_dst.h>

#define BL31_PANIC 0x00000066

void cix_exception_handler()
{
	backtrace(__func__);

#ifdef RAM_LOG_SUPPORT
	rlog_flush_data();
#endif

	sdei_dispatch_event(CIX_TFA_EXCEPTION_EVENT);

	return;
}

void __dead2 cix_panic_handler(void)
{
	char *str = NULL;

	str = cix_get_cur_dst_init_name();
	if (str)
		ERROR("panic in %s\n", str);
	cix_exception_handler();

	cix_set_reboot_reason(BL31_PANIC);
	plat_watchdog_set_timeout(1);
	while (true) {
		wfi();
	}
}

void __dead2 plat_error_handler(int err)
{
	cix_exception_handler();

	while (true) {
		wfi();
	}
}
