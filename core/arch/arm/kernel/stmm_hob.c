// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2019, Linaro Limited
 * Copyright (c) 2020, Arm Limited.
 * Copyright (c) 2026, NVIDIA CORPORATION & AFFILIATES.
 *
 * Adapted from upstream OP-TEE stmm_sp_ffa.c at
 * baf391d0726a0224c79a732502554dd5b92e1cbe for the CIX 3.17 platform.
 */
#include <efi/hob.h>
#include <ffa.h>
#include <string.h>
#include <trace.h>
#include "stmm_hob.h"

static const TEE_UUID ns_buf_guid = MM_NS_BUFFER_GUID;
static const TEE_UUID mmram_resv_guid = MM_PEI_MMRAM_MEMORY_RESERVE_GUID;
static const TEE_UUID cix_nvram_guid = {
	0x4c8167fc, 0x5a9d, 0x468e, {0xa9, 0x42, 0x86, 0x1f, 0xb5, 0x0a, 0xb5, 0x39}
};

static struct efi_hob_handoff_info_table *
build_stmm_boot_hob_list(const struct stmm_ffa_mem *mem,
			 uint32_t *hob_table_size,
			 const struct stmm_flash_nvram_storage_info *flash)
{
	struct efi_hob_handoff_info_table *hob_table = NULL;
	struct efi_mmram_descriptor *mmram_desc_data = NULL;
	struct efi_mmram_hob_descriptor_block *mmram_resv_data = NULL;
	uint16_t mmram_resv_data_size = 0;
	TEE_Result ret = TEE_ERROR_GENERIC;
	uint32_t hob_table_offset = 0;
	void *guid_hob_data = NULL;

	hob_table_offset = sizeof(struct ffa_boot_info_header_1_1) +
			   sizeof(struct ffa_boot_info_1_1);

	if (mem->sec_buf_size <= hob_table_offset)
		return NULL;

	hob_table = efi_create_hob_list(mem->sp_addr, mem->sp_size,
					mem->sec_buf_addr + hob_table_offset,
					mem->sec_buf_size - hob_table_offset);
	if (!hob_table) {
		EMSG("Failed to create hob_table.");
		return NULL;
	}

	ret = efi_create_fv_hob(hob_table, mem->image_addr,
				mem->image_region_size);
	if (ret) {
		EMSG("Failed to create fv hob.");
		return NULL;
	}

	ret = efi_create_guid_hob(hob_table, &ns_buf_guid,
				  sizeof(struct efi_mmram_descriptor),
				  &guid_hob_data);
	if (ret) {
		EMSG("Failed to create ns buffer hob.");
		return NULL;
	}

	mmram_desc_data = guid_hob_data;
	mmram_desc_data->physical_start = mem->ns_comm_buf_addr;
	mmram_desc_data->physical_size = mem->ns_comm_buf_size;
	mmram_desc_data->cpu_start = mem->ns_comm_buf_addr;
	mmram_desc_data->region_state = EFI_CACHEABLE | EFI_ALLOCATED;

	mmram_resv_data_size = sizeof(struct efi_mmram_hob_descriptor_block) +
			       sizeof(struct efi_mmram_descriptor) * 4;

	ret = efi_create_guid_hob(hob_table, &mmram_resv_guid,
				  mmram_resv_data_size, &guid_hob_data);
	if (ret) {
		EMSG("Failed to create mm range hob");
		return NULL;
	}

	mmram_resv_data = guid_hob_data;
	mmram_resv_data->number_of_mm_reserved_regions = 4;
	mmram_desc_data = &mmram_resv_data->descriptor[0];

	mmram_desc_data[0].physical_start = mem->image_addr;
	mmram_desc_data[0].physical_size = mem->image_region_size;
	mmram_desc_data[0].cpu_start = mem->image_addr;
	mmram_desc_data[0].region_state = EFI_CACHEABLE | EFI_ALLOCATED;

	mmram_desc_data[1].physical_start = mem->sec_buf_addr;
	mmram_desc_data[1].physical_size = mem->sec_buf_size;
	mmram_desc_data[1].cpu_start = mem->sec_buf_addr;
	mmram_desc_data[1].region_state = EFI_CACHEABLE | EFI_ALLOCATED;

	mmram_desc_data[2].physical_start = mem->ns_comm_buf_addr;
	mmram_desc_data[2].physical_size = mem->ns_comm_buf_size;
	mmram_desc_data[2].cpu_start = mem->ns_comm_buf_addr;
	mmram_desc_data[2].region_state = EFI_CACHEABLE | EFI_ALLOCATED;

	mmram_desc_data[3].physical_start = mem->heap_addr;
	mmram_desc_data[3].physical_size = mem->heap_size;
	mmram_desc_data[3].cpu_start = mem->heap_addr;
	mmram_desc_data[3].region_state = EFI_CACHEABLE;

	/*
	 * CIX vendor NVRAM HOB: three u64 bases followed by three u32 sizes.
	 * The payload is 36 bytes; do not serialize C structure tail padding.
	 */
	ret = efi_create_guid_hob(hob_table, &cix_nvram_guid, 36, &guid_hob_data);
	if (ret)
		return NULL;
	memcpy(guid_hob_data, &flash->variable_base, 8);
	memcpy((uint8_t *)guid_hob_data + 8, &flash->ftw_working_base, 8);
	memcpy((uint8_t *)guid_hob_data + 16, &flash->ftw_spare_base, 8);
	memcpy((uint8_t *)guid_hob_data + 24, &flash->variable_size, 4);
	memcpy((uint8_t *)guid_hob_data + 28, &flash->ftw_working_size, 4);
	memcpy((uint8_t *)guid_hob_data + 32, &flash->ftw_spare_size, 4);

	*hob_table_size = hob_table->efi_free_memory_bottom -
			  (efi_physical_address_t)hob_table;

	return hob_table;
}

TEE_Result stmm_ffa_init(const struct stmm_ffa_mem *mem,
			 const struct stmm_flash_nvram_storage_info *flash,
			 struct stmm_ffa_init_regs *regs)
{
	struct ffa_boot_info_header_1_1 *hdr = NULL;
	struct ffa_boot_info_1_1 *desc = NULL;
	struct efi_hob_handoff_info_table *hob_table = NULL;
	uint32_t hob_table_size = 0;

	hob_table = build_stmm_boot_hob_list(mem, &hob_table_size, flash);
	if (!hob_table)
		return TEE_ERROR_NO_DATA;

	hdr = (void *)mem->sec_buf_addr;
	hdr->signature = FFA_BOOT_INFO_SIGNATURE;
	hdr->version = FFA_BOOT_INFO_VERSION_1_2;
	hdr->desc_size = sizeof(struct ffa_boot_info_1_1);
	hdr->desc_count = 1;
	hdr->desc_offset = sizeof(struct ffa_boot_info_header_1_1);
	hdr->reserved = 0;
	hdr->blob_size = hdr->desc_size * hdr->desc_count + hdr->desc_offset;

	desc = (void *)(mem->sec_buf_addr + hdr->desc_offset);
	memset(desc->name, 0, FFA_BOOT_INFO_NAME_LEN);
	desc->reserved = 0;
	desc->type = FFA_BOOT_INFO_TYPE_ID_HOB;
	desc->flags = FFA_BOOT_INFO_FLAG_NAME_FORMAT_UUID |
		      (FFA_BOOT_INFO_FLAG_CONTENT_FORMAT_ADDR <<
		       FFA_BOOT_INFO_FLAG_CONTENT_FORMAT_SHIFT);
	desc->size = hob_table_size;
	desc->contents = (vaddr_t)hob_table;

	regs->a0 = (unsigned long)hdr;

	return TEE_SUCCESS;
}

