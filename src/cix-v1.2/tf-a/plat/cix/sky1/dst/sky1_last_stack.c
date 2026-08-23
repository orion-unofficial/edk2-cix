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
#include "arch_helpers.h"
#include "lib/mmio.h"
#include <common/debug.h>
#include <stdint.h>
#include <string.h>
#include <cix_dst.h>

#define STACK_SAVE_ADDR 0x83df0000 /*mntn dump addr*/
#define STACK_MAGIC 0xbabeface

#define GET_STACK(head, cpu) \
	(((void *)head) + head->stack_offset + cpu * head->stack_size)
#define GET_STACK_DATA(stack, name) (((void *)stack) + stack->name##_offset)

struct stack_data {
	uint64_t vaddr;
	uint32_t comm_offset;
	uint32_t pa_offset;
	uint32_t data_offset;
};

struct stack_head {
	uint64_t size;
	uint64_t page_size;
	uint64_t kaslr;
	uint64_t stext;
	uint64_t etext;
	uint32_t cpu_num;
	uint32_t stack_size;
	uint32_t stack_offset;
	struct stack_data *stack;
};

int set_last_stack_address(uint64_t magic, uintptr_t addr, uint64_t arg2)
{
	int32_t magic_32 = magic;

	INFO("magic[0x%x], addr=0x%lx\n", magic_32, addr);
	mmio_write_64(STACK_SAVE_ADDR, magic_32);
	mmio_write_64(STACK_SAVE_ADDR + 8, addr);
	return 0;
}

static void show_stack_head(struct stack_head *head)
{
	struct stack_data *stack;
	int nr_pages = head->size / head->page_size;
	uint64_t *paddr;

	VERBOSE("head: 0x%lx\n", (uint64_t)head);
	VERBOSE("stack_size: 0x%lx\n", head->size);
	VERBOSE("page_size: 0x%lx\n", head->page_size);
	VERBOSE("kaslr: 0x%lx\n", head->kaslr);
	VERBOSE("stext: 0x%lx\n", head->stext);
	VERBOSE("etext: 0x%lx\n", head->etext);
	VERBOSE("cpu_num: %d\n", head->cpu_num);
	VERBOSE("data_size: %d\n", head->stack_size);
	VERBOSE("data_offset: %d\n", head->stack_offset);

	for (int i = 0; i < head->cpu_num; i++) {
		stack = GET_STACK(head, i);
		VERBOSE("stack: 0x%lx\n", (uint64_t)stack);
		VERBOSE("vaddr: 0x%lx\n", (uint64_t)stack->vaddr);
		VERBOSE("cpu[%d] comm: %s\n", i,
			(char *)GET_STACK_DATA(stack, comm));
		for (int j = 0; j < nr_pages; j++) {
			paddr = GET_STACK_DATA(stack, pa);
			VERBOSE("\tstatic paddr: 0x%lx\n", paddr[j]);
		}
		VERBOSE("stack: 0x%lx, val: 0x%lx\n",
			(uint64_t)GET_STACK_DATA(stack, data),
			mmio_read_64((uint64_t)GET_STACK_DATA(stack, data)));
	}
}

void backup_last_stack(void)
{
	struct stack_head *head = NULL;
	struct stack_data *stack;
	uint64_t magic;
	int nr_pages = 0;
	uint64_t *paddr, back_addr;

	if (!check_exception_boot()) {
		INFO("is not exception boot\n");
		return;
	}

	magic = mmio_read_64(STACK_SAVE_ADDR);
	if ((magic & 0xffffffff) != STACK_MAGIC) {
		INFO("magic[0x%lx] err, not backup laststack\n", magic);
		return;
	}
	mmio_write_64(STACK_SAVE_ADDR, 0);
	clean_dcache_range(STACK_SAVE_ADDR, 8);

	head = (struct stack_head *)mmio_read_64(STACK_SAVE_ADDR + 8);
	if (!head || !check_memory_valid((uint64_t)head, sizeof(*head))) {
		INFO("NULL not backup laststack\n");
		return;
	}
	mmio_write_64(STACK_SAVE_ADDR + 8, 0);

	nr_pages = head->size / head->page_size;
	for (int i = 0; i < head->cpu_num; i++) {
		stack = GET_STACK(head, i);
		if (!check_memory_valid((uint64_t)stack, sizeof(*stack)))
			continue;
		if (nr_pages !=
		    (stack->data_offset - stack->pa_offset) / sizeof(uint64_t))
			continue;
		back_addr = (uint64_t)GET_STACK_DATA(stack, data);
		if (!check_memory_valid(back_addr, head->size))
			continue;
		for (int j = 0; j < nr_pages; j++) {
			paddr = GET_STACK_DATA(stack, pa);
			if (!check_memory_valid(paddr[j], head->page_size))
				continue;
			memcpy(((void *)back_addr), (void *)paddr[j],
			       head->page_size);
			back_addr += head->page_size;
		}
	}
	show_stack_head(head);
}
