/*
 * Copyright (c) 2014-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>
#include <common/debug.h>
#include <drivers/arm/tzc400.h>
#include <plat_cix.h>

/* Weak definitions may be overridden in specific ARM standard platform */
#pragma weak plat_cix_security_setup

/*******************************************************************************
 * Initialize the TrustZone Controller for ARM standard platforms.
 * When booting an EL3 payload, this is simplified: we configure region 0 with
 * secure access only and do not enable any other region.
 ******************************************************************************/
void cix_tzc400_setup(uintptr_t tzc_base,
			const arm_tzc_regions_info_t *tzc_regions)
{
	const arm_tzc_regions_info_t *p;
	unsigned int index  = 0;

	INFO("Configuring TrustZone Controller\n");

	tzc400_init(tzc_base);

	/* Disable filters. */
	tzc400_disable_filters();

	if (tzc_regions == NULL){
		ERROR("%s: !!! no region configed !!!\n",__func__);
		return;
	}

	/* Rest Regions set according to tzc_regions array */
	p = tzc_regions;
	for (; p->base != CIX_REGION_NULL; p++) {
		tzc400_configure_region(PLAT_CIX_TZC_FILTERS, p->region_index,
			p->base, p->end, p->sec_attr, p->nsaid_permissions);
		index++;
	}

	INFO("Total %u regions set.\n", index);

	/*
	 * Raise an exception if a NS device tries to access secure memory
	 * TODO: Add interrupt handling support.
	 */
	tzc400_set_action(TZC_ACTION_INT);

	/* Enable filters. */
	tzc400_enable_filters();
}
