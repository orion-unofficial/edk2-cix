/*
 * Copyright (c) 2022, CIX Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <lib/psci/psci.h>
#include <platform_def.h>
#include <arch_helpers.h>
#include <lib/cassert.h>
#include <common/debug.h>
#include <assert.h>
#include <plat_cix.h>
#include "plat_pwrc.h"
#include <drivers/delay_timer.h>
#include <plat/common/platform.h>
#include <drivers/arm/css/scmi.h>
#include <sky1_plat.h>
#include <mailbox.h>
#include <sky1_pdc.h>
#if ENABLE_AMU
#include <lib/extensions/amu.h>
#endif
#if ENABLE_MPMM
#include <lib/mpmm/mpmm.h>
#endif
#ifdef RAM_LOG_SUPPORT
#include <lib/rlog.h>
#endif

/* sky1 PPU register define */
#define SKY1_UTILITY_BUS_BASE 0xf000000
#define SKY1_PPU_CLUSTER_BASE (SKY1_UTILITY_BUS_BASE + 0x30000)
#define SKY1_PPU_CORE_BASE(n) (SKY1_UTILITY_BUS_BASE + 0x80000 + (n * 0x100000))
#define CLUSTER_CTRL	0x0
#define CLUSTER_MPAM	0x10000
#define CLUSTER_RAS	0x20000
#define ACTIVITY_MON	0x40000

/* Cluster and core register define */
#define PPU_PWPR	0x0
#define PPU_PMER	0x4
#define PPU_PWSR	0x8
#define PPU_PWSR_MASK	0xf

#define PWR_ON	0x1
#define PWR_OFF	0x0
#define CORE_PWR_DYN_EN   (0x1 << 8)
#define CORE_PWRDN_EN     (0x1)
#define BOOT_CORE_OFFSET  12
#define BOOT_CORE_MASK    0xf

#define CORE_ONLINE_NOTIFY   0x40
#define CORE_OFFLINE_NOTIFY  0x41
#define STR_BOOT_CORE_NOTIFY 0x42

/* CLUSTERPWRDN_EL1 register definitions */
#define DSU_CLUSTER_PWR_RET            (0x1 << 1)
#define DSU_CLUSTER_PWR_SHORTSLP       (0x1 << 2)
#define DSU_CLUSTER_PWRDN_MASK         U(7)

#define MEM_DDR_SIZE_ADDR 0x83C0000C

typedef enum {
	PWR_CORE_OFF = 0x0,
	PWR_CORE_OFF_EMU,
	PWR_CORE_FULL_RET = 0x5,
	PWR_CORE_FUNC_RET = 0x7,
	PWR_CORE_ON,
	PWR_CORE_WRAM_RST,
} core_pwr_policy;

#define CLUSTER_OP_DYN_EN (0x1 << 24)
#define CLUSTER_PWR_DYN_EN (0x1 << 8)
typedef enum {
	OP_POLICY_MODE00 = 0x0,
	OP_POLICY_MODE01,
	OP_POLICY_MODE03 = 0x3,
	OP_POLICY_MODE04 = 0x4,
	OP_POLICY_MODE05 = 0x5,
	OP_POLICY_MODE07 = 0x7,
} cluster_op_policy;

typedef enum {
	PWR_CLUSTER_OFF = 0x0,
	PWR_CLUSTER_OFF_EMU,
	PWR_CLUSTER_MEM_RET,
	PWR_CLUSTER_RET_EMU,
	PWR_CLUSTER_FULL_RET = 0x5,
	PWR_CLUSTER_FUNC_RET = 0x7,
	PWR_CLUSTER_ON,
	PWR_CLUSTER_WRAM_RST,
} cluster_pwr_policy;

#define CORE_PWR_DYNAMIC_OFF	\
				(CORE_PWR_DYN_EN	\
				| PWR_CORE_OFF)
#define CORE_PWR_DYNAMIC_ON	\
				(CORE_PWR_DYN_EN	\
				| PWR_CORE_ON)
#define CLUSTER_PWR_DYNAMIC_OFF	\
					(CLUSTER_OP_DYN_EN	\
					| CLUSTER_PWR_DYN_EN	\
					| PWR_CLUSTER_OFF)
#define CLUSTER_PWR_DYNAMIC_MEM_RET \
					(CLUSTER_OP_DYN_EN	\
					| CLUSTER_PWR_DYN_EN	\
					| PWR_CLUSTER_MEM_RET)
#define CLUSTER_PWR_DYNAMIC_ON	\
					(CLUSTER_OP_DYN_EN	\
					| OP_POLICY_MODE07	\
					| CLUSTER_PWR_DYN_EN	\
					| PWR_CLUSTER_ON)
/*DSU RCSU BASE */
#define DSU_RCSU_BASE   0x0fc50000
#define DSU_RCSU_OFFSET 0x300
#define CPU_RVB_ADDR(n) (DSU_RCSU_BASE + DSU_RCSU_OFFSET + ((n + 1) << 0x3))

#define CPU_RESET_VECTOR_DEF_ADDR 0x80000000

/* PMCTRL_S5 BASE */
#define S5_PWRBTN_WKUP  BIT(0)
#define S5_PWRBTN_WKUP     BIT(0)
#define S5_GPIO_WKUP       BIT(1)
#define S5_SENSORHUB_WKUP  BIT(2)
#define PMCTRL_S5_BASE  0x16000000
#define SLP_TYPE_OFFSET 0x100
#define WKUP_EN_OFFSET  0x20c
#define WARM_RST_OFFSET 0x300

#define PMCTRL_S5_GPIO0_BASE            0x16004000
#define PMCTRL_S5_GPIO_GROUP_NUM        3
#define PMCTRL_S5_GPIO_REG(n,offset)    (PMCTRL_S5_GPIO0_BASE + offset + n * 0x1000)
#define CDNS_GPIO_IRQ_MASK              0x14
#define CDNS_GPIO_IRQ_EN                0x18
#define CDNS_GPIO_IRQ_DIS               0x1c
#define CDNS_GPIO_IRQ_STATUS            0x20

/* Sys counter register */
#define SYS_COUNTER_BASE	0x16002000
#define LOW_OFFSET		0x8
#define UP_OFFSET		0xC
#define SYS_COUNTER_ENABLE	(1 << 0) /*Enable sys counter*/

/* Sys counter suspend & resume value */
static uint64_t suspend_value, resume_value;

/* Allow SKY1 platforms to override `plat_cix_psci_pm_ops` */
#pragma weak plat_cix_psci_pm_ops
static uintptr_t plat_sec_entrypoint;
static uintptr_t sky1_dram1_size;
unsigned int boot_cpus;
static struct cix_plat_gic_ctx sky1_gic_ctx;
void mbox_set_suspend(void *suspend_info);

#define GICD_QRENQN_ITS0        BIT(31)
#define GITS_CTRL_ENABLE        BIT(0)

#define GIC_STARPPIN_OFFSET     (0x300)
#define GIC_STRAP_STATUS0       (0x400)

/*define register for recording core off time*/
#define AP_OFF_RISD_CNT_ADDR(idx)	(AP_SW_CNT_DDR_PHY_ADDR_BASE + 0x300 + (idx) * 0x40)
uint64_t coreidle_start[PLATFORM_CORE_COUNT] = {0};

#if ENABLE_AMU
#if !ENABLE_AMU_FCONF
/*amu top config*/
static struct amu_topology amu_topo = {{{0x0}}};
const struct amu_topology * plat_amu_topology() {
	uint32_t cpu_possible;

	cpu_possible = mmio_read_32(SW_USED_REG2);

	for (int cpu_index = 0; cpu_index < PLATFORM_CORE_COUNT; cpu_index++) {
		int is_possible = (cpu_possible >> cpu_index) & 1;
		if (!is_possible) {
			amu_topo.cores[cpu_index].enable = 0x7;
		}
	}

	return &amu_topo;
}
#endif
#endif

#if ENABLE_MPMM
#if !ENABLE_MPMM_FCONF
static struct mpmm_topology mpmm_topo = {{{0x0}}};
const struct mpmm_topology * plat_mpmm_topology() {
	uint32_t cpu_possible;

	cpu_possible = mmio_read_32(SW_USED_REG2);

	for (int cpu_index = 0; cpu_index < PLATFORM_CORE_COUNT; cpu_index++) {
		int is_possible = (cpu_possible >> cpu_index) & 1;
		if (!is_possible) {
			mpmm_topo.cores[cpu_index].supported = 0x1;
		}
	}

	return &mpmm_topo;
};

#endif
#endif

void pciehub_power_ctrl(bool on)
{
	if (on) {
		/*assert qreqn_its and enable ITS*/
		mmio_write_32(PCIEHUB_RCSU_PD_REG, 0x108);
		mmio_clrbits_32((GIC_RCSU_BASE + GIC_STARPPIN_OFFSET), GICD_QRENQN_ITS0);
		//mmio_setbits_32(PLAT_SKY1_GICD_ITS, GITS_CTRL_ENABLE);
	} else {
		/*disable ITS and de-assert qreqn_its*/
		mmio_clrbits_32(PLAT_SKY1_GICD_ITS, GITS_CTRL_ENABLE);
		mmio_setbits_32((GIC_RCSU_BASE + GIC_STARPPIN_OFFSET), GICD_QRENQN_ITS0);
		mmio_write_32(PCIEHUB_RCSU_PD_REG, 0x100);
	}
}

/*
 * To get the value from the sys counter register proceed as follows:
 * 1. Read the upper 32-bit timer counter register
 * 2. Read the lower 32-bit timer counter register
 * 3. Read the upper 32-bit timer counter register again. If the value is
 *  different to the 32-bit upper value read previously, go back to step 2.
 *  Otherwise the 64-bit timer counter value is correct.
 */
static unsigned long sky1_syscounter_read(void)
{
	uint64_t  counter;
	uint32_t  lower;
	uint32_t  upper, old_upper;

	upper = mmio_read_32(SYS_COUNTER_BASE + UP_OFFSET);
	do {
		old_upper = upper;
		lower = mmio_read_32(SYS_COUNTER_BASE + LOW_OFFSET);
		upper = mmio_read_32(SYS_COUNTER_BASE + UP_OFFSET);

	} while (upper != old_upper);

	counter = upper;
	counter <<= 32;
	counter |= lower;
	return counter;
}

static void update_suspend_syscounter()
{
	uint32_t tmp;
	tmp = mmio_read_32(SYS_COUNTER_BASE);
	suspend_value += (resume_value - suspend_value) << 2;
	mmio_write_32(SYS_COUNTER_BASE, tmp & ~ SYS_COUNTER_ENABLE);
	mmio_write_32(SYS_COUNTER_BASE + UP_OFFSET, suspend_value >> 32);
	mmio_write_32(SYS_COUNTER_BASE + LOW_OFFSET, (uint32_t)suspend_value);
	tmp = mmio_read_32(SYS_COUNTER_BASE);
	mmio_write_32(SYS_COUNTER_BASE, tmp | SYS_COUNTER_ENABLE);
}

extern unsigned int gpio_wake_en[];
static void sky1_gpio_wakeup_set(void)
{
	unsigned int i;

	for (i = 0; i < PMCTRL_S5_GPIO_GROUP_NUM; i++) {
		mmio_write_32(PMCTRL_S5_GPIO_REG(i, CDNS_GPIO_IRQ_EN), gpio_wake_en[i]);
	}
}

static void sky1_wakeup_source_mask(void)
{
	unsigned long status;
	unsigned int i;

	for (i = 0; i < PMCTRL_S5_GPIO_GROUP_NUM; i++) {
		status = mmio_read_32(PMCTRL_S5_GPIO_REG(i, CDNS_GPIO_IRQ_STATUS)) &
			~mmio_read_32(PMCTRL_S5_GPIO_REG(i, CDNS_GPIO_IRQ_MASK));

		mmio_write_32(PMCTRL_S5_GPIO_REG(i, CDNS_GPIO_IRQ_DIS), status);
	}
}

static void sky1_boot_cpu_save(unsigned int core)
{
	boot_cpus |= 1 << core;
}

static void sky1_core_pwrctrl(unsigned int core, unsigned int val)
{
	mmio_write_32(SKY1_PPU_CORE_BASE(core) + PPU_PWPR, val);
}

static void sky1_cluster_pwrctrl(unsigned int val)
{
	mmio_write_32(SKY1_PPU_CLUSTER_BASE, val);
}

static int sky1_cluster_is_powered_on(unsigned int cluster)
{
	int ret;
	unsigned int val = mmio_read_32(SKY1_PPU_CLUSTER_BASE + PPU_PWSR);

	ret = val & PPU_PWSR_MASK;

	return !!ret;
}

static void sky1_core_pd_enable_ctrl(bool on)
{
	uint32_t reg;

	reg = read_cpupwrctrl();
	if (on) {
		reg &= ~CORE_PWRDN_EN;
		write_cpupwrctrl(reg);
	} else {
		reg |= CORE_PWRDN_EN;
		write_cpupwrctrl(reg);
	}
}

/* 0 : core power down, 1: core power on */
static void sky1_pwr_core_dynamic_ctrl(unsigned int core, bool on)
{
	if (on) {
		sky1_core_pwrctrl(core, CORE_PWR_DYNAMIC_ON);
	} else {
		/* disable core power down dynamic mode */
		sky1_core_pwrctrl(core, CORE_PWR_DYNAMIC_OFF);
		sky1_core_pd_enable_ctrl(PWR_OFF);
	}
}

static void sky1_pwr_cluster_dynamic_ctrl(bool on, int target_state)
{
	uint32_t reg;
	int pwr_status;

	reg = read_clusterpwrdn();
	if (on) {
		reg &= ~(DSU_CLUSTER_PWR_MASK | DSU_CLUSTER_PWR_OFF);
		write_clusterpwrdn(reg);
		sky1_cluster_pwrctrl(CLUSTER_PWR_DYNAMIC_ON);
	} else {
		if (target_state == CIX_LOCAL_STATE_RET) {
			reg &= ~DSU_CLUSTER_PWR_RET;
			reg |= DSU_CLUSTER_PWR_RET;
			pwr_status = CLUSTER_PWR_DYNAMIC_MEM_RET;
		} else {
			reg &= ~DSU_CLUSTER_PWR_RET;
			reg &= ~DSU_CLUSTER_PWR_MASK;
			reg |= DSU_CLUSTER_PWR_OFF;
			pwr_status = CLUSTER_PWR_DYNAMIC_OFF;
		}
		write_clusterpwrdn(reg);
		sky1_cluster_pwrctrl(pwr_status);
	}
}

/*******************************************************************************
 * Sky1 Handler called when the CPU power domain is about to enter standby.
 ******************************************************************************/
void sky1_pwr_domain_standby(plat_local_state_t cpu_state)
{
	unsigned int scr;

	assert(cpu_state == CIX_LOCAL_STATE_RET);

	scr = read_scr_el3();
	/*
	 * Enable the Non secure interrupt to wake the CPU.
	 * In GICv3 affinity routing mode, the non secure group1 interrupts use
	 * the PhysicalFIQ at EL3 whereas in GICv2, it uses the PhysicalIRQ.
	 * Enabling both the bits works for both GICv2 mode and GICv3 affinity
	 * routing mode.
	 */
	write_scr_el3(scr | SCR_IRQ_BIT | SCR_FIQ_BIT);
	isb();
	dsb();
	wfi();

	/*
	 * Restore SCR to the original value, synchronisation of scr_el3 is
	 * done by eret while el3_exit to save some execution cycles.
	 */
	write_scr_el3(scr);
}

/*******************************************************************************
 * Sky1 Handler called when a power domain is about to be turned on. The
 * level and mpidr determine the affinity instance.
 ******************************************************************************/
int sky1_pwr_domain_on(u_register_t mpidr)
{
	unsigned int core = MPIDR_AFFLVL1_VAL(mpidr);
	unsigned int cluster = MPIDR_AFFLVL2_VAL(mpidr);
	int cluster_stat = sky1_cluster_is_powered_on(cluster);
	uint32_t cpu_possible, is_impossible;

	cpu_possible = mmio_read_32(SW_USED_REG2);
	is_impossible = (cpu_possible >> core) & 0x1;
	if (is_impossible) {
		NOTICE("core=%d is fail return \n", core);
		return PSCI_E_INTERN_FAIL;
	}

	mmio_write_32(CPU_RVB_ADDR(core), plat_sec_entrypoint);

	if (!cluster_stat)
		sky1_pwr_cluster_dynamic_ctrl(PWR_ON, CIX_LOCAL_STATE_RUN);

	sky1_pwr_core_dynamic_ctrl(core, PWR_ON);
	return PSCI_E_SUCCESS;
}

/*******************************************************************************
 * Sky1 Handler called when a power domain is about to be turned off. The
 * target_state encodes the power state that each level should transition to.
 ******************************************************************************/
void sky1_pwr_domain_off(const psci_power_state_t *target_state)
{
	unsigned long mpidr = read_mpidr();
	unsigned int core = MPIDR_AFFLVL1_VAL(mpidr);

	assert(CORE_PWR_STATE(target_state) == CIX_LOCAL_STATE_OFF);
	/* Prevent interrupts from spuriously waking up this cpu */
	plat_cix_gic_cpuif_disable();

	/* Turn redistributor off */
	plat_cix_gic_redistif_off();
	sky1_pwr_cluster_dynamic_ctrl(PWR_OFF, CIX_LOCAL_STATE_OFF);

	sky1_pwr_core_dynamic_ctrl(core, PWR_OFF);
}

/*******************************************************************************
 * Sky1 Handler called when a power domain is about to be suspended. The
 * target_state encodes the power state that each level should transition to.
 ******************************************************************************/
void sky1_pwr_domain_suspend(const psci_power_state_t *target_state)
{
	unsigned long mpidr = read_mpidr();
	unsigned int core = MPIDR_AFFLVL1_VAL(mpidr);
	plat_local_state_t state = SYSTEM_PWR_STATE(target_state);

#ifndef CONFIG_CIX_STR_SE
	void *scmi_handle = sky1_get_scmi_handle();
#endif

	/*
	 * sky1 currently supports retention only at cpu level. Just return
	 * as nothing is to be done for retention.
	 */
	if (CORE_PWR_STATE(target_state) == CIX_LOCAL_STATE_RET)
		return;

	assert(CORE_PWR_STATE(target_state) == CIX_LOCAL_STATE_OFF);

	/*reset vector address*/
	mmio_write_32(CPU_RVB_ADDR(core), plat_sec_entrypoint);

	/* Prevent interrupts from spuriously waking up this cpu */
	plat_cix_gic_cpuif_disable();

	/* Cluster is to be turned off, so disable coherency */
	if (CLUSTER_PWR_STATE(target_state) == CIX_LOCAL_STATE_OFF &&
			SYSTEM_PWR_STATE(target_state) != CIX_LOCAL_STATE_OFF) {
	#if HW_ASSISTED_COHERENCY
		sky1_pwr_cluster_dynamic_ctrl(PWR_OFF, CIX_LOCAL_STATE_RET);
	#endif
	}

	/* Perform system domain state saving if issuing system suspend */
	if (SYSTEM_PWR_STATE(target_state) == CIX_LOCAL_STATE_OFF) {
		pciehub_power_ctrl(PWR_OFF);
		sky1_pwr_cluster_dynamic_ctrl(PWR_OFF, CIX_LOCAL_STATE_OFF);
		plat_cix_gic_save(&sky1_gic_ctx);

		/* Turn redistributor off */
		plat_cix_gic_redistif_off();

		/* Enable s5 gpio wakeup */
		sky1_set_wakeup_enable(S5_GPIO_WKUP_EN, S5_GPIO_WKUP);
		sky1_gpio_wakeup_set();

		/*
		 * Unregister console now so that it is not registered for a second
		 * time during resume.
		 */
		cix_console_runtime_end();
		mmio_write_32(SW_USED_REG7, plat_sec_entrypoint);

		/*Enter STR, Send Mailbox to csu_se and csu_pm*/
		mbox_set_suspend(&state);

	/* Notify csu_pm with boot core when enter str */
        scmi_core_pwr_state_notify(scmi_handle, STR_BOOT_CORE_NOTIFY, core);
	#ifndef CONFIG_CIX_STR_SE
		scmi_sys_pwr_state_set(scmi_handle, SCMI_SYS_PWR_FORCEFUL_REQ, SCMI_SYS_PWR_SUSPEND);
	#endif
		suspend_value = sky1_syscounter_read();
	}

	sky1_pwr_core_dynamic_ctrl(core, PWR_OFF);
	coreidle_start[core] = sky1_syscounter_read();
}



/*******************************************************************************
 * Sky1 Handler called when a power level has just been powered on after
 * being turned off earlier. The target_state encodes the low power state that
 * each level has woken up from. This handler would never be invoked with
 * the system power domain uninitialized as either the primary would have taken
 * care of it as part of cold boot or the first core awakened from system
 * suspend would have already initialized it.
 ******************************************************************************/
void sky1_pwr_domain_on_finish(const psci_power_state_t *target_state)
{
	unsigned long mpidr = read_mpidr();
	unsigned int core = MPIDR_AFFLVL1_VAL(mpidr);

	/* Assert that the system power domain need not be initialized */
	assert(SYSTEM_PWR_STATE(target_state) == CIX_LOCAL_STATE_RUN);
	assert(CORE_PWR_STATE(target_state) == CIX_LOCAL_STATE_OFF);

	sky1_core_pd_enable_ctrl(PWR_ON);
	/* Program the gic per-cpu distributor or re-distributor interface */
	plat_cix_gic_pcpu_init();

	/* Enable the gic cpu interface */
	plat_cix_gic_cpuif_enable();

	/* save boot cpus when cpu online */
	sky1_boot_cpu_save(core);
}

/*******************************************************************************
 * Sky1 Handler called when a power domain has just been powered on after
 * having been suspended earlier. The target_state encodes the low power state
 * that each level has woken up from.
 ******************************************************************************/
void sky1_pwr_domain_suspend_finish(const psci_power_state_t *target_state)
{
	unsigned long mpidr = read_mpidr();
	unsigned int core = MPIDR_AFFLVL1_VAL(mpidr);
	unsigned int val = 0, delta_time;
	uint64_t cpuidle_end = 0;

	/*Calculate the cpuidle off time*/
	cpuidle_end = sky1_syscounter_read();
	val = mmio_read_32(AP_OFF_RISD_CNT_ADDR(core));
	delta_time = cpuidle_end - coreidle_start[core];
	delta_time += val;
	mmio_write_32(AP_OFF_RISD_CNT_ADDR(core), delta_time);

	/* Return as nothing is to be done on waking up from retention. */
	if (CORE_PWR_STATE(target_state) == CIX_LOCAL_STATE_RET)
		return;

	assert(CORE_PWR_STATE(target_state) == CIX_LOCAL_STATE_OFF);
	/* Perform system domain restore if woken up from system suspend */
	if (SYSTEM_PWR_STATE(target_state) == CIX_LOCAL_STATE_OFF) {
		/* Initialize the console */
		cix_console_runtime_init();
		/* Assert system power domain is available on the platform */
		plat_cix_gic_resume(&sky1_gic_ctx);
		pciehub_power_ctrl(PWR_ON);
	#ifdef CIX_GENERIC_TIMER_BASE
		cix_configure_sys_timer();
	#endif
		resume_value = sky1_syscounter_read();
		update_suspend_syscounter();

		/* mask s5 gpio wakeup source */
		sky1_set_wakeup_enable(S5_GPIO_WKUP_DIS, S5_GPIO_WKUP);
		sky1_wakeup_source_mask();
		/* tfa to se mailbox resume */
		plat_cix_mbox_resume();
		cix_sky1_qos_setting_init();
		cix_sky1_qos_setting_dump();
		cix_sky1_nsaid_setting_init();
#ifdef RAS_EXTENSION
		sky1_ras_setup_resume();
#endif
		NOTICE("%s:ready to exit from suspend!\n", __func__);
	}

	if (CLUSTER_PWR_STATE(target_state) == CIX_LOCAL_STATE_OFF ||
			CLUSTER_PWR_STATE(target_state) == CIX_LOCAL_STATE_RET) {
#if HW_ASSISTED_COHERENCY
		uint32_t reg;

		reg = read_clusterpwrdn();
		reg &= ~DSU_CLUSTER_PWR_MASK;
		reg |= DSU_CLUSTER_PWR_ON;
		write_clusterpwrdn(reg);
#ifdef SKY1_CPU_RAS_SUPPORT
		sky1_ras_dsu_cache_resume();
#endif
#endif
	}

	sky1_core_pd_enable_ctrl(PWR_ON);
	/* Enable the gic cpu interface */
	plat_cix_gic_cpuif_enable();
}

/*******************************************************************************
 * Sky1 Handlers to shutdown/reboot the system
 ******************************************************************************/
void __dead2 sky1_system_off(void)
{
	/*
	 * Disable GIC CPU interface to prevent pending interrupt from waking
	 * up the AP from WFI.
	 */
	unsigned long mpidr = read_mpidr();
	unsigned int core = MPIDR_AFFLVL1_VAL(mpidr);
	void *scmi_handle = sky1_get_scmi_handle();

	/*reset vector address*/
        mmio_write_32(CPU_RVB_ADDR(core), CPU_RESET_VECTOR_DEF_ADDR);

#if defined (CIX_ARCH_SMP)
	uint32_t boot_core_id, cpu_possible;
	cpu_possible = mmio_read_32(SW_USED_REG2);
	boot_core_id = (cpu_possible >> BOOT_CORE_OFFSET) & BOOT_CORE_MASK;
	boot_cpus |= 1 << boot_core_id;
	for (int i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if ((boot_cpus >> i) & 0x1) {
			gicv3_cpuif_disable(i);
			gicv3_rdistif_off(i);
		}
	}
#else
	plat_cix_gic_cpuif_disable();
	/* Turn redistributor off */
	plat_cix_gic_redistif_off();
#endif
	/* Enable s5 gpio wakeup */
	sky1_set_wakeup_enable(S5_GPIO_WKUP_EN, S5_GPIO_WKUP);
	sky1_gpio_wakeup_set();

	#if HW_ASSISTED_COHERENCY
		sky1_pwr_cluster_dynamic_ctrl(PWR_OFF, CIX_LOCAL_STATE_OFF);
	#endif
#if CIX_DST_SUPPORT
	drv_ci700_slc_flush();
#endif
#ifdef RAM_LOG_SUPPORT
	rlog_flush_data();
#endif
	/* set poweroff */
	scmi_sys_pwr_state_set(scmi_handle, SCMI_SYS_PWR_FORCEFUL_REQ, SCMI_SYS_PWR_SHUTDOWN);
	sky1_pwr_core_dynamic_ctrl(core, PWR_OFF);
	wfi();
	ERROR("system off failed.\n");
	panic();
}

void __dead2 sky1_system_reset(void)
{
#ifdef SKY1_PM_I2C_HANG_PATCH
	void *scmi_handle = sky1_get_scmi_handle();
#endif

	plat_cix_gic_cpuif_disable();
#if CIX_DST_SUPPORT
	drv_ci700_slc_flush();
#endif
#ifdef RAM_LOG_SUPPORT
	rlog_flush_data();
#endif
	/* trigger warm reset */
#ifdef SKY1_PM_I2C_HANG_PATCH
	scmi_sys_pwr_state_set(scmi_handle, SCMI_SYS_PWR_FORCEFUL_REQ, SCMI_SYS_PWR_WARM_RESET);
#else
	mmio_write_32(PMCTRL_S5_BASE + WARM_RST_OFFSET, 0x1);
#endif
	wfi();
	ERROR("system reset failed.\n");
	panic();
}

static int sky1_validate_power_state(unsigned int power_state,
		psci_power_state_t *req_state)
{
	unsigned int pstate = psci_get_pstate_type(power_state);
	unsigned int pwr_lvl = psci_get_pstate_pwrlvl(power_state);
	unsigned int i;

	assert(req_state != NULL);

	if (pwr_lvl > PLAT_MAX_PWR_LVL)
		return PSCI_E_INVALID_PARAMS;

	/* Sanity check the requested state */
	if (pstate == PSTATE_TYPE_STANDBY) {
		/*
		 * It's possible to enter standby only on power level 0
		 * Ignore any other power level.
		 */
		if (pwr_lvl != MPIDR_AFFLVL0)
			return PSCI_E_INVALID_PARAMS;

		req_state->pwr_domain_state[MPIDR_AFFLVL0] =
					CIX_LOCAL_STATE_RET;
	} else {
		for (i = MPIDR_AFFLVL0; i <= pwr_lvl; i++)
			req_state->pwr_domain_state[i] =
					CIX_LOCAL_STATE_OFF;
	}

	/*
	 * We expect the 'state id' to be zero.
	 */
	if (psci_get_pstate_id(power_state) != 0U)
		return PSCI_E_INVALID_PARAMS;

	/*
	 * Ensure that we don't overrun the pwr_domain_state array in the case
	 * where the platform supported max power level is less than the system
	 * power level
	 */

#if (PLAT_MAX_PWR_LVL == CSS_SYSTEM_PWR_DMN_LVL)

	/*
	 * Ensure that the system power domain level is never suspended
	 * via PSCI CPU SUSPEND API. Currently system suspend is only
	 * supported via PSCI SYSTEM SUSPEND API.
	 */

	req_state->pwr_domain_state[CSS_SYSTEM_PWR_DMN_LVL] =
							CIX_LOCAL_STATE_RUN;
#endif

	return PSCI_E_SUCCESS;
}

int sky1_validate_ns_entrypoint(uintptr_t entrypoint)
{
	/*
	 * Check if the non secure entrypoint lies within the non
	 * secure DRAM.
	 */
	uint32_t dram1_size;
	dram1_size = mmio_read_32(MEM_DDR_SIZE_ADDR);
	if (dram1_size)
		sky1_dram1_size = (uintptr_t)dram1_size << 20;
	else
		sky1_dram1_size = SKY1_DRAM1_SIZE;

	if ((entrypoint >= SKY1_NS_DRAM1_BASE) && (entrypoint <
			(SKY1_NS_DRAM1_BASE + sky1_dram1_size - SKY1_TZC_DRAM1_SIZE))) {
		return PSCI_E_SUCCESS;
	}
#ifdef __aarch64__
	if ((entrypoint >= SKY1_DRAM2_BASE) && (entrypoint <
			(SKY1_DRAM2_BASE + SKY1_DRAM2_SIZE))) {
		return PSCI_E_SUCCESS;
	}
#endif

	return PSCI_E_INVALID_ADDRESS;
}

/*******************************************************************************
 * Handler called to return the 'req_state' for system suspend.
 ******************************************************************************/
void sky1_get_sys_suspend_power_state(psci_power_state_t *req_state)
{
	unsigned int i;

	for (i = MPIDR_AFFLVL0; i <= PLAT_MAX_PWR_LVL; i++)
		req_state->pwr_domain_state[i] = CIX_LOCAL_STATE_OFF;
}

/*
 * The system power domain suspend is only supported only via
 * PSCI SYSTEM_SUSPEND API. PSCI CPU_SUSPEND request to system power domain
 * will be downgraded to the lower level.
 */

int sky1_system_reset2(int is_vendor, int reset_type, u_register_t cookie)
{
#if CIX_DST_SUPPORT
	drv_ci700_slc_flush();
#endif
#ifdef RAM_LOG_SUPPORT
	rlog_flush_data();
#endif
	/*TODO*/
	return 0;
}

plat_psci_ops_t plat_cix_psci_pm_ops = {
	.cpu_standby		= sky1_pwr_domain_standby,
	.pwr_domain_on		= sky1_pwr_domain_on,
	.pwr_domain_off		= sky1_pwr_domain_off,
	.pwr_domain_suspend	= sky1_pwr_domain_suspend,
	.pwr_domain_on_finish	= sky1_pwr_domain_on_finish,
	.pwr_domain_suspend_finish	= sky1_pwr_domain_suspend_finish,
	.system_off		= sky1_system_off,
	.system_reset		= sky1_system_reset,
	.validate_power_state	= sky1_validate_power_state,
	.validate_ns_entrypoint = sky1_validate_ns_entrypoint,
	.get_sys_suspend_power_state = sky1_get_sys_suspend_power_state,
	.system_reset2		= sky1_system_reset2,
};

/*******************************************************************************
 * The CIX Standard platform definition of platform porting API
 * `plat_setup_psci_ops`.
 ******************************************************************************/
int __init plat_setup_psci_ops(uintptr_t sec_entrypoint,
				const plat_psci_ops_t **psci_ops)
{
	*psci_ops = &plat_cix_psci_pm_ops;
	plat_sec_entrypoint = sec_entrypoint;

	return 0;
}
