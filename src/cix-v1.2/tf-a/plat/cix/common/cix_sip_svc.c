/*
 * Copyright (c) 2016-2019,2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>

#include <common/debug.h>
#include <common/runtime_svc.h>
#include <drivers/arm/ethosn.h>
#include <lib/debugfs.h>
#include <lib/pmf/pmf.h>
#include <lib/utils.h>
#include <cix_sip_svc.h>
#include <plat_cix.h>
#include <tools_share/uuid.h>
#include <drivers/scmi-msg.h>
#include <drivers/dsu_portion_pd.h>
#include <sky1_pdc.h>
#include <sky1_qos.h>
#include <sky1_dp_gop.h>
#include <sky1_ddrlp.h>
#include <sky1_plat.h>
#include <drivers/spi_mem.h>

typedef enum BUS_MODE_T
{
    BUS_MODE_INVALID = 0,
    BUS_MODE_1_1_1   = 1,
    BUS_MODE_1_1_2   = 2,
    BUS_MODE_1_2_2   = 3,
    BUS_MODE_1_1_4   = 4,
    BUS_MODE_1_4_4   = 5,
    BUS_MODE_4_4_4   = 6,
    BUS_MODE_1_8_8   = 7,
    BUS_MODE_8_8_8   = 8,
    BUS_MODE_MAX     = BUS_MODE_8_8_8
} BUS_MODE_T;

/* ARM SiP Service UUID */
DEFINE_SVC_UUID2(cix_sip_svc_uid,
	0x556d75e2, 0x6033, 0xb54b, 0xb5, 0x75,
	0x62, 0x79, 0xfd, 0x11, 0x37, 0xff);

const struct plat_sip_svc_ops_t *sip_svc_plat_ops;

int cix_nor_storage_handler(uint32_t smc_fid,
                u_register_t x1,
                u_register_t x2,
                u_register_t x3,
                u_register_t x4)
{
	int ret = -1;
	uint8_t cmd;
	uint32_t address;
	uint32_t addressSize;
	uint32_t data_dir;
	uint32_t bus_format;
	uint32_t dummyCycles;
	uintptr_t data_buff;
	uint32_t data_buffsize;
	uint8_t *_data_buff;
	struct spi_mem_op op;

	cmd = (uint32_t)x1;
	address = (uint32_t)(x1 >> 32);
	addressSize = (uint32_t)x2;
	data_dir = (uint32_t)(x2 >> 32);
	bus_format = (uint32_t)x3;
	dummyCycles = (uint32_t)(x3 >> 32);
	data_buff = (uint32_t)x4;
	data_buffsize = (uint32_t)(x4 >> 32);

	VERBOSE("%s cmd:0x%x address:%x addresssize:%x data_dir:%x bus_format:%x dummyCycles:%x data_buff:%lx data_buffsize:%x\n", __func__, cmd, address, addressSize,data_dir,bus_format,dummyCycles,data_buff,data_buffsize);

	zeromem(&op, sizeof(struct spi_mem_op));
	op.cmd.opcode = cmd;
	op.addr.val = address;
	op.addr.nbytes = addressSize;

	switch (bus_format)
	{
		case BUS_MODE_1_1_2:
			op.cmd.buswidth = 1;
			op.addr.buswidth = 1;
			op.dummy.buswidth = 2;
			op.data.buswidth = 2;
			break;
		case BUS_MODE_1_1_4:
			op.cmd.buswidth = 1;
			op.addr.buswidth = 1;
			op.dummy.buswidth = 1;
			op.data.buswidth = 4;
			break;
		case BUS_MODE_1_2_2:
			op.cmd.buswidth = 1;
			op.addr.buswidth = 2;
			op.dummy.buswidth = 2;
			op.data.buswidth = 2;
			break;
		case BUS_MODE_1_4_4:
			op.cmd.buswidth = 1;
			op.addr.buswidth = 4;
			op.dummy.buswidth = 4;
			op.data.buswidth = 4;
			break;
		case BUS_MODE_4_4_4:
			op.cmd.buswidth = 4;
			op.addr.buswidth = 4;
			op.dummy.buswidth = 4;
			op.data.buswidth = 4;
			break;
		default:
			/*if bus format invalid, then use 1-1-1 line mode*/
			op.cmd.buswidth = 1;
			op.data.buswidth = 1;
			op.dummy.buswidth = 1;
			op.addr.buswidth = 1;
			break;
	}

	op.dummy.nbytes = (dummyCycles *  op.dummy.buswidth ) / 8;

	if(data_buffsize == 0){
		data_dir = SPI_MEM_NO_DATA;
		op.data.buswidth = 0;
	}
	if(addressSize == 0){
		op.addr.buswidth = 0;
		op.addr.val = 0;
		op.addr.nbytes = 0;
	}

	op.data.dir = data_dir;
	_data_buff = (uint8_t *)data_buff;
	op.data.buf = _data_buff;
	op.data.nbytes = data_buffsize;
	ret = spi_mem_exec_op(&op);

	return ret;
}

static int cix_sip_setup(void)
{
	/* register Sip service function ops*/
	platform_setup_sip_ops(&sip_svc_plat_ops);
	assert(sip_svc_plat_ops);

	// if (pmf_setup() != 0) {
	// 	return 1;
	// }

#if USE_DEBUGFS

	if (debugfs_smc_setup() != 0) {
		return 1;
	}

#endif /* USE_DEBUGFS */

	return 0;
}

/*
 * This function handles ARM defined SiP Calls
 */
static uintptr_t cix_sip_handler(unsigned int smc_fid,
			u_register_t x1,
			u_register_t x2,
			u_register_t x3,
			u_register_t x4,
			void *cookie,
			void *handle,
			u_register_t flags)
{
	int call_count = 0;
	uint64_t x5;

#if ENABLE_PMF

	/*
	 * Dispatch PMF calls to PMF SMC handler and return its return
	 * value
	 */
	if (is_pmf_fid(smc_fid)) {
		return pmf_smc_handler(smc_fid, x1, x2, x3, x4, cookie,
				handle, flags);
	}

#endif /* ENABLE_PMF */

#if USE_DEBUGFS

	if (is_debugfs_fid(smc_fid)) {
		return debugfs_smc_handler(smc_fid, x1, x2, x3, x4, cookie,
					   handle, flags);
	}

#endif /* USE_DEBUGFS */

#if ARM_ETHOSN_NPU_DRIVER

	if (is_ethosn_fid(smc_fid)) {
		return ethosn_smc_handler(smc_fid, x1, x2, x3, x4, cookie,
					  handle, flags);
	}

#endif /* ARM_ETHOSN_NPU_DRIVER */

	switch (smc_fid) {
	case ARM_SIP_SVC_EXE_STATE_SWITCH: {
		/* Execution state can be switched only if EL3 is AArch64 */
#ifdef __aarch64__
		/* Allow calls from non-secure only */
		if (!is_caller_non_secure(flags))
			SMC_RET1(handle, STATE_SW_E_DENIED);

		/*
		 * Pointers used in execution state switch are all 32 bits wide
		 */
		return (uintptr_t) cix_execution_state_switch(smc_fid,
				(uint32_t) x1, (uint32_t) x2, (uint32_t) x3,
				(uint32_t) x4, handle);
#else
		/* State switch denied */
		SMC_RET1(handle, STATE_SW_E_DENIED);
#endif /* __aarch64__ */
		}

	case CIX_SIP_NOR_STORAGE:
                SMC_RET1(handle, cix_nor_storage_handler(smc_fid, x1, x2, x3, x4));
                break;

	case ARM_SIP_SVC_CALL_COUNT:
		/* PMF calls */
		call_count += PMF_NUM_SMC_CALLS;

#if ARM_ETHOSN_NPU_DRIVER
		/* ETHOSN calls */
		call_count += ETHOSN_NUM_SMC_CALLS;
#endif /* ARM_ETHOSN_NPU_DRIVER */

		/* State switch call */
		call_count += 1;

		SMC_RET1(handle, call_count);

	case ARM_SIP_SVC_UID:
		/* Return UID to the caller */
		SMC_UUID_RET(handle, cix_sip_svc_uid);

	case ARM_SIP_SVC_VERSION:
		/* Return the version of current implementation */
		SMC_RET2(handle, ARM_SIP_SVC_VERSION_MAJOR, ARM_SIP_SVC_VERSION_MINOR);

	case CIX_SIP_SVC_SET_REBOOT_REASON:
		if (sip_svc_plat_ops->set_reboot_reason)
			sip_svc_plat_ops->set_reboot_reason(x1);
		SMC_RET1(handle, SMC_OK);

	case CIX_SIP_POWER_DOMAIN_SET:
		if (sip_svc_plat_ops->set_clr_pd_cnt)
			sip_svc_plat_ops->set_clr_pd_cnt(x1, x2);
		SMC_RET1(handle, SMC_OK);

	case CIX_SIP_SCMI:
		scmi_smt_fastcall_smc_entry(0);
		SMC_RET1(handle, SMC_OK);
		break;
	case CIX_SIP_SVC_DSU_HW_CTL:
		SMC_RET1(handle, dsu_pd_setting(DSU_HW_CTRL_ENABLE, x1, 0, 0, 0, 0));
	case CIX_SIP_SVC_DSU_SET_PD:
		SMC_RET1(handle, dsu_pd_setting(DSU_SW_CTRL_SET_PD, x1, 0, 0, 0, 0));
	case CIX_SIP_SVC_DSU_GET_PD:
		SMC_RET1(handle, dsu_pd_setting(DSU_SW_CTRL_GET_PD, 0, 0, 0, 0, 0));
	case CIX_SIP_SVC_DSU_SET_TH:
		x5 = SMC_GET_GP(handle, CTX_GPREG_X5);
		SMC_RET1(handle, dsu_pd_setting(DSU_HW_CTRL_SET, x1, x2, x3, x4, x5));
	case CIX_SIP_SVC_DSU_GET_HITCNT:
		SMC_RET1(handle, dsu_pd_setting(DSU_SW_CTRL_READ_CLEAR_HITCNT, 0, 0, 0, 0, 0));
	case CIX_SIP_SVC_DSU_GET_MISSCNT:
		SMC_RET1(handle, dsu_pd_setting(DSU_SW_CTRL_READ_CLEAR_MISSCNT, 0, 0, 0, 0, 0));
	case CIX_SIP_SVC_PDC_SET_WAKEUP:
                SMC_RET1(handle, sky1_pdc_handler(x1, x2, x3, x4));
	case CIX_SIP_SMMU_GOP_CTRL:
                SMC_RET1(handle, sky1_smmu_gop_handler(x1, 0, 0, 0, 0, 0));
	case CIX_SIP_CPUFREQ_SUPPORT:
		if (sip_svc_plat_ops->board_id_check)
			if (!sip_svc_plat_ops->board_id_check())
				SMC_RET1(handle, SMC_OK);
		SMC_RET1(handle, SMC_UNK);
	case CIX_SIP_PI_TEST_ENABLE:
		write_actlr_el3(read_actlr_el3() | 0x183);//ACTLR_ACTLREN_BIT | ACTLR_ECTLREN_BIT | ACTLR_PWREN_BIT | ACTLR_PDPEN_BIT
		write_mdcr_el3(read_mdcr_el3() & (~(MDCR_TDOSA_BIT | MDCR_TDA_BIT )));
		SMC_RET1(handle, SMC_OK);
	case CIX_SIP_DP_GOP_CTRL:
		SMC_RET1(handle, sky1_dp_gop_handler(x1, x2));
	case CIX_SIP_SVC_SET_DDRLP:
		SMC_RET1(handle, sky1_set_ddrlp(x1));
	case CIX_SIP_CPU_BOOST_TRIGGER:
	  SMC_RET1(handle, sky1_set_cpu_boost_trigger(x1));
	case CIX_SIP_SVC_DST_CMD:
		if (sip_svc_plat_ops->dst_cmd)
			if (!sip_svc_plat_ops->dst_cmd(x1, x2, x3, x4))
				SMC_RET1(handle, SMC_OK);
		SMC_RET1(handle, SMC_UNK);
	default:
		WARN("Unimplemented ARM SiP Service Call: 0x%x \n", smc_fid);
		SMC_RET1(handle, SMC_UNK);
	}

}


/* Define a runtime service descriptor for fast SMC calls */
DECLARE_RT_SVC(
	cix_sip_svc,
	OEN_SIP_START,
	OEN_SIP_END,
	SMC_TYPE_FAST,
	cix_sip_setup,
	cix_sip_handler
);
