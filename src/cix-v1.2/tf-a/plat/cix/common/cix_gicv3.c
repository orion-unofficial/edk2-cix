/*
 * Copyright (c) 2015-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <platform_def.h>

#include <common/interrupt_props.h>
#include <drivers/arm/gicv3.h>
#include <lib/utils.h>
#include <plat_cix.h>
#include <plat/common/platform.h>
#include "../drivers/arm/gic/v3/gicv3_private.h"

/******************************************************************************
 * The following functions are defined as weak to allow a platform to override
 * the way the GICv3 driver is initialised and used.
 *****************************************************************************/
#pragma weak plat_cix_gic_driver_init
#pragma weak plat_cix_gic_init
#pragma weak plat_cix_gic_cpuif_enable
#pragma weak plat_cix_gic_cpuif_disable
#pragma weak plat_cix_gic_pcpu_init
#pragma weak plat_cix_gic_redistif_on
#pragma weak plat_cix_gic_redistif_off

/* The GICv3 driver only needs to be initialized in EL3 */
static uintptr_t rdistif_base_addrs[PLATFORM_CORE_COUNT];

/* Default GICR base address to be used for GICR probe. */
static const uintptr_t gicr_base_addrs[2] = {
	PLAT_SKY1_GICR_BASE,	/* GICR Base address of the primary CPU */
	0U			/* Zero Termination */
};

/* List of zero terminated GICR frame addresses which CPUs will probe */
static const uintptr_t *gicr_frames = gicr_base_addrs;

/* register SGI8 as group0 interrupt and register SGI9-15 as s-group1 interrupt*/
static const interrupt_prop_t arm_interrupt_props[] = {
	PLAT_ARM_G1S_IRQ_PROPS(INTR_GROUP1S),
#if SDEI_SUPPORT
	PLAT_ARM_G0_IRQ_PROPS(INTR_GROUP0)
#endif
};


/*
 * MPIDR hashing function for translating MPIDRs read from GICR_TYPER register
 * to core position.
 *
 * Calculating core position is dependent on MPIDR_EL1.MT bit. However, affinity
 * values read from GICR_TYPER don't have an MT field. To reuse the same
 * translation used for CPUs, we insert MT bit read from the PE's MPIDR into
 * that read from GICR_TYPER.
 *
 * Assumptions:
 *
 *   - All CPUs implemented in the system have MPIDR_EL1.MT bit set;
 *   - No CPUs implemented in the system use affinity level 3.
 */
static unsigned int arm_gicv3_mpidr_hash(u_register_t mpidr)
{
	mpidr |= (read_mpidr_el1() & MPIDR_MT_MASK);
	return plat_cix_calc_core_pos(mpidr);
}

static const gicv3_driver_data_t arm_gic_data __unused = {
	.gicd_base = PLAT_SKY1_GICD_BASE,
	.gicr_base = 0,
	.interrupt_props = arm_interrupt_props,
	.interrupt_props_num = ARRAY_SIZE(arm_interrupt_props),
	.rdistif_num = PLATFORM_CORE_COUNT,
	.rdistif_base_addrs = rdistif_base_addrs,
	.mpidr_to_core_pos = arm_gicv3_mpidr_hash
};

/*
 * By default, gicr_frames will be pointing to gicr_base_addrs. If
 * the platform supports a non-contiguous GICR frames (GICR frames located
 * at uneven offset), plat_cix_override_gicr_frames function can be used by
 * such platform to override the gicr_frames.
 */
void plat_cix_override_gicr_frames(const uintptr_t *plat_gicr_frames)
{
	assert(plat_gicr_frames != NULL);
	gicr_frames = plat_gicr_frames;
}

unsigned int plat_get_ap_bootcore_index(void)
{

#ifndef CIX_BOARD_EVB
	/*read debug-register*/
	unsigned int cluster = (mmio_read_32(PLAT_IP_INFO) & PLAT_IP_INFO_CLUSTER_MSK) >> PLAT_IP_INFO_CLUSTER_POS;
	unsigned int boot_core;

	switch (cluster) {
		/*hunter0 only */
		case 1:
			boot_core = 4;
			break;
		/*hayes0 only */
		case 2:
			boot_core = 0;
			break;
		/*hunter0 + hayes0*/
		case 3:
			boot_core = 4;
			break;
		/*hayes3 only */
		case 4:
			boot_core = 3;
			break;
		/*12 cores*/
		case 5:
			boot_core = 4;
			break;
		/*hunter4 only */
		case 6:
			boot_core = 8;
			break;

		/*hayes0  + hayes1*/
		case 7:
			boot_core = 0;
			break;

		default:
			boot_core = 4;
			break;
	}

		return boot_core;
#else
	return plat_my_core_pos();
#endif /* CIX_BOARD_EVB */
}

void plat_cix_gicr_gicd_config(void)
{
	unsigned int boot_core_index = plat_get_ap_bootcore_index();

	INFO("boot_core_index %u \n",boot_core_index);

#if defined (CIX_ARCH_SMP)
	int gicr_index = 0;
	/* cpu possible: 1: cpu is ok, 0: cpu is masked */
	uint32_t cpu_possible = mmio_read_32(SW_USED_REG2);
	uint32_t bits = (1 << PLATFORM_CORE_COUNT) -1;
	INFO("cpu mask 0x%x\n", cpu_possible);
	cpu_possible = cpu_possible & bits;

	gicd_write_rdoffr(arm_gic_data.gicd_base, cpu_possible);

	for (int cpu_pos = 0; cpu_pos < PLATFORM_CORE_COUNT; cpu_pos++) {
		int is_possible = (cpu_possible >> cpu_pos) & 1;
		if (!is_possible) {
			gicr_write_mpidr(PLAT_SKY1_GICR_BASE + GICR_AFF(gicr_index), cpu_pos * 0x100);
			gicr_index ++;
		}
	}
#else
	gicd_write_rdoffr(arm_gic_data.gicd_base, 0xfff & (~(1 << boot_core_index)) );
	gicr_write_mpidr(PLAT_SKY1_GICR_BASE, 0x100 * boot_core_index);
#endif
}
void __init plat_cix_gic_driver_init(void)
{
	/*
	 * The GICv3 driver is initialized in EL3 and does not need
	 * to be initialized again in SEL1. This is because the S-EL1
	 * can use GIC system registers to manage interrupts and does
	 * not need GIC interface base addresses to be configured.
	 */
#if (!defined(__aarch64__) && defined(IMAGE_BL32)) || \
	(defined(__aarch64__) && defined(IMAGE_BL31))
	plat_cix_gicr_gicd_config();
	gicv3_driver_init(&arm_gic_data);
#endif

	if (gicv3_rdistif_probe(gicr_base_addrs[0]) == -1) {
		ERROR("No GICR base frame found for Primary CPU\n");
		panic();
	}
}

/******************************************************************************
 * ARM common helper to initialize the GIC. Only invoked by BL31
 *****************************************************************************/
void __init plat_cix_gic_init(void)
{
	gicv3_distif_init();
	gicv3_rdistif_init(plat_my_core_pos());
	gicv3_cpuif_enable(plat_my_core_pos());
}

/******************************************************************************
 * ARM common helper to enable the GIC CPU interface
 *****************************************************************************/
void plat_cix_gic_cpuif_enable(void)
{
	gicv3_cpuif_enable(plat_my_core_pos());
}

/******************************************************************************
 * ARM common helper to disable the GIC CPU interface
 *****************************************************************************/
void plat_cix_gic_cpuif_disable(void)
{
	gicv3_cpuif_disable(plat_my_core_pos());
}

/******************************************************************************
 * ARM common helper function to iterate over all GICR frames and discover the
 * corresponding per-cpu redistributor frame as well as initialize the
 * corresponding interface in GICv3.
 *****************************************************************************/
void plat_cix_gic_pcpu_init(void)
{
	int result;
	const uintptr_t *plat_gicr_frames = gicr_frames;

	do {
		result = gicv3_rdistif_probe(*plat_gicr_frames);

		/* If the probe is successful, no need to proceed further */
		if (result == 0)
			break;

		plat_gicr_frames++;
	} while (*plat_gicr_frames != 0U);

	if (result == -1) {
		ERROR("No GICR base frame found for CPU 0x%lx\n", read_mpidr());
		panic();
	}
	gicv3_rdistif_init(plat_my_core_pos());
}

/******************************************************************************
 * ARM common helpers to power GIC redistributor interface
 *****************************************************************************/
void plat_cix_gic_redistif_on(void)
{
	gicv3_rdistif_on(plat_my_core_pos());
}

void plat_cix_gic_redistif_off(void)
{
	gicv3_rdistif_off(plat_my_core_pos());
}

extern unsigned int boot_cpus;
/******************************************************************************
 * ARM common helper to save & restore the GICv3 on resume from system suspend
 *****************************************************************************/
void plat_cix_gic_save(struct cix_plat_gic_ctx *ctx)
{
#if defined (CIX_ARCH_SMP)
	boot_cpus |= 1 << plat_my_core_pos();
	for (int i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if ((boot_cpus >> i) & 0x1) {
		gicv3_rdistif_save(i, &ctx->rdist_ctx[i]);
		}
	}
#else
	gicv3_rdistif_save(plat_my_core_pos(), &ctx->rdist_ctx[0]);
#endif
	gicv3_distif_save(&ctx->dist_ctx);
}

void plat_cix_gic_resume(struct cix_plat_gic_ctx *ctx)
{
	/*gicv3 rdoffr and mpidr config*/
	plat_cix_gicr_gicd_config();

	/* restore the gic rdist/dist context */
	gicv3_distif_init_restore(&ctx->dist_ctx);
#if defined (CIX_ARCH_SMP)
	for (int i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if ((boot_cpus >> i) & 0x1) {
			gicv3_rdistif_init_restore(i, &ctx->rdist_ctx[i]);
		}
	}
#else
	gicv3_rdistif_init_restore(plat_my_core_pos(), &ctx->rdist_ctx[0]);
#endif
}
