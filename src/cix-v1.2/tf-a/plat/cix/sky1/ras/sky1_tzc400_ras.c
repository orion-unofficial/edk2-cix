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
#include "plat/common/platform.h"
#include "services/sdei.h"
#include <lib/extensions/ras.h>
#include "sky1_ras_private.h"
#ifdef RAM_LOG_SUPPORT
#include <lib/rlog.h>
#endif

#define TZC400_BUILD_CFG (0x00)
#define TZC400_ACTION_REGISTER (0x04)
#define TZC400_INT_STATUS (0x10)
#define TZC400_INT_CLEAR (0x14)
#define TZC400_FAIL_ADDRESS_LOW(x) (0x20 + 0x10 * x)
#define TZC400_FAIL_ADDRESS_HIGH(x) (0x24 + 0x10 * x)
#define TZC400_FAIL_CONTROL(x) (0x28 + 0x10 * x)
#define TZC400_FAIL_ID(x) (0x2C + 0x10 * x)

#define TZC400_INT_STATUS_BIT_OFFSET (0)
#define TZC400_INT_OVERRUN_BIT_OFFSET (8)
#define TZC400_INT_OVERLAP_BIT_OFFSET (16)

#define TZC400_FILTER_CNT_BIT_OFFSET (24)
#define TZC400_FILTER_CNT_MASK (0x03)

#define TZC400_FAIL_CTRL_MASK (0x01300000)
#define TZC400_FAIL_ID_MASK (0x000000FF)

#define MAX_TZC400_NAME (8)
#define MAX_TZC400_FILTERS (4)

/* Output tzc400 logs as verbose */
#define TZC400_VERBOSE(...) VERBOSE("TZC400: " __VA_ARGS__)
#define TZC400_LOG(...) INFO("TZC400: " __VA_ARGS__)
#define TZC400_ERR(...) ERROR("TZC400: " __VA_ARGS__)

#define TZC400_RECORD_DEF(index)                             \
	ERR_RECORD_MEMMAP_V1(TZC400_BASE(index), 1,          \
			     sky1_tzc400_err_record_probe,   \
			     sky1_tzc400_err_record_handler, \
			     (void *)&tzc400_aux[index])

struct TZC400_ERROR_INFO {
	uintptr_t fail_address;
	unsigned fail_ctrl;
	unsigned fail_id;
	unsigned intr_status; // store intr status
};

struct TZC400_AUX_DATA {
	char name[MAX_TZC400_NAME];
	int sdei_ev_num; /* SDEI Event number */
	uint32_t filter_count;
	struct TZC400_ERROR_INFO *error_info;
};

static int sky1_tzc400_err_record_probe(const struct err_record_info *info,
					int *probe_data);
static int
sky1_tzc400_err_record_handler(const struct err_record_info *info,
			       int probe_data,
			       const struct err_handler_data *const data);

static struct TZC400_AUX_DATA tzc400_aux[] = {
	{ "TZC0", SDEI_SKY1_TZC400_0_EVENT, 0, NULL },
	{ "TZC1", SDEI_SKY1_TZC400_1_EVENT, 0, NULL },
	{ "TZC2", SDEI_SKY1_TZC400_2_EVENT, 0, NULL },
	{ "TZC3", SDEI_SKY1_TZC400_3_EVENT, 0, NULL },
};

struct err_record_info tzc400_records[] = {
	TZC400_RECORD_DEF(0),
	TZC400_RECORD_DEF(1),
	TZC400_RECORD_DEF(2),
	TZC400_RECORD_DEF(3),
};

int set_tzc400_data_address(uint64_t ev_num, uint64_t addr, uint64_t arg2)
{
	int index;

	for (index = 0; index < ARRAY_SIZE(tzc400_aux); index++) {
		if (tzc400_aux[index].sdei_ev_num == ev_num)
			break;
	}
	if (index >= ARRAY_SIZE(tzc400_aux)) {
		TZC400_ERR("%s, event%ld out of range \n", __func__, ev_num);
		return -1;
	}

	tzc400_aux[index].error_info = (struct TZC400_ERROR_INFO *)addr;
	return 0;
}

static int sky1_tzc400_err_record_probe(const struct err_record_info *info,
					int *probe_data)
{
	struct TZC400_AUX_DATA *aux = NULL;
	uint32_t data;

	assert(info->version == ERR_HANDLER_VERSION);

	aux = info->aux_data;
	assert(aux);

	data = mmio_read_32(info->memmap.base_addr + TZC400_BUILD_CFG);
	aux->filter_count = ((data >> TZC400_FILTER_CNT_BIT_OFFSET) &
			     TZC400_FILTER_CNT_MASK) +
			    1;
	data = mmio_read_32(info->memmap.base_addr + TZC400_INT_STATUS);
	if (!data)
		return 0;
	mmio_write_32(info->memmap.base_addr + TZC400_INT_CLEAR, data);
	*probe_data = data;

	return 1;
}

static int
sky1_tzc400_err_record_handler(const struct err_record_info *info,
			       int probe_data,
			       const struct err_handler_data *const data)
{
	struct TZC400_ERROR_INFO err;
	struct TZC400_AUX_DATA *aux = NULL;
	uint32_t phy_addr;
	int ret;

	aux = info->aux_data;
	assert(aux);

	phy_addr = info->memmap.base_addr;
	for (int i = 0; i < aux->filter_count; i++) {
		if (!(probe_data & BIT(i)))
			continue;
		err.fail_address =
			mmio_read_32(phy_addr + TZC400_FAIL_ADDRESS_HIGH(i));
		err.fail_address = err.fail_address << 32;
		err.fail_address +=
			mmio_read_32(phy_addr + TZC400_FAIL_ADDRESS_LOW(i));
		err.fail_ctrl = mmio_read_32(phy_addr + TZC400_FAIL_CONTROL(i));
		err.fail_ctrl &= TZC400_FAIL_CTRL_MASK;
		err.fail_id = mmio_read_32(phy_addr + TZC400_FAIL_ID(i));
		err.fail_id &= TZC400_FAIL_ID_MASK;
		TZC400_ERR(
			"error[%d] address=0x%lx fail_ctl=0x%x fail_id=0x%x \n",
			i, err.fail_address, err.fail_ctrl, err.fail_id);

		if (!aux->error_info)
			continue;
		memcpy(&aux->error_info[i], &err, sizeof(err));
	}
	plat_ic_end_of_interrupt(data->interrupt);

	if (!aux->error_info)
		return 0;

#if !SDEI_SUPPORT
	return 0;
#else
	flush_dcache_range((uintptr_t)aux->error_info,
			   sizeof(*aux->error_info) * MAX_TZC400_FILTERS);
	/*send sdei event*/
#ifdef RAM_LOG_SUPPORT
	rlog_flush_data();
#endif
	ret = sdei_dispatch_event(aux->sdei_ev_num);
	if (ret)
		TZC400_ERR("send ras event[%d] failed, ret=%d \n",
			   aux->sdei_ev_num, ret);

	return 0;
#endif
}
