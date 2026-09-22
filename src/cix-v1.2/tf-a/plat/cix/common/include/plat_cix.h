/*
 * Copyright (c) 2015-2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PLAT_CIX_H
#define PLAT_CIX_H

#include <stdbool.h>
#include <stdint.h>

#include <drivers/arm/tzc_common.h>
#include <lib/bakery_lock.h>
#include <lib/cassert.h>
#include <lib/el3_runtime/cpu_data.h>
#include <lib/spinlock.h>
#include <lib/utils_def.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <drivers/arm/gicv3.h>

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
struct meminfo;
struct image_info;
struct bl_params;

struct cix_plat_gic_ctx {
	gicv3_redist_ctx_t rdist_ctx[PLATFORM_CORE_COUNT];
	gicv3_dist_ctx_t dist_ctx;
};

typedef struct arm_tzc_regions_info {
	unsigned long long base;
	unsigned long long end;
	unsigned int sec_attr;
	unsigned int nsaid_permissions;
	unsigned int region_index;
} arm_tzc_regions_info_t;

#define OEM_ROTPK_DDR_BASE_ADDRESS_AP (0x80200000U - 0x1000U)

/* define same as fuse table this version */
/* common */
#define KM_CFG_VERSION 0
#define KM_CFG_LC 1
#define KM_CFG_ASYM_ALG_TYPE 2
#define KM_CFG_SEC_ENABLE 3
#define KM_CFG_ENC_ENABLE 4
/* 64 bits serial num + 64 bits chip ip in fuse, 4 word*/
#define KM_CFG_SERIAL_NUM 5
#define KM_CFG_MAX 16

/* boot time config*/
#define BOOT_CFG_MAX 256
#define CMD_STR_MAX 12

/* key config*/
#define KEY_CFG_MAX 1024 * 2

/* pbl key config */
#define KEY_OEM_ROTPK_HASH_OFFSET 0 //32 bytes
#define KEY_OEM_ROTPK_VALID_OFFSET 32 //1 byte

/* max size 4k */
typedef struct _km_meta_tag {
	uint8_t hpubk[32];
	uint8_t rng[64];
	uint32_t config[KM_CFG_MAX];
	uint8_t boot_cfg[BOOT_CFG_MAX];
	uint8_t key_cfg[KEY_CFG_MAX];
} csec_km_meta_t;

typedef struct _mntr_boot_cfg_item {
	char cmd_string[CMD_STR_MAX];
	uint32_t val;
} mntr_boot_cfg_item;

/* ARM State switch error codes */
#define STATE_SW_E_PARAM		(-2)
#define STATE_SW_E_DENIED		(-3)

/* DDR memory output */
#define MEM_OUTPUT_BUFFER_ADDR 0x83C00000
typedef struct {
  uint32_t Signature;
  uint16_t MajorVer;
  uint16_t MinorVer;
  uint8_t  DdrType;
  uint8_t  ChannelMask;
  uint8_t  RanksPerChannel;
  uint8_t  Reserved0;
  uint32_t TotalSize; // MB
  uint32_t AvailableSize; // MB
} MEM_INIT_OUTPUT_BUFFER;

/* DDR info */
MEM_INIT_OUTPUT_BUFFER* GetMemOutputBuffer(void);

typedef struct {
	uint32_t key_exponent;
	size_t module_size;
	uint8_t key_module[512];
}ta_pub_key_info;


/* IO storage utility functions */
int cix_io_setup(void);

/* Security utility functions */
void cix_tzc400_setup(uintptr_t tzc_base,
			const arm_tzc_regions_info_t *tzc_regions);

/* Console utility functions */
void cix_console_boot_init(void);
void cix_console_boot_end(void);
void cix_console_runtime_init(void);
void cix_console_runtime_end(void);
void cix_console_print_logo(void);

/* Systimer utility function */
void cix_configure_sys_timer(void);

/* PM utility functions */
int cix_validate_power_state(unsigned int power_state,
			    psci_power_state_t *req_state);
int cix_validate_psci_entrypoint(uintptr_t entrypoint);
int cix_validate_ns_entrypoint(uintptr_t entrypoint);
void cix_system_pwr_domain_save(void);
void cix_system_pwr_domain_resume(void);
int cix_psci_read_mem_protect(int *enabled);
int cix_nor_psci_write_mem_protect(int val);
void cix_nor_psci_do_static_mem_protect(void);
void cix_nor_psci_do_dyn_mem_protect(void);
int cix_psci_mem_protect_chk(uintptr_t base, u_register_t length);

/* BL2 at EL3 functions */
void cix_bl2_el3_early_platform_setup(void);
void cix_bl2_el3_plat_arch_setup(void);

/* BL31 utility functions */
void cix_bl31_early_platform_setup(u_register_t arg0, u_register_t arg1, u_register_t arg2, u_register_t arg3);
void cix_bl31_platform_setup(void);
void cix_bl31_plat_runtime_setup(void);
void cix_bl31_plat_arch_setup(void);

/*
 * Mandatory functions required in ARM standard platforms
 */
unsigned int plat_cix_get_cluster_core_count(u_register_t mpidr);
void plat_cix_gic_driver_init(void);
void plat_cix_gic_init(void);
void plat_cix_gic_cpuif_enable(void);
void plat_cix_gic_cpuif_disable(void);
void plat_cix_gic_redistif_on(void);
void plat_cix_gic_redistif_off(void);
void plat_cix_gic_pcpu_init(void);
void plat_cix_gic_save(struct cix_plat_gic_ctx *ctx);
void plat_cix_gic_resume(struct cix_plat_gic_ctx *ctx);
void plat_cix_security_setup(void);
void plat_cix_pwrc_setup(void);
void plat_cix_program_trusted_mailbox(uintptr_t address);
void plat_cix_gicr_gicd_config(void);
__dead2 void plat_cix_error_handler(int err);

/*
 * Optional functions in ARM standard platforms
 */
void plat_cix_override_gicr_frames(const uintptr_t *plat_gicr_frames);
int cix_get_rotpk_info(void *cookie, void **key_ptr, unsigned int *key_len,
	unsigned int *flags);
void cix_share_oem_public_key(uint8_t *pk_data, uint32_t pk_len, uint32_t module_len);
int cix_get_oem_rotpk_info(void **key_ptr, unsigned int *key_len, uint8_t* valid);

#if ARM_PLAT_MT
unsigned int plat_cix_get_cpu_pe_count(u_register_t mpidr);
#endif

/*
 * Optional functions required in ARM standard platforms
 */
void plat_cix_io_setup(void);
unsigned int plat_cix_calc_core_pos(u_register_t mpidr);

/* Allow platform to override psci_pm_ops during runtime */
const plat_psci_ops_t *plat_cix_psci_override_pm_ops(plat_psci_ops_t *ops);

/* Execution state switch in ARM platforms */
int cix_execution_state_switch(unsigned int smc_fid,
		uint32_t pc_hi,
		uint32_t pc_lo,
		uint32_t cookie_hi,
		uint32_t cookie_lo,
		void *handle);

/* global variables */
extern plat_psci_ops_t plat_cix_psci_pm_ops;
extern const mmap_region_t plat_sky1_mmap[];

void cix_exception_handler();

bool cix_bl_mode_is_backdoor();
int sdei_tee_exception_handler(uint32_t data_len);

uint32_t btcfg_get_value(void *base, char *item, uint32_t *value);

void cix_set_reboot_reason(uint32_t reason);
uint32_t cix_get_reboot_reason(void);

#if CIX_DST_SUPPORT
void drv_ci700_slc_flush(void);
#endif

#endif /* PLAT_CIX_H */
