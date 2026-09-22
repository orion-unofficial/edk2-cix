/*
 * Copyright (C) 2025 Cixcomputing, Inc. All rights reserved.
 *
 * All information contained herein is Cix confidential.
 *
 * This software is provided to you pursuant to Software License
 * Agreement (SLA) with Cix Inc ("Cix"). This software may be
 * used only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission
 * from Cix.
 *
*/

#include "../include/platform_def.h"
#include "plat/common/platform.h"
#include <services/sdei.h>
#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <lib/el3_runtime/pubsub.h>
#include <lib/extensions/ras.h>
#include "sky1_ras_private.h"
#ifdef RAM_LOG_SUPPORT
#include <lib/rlog.h>
#endif

#define IDM_ARGS_DEF(arg0, arg1, arg2)
#define IDM_ARGS_LIST                                                       \
	IDM_ARGS_DEF(CI700, 0x0d0db000, SKY1_IDM_DEF_TIMEOUT_VALUE)         \
	IDM_ARGS_DEF(CSUPM, 0x0d0dc000, SKY1_IDM_DEF_TIMEOUT_VALUE)         \
	IDM_ARGS_DEF(CSUSE, 0x0d0dd000, SKY1_IDM_DEF_TIMEOUT_VALUE)         \
	IDM_ARGS_DEF(DDRBRCAST, 0x0d0de000, SKY1_IDM_DEF_TIMEOUT_VALUE)     \
	IDM_ARGS_DEF(DDRCTL0, 0x0d0df000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(DDRCTL1, 0x0d0e0000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(DDRCTL2, 0x0d0e1000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(DDRCTL3, 0x0d0e2000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(DDRTZC0, 0x0d0e3000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(DDRTZC1, 0x0d0e4000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(DDRTZC2, 0x0d0e5000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(DDRTZC3, 0x0d0e6000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(DSUCORE, 0x0d0e7000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	/*IDM_ARGS_DEF(DSUTILITY, 0x0d0e8000, SKY1_IDM_DEF_TIMEOUT_VALUE)*/ \
	IDM_ARGS_DEF(FCH, 0x0d0e9000, SKY1_IDM_DEF_TIMEOUT_VALUE)           \
	IDM_ARGS_DEF(GIC, 0x0d0ea000, SKY1_IDM_DEF_TIMEOUT_VALUE)           \
	IDM_ARGS_DEF(GPU, 0x0d0eb000, SKY1_IDM_DEF_TIMEOUT_VALUE)           \
	IDM_ARGS_DEF(CSI0_1, 0x0d0ec000, SKY1_IDM_DEF_TIMEOUT_VALUE)        \
	IDM_ARGS_DEF(CSI2_3, 0x0d0ed000, SKY1_IDM_DEF_TIMEOUT_VALUE)        \
	IDM_ARGS_DEF(DP0, 0x0d0ee000, SKY1_IDM_DEF_TIMEOUT_VALUE)           \
	IDM_ARGS_DEF(DP1, 0x0d0ef000, SKY1_IDM_DEF_TIMEOUT_VALUE)           \
	IDM_ARGS_DEF(DP2, 0x0d0f0000, SKY1_IDM_DEF_TIMEOUT_VALUE)           \
	IDM_ARGS_DEF(DP3, 0x0d0f1000, SKY1_IDM_DEF_TIMEOUT_VALUE)           \
	IDM_ARGS_DEF(DP4, 0x0d0f2000, SKY1_IDM_DEF_TIMEOUT_VALUE)           \
	IDM_ARGS_DEF(DPU0, 0x0d0f3000, SKY1_IDM_DEF_TIMEOUT_VALUE)          \
	IDM_ARGS_DEF(DPU1, 0x0d0f4000, SKY1_IDM_DEF_TIMEOUT_VALUE)          \
	IDM_ARGS_DEF(DPU2, 0x0d0f5000, SKY1_IDM_DEF_TIMEOUT_VALUE)          \
	IDM_ARGS_DEF(DPU3, 0x0d0f6000, SKY1_IDM_DEF_TIMEOUT_VALUE)          \
	IDM_ARGS_DEF(DPU4, 0x0d0f7000, SKY1_IDM_DEF_TIMEOUT_VALUE)          \
	IDM_ARGS_DEF(ISP0, 0x0d0f8000, SKY1_IDM_DEF_TIMEOUT_VALUE)          \
	IDM_ARGS_DEF(ISP1, 0x0d0f9000, SKY1_IDM_DEF_TIMEOUT_VALUE)          \
	IDM_ARGS_DEF(NPU, 0x0d0fa000, SKY1_IDM_DEF_TIMEOUT_VALUE)           \
	IDM_ARGS_DEF(VPU, 0x0d0fb000, SKY1_IDM_DEF_TIMEOUT_VALUE)           \
	IDM_ARGS_DEF(NI700MMHUB, 0x0d0fc000, SKY1_IDM_DEF_TIMEOUT_VALUE)    \
	IDM_ARGS_DEF(NI700PCIE, 0x0d0fd000, SKY1_IDM_DEF_TIMEOUT_VALUE)     \
	IDM_ARGS_DEF(NI700SYS, 0x0d0fe000, SKY1_IDM_DEF_TIMEOUT_VALUE)      \
	IDM_ARGS_DEF(PCIEX421, 0x0d0ff000, SKY1_IDM_PCIE_TIMEOUT_VALUE)     \
	IDM_ARGS_DEF(PCIEX8, 0x0d100000, SKY1_IDM_PCIE_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(S5SYS, 0x0d101000, SKY1_IDM_DEF_TIMEOUT_VALUE)         \
	IDM_ARGS_DEF(SF, 0x0d102000, SKY1_IDM_DEF_TIMEOUT_VALUE)            \
	IDM_ARGS_DEF(SMMU0, 0x0d103000, SKY1_IDM_DEF_TIMEOUT_VALUE)         \
	IDM_ARGS_DEF(SMMU1, 0x0d104000, SKY1_IDM_DEF_TIMEOUT_VALUE)         \
	IDM_ARGS_DEF(SMMU2, 0x0d105000, SKY1_IDM_DEF_TIMEOUT_VALUE)         \
	IDM_ARGS_DEF(SMNRCSU, 0x0d106000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(SYS_ETH, 0x0d107000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(USB2H0, 0x0d108000, SKY1_IDM_DEF_TIMEOUT_VALUE)        \
	IDM_ARGS_DEF(USB2H1, 0x0d109000, SKY1_IDM_DEF_TIMEOUT_VALUE)        \
	IDM_ARGS_DEF(USB2H2, 0x0d10a000, SKY1_IDM_DEF_TIMEOUT_VALUE)        \
	IDM_ARGS_DEF(USB2H3, 0x0d10b000, SKY1_IDM_DEF_TIMEOUT_VALUE)        \
	IDM_ARGS_DEF(USB3A, 0x0d10c000, SKY1_IDM_DEF_TIMEOUT_VALUE)         \
	IDM_ARGS_DEF(USB3CH0, 0x0d10d000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(USB3CH1, 0x0d10e000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(USB3CH2, 0x0d10f000, SKY1_IDM_DEF_TIMEOUT_VALUE)       \
	IDM_ARGS_DEF(USB3CH3, 0x0d110000, SKY1_IDM_DEF_TIMEOUT_VALUE)

#define IDM_ERRCTLR 0x108
#define IDM_TIMEOUT_CONTROL 0x150
#define IDM_TIMEOUT_VALUE 0x154
#define IDM_RESET_CONTROL 0x140

#define IDM_ERRSTATUS_S 0x110
#define IDM_ERRADDR_LSB_S 0x114
#define IDM_ERRADDR_MSB_S 0x118
#define IDM_RESET_STATUS_S 0X144
#define IDM_INTERRUPT_STATUS_S 0x158
#define IDM_INTERRUPT_MASK_S 0X15C

#define IDM_ERRSTATUS_NS 0x160
#define IDM_ERRADDR_LSB_NS 0x164
#define IDM_ERRADDR_MSB_NS 0x168
#define IDM_RESET_STATUS_NS 0X194
#define IDM_INTERRUPT_STATUS_NS 0x1A8
#define IDM_INTERRUPT_MASK_NS 0X1AC
#define MAX_INTR_EVENT_BITS 4

#define SKY1_IDM_DEF_TIMEOUT_VALUE (0x10)
#define SKY1_IDM_PCIE_TIMEOUT_VALUE (0x14)

#define MAX_IDM_NAME (12)

/* Output idm logs as verbose */
#define IDM_VERBOSE(...) VERBOSE("IDM: " __VA_ARGS__)
#define IDM_LOG(...) INFO("IDM: " __VA_ARGS__)
#define IDM_ERR(...) ERROR("IDM: " __VA_ARGS__)

#define IDM_AUXDATA(name, base, timeout)                    \
	static const struct IDM_AUX_DATA name##_idm_aux = { \
		#name,                                      \
		base,                                       \
		timeout,                                    \
	}

#define IDM_RECORD_DEF(name)                              \
	ERR_RECORD_MEMMAP_V1(name##_idm_aux.phy_addr, 1,  \
			     sky1_idm_err_record_probe,   \
			     sky1_idm_err_record_handler, \
			     (void *)&name##_idm_aux)

struct IDM_INFO {
	uint8_t is_secure;
	int8_t error_idm_index;
	uintptr_t error_address;
	unsigned int error_type; // store intr status
	unsigned int error_status; // store error status
	uint32_t error_intr_num;
	char name[MAX_IDM_NAME];
};

struct IDM_AUX_DATA {
	char name[MAX_IDM_NAME];
	uintptr_t phy_addr;
	uint8_t timeout;
};

struct IDM_DATAREGS_OFFSET {
	uint32_t ERRSTATS;
	uint32_t ERRADDR_LSB;
	uint32_t ERRADDR_MSB;
	uint32_t RESET_STATUS;
	uint32_t INTR_STATUS;
	uint32_t INTR_MASK;
};

static int sky1_idm_err_record_probe(const struct err_record_info *info,
				     int *probe_data);
static int
sky1_idm_err_record_handler(const struct err_record_info *info, int probe_data,
			    const struct err_handler_data *const data);

#undef IDM_ARGS_DEF
#define IDM_ARGS_DEF(arg0, arg1, arg2) IDM_AUXDATA(arg0, arg1, arg2);
IDM_ARGS_LIST;

#undef IDM_ARGS_DEF
#define IDM_ARGS_DEF(arg0, arg1, arg2) IDM_RECORD_DEF(arg0),
struct err_record_info idm_records[] = { IDM_ARGS_LIST };

static const struct IDM_DATAREGS_OFFSET idm_data_offset[] = {
	[0] = {
		.ERRSTATS = IDM_ERRSTATUS_S,
		.ERRADDR_LSB = IDM_ERRADDR_LSB_S,
		.ERRADDR_MSB = IDM_ERRADDR_MSB_S,
		.RESET_STATUS = IDM_RESET_STATUS_S,
		.INTR_STATUS = IDM_INTERRUPT_STATUS_S,
		.INTR_MASK = IDM_INTERRUPT_MASK_S,
	},
	[1] = {
		.ERRSTATS = IDM_ERRSTATUS_NS,
		.ERRADDR_LSB = IDM_ERRADDR_LSB_NS,
		.ERRADDR_MSB = IDM_ERRADDR_MSB_NS,
		.RESET_STATUS = IDM_RESET_STATUS_NS,
		.INTR_STATUS = IDM_INTERRUPT_STATUS_NS,
		.INTR_MASK = IDM_INTERRUPT_MASK_NS,
	}
};

static uintptr_t g_data_addr = 0;

static void set_idm_enable(struct IDM_AUX_DATA *aux, int enable)
{
	if (!aux)
		return;
	if (enable) {
		IDM_VERBOSE("set %s idm enable ... \n", aux->name);
		mmio_write_32(aux->phy_addr + IDM_TIMEOUT_VALUE, aux->timeout);
		mmio_write_32(aux->phy_addr + IDM_TIMEOUT_CONTROL, 0x1);
		mmio_write_32(aux->phy_addr + IDM_ERRCTLR, 7);
	} else {
		IDM_VERBOSE("set %s idm disable ... \n", aux->name);
		mmio_write_32(aux->phy_addr + IDM_TIMEOUT_CONTROL, 0x0);
		mmio_write_32(aux->phy_addr + IDM_ERRCTLR, 0);
	}
}

void sky1_idm_init(void)
{
	struct IDM_AUX_DATA *aux = NULL;

	for (int i = 0; i < ARRAY_SIZE(idm_records); i++) {
		aux = (struct IDM_AUX_DATA *)idm_records[i].aux_data;
		set_idm_enable(aux, 1);
	}
}

int set_idm_data_address(uint64_t addr, uint64_t arg1, uint64_t arg2)
{
	g_data_addr = addr;
	return 0;
}

static int sky1_idm_err_record_probe(const struct err_record_info *info,
				     int *probe_data)
{
	struct IDM_AUX_DATA *aux = NULL;
	uint32_t data;
	struct IDM_INFO *idm_info = (struct IDM_INFO *)g_data_addr;

	assert(info->version == ERR_HANDLER_VERSION);

	for (int i = 0; i < ARRAY_SIZE(idm_records); i++) {
		aux = (struct IDM_AUX_DATA *)idm_records[i].aux_data;
		if (!aux)
			continue;
		for (int j = 0; j < ARRAY_SIZE(idm_data_offset); j++) {
			data = mmio_read_32(aux->phy_addr +
					    idm_data_offset[j].INTR_STATUS);
			if (!data)
				continue;

			/*clear status*/
			mmio_write_32(aux->phy_addr +
					      idm_data_offset[j].INTR_STATUS,
				      data);
			/*disable idm*/
			set_idm_enable(aux, 0);
			if (idm_info) {
				memset(idm_info, 0, sizeof(*idm_info));
				idm_info->error_type = data;
			}

			*probe_data = i * ARRAY_SIZE(idm_data_offset) + j;
			return 1;
		}
	}

	return 0;
}

static int
sky1_idm_err_record_handler(const struct err_record_info *info, int probe_data,
			    const struct err_handler_data *const data)
{
	struct IDM_AUX_DATA *aux = NULL;
	const struct IDM_DATAREGS_OFFSET *offset = NULL;
	struct IDM_INFO *idm_info = (struct IDM_INFO *)g_data_addr;
	uint64_t erraddr;
	uint32_t errstatus, nsecure;
	const struct sky1_ras_ev_map *evmap;
	int ret;

	if (probe_data < 0 ||
	    probe_data >= ARRAY_SIZE(idm_records) * ARRAY_SIZE(idm_data_offset))
		return 0;

	aux = idm_records[probe_data / ARRAY_SIZE(idm_data_offset)].aux_data;
	nsecure = probe_data % ARRAY_SIZE(idm_data_offset);
	offset = &idm_data_offset[nsecure];

	errstatus = mmio_read_32(aux->phy_addr + offset->ERRSTATS);
	erraddr = mmio_read_32(aux->phy_addr + offset->ERRADDR_LSB);
	erraddr |= ((uint64_t)mmio_read_32(aux->phy_addr + offset->ERRADDR_MSB)
		    << 32);

	IDM_ERR("%s %s access, error status=0x%x, addr=0x%lx \n", aux->name,
		nsecure ? "non-secure" : "secure", errstatus, erraddr);
	plat_ic_end_of_interrupt(data->interrupt);

	if (!idm_info)
		return 0;

	/*prepare err data*/
	idm_info->error_address = erraddr;
	idm_info->is_secure = !nsecure;
	idm_info->error_intr_num = data->interrupt;
	idm_info->error_status = errstatus;
	memcpy(idm_info->name, aux->name, sizeof(idm_info->name));
	flush_dcache_range((uintptr_t)idm_info, sizeof(*idm_info));
#if !SDEI_SUPPORT
	return 0;
#else
	/*find events*/
	evmap = find_ras_event_map_by_intr(data->interrupt);
	/*send to kernel*/
#ifdef RAM_LOG_SUPPORT
	rlog_flush_data();
#endif
	ret = sdei_dispatch_event(evmap->sdei_ev_num);
	if (ret) {
		IDM_ERR("send ras event[%d] failed, ret=%d \n",
			evmap->sdei_ev_num, ret);
		return 0;
	}

	/*reset idm*/
	mmio_write_32(aux->phy_addr + IDM_RESET_CONTROL, 0x2);
	for (int i = 0; i < 100; i++) {
		mdelay(1);
		if (!mmio_read_32(aux->phy_addr + offset->RESET_STATUS)) {
			set_idm_enable(aux, 1);
			return 0;
		}
	}
	IDM_ERR("%s idm reset failed... \n", aux->name);

	return 0;
#endif
}
