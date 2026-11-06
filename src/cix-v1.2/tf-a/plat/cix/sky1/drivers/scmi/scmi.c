/*
 * Copyright 2022 CIX
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <assert.h>
#include <stdint.h>

#include <platform_def.h>

#include <drivers/scmi-msg.h>
#include <drivers/scmi.h>

#define SMT_BUFFER_BASE		SKY1_AP_ATF_NS_BASE

static struct scmi_msg_channel scmi_channel[] = {
	[0] = {
		.shm_addr = SMT_BUFFER_BASE,
		.shm_size = SMT_BUF_SLOT_SIZE,
	},
};

struct scmi_msg_channel *plat_scmi_get_channel(unsigned int agent_id)
{
	assert(agent_id < ARRAY_SIZE(scmi_channel));

	return &scmi_channel[agent_id];
}

static const char vendor[] = "CIX";
static const char sub_vendor[] = "";

const char *plat_scmi_vendor_name(void)
{
	return vendor;
}

const char *plat_scmi_sub_vendor_name(void)
{
	return sub_vendor;
}

/* Currently supporting Power Domains */
static const uint8_t plat_protocol_list[] = {
	SCMI_PROTOCOL_ID_POWER_DOMAIN,
	0U /* Null termination */
};

size_t plat_scmi_protocol_count(void)
{
	const size_t count = ARRAY_SIZE(plat_protocol_list) - 1U;

	return count;
}

const uint8_t *plat_scmi_protocol_list(unsigned int agent_id __unused)
{
	return plat_protocol_list;
}

void sky1_init_scmi_server(void)
{
	size_t i;

	INFO("Start sky1 scmi server!\n");
	for (i = 0U; i < ARRAY_SIZE(scmi_channel); i++)
		scmi_smt_init_agent_channel(&scmi_channel[i]);
}