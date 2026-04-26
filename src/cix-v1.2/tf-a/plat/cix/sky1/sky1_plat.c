/*
 * Copyright (c) 2020-2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <platform_def.h>

#include <plat/common/platform.h>
#include <common/bl_common.h>
#include <common/debug.h>
#include <drivers/arm/ccn.h>
#include <plat_cix.h>
#include <plat/common/platform.h>
#include <drivers/arm/sbsa.h>

/*
 * Table of regions for different BL stages to map using the MMU.
 * This doesn't include Trusted RAM as the 'mem_layout' argument passed to
 * cix_configure_mmu_elx() will give the available subset of that.
 */
#if IMAGE_BL2
const mmap_region_t plat_sky1_mmap[] = {
	SKY1_MAP_DEVICE,
	/* set this region attr as MT_SECURE in bl2 for security*/
	MAP_REGION_FLAT(BL31_BASE, BL31_SIZE, MT_MEMORY | MT_RW | MT_SECURE),
#ifdef BL32_BASE
	MAP_REGION_FLAT(BL32_BASE, BL32_SIZE, MT_MEMORY | MT_RW | MT_SECURE),
#endif
	{0}
};
#endif
#if IMAGE_BL31
const mmap_region_t plat_sky1_mmap[] = {
	SKY1_MAP_DEVICE,
#ifdef BL32_BASE
        MAP_REGION_FLAT(BL32_BASE, BL32_SIZE, MT_MEMORY | MT_RW | MT_SECURE),
#endif
	{0}
};
#endif

#if TRUSTED_BOARD_BOOT
int plat_get_mbedtls_heap(void **heap_addr, size_t *heap_size)
{
	assert(heap_addr != NULL);
	assert(heap_size != NULL);

	return get_mbedtls_heap_helper(heap_addr, heap_size);
}
#endif

MEM_INIT_OUTPUT_BUFFER* GetMemOutputBuffer(void)
{
  uint32_t *addr;
  addr = (uint32_t *)MEM_OUTPUT_BUFFER_ADDR;
  return (MEM_INIT_OUTPUT_BUFFER*)(addr);
}

#define PMCTRL_S5_BASE  0x16000000

int sky1_set_cpu_boost_trigger(int set)
{
	uint32_t regval = mmio_read_32(PMCTRL_S5_BASE + 0x50C);
	if (set) {
		regval |= (1 << 4);
	} else {
		regval &= ~(1 << 4);
	}
	mmio_write_32(PMCTRL_S5_BASE + 0x50C, regval);
	return 0;
}