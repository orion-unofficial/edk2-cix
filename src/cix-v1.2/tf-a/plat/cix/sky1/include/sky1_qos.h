/*
 * Copyright (c) 2018-2019, CIX Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SKY1_QOS_H
#define SKY1_QOS_H

#define SMMU_RESET_BEFORE       0x0
#define SMMU_RESET_AFTER        0x1

uint32_t sky1_smmu_gop_handler(uint64_t arg0, uint64_t arg1,
			 uint64_t arg2, uint64_t arg3,
			 uint64_t arg4, uint64_t arg5);

#endif /*SKY1_PDC_H */
