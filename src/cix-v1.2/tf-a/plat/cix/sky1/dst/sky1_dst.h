/*
 * Copyright 2025 Cix Technology Group Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SKY1_BL31_DST_H
#define SKY1_BL31_DST_H

#include <cix_dst.h>

/* CMD values */
#define DST_SET_IDM_MEMORY 1
#define DST_SET_TEE_MEMORY 3
#define DST_SET_TZC400_MEMORY (0x11)
#define DST_SET_OS_MEM_SIZE (0x12)
#define DST_SET_TFA_TRACE_MEMORY (0x13)
#define DST_SET_LAST_STACK_MEMORY (0x14)

#define DST_EXCEPTION_DEBUG (0xFF)

int set_idm_data_address(uint64_t addr, uint64_t arg1, uint64_t arg2);
int set_tee_dump_data_address(uint64_t addr, uint64_t arg1, uint64_t arg2);
int set_tzc400_data_address(uint64_t ev_num, uint64_t addr, uint64_t arg2);
int set_os_mem_size(uint64_t base, uint64_t size, uint64_t arg2);
int set_fiq_data_address(uint64_t size, uintptr_t addr, uint64_t arg2);
int set_last_stack_address(uint64_t magic, uintptr_t addr, uint64_t arg2);
#ifdef DEBUG
int sky1_exception_test(uint64_t key, uint64_t arg1, uint64_t arg2);
#endif

void backup_last_stack(void);

#endif
