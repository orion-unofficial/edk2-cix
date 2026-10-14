/* SPDX-License-Identifier: BSD-2-Clause */
/* Memory contract from upstream OP-TEE stmm_sp_ffa.h. */
#ifndef STMM_HOB_H
#define STMM_HOB_H

#include <kernel/stmm_sp.h>

struct stmm_ffa_mem {
	vaddr_t sp_addr;
	unsigned int sp_size;
	vaddr_t image_addr;
	unsigned int image_region_size;
	vaddr_t heap_addr;
	unsigned int heap_size;
	vaddr_t ns_comm_buf_addr;
	unsigned int ns_comm_buf_size;
	vaddr_t sec_buf_addr;
	unsigned int sec_buf_size;
};

struct stmm_ffa_init_regs {
	unsigned long a0;
};

TEE_Result stmm_ffa_init(const struct stmm_ffa_mem *mem,
			 const struct stmm_flash_nvram_storage_info *flash,
			 struct stmm_ffa_init_regs *regs);
#endif
