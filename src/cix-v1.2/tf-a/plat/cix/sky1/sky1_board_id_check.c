/*
 * Copyright (c) 2023, CIX Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <arch_helpers.h>
#include <common/debug.h>
#include <common/runtime_svc.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>
#include <lib/psci/psci.h>
#include <lib/spinlock.h>
#include <platform_def.h>
#include <plat/common/platform.h>

#define PMIC_ID_ADDRESS 0x83E01480
/*PMIC ID Informationi
 *PMIC_ID = 1, Boards include merak/mizar/merak dap
 *PMIC_ID = 2, Boards include phecda slt/phecda/phecda dap/makalu...
 *PMIC_ID = 3, Board include megrez
 *PMIC_ID = 4, Board include niobox
 *PMIC_ID = 5, Board include cloudbook
 */
typedef enum {
        MERAK_PMIC_ID = 0x1,
        PHECDA_PMIC_ID,
        MEGREZ_PMIC_ID,
        NIBOX_PMIC_ID,
        CLOUDBOOK_PMIC_ID,
} sky1_pmic_id;

static bool check_board_dvfs_support(void)
{
	unsigned int pmic_id = 0;
	bool supported;

	pmic_id = mmio_read_32(PMIC_ID_ADDRESS) & 0xff;
	INFO("%s,%d,pmicid is %u!\n", __func__, __LINE__, pmic_id);

	switch(pmic_id) {
	case MERAK_PMIC_ID:
		supported = false;
		break;
	case PHECDA_PMIC_ID:
	case MEGREZ_PMIC_ID:
	case NIBOX_PMIC_ID:
	case CLOUDBOOK_PMIC_ID:
		supported = true;
		break;
	default:
		supported = true;
		break;
	}

	return supported;
}

int cix_board_id_check(void)
{
	bool supported;

	supported = check_board_dvfs_support();

	if (supported)
		return 0;
	else
		return -1;
}
