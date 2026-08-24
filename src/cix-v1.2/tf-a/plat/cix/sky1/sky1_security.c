/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <plat_cix.h>
#include <platform_def.h>

#if TZC_ENABLE
static arm_tzc_regions_info_t tzc_regions[] = {
	SKY1_TZC_REGIONS_DEF,
	{}
};

static arm_tzc_regions_info_t tzc_regions_debug[] = {
	SKY1_TZC_REGIONS_DEF_DEBUG,
	{}
};
#define DDR_SIZE_30G	0x780000000
#endif

/* Initialize the secure environment */
void plat_cix_security_setup(void)
{
	/*enable memory region protect by tzc400*/
#if TZC_ENABLE
	unsigned int i;
	uint32_t enable_ctrl;
	unsigned long long dram1_size;
	arm_tzc_regions_info_t *p;
	MEM_INIT_OUTPUT_BUFFER *MemOutputBuffer;

	enable_ctrl = mmio_read_32(SW_USED_REG5);
	enable_ctrl = (enable_ctrl & BIT(16));

	MemOutputBuffer = GetMemOutputBuffer();
	dram1_size = MemOutputBuffer->TotalSize;
        dram1_size *= 0x100000; // size unit is MB

	/* Change CIX_REGION_TEMP Regions size according to ddr size */
	if (enable_ctrl) {
		if (dram1_size > DDR_SIZE_30G) {
			p = tzc_regions_debug;
			for (; p->base != CIX_REGION_NULL; p++){
				if (p->base == SKY1_DRAM2_BASE) {
					p->base = SKY1_DRAM2_BASE + dram1_size - DDR_SIZE_30G;
					break;
				}
			}
			p = tzc_regions_debug;
			for (; p->base != CIX_REGION_NULL; p++){
				if (p->base == CIX_REGION_TEMP) {
					p->base = CIX_REGION_NULL;
					break;
				}
			}
		} else {
			p = tzc_regions_debug;
			for (; p->base != CIX_REGION_NULL; p++){
				if (p->base == CIX_REGION_TEMP) {
					p->base = SKY1_DRAM1_BASE + dram1_size;
					break;
				}
			}
		}
	}

	if (MemOutputBuffer->ChannelMask == 0xF) {
		/* will init 4 channel */
		i = 0;
	} else if (MemOutputBuffer->ChannelMask == 0xC) {
		/* will init 2 channel : ddr-ch2 & ddr-ch3 */
		i = 2;
	} else {
		/* will init 2 channel : ddr-ch2 & ddr-ch3 */
		i = 2;
	}

	for (; i < TZC400_COUNT; i++) {
		if (enable_ctrl) {
			cix_tzc400_setup(TZC400_BASE(i), tzc_regions_debug);
		} else {
			cix_tzc400_setup(TZC400_BASE(i), tzc_regions);
		}
	}
#endif
}
