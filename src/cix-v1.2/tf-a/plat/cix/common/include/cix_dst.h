/*
 * Copyright 2025 Cix Technology Group Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CIX_BL31_DST_H
#define CIX_BL31_DST_H

#include <common/debug.h>

typedef struct {
	char cmd;
	char res[3];
	int (*callback)(uint64_t arg0, uint64_t arg1, uint64_t arg2);
} dst_cmd_t;

typedef struct {
	uint32_t num_cmds;
	dst_cmd_t *cmds;
} dst_cmd_mapping;

#define DST_CMD_DEF(id, func) { .cmd = id, .callback = func }

#define REGISTER_DST_CMD(_array)                   \
	const dst_cmd_mapping dst_cmd_mappings = { \
		.cmds = (_array),                  \
		.num_cmds = ARRAY_SIZE(_array),    \
	}

extern const dst_cmd_mapping dst_cmd_mappings;

typedef enum {
	STAGE_OK,
	MEMORY_CHECK,
	BACKUP_LAST_STACK,
	STAGE_MAX,
} dst_init_stage;

typedef struct {
	char *name;
	uint32_t stage;
	void (*init)(void);
} dst_init_t;

typedef struct {
	uint32_t num_inits;
	dst_init_t *inits;
} dst_init_mapping;

#define DST_INIT_DEF(id, func) \
	{ .stage = id, .init = func, .name = #func }

#define REGISTER_DST_INIT(_array)                    \
	const dst_init_mapping dst_init_mappings = { \
		.inits = (_array),                   \
		.num_inits = ARRAY_SIZE(_array),     \
	}

extern const dst_init_mapping dst_init_mappings;

bool check_exception_boot(void);
bool check_memory_valid(uint64_t addr, uint64_t size);
void cix_dst_init(void);
char *cix_get_cur_dst_init_name(void);

#endif
