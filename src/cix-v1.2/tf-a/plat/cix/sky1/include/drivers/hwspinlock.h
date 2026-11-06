/*
 * Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _HWSPINLOCK_H
#define _HWSPINLOCK_H

typedef enum {
	NPU_MUTEX_IDX = 0x2f,
	CORE_MUTEX_IDX = 0x50,
} hw_mutex_idx;

int sky1_hwspinlock_trylock(uint8_t mutex_idx, uint32_t timeout_us);
void sky1_hwspinlock_unlock(uint8_t mutex_idx);

#endif
