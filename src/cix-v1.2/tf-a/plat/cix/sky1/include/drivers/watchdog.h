/*
 * Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _WATCHDOG_H
#define _WATCHDOG_H

void plat_watchdog_init(void);
void plat_watchdog_set_timeout(unsigned int timeout_sec);

#endif
