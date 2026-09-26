/*
 * Copyright (c) 2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <plat_cix.h>

/*
 * sky1 error handler
 */
void __dead2 plat_cix_error_handler(int err)
{
	while (true) {
		wfi();
	}
}
