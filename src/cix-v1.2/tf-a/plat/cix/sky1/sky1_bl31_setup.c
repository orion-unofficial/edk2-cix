/*
 * Copyright (c) 2020-2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <libfdt.h>
#include <sky1_plat.h>
#include <plat_cix.h>

#include <common/bl_common.h>
#include <common/debug.h>
#include <drivers/arm/css/css_mhu_doorbell.h>
#include <drivers/arm/css/scmi.h>
#include <drivers/generic_delay_timer.h>
#include <lib/fconf/fconf.h>
#include <lib/fconf/fconf_dyn_cfg_getter.h>
#include <plat/common/platform.h>
#include <mailbox.h>
#include <sky1_pdc.h>
#include <lib/extensions/ras.h>
#include <cix_dst.h>

#define CIX_MBOX_MSG_LEN 	(32)
#define MBOX_BASE_TX0		(0x065b0000)
#define SCMI_PAYLOAD_BASE 	(MBOX_BASE_TX0 + 0x0)
#define MBOX_DB_REG_ADDR	(MBOX_BASE_TX0 + 0x4*CIX_MBOX_MSG_LEN)

static entry_point_info_t bl33_image_ep_info;
static entry_point_info_t bl32_image_ep_info;
extern void sky1_init_scmi_server(void);

void *cix_scmi_handle = NULL;
static scmi_channel_t channel;
scmi_lock_t cix_scmi_lock;

static void cix_mbox_ring_doorbell(struct scmi_channel_plat_info *plat_info)
{
	uint32_t db;

	db = mmio_read_32(plat_info->db_reg_addr);
	db &= plat_info->db_preserve_mask;
	db |= plat_info->db_modify_mask;
	mmio_write_32(plat_info->db_reg_addr, db);
}

static scmi_channel_plat_info_t sky1_scmi_plat_info[] = {
	{
		.scmi_mbx_mem = SCMI_PAYLOAD_BASE,
		.db_reg_addr = MBOX_DB_REG_ADDR,
		.db_preserve_mask = 0xfffffffe,
		.db_modify_mask = 0x1,
		.ring_doorbell = &cix_mbox_ring_doorbell,
	}
};

static void plat_cix_scmi_setup(void)
{
	INFO("plat_cix_scmi_setup\n");

	if (channel.is_initialized == 1) {
		INFO("Already intialized\n");
		return;
	}

	channel.info = &sky1_scmi_plat_info[0];
	channel.lock = &cix_scmi_lock;

	cix_scmi_handle = scmi_init(&channel);
	if (cix_scmi_handle == NULL) {
		ERROR("SCMI Initialization failed\n");
		return;
	}
}

void *sky1_get_scmi_handle(void)
{
	return cix_scmi_handle;
}

void cix_bl31_early_platform_setup(u_register_t arg0, u_register_t arg1,
					u_register_t arg2, u_register_t arg3)
{
	/* Initialize the console to provide early debug support */
	cix_postcode_debug(0x203);
	cix_console_boot_init();

	/* write Firmware version to share memory */
	strlcpy((void*)BL31_SHMEM_VER_STR, shmem_version_string, SHMEM_VER_STR_LEN);

	/*
	 * Check params passed from BL2 should not be NULL,
	 */
	bl_params_t *params_from_bl2 = (bl_params_t *)arg0;
	assert(params_from_bl2 != NULL);
	assert(params_from_bl2->h.type == PARAM_BL_PARAMS);
	assert(params_from_bl2->h.version >= VERSION_2);

	bl_params_node_t *bl_params = params_from_bl2->head;

	/*
	 * Copy BL33, BL32, entry point information.
	 * They are stored in Secure RAM, in BL2's address space.
	 */
	while (bl_params != NULL) {
		if (bl_params->image_id == BL32_IMAGE_ID) {
			bl32_image_ep_info = *bl_params->ep_info;
		} else if (bl_params->image_id == BL33_IMAGE_ID) {
			bl33_image_ep_info = *bl_params->ep_info;
		}
		bl_params = bl_params->next_params_info;
	}

	if (bl33_image_ep_info.pc == 0U)
		panic();
}

void bl31_platform_setup(void)
{
	sky1_bl31_common_platform_setup();
}

scmi_channel_plat_info_t *plat_css_get_scmi_info(int channel_id)
{

	return &sky1_scmi_plat_info[channel_id];

}

void bl31_early_platform_setup2(u_register_t arg0, u_register_t arg1,
				u_register_t arg2, u_register_t arg3)
{
	cix_postcode_debug(0x202);
	cix_bl31_early_platform_setup(arg0, arg1, arg2, arg3);
}

void sky1_bl31_common_platform_setup(void)
{
	plat_cix_gic_driver_init();
	plat_cix_gic_init();

	/* Init arch timer */
	generic_delay_timer_init();
#ifdef CIX_GENERIC_TIMER_BASE
	//cix_configure_sys_timer();
#endif
	plat_cix_mbox_init();

	plat_cix_scmi_setup();

	sky1_set_wakeup_enable(S5_GPIO_WKUP_DIS, S5_GPIO_WKUP);

	cix_sky1_qos_setting_init();
#ifdef CONFIG_CIX_DEBUG
	cix_sky1_qos_setting_dump();
#endif
	cix_sky1_nsaid_setting_init();
#if RAS_EXTENSION
	ras_init();
	sky1_ras_setup();
#endif
	cix_dst_init();
}

void cix_bl31_plat_runtime_setup(void)
{
#ifdef CONFIG_DEBUG_FOOTPRINT_ENABLE
	get_stack_status();
#endif
	console_switch_state(CONSOLE_FLAG_RUNTIME);

	/* Initialize the runtime console */
	cix_console_runtime_init();

}

void bl31_plat_runtime_setup(void)
{
	cix_bl31_plat_runtime_setup();
}

entry_point_info_t *bl31_plat_get_next_image_ep_info(unsigned int type)
{
	entry_point_info_t *next_image_info;

	assert(sec_state_is_valid(type));
	if (type == NON_SECURE) {
		next_image_info = &bl33_image_ep_info;
	} else {
		next_image_info = &bl32_image_ep_info;
	}

	if (next_image_info->pc)
		return next_image_info;
	else
		return NULL;
}

const plat_psci_ops_t *plat_cix_psci_override_pm_ops(plat_psci_ops_t *ops)
{
	return css_scmi_override_pm_ops(ops);
}

void __init bl31_plat_arch_setup(void)
{
	unsigned long long dram1_size;
	MEM_INIT_OUTPUT_BUFFER *MemOutputBuffer;

	const mmap_region_t bl_regions[] = {
		MAP_REGION_FLAT(BL31_START, BL31_END - BL31_START,
				MT_MEMORY | MT_RW | MT_SECURE),
		MAP_REGION_FLAT(BL_CODE_BASE, BL_CODE_END - BL_CODE_BASE,
				MT_CODE | MT_SECURE),
#if SEPARATE_CODE_AND_RODATA
		MAP_REGION_FLAT(BL_RO_DATA_BASE,
				BL_RO_DATA_END - BL_RO_DATA_BASE,
				MT_RO_DATA | MT_SECURE),
#endif
		MAP_REGION_FLAT(SKY1_AP_ATF_NS_BASE,
				SKY1_AP_ATF_NS_SIZE,
				MT_MEMORY | MT_RW | MT_NS),

		MAP_REGION_FLAT(AP_SW_CNT_DDR_PHY_ADDR_BASE,
				AP_SW_CNT_DDR_PHY_SIZE,
				MT_MEMORY | MT_RW | MT_NS),
		MAP_REGION_FLAT(SKY1_ATF_TEE_SHM_BASE, SKY1_ATF_TEE_SHM_SIZE,
			MT_MEMORY | MT_RW | MT_SECURE),
		{0},
	};

	MemOutputBuffer = GetMemOutputBuffer();

	dram1_size = MemOutputBuffer->AvailableSize;
	dram1_size *= 0x100000; // size unit is MB

	/* Add Dram1 region */
	mmap_add_region(SKY1_NS_DRAM1_BASE, SKY1_NS_DRAM1_BASE, dram1_size - SKY1_TZC_DRAM1_SIZE, MT_MEMORY | MT_RW | MT_NS);

	setup_page_tables(bl_regions, plat_sky1_mmap);
	enable_mmu_el3(0);
	sky1_init_scmi_server();
}
