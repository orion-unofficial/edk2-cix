/*
 * Copyright (c) 2015-2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <platform_def.h>
#include <plat_cix.h>

#include <arch.h>
#include <arch_helpers.h>
#include <common/debug.h>
#include <common/romlib.h>
#include <lib/mmio.h>
#include <lib/smccc.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <services/arm_arch_svc.h>
#include <plat/common/platform.h>

/* Weak definitions may be overridden in specific ARM standard platform */
#pragma weak plat_get_ns_image_entrypoint

/* Conditionally provide a weak definition of plat_get_syscnt_freq2 to avoid
 * conflicts with the definition in plat/common. */
#pragma weak plat_get_syscnt_freq2

uintptr_t plat_get_ns_image_entrypoint(void)
{
	return BL33_BASE;
}

/*******************************************************************************
 * Gets SPSR for BL32 entry
 ******************************************************************************/
uint32_t cix_get_spsr_for_bl32_entry(void)
{
	/*
	 * The Secure Payload Dispatcher service is responsible for
	 * setting the SPSR prior to entry into the BL32 image.
	 */
	return 0;
}

/*******************************************************************************
 * Gets SPSR for BL33 entry
 ******************************************************************************/
uint32_t cix_get_spsr_for_bl33_entry(void)
{
	unsigned int mode;
	uint32_t spsr;

	/* Figure out what mode we enter the non-secure world in */
	mode = (el_implemented(2) != EL_IMPL_NONE) ? MODE_EL2 : MODE_EL1;

	/*
	 * TODO: Consider the possibility of specifying the SPSR in
	 * the FIP ToC and allowing the platform to have a say as
	 * well.
	 */
	spsr = SPSR_64((uint64_t)mode, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
	return spsr;
}

/*******************************************************************************
 * Configures access to the system counter timer module.
 ******************************************************************************/
#ifdef CIX_GENERIC_TIMER_BASE
void cix_configure_sys_timer(void)
{
	unsigned int reg_val;

	/* Read the frequency of the system counter */
	unsigned int freq_val = plat_get_syscnt_freq2();

	reg_val = (1U << CNTACR_RPCT_SHIFT) | (1U << CNTACR_RVCT_SHIFT);
	reg_val |= (1U << CNTACR_RFRQ_SHIFT) | (1U << CNTACR_RVOFF_SHIFT);
	reg_val |= (1U << CNTACR_RWVT_SHIFT) | (1U << CNTACR_RWPT_SHIFT);
	mmio_write_32(CIX_GENERIC_TIMER_BASE + CNTACR_BASE(0), reg_val);

	reg_val = (1U << CNTNSAR_NS_SHIFT(0));
	mmio_write_32(CIX_GENERIC_TIMER_BASE + CNTNSAR, reg_val);

	/*
	 * Initialize CNTFRQ register in CNTCTLBase frame. The CNTFRQ
	 * system register initialized during psci_arch_setup() is different
	 * from this and has to be updated independently.
	 */
	mmio_write_32(CIX_GENERIC_TIMER_BASE + CNTCTLBASE_CNTFRQ, freq_val);

}
#endif /* CIX_GENERIC_TIMER_BASE */


#ifdef CIX_SYS_COUNTER_BASE

unsigned int plat_get_syscnt_freq2(void)
{
	unsigned int counter_base_frequency;

	/* Read the frequency from Frequency modes table */
	counter_base_frequency = mmio_read_32(CIX_SYS_COUNTER_BASE + CNTFID_OFF);

	/* The first entry of the frequency modes table must not be 0 */
	if (counter_base_frequency == 0U)
		panic();

	return counter_base_frequency;
}
#endif /* CIX_SYS_COUNTER_BASE */

#ifdef CONFIG_DEBUG_FOOTPRINT_ENABLE
void get_stack_status()
{
	int index = 0;
	int cmp_result = 0;
	uint8_t peak_arry[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	uintptr_t stack_top = plat_get_my_stack();
	uintptr_t stack_base = stack_top - PLATFORM_STACK_SIZE;
	uintptr_t pstack = stack_base;

	for (index = 0; index < PLATFORM_STACK_SIZE / 8 - 1 ; index++) {
		cmp_result = memcmp(peak_arry, (void*)pstack, 8);
		if (0 != cmp_result) {
			break;
		} else {
			pstack = pstack + 8;
		}
	}
	INFO("stack 0x%lx - 0x%lx\n",stack_base, stack_top);
	if ((uintptr_t)pstack == stack_base) {
		ERROR("stack overflow!\n");
		panic();
	} else {
		INFO("Max stack size : 0x%lx\n", stack_top - (uintptr_t)pstack);
	}
}
#endif

