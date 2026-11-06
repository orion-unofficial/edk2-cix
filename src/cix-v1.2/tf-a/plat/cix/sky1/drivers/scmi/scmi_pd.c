/*
 * Copyright 2022 CIX
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <common/debug.h>
#include <drivers/scmi.h>
#include <lib/utils_def.h>
#include <lib/libc/errno.h>
#include <lib/mmio.h>
#include <platform_def.h>
#include "mailbox.h"
#include <drivers/delay_timer.h>
#include <plat/arm/common/arm_sip_svc.h>
#include  "hwspinlock.h"

#define POWER_STATE_ON	(0 << 30)
#define POWER_STATE_OFF	(1 << 30)
const char *power_states[] = {"OFF", "ON"};

struct power_domain {
	char *name;
	uint32_t reg;
	uint32_t pd_mask;
	uint32_t pu_mask;
	uint32_t count;
	uint32_t power_state;
	uint8_t parent_id;
	uint8_t pd_id;
	uint8_t flags;
	uint8_t mutex_idx;
	bool do_mem_repair;
	bool hw_spinlock;
	uint32_t rcsu_addr;
	uint32_t mem_index;
	uint32_t group_id;
};

/*Do meory repair*/
#define MEMORY_ENABLE	1
#define GROUP_INDEX_NONE	0x00000001
#define REPAIR_ALL_DONE_PASS_REG	0x10
#define GROUP_REPAIR_EN	0x14
#define GROUP_REPAIR_DONE_BUSY_REG	0x18
#define MEMORY_REAPIR_CNT	(1000 * 10000)

/*RCSU OFFSET ADDRESS*/
#define PGFSM_REG_CTRL	BIT(0)
#define PGFSM_REG_CTRL2	BIT(12)
#define PD_PG_EN	BIT(0)
#define PD_PG_EN1	BIT(1)
#define GICD_QRENQN_ITS0	BIT(31)
#define GITS_CTRL_ENABLE 	BIT(0)

#define GIC_STARPPIN_OFFSET	(0x300)
#define GIC_STRAP_STATUS0	(0x400)

/*ppu power control*/
#define PWR_DYN_EN (0x1 << 8)
typedef enum {
        PWR_OFF = 0x0,
        PWR_OFF_EMU,
        PWR_FULL_RET = 0x5,
        PWR_FUNC_RET = 0x7,
        PWR_ON,
        PWR_WRAM_RST,
} pwr_policy;

#define IP_PWR_DYNAMIC_OFF	(PWR_DYN_EN        \
				| PWR_ON)
#define IP_PWR_DYNAMIC_ON	(PWR_DYN_EN        \
				| PWR_ON)

#define T5_CYCLE_CNT	(0x3 << 2)
#define T7_CYCLE_CNT	(0x3 << 4)
#define T8_CYCLE_CNT	(0x3 << 6)
#define T9_CYCLE_CNT	(0xf << 8)
#define TIME_CYCLE_CNT	(T5_CYCLE_CNT \
			| T7_CYCLE_CNT	\
			| T8_CYCLE_CNT	\
			| T9_CYCLE_CNT)

/*isp1 rcsu save and restore*/
#define ISP1_STRAP_PIN_BASE	(0x14350000)
#define ISP1_STRAP_PIN0	(ISP1_STRAP_PIN_BASE + 0x300)
#define ISP1_STRAP_PIN1	(ISP1_STRAP_PIN_BASE + 0x304)
unsigned int isp_strap[] = {0, 0};
static unsigned int isp1_flag = 0;

/* The Rich OS need flow the macro */
#define SKY1_PD_AUDIO		0
#define SKY1_PD_PCIE_CTRL0	1
#define SKY1_PD_PCIE_DUMMY	2
#define SKY1_PD_PCIEHUB		3
#define SKY1_PD_MMHUB		4
#define SKY1_PD_MMHUB_SMMU	5
#define SKY1_PD_DPU0		6
#define SKY1_PD_DPU1		7
#define	SKY1_PD_DPU2		8
#define SKY1_PD_DPU3		9
#define SKY1_PD_DPU4		10
#define SKY1_PD_VPU_TOP		11
#define SKY1_PD_VPU_CORE0	12
#define SKY1_PD_VPU_CORE1	13
#define SKY1_PD_VPU_CORE2	14
#define SKY1_PD_VPU_CORE3	15
#define SKY1_PD_NPU_CORE0	16
#define SKY1_PD_NPU_CORE1	17
#define SKY1_PD_NPU_CORE2	18
#define SKY1_PD_NPU_TOP		19
#define SKY1_PD_ISP0		20
#define SKY1_PD_GPU		21
#define SKY1_PD_MAX		32

#define ALWAYS_ON	BIT(0)
#define DUMMY_PD	BIT(1)

static struct power_domain scmi_power_domains[] = {
	[SKY1_PD_AUDIO] = {
		.name = "audio_pd",
		.reg = AUDIO_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_AUDIO,
		.pd_mask = PGFSM_REG_CTRL,
		.pu_mask = PGFSM_REG_CTRL | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = AUDIO_RCSU_BASE_REG,
		.mem_index = GROUP_INDEX_NONE,
		.group_id =  MEMR_GROUP_ID_AUDIO
	},
	[SKY1_PD_PCIE_CTRL0] = {
		.name = "pcieX8_ctrl0_pd",
		.reg = PCIE_X8_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_PCIE_CTRL0,
		.pd_mask = PGFSM_REG_CTRL,
		.pu_mask = PGFSM_REG_CTRL | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = PCIE_X8_RCSU_BASE_REG,
		.mem_index = BIT(0),
		.group_id = MEMR_GROUP_ID_PCIE
	},
	[SKY1_PD_PCIE_DUMMY] = {
		.name = "pcie_ctrl_X_pd",
		.parent_id = SKY1_PD_PCIEHUB,
		.pd_id = SKY1_PD_PCIE_DUMMY,
		.flags = DUMMY_PD,
		.power_state = POWER_STATE_OFF
	},
	[SKY1_PD_PCIEHUB] = {
		.name = "pcie_ni700_pd",
		.reg = PCIEHUB_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_PCIEHUB,
		.pd_mask = IP_PWR_DYNAMIC_OFF,
		.pu_mask = IP_PWR_DYNAMIC_ON,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = PCIEHUB_SMMU_RCSU_BASE_REG,
		.mem_index = GROUP_INDEX_NONE,
		.flags = DUMMY_PD,
		.group_id = MEMR_GROUP_ID_SMMU_PCIE
	},
	[SKY1_PD_MMHUB] = {
		.name = "mm_ni700_pd",
		.reg = MMHUB_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_MMHUB,
		.pd_mask = IP_PWR_DYNAMIC_OFF,
		.pu_mask = IP_PWR_DYNAMIC_ON,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = MMHUB_RCSU_BASE_REG,
		.mem_index = GROUP_INDEX_NONE,
		.group_id = MEMR_GROUP_ID_MMHUB
	},
	[SKY1_PD_MMHUB_SMMU] = {
		.name = "mmhub_smmu_pd",
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = MMHUB_SMMU_RCSU_BASE,
		.mem_index = GROUP_INDEX_NONE,
		.group_id = MEMR_GROUP_ID_SMMU_MM,
		.flags = DUMMY_PD
	},
	[SKY1_PD_DPU0] = {
		.name = "dpu0_pd",
		.reg = DPU0_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_DPU0,
		.pd_mask = PD_PG_EN1,
		.pu_mask = PD_PG_EN1 | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = DPU_RCSU_BASE_REG,
		.mem_index = BIT(0),
		.group_id = MEMR_GROUP_ID_DPU
	},
	[SKY1_PD_DPU1] = {
		.name = "dpu1_pd",
		.reg = DPU1_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_DPU1,
		.pd_mask = PD_PG_EN1,
		.pu_mask = PD_PG_EN1 | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = DPU_RCSU_BASE_REG,
		.mem_index = BIT(1),
		.group_id = MEMR_GROUP_ID_DPU
	},
	[SKY1_PD_DPU2] = {
		.name = "dpu2_pd",
		.reg = DPU2_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_DPU2,
		.pd_mask = PD_PG_EN1,
		.pu_mask = PD_PG_EN1 | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = DPU_RCSU_BASE_REG,
		.mem_index = BIT(2),
		.group_id = MEMR_GROUP_ID_DPU
	},
	[SKY1_PD_DPU3] = {
		.name = "dpu3_pd",
		.reg = DPU3_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_DPU3,
		.pd_mask = PD_PG_EN1,
		.pu_mask = PD_PG_EN1 | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = DPU_RCSU_BASE_REG,
		.mem_index = BIT(3),
		.group_id = MEMR_GROUP_ID_DPU
	},
	[SKY1_PD_DPU4] = {
		.name = "dpu4_pd",
		.reg = DPU4_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_DPU4,
		.pd_mask = PD_PG_EN1,
		.pu_mask = PD_PG_EN1 | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = DPU_RCSU_BASE_REG,
		.mem_index = BIT(4),
		.group_id = MEMR_GROUP_ID_DPU
	},
	[SKY1_PD_VPU_TOP] = {
		.name = "vpu_top_pd",
		.reg = VPU_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_VPU_TOP,
		.pd_mask = PGFSM_REG_CTRL2,
		.pu_mask = PGFSM_REG_CTRL2 | TIME_CYCLE_CNT,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = VPU_RCSU_BASE_REG,
		.mem_index = BIT(0),
		.group_id = MEMR_GROUP_ID_VPU
	},
	[SKY1_PD_VPU_CORE0] = {
		.name = "vpu_c0_dummy_pd",
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = VPU_RCSU_BASE_REG,
		.mem_index = BIT(1),
		.group_id = MEMR_GROUP_ID_VPU,
		.flags = DUMMY_PD
	},
	[SKY1_PD_VPU_CORE1] = {
		.name = "vpu_c1_dummy_pd",
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = VPU_RCSU_BASE_REG,
		.mem_index = BIT(2),
		.group_id = MEMR_GROUP_ID_VPU,
		.flags = DUMMY_PD
	},
	[SKY1_PD_VPU_CORE2] = {
		.name = "vpu_c2_dummy_pd",
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = VPU_RCSU_BASE_REG,
		.mem_index = BIT(3),
		.group_id = MEMR_GROUP_ID_VPU,
		.flags = DUMMY_PD
	},
	[SKY1_PD_VPU_CORE3] = {
		.name = "vpu_c3_dummy_pd",
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = VPU_RCSU_BASE_REG,
		.mem_index = BIT(4),
		.group_id = MEMR_GROUP_ID_VPU,
		.flags = DUMMY_PD
	},
	[SKY1_PD_NPU_CORE0] = {
		.name = "npu_core0_pd",
		.reg = NPU_CORE0_PD_REG,
		.parent_id = SKY1_PD_NPU_TOP,
		.pd_id = SKY1_PD_NPU_CORE0,
		.pd_mask = PD_PG_EN,
		.pu_mask = PD_PG_EN | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.hw_spinlock = 1,
		.mutex_idx = NPU_MUTEX_IDX,
		.rcsu_addr = NPU_RCSU_BASE_REG,
		.mem_index = BIT(1),
		.group_id = MEMR_GROUP_ID_NPU
	},
	[SKY1_PD_NPU_CORE1] = {
		.name = "npu_core1_pd",
		.reg = NPU_CORE1_PD_REG,
		.parent_id = SKY1_PD_NPU_TOP,
		.pd_id = SKY1_PD_NPU_CORE1,
		.pd_mask = PD_PG_EN,
		.pu_mask = PD_PG_EN | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.hw_spinlock = 1,
		.mutex_idx = NPU_MUTEX_IDX,
		.rcsu_addr = NPU_RCSU_BASE_REG,
		.mem_index = BIT(2),
		.group_id = MEMR_GROUP_ID_NPU
	},
	[SKY1_PD_NPU_CORE2] = {
		.name = "npu_core2_pd",
		.reg = NPU_CORE2_PD_REG,
		.parent_id = SKY1_PD_NPU_TOP,
		.pd_id = SKY1_PD_NPU_CORE2,
		.pd_mask = PD_PG_EN,
		.pu_mask = PD_PG_EN | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.hw_spinlock = 1,
		.mutex_idx = NPU_MUTEX_IDX,
		.rcsu_addr = NPU_RCSU_BASE_REG,
		.mem_index = BIT(3),
		.group_id = MEMR_GROUP_ID_NPU
	},
	[SKY1_PD_NPU_TOP] = {
		.name = "npu_top_pd",
		.reg = NPU_TOP_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_NPU_TOP,
		.pd_mask = PD_PG_EN,
		.pu_mask = PD_PG_EN | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.hw_spinlock = 1,
		.mutex_idx = NPU_MUTEX_IDX,
		.rcsu_addr = NPU_RCSU_BASE_REG,
		.mem_index = BIT(0),
		.group_id = MEMR_GROUP_ID_NPU
	},
	[SKY1_PD_ISP0] = {
		.name = "isp_pd0",
		.reg = ISP0_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_ISP0,
		.pd_mask = PGFSM_REG_CTRL,
		.pu_mask = PGFSM_REG_CTRL | TIME_CYCLE_CNT,
		.power_state = POWER_STATE_OFF,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = ISP0_RCSU_BASE_REG,
		.mem_index = GROUP_INDEX_NONE,
		.group_id = MEMR_GROUP_ID_ISP
	},
	[SKY1_PD_GPU] = {
		.name = "gpu_pd",
		.reg = GPU_RCSU_PD_REG,
		.parent_id = 0,
		.pd_id = SKY1_PD_GPU,
		.pd_mask = PGFSM_REG_CTRL2,
		.pu_mask = PGFSM_REG_CTRL2 | TIME_CYCLE_CNT,
		.do_mem_repair = MEMORY_ENABLE,
		.rcsu_addr = GPU_RCSU_BASE_REG,
		.mem_index = BIT(0),
		.group_id = MEMR_GROUP_ID_GPU
	},
};

void set_clr_power_domain_cnt(uint32_t pd_id, bool flag)
{
	struct power_domain pd_info, parent_pd;
	uint32_t parent_id;

	pd_info = scmi_power_domains[pd_id];
	parent_id = pd_info.parent_id;

	if (!parent_id)
		return;

	parent_pd = scmi_power_domains[parent_id];

	if (flag) {
		parent_pd.count++;
	} else {
		if(parent_pd.count > 0)
			parent_pd.count--;
	}

	set_clr_power_domain_cnt(parent_pd.pd_id, flag);
	INFO("Do power domain: %s, %s count, count=%d!\n", parent_pd.name, flag ? "set" : "clr", parent_pd.count);
}

void do_memory_repair(struct power_domain *pd_info)
{
	unsigned int index, busy_status, done_status, count = MEMORY_REAPIR_CNT;
	uintptr_t rcsu_addr = pd_info->rcsu_addr;

	do {
		busy_status = mmio_read_32((rcsu_addr + GROUP_REPAIR_DONE_BUSY_REG));
		busy_status = (busy_status  >> 16) & 0xffff;

		count--;
		if (count == 0x0)
			INFO("%s, %d, do memory busy, status = %d!\n",
				__func__, __LINE__, busy_status);
	} while (busy_status != 0x0 && count !=0);

	index = pd_info->mem_index;
	mmio_write_32((rcsu_addr + GROUP_REPAIR_EN), index);

	INFO("group_en = 0x%x!\n", mmio_read_32(rcsu_addr + GROUP_REPAIR_EN));
	do {
		done_status = mmio_read_32((rcsu_addr + REPAIR_ALL_DONE_PASS_REG));
		done_status = (done_status  >> 0x1) & 0x3;

		count--;
		if (count == 0x0)
			INFO("%s, %d, done and pass failed, status = %d!\n",
				__func__, __LINE__, done_status);
	} while (done_status != 0x3 && count !=0);

	mmio_write_32((rcsu_addr + GROUP_REPAIR_EN), 0x0);
	INFO("group_en = 0x%x!\n", mmio_read_32(rcsu_addr + GROUP_REPAIR_EN));
}

size_t plat_scmi_pd_count(uint32_t agent_id __unused)
{
	return ARRAY_SIZE(scmi_power_domains);
}

const char *plat_scmi_pd_get_name(uint32_t agent_id __unused,
				  uint32_t pd_id)
{
	return scmi_power_domains[pd_id].name;
}

uint32_t plat_scmi_pd_get_state(uint32_t agent_id __unused,
				    uint32_t pd_id __unused)
{
	return scmi_power_domains[pd_id].power_state;
}

void pcie_hub_its_ctrl(bool on)
{
	if (on) {
		/*assert qreqn_its and enable ITS*/
		mmio_clrbits_32((GIC_RCSU_BASE + GIC_STARPPIN_OFFSET), GICD_QRENQN_ITS0);
		mmio_setbits_32(PLAT_SKY1_GICD_ITS, GITS_CTRL_ENABLE);
	} else {
		/*disable ITS and de-assert qreqn_its*/
		mmio_clrbits_32(PLAT_SKY1_GICD_ITS, GITS_CTRL_ENABLE);
		mmio_setbits_32((GIC_RCSU_BASE + GIC_STARPPIN_OFFSET), GICD_QRENQN_ITS0);
	}
}

void isp1_strappin_save_restore(bool on)
{
	if (isp1_flag == 0) {
		isp_strap[0] = mmio_read_32(ISP1_STRAP_PIN0);
		isp_strap[1] = mmio_read_32(ISP1_STRAP_PIN1);
		isp1_flag++;
	}

	if (on) {
		/*restore isp1 strap pin status*/
		mmio_write_32(ISP1_STRAP_PIN0, isp_strap[0]);
		mmio_write_32(ISP1_STRAP_PIN1, isp_strap[1]);
	} else {
		/*save isp1 strap pin status*/
		isp_strap[0] = mmio_read_32(ISP1_STRAP_PIN0);
		isp_strap[1] = mmio_read_32(ISP1_STRAP_PIN1);
	}
}

void power_domain_control(struct power_domain *pd_info, bool on)
{
	uintptr_t addr;
	uint32_t bit_mask, group_id;
	bool mem_valid;

	group_id = pd_info->group_id;
	mem_valid = (mmio_read_32(SW_USED_REG1) >> group_id ) & 0x1;

	if ((pd_info->flags & DUMMY_PD)) {
		if (on) {
			if (pd_info->do_mem_repair && mem_valid)
				do_memory_repair(pd_info);
		}

		return;
	}

	addr = pd_info->reg;

	INFO("Do power domain control:%s, addr = 0x%lx, on =%d, mem_valid=%d!\n", pd_info->name, addr, on, mem_valid);
	if (on) {
		bit_mask = pd_info->pu_mask;
		mmio_setbits_32(addr, bit_mask);
		udelay(5);

		if (pd_info->pd_id == SKY1_PD_PCIEHUB)
			pcie_hub_its_ctrl(on);

		if (pd_info->pd_id == SKY1_PD_ISP0)
			isp1_strappin_save_restore(on);

		if (pd_info->do_mem_repair && mem_valid) {
			do_memory_repair(pd_info);

			if (pd_info->pd_id == SKY1_PD_MMHUB)
				do_memory_repair(&scmi_power_domains[SKY1_PD_MMHUB_SMMU]);
		}
	} else {
		bit_mask = pd_info->pd_mask;
		if (pd_info->pd_id == SKY1_PD_ISP0)
			isp1_strappin_save_restore(on);

		if (pd_info->pd_id == SKY1_PD_PCIEHUB)
			pcie_hub_its_ctrl(on);

#ifdef CONFIG_CIX_HW_SPINLOCK
		if (pd_info->hw_spinlock)
			sky1_hwspinlock_trylock(pd_info->mutex_idx, 20000);
#endif
		mmio_clrbits_32(addr, bit_mask);
#ifdef CONFIG_CIX_HW_SPINLOCK
		if (pd_info->hw_spinlock)
			sky1_hwspinlock_unlock(pd_info->mutex_idx);
#endif
	}
	INFO("Do power domain control: val = 0x%x!\n", mmio_read_32(addr));
}

void power_domain_parent_control(struct power_domain *pd_info, uint32_t state)
{
	uint32_t parent_id = pd_info->parent_id;
	struct power_domain parent_pd = scmi_power_domains[parent_id];
	bool on;

	if (!parent_id)
		return;

	on = (state == POWER_STATE_ON ? true : false);

	INFO("Do power domain control:%s, state = 0x%x, count = %d!\n", parent_pd.name, state, scmi_power_domains[parent_id].count);
	if (!scmi_power_domains[parent_id].count) {
		if (on) {
			power_domain_control(&parent_pd, on);
			scmi_power_domains[parent_id].count++;
		} else {
			INFO("power domain %s already in off state!\n", parent_pd.name);
		}
	} else {
		if (on) {
			scmi_power_domains[parent_id].count++;
		} else {
			scmi_power_domains[parent_id].count--;
			if (!scmi_power_domains[parent_id].count) {
				power_domain_control(&parent_pd, on);
			}
		}
	}

	INFO("Do parent power domain: %s, %s, count=%d!\n", parent_pd.name, power_states[on], scmi_power_domains[parent_id].count);
	scmi_power_domains[parent_id].power_state = state;
}

int32_t plat_scmi_pd_set_state(uint32_t agent_id __unused,
			       uint32_t flags,
			       uint32_t pd_id,
			       uint32_t state)
{
	struct power_domain pd_info;
	bool on;
	int i = pd_id;

	INFO("%s: agend_id: %d flags: 0x%x: pd_id: %d, state: 0x%x\n", __func__, agent_id, flags, pd_id, state);
	pd_info = scmi_power_domains[i];

	if (flags != 0 || pd_id >= SKY1_PD_MAX)
		return SCMI_NOT_SUPPORTED;

	if (state == pd_info.power_state)
		return SCMI_SUCCESS;

	on = (state == POWER_STATE_ON ? true : false);

	if ((pd_info.flags & ALWAYS_ON) && (pd_info.count != 0))
		return SCMI_SUCCESS;

	if (on) {
		/*power domain on*/
		power_domain_parent_control(&pd_info, state);
		power_domain_control(&pd_info, on);
	} else {
		/*power domain off*/
		power_domain_control(&pd_info, on);
		power_domain_parent_control(&pd_info, state);
	}

	INFO("Done power_domain:%s, %s\n", pd_info.name, on ? "on" : "off");
	scmi_power_domains[pd_id].power_state = state;

	return SCMI_SUCCESS;
}
