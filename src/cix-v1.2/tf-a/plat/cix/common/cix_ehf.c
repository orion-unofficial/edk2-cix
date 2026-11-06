/*
 * Copyright (c) 2023 Cix Technology Group Co., Ltd. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bl31/ehf.h>
#include <platform_def.h>

ehf_pri_desc_t cix_exceptions[] = {
#if RAS_EXTENSION
	/* RAS Priority */
	EHF_PRI_DESC(PLAT_PRI_BITS, PLAT_RAS_PRI),
#endif
#if SDEI_SUPPORT
	/* Critical priority SDEI */
	EHF_PRI_DESC(PLAT_PRI_BITS, PLAT_SDEI_CRITICAL_PRI),

	/* Normal priority SDEI */
	EHF_PRI_DESC(PLAT_PRI_BITS, PLAT_SDEI_NORMAL_PRI),
#endif
};

/* Plug in ARM exceptions to Exception Handling Framework. */
EHF_REGISTER_PRIORITIES(cix_exceptions, ARRAY_SIZE(cix_exceptions), PLAT_PRI_BITS);
