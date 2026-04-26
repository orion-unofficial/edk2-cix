/*
 * Copyright (c) 2015-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>

#include <arch.h>
#include <plat_cix.h>

/*******************************************************************************
 * This function validates an MPIDR by checking whether it falls within the
 * acceptable bounds. An error code (-1) is returned if an incorrect mpidr
 * is passed.
 ******************************************************************************/
int cix_check_mpidr(u_register_t mpidr)
{
	unsigned int cpu_id;
	uint64_t valid_mask;

	valid_mask = ~(MPIDR_AFFLVL_MASK << MPIDR_AFF1_SHIFT);
	cpu_id = (unsigned int) ((mpidr >> MPIDR_AFF1_SHIFT) &
						MPIDR_AFFLVL_MASK);

	mpidr &= MPIDR_AFFINITY_MASK;
	if ((mpidr & valid_mask) != 0U)
		return -1;

	if (cpu_id >= PLATFORM_CORE_COUNT)
		return -1;

	return 0;
}

int plat_core_pos_by_mpidr(u_register_t mpidr)
{
	if (cix_check_mpidr(mpidr) == 0) {
		return plat_cix_calc_core_pos(mpidr);
	}
	return -1;
}
