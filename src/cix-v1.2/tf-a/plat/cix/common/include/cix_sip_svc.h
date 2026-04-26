/*
 * Copyright (c) 2016-2019,2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARM_SIP_SVC_H
#define ARM_SIP_SVC_H

#include <lib/utils_def.h>

/* SMC function IDs for SiP Service queries */

#define ARM_SIP_SVC_CALL_COUNT		U(0x8200ff00)
#define ARM_SIP_SVC_UID			U(0x8200ff01)
/*					U(0x8200ff02) is reserved */
#define ARM_SIP_SVC_VERSION		U(0x8200ff03)

/* PMF_SMC_GET_TIMESTAMP_32		0x82000010 */
/* PMF_SMC_GET_TIMESTAMP_64		0xC2000010 */

/* Function ID for requesting state switch of lower EL */
#define ARM_SIP_SVC_EXE_STATE_SWITCH	U(0x82000020)

/* DEBUGFS_SMC_32			0x82000030U */
/* DEBUGFS_SMC_64			0xC2000030U */

/*
 * Arm Ethos-N NPU SiP SMC function IDs
 * 0xC2000050-0xC200005F
 * 0x82000050-0x8200005F
 */

/*CIX Sip Service Call SCMI*/
#define CIX_SIP_SCMI		U(0xc2000001)

/*Function ID for setting reboot reason*/
#define CIX_SIP_SVC_SET_REBOOT_REASON U(0xc2000002)
#define CIX_SIP_SVC_DSU_HW_CTL        U(0xc2000003)
#define CIX_SIP_SVC_DSU_SET_PD        U(0xc2000004)
#define CIX_SIP_SVC_DSU_GET_PD        U(0xc2000005)
#define CIX_SIP_SVC_DSU_SET_TH        U(0xc2000006)
#define CIX_SIP_SVC_DSU_GET_HITCNT    U(0xc2000007)
#define CIX_SIP_SVC_DSU_GET_MISSCNT   U(0xc2000008)
#define CIX_SIP_SVC_PDC_SET_WAKEUP    U(0xc2000009)
#define CIX_SIP_POWER_DOMAIN_SET      U(0xc200000a)
#define CIX_SIP_NOR_STORAGE           U(0xC200000b)
#define CIX_SIP_SMMU_GOP_CTRL	      U(0xc200000c)
#define CIX_SIP_CPUFREQ_SUPPORT       U(0xc200000d)
#define CIX_SIP_PI_TEST_ENABLE        U(0xc200000e)
#define CIX_SIP_DP_GOP_CTRL           U(0xc200000f)
#define CIX_SIP_SVC_SET_DDRLP         U(0xc2000010)
#define CIX_SIP_CPU_BOOST_TRIGGER     U(0xc2000011)
#define CIX_SIP_SVC_DST_CMD           U(0xc2000012)
/* ARM SiP Service Calls version numbers */
#define ARM_SIP_SVC_VERSION_MAJOR		U(0x0)
#define ARM_SIP_SVC_VERSION_MINOR		U(0x2)


int cix_nor_storage_handler(uint32_t smc_fid, u_register_t x1,
                             u_register_t x2, u_register_t x3,
                             u_register_t x4);

struct plat_sip_svc_ops_t {
	void (*set_reboot_reason)(uint32_t reason);
	void (*set_clr_pd_cnt)(uint32_t reason, bool flag);
	int (*board_id_check)(void);
	int (*dst_cmd)(char cmd, uint64_t arg0, uint64_t arg1, uint64_t arg2);
};

int platform_setup_sip_ops(const struct plat_sip_svc_ops_t **plat_ops);

void set_clr_power_domain_cnt(uint32_t reason, bool flag);
int cix_board_id_check(void);
int cix_dst_cmd(char cmd, uint64_t arg0, uint64_t arg1, uint64_t arg2);

#endif /* ARM_SIP_SVC_H */
