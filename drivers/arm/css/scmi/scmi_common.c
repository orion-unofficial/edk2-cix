/*
 * Copyright (c) 2017-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/arm/css/scmi.h>
#include <drivers/delay_timer.h>
#include "scmi_private.h"

#if HW_ASSISTED_COHERENCY
#define scmi_lock_init(lock)
#define scmi_lock_get(lock)		spin_lock(lock)
#define scmi_lock_release(lock)		spin_unlock(lock)
#else
#define scmi_lock_init(lock)		bakery_lock_init(lock)
#define scmi_lock_get(lock)		bakery_lock_get(lock)
#define scmi_lock_release(lock)		bakery_lock_release(lock)
#endif


/*
 * Private helper function to get exclusive access to SCMI channel.
 */
int scmi_get_channel(scmi_channel_t *ch)
{
	assert(ch->lock);
	scmi_lock_get(ch->lock);

	/* Make sure any previous command has finished */
	if (SCMI_IS_CHANNEL_FREE(
			((mailbox_mem_t *)(ch->info->scmi_mbx_mem))->status))
		return SCMI_E_SUCCESS;

	scmi_lock_release(ch->lock);
	return SCMI_E_BUSY;
}

/*
 * Private helper function to transfer ownership of channel from AP to SCP.
 */
void scmi_send_sync_command(scmi_channel_t *ch)
{
	mailbox_mem_t *mbx_mem = (mailbox_mem_t *)(ch->info->scmi_mbx_mem);

	SCMI_MARK_CHANNEL_BUSY(mbx_mem->status);

	/*
	 * Ensure that any write to the SCMI payload area is seen by SCP before
	 * we write to the doorbell register. If these 2 writes were reordered
	 * by the CPU then SCP would read stale payload data
	 */
	dmbst();

	ch->info->ring_doorbell(ch->info);
	/*
	 * Ensure that the write to the doorbell register is ordered prior to
	 * checking whether the channel is free.
	 */
	dmbsy();

	/* Wait for channel to be free */
	while (!SCMI_IS_CHANNEL_FREE(mbx_mem->status))
		;

	/*
	 * Ensure that any read to the SCMI payload area is done after reading
	 * mailbox status. If these 2 reads were reordered then the CPU would
	 * read invalid payload data
	 */
	dmbld();
}

/*
 * Private helper function to release exclusive access to SCMI channel.
 */
void scmi_put_channel(scmi_channel_t *ch)
{
	/* Make sure any previous command has finished */
	assert(SCMI_IS_CHANNEL_FREE(
			((mailbox_mem_t *)(ch->info->scmi_mbx_mem))->status));

	assert(ch->lock);
	scmi_lock_release(ch->lock);
}

/*
 * API to query the SCMI protocol version.
 */
int scmi_proto_version(void *p, uint32_t proto_id, uint32_t *version)
{
	mailbox_mem_t *mbx_mem;
	unsigned int token = 0;
	int ret;
	scmi_channel_t *ch = (scmi_channel_t *)p;

	ret = validate_scmi_channel(ch);
	if (ret != SCMI_E_SUCCESS)
		return ret;

	ret = scmi_get_channel(ch);
	if (ret != SCMI_E_SUCCESS)
		return ret;

	mbx_mem = (mailbox_mem_t *)(ch->info->scmi_mbx_mem);
	mbx_mem->msg_header = SCMI_MSG_CREATE(proto_id, SCMI_PROTO_VERSION_MSG,
							token);
	mbx_mem->len = SCMI_PROTO_VERSION_MSG_LEN;
	mbx_mem->flags = SCMI_FLAG_RESP_POLL;

	scmi_send_sync_command(ch);

	/* Get the return values */
	SCMI_PAYLOAD_RET_VAL2(mbx_mem->payload, ret, *version);
	assert(mbx_mem->len == SCMI_PROTO_VERSION_RESP_LEN);
	assert(token == SCMI_MSG_GET_TOKEN(mbx_mem->msg_header));

	scmi_put_channel(ch);

	return ret;
}

/*
 * API to query the protocol message attributes for a SCMI protocol.
 */
int scmi_proto_msg_attr(void *p, uint32_t proto_id,
		uint32_t command_id, uint32_t *attr)
{
	mailbox_mem_t *mbx_mem;
	unsigned int token = 0;
	int ret;
	scmi_channel_t *ch = (scmi_channel_t *)p;

	ret = validate_scmi_channel(ch);
	if (ret != SCMI_E_SUCCESS)
		return ret;

	ret = scmi_get_channel(ch);
	if (ret != SCMI_E_SUCCESS)
		return ret;

	mbx_mem = (mailbox_mem_t *)(ch->info->scmi_mbx_mem);
	mbx_mem->msg_header = SCMI_MSG_CREATE(proto_id,
				SCMI_PROTO_MSG_ATTR_MSG, token);
	mbx_mem->len = SCMI_PROTO_MSG_ATTR_MSG_LEN;
	mbx_mem->flags = SCMI_FLAG_RESP_POLL;
	SCMI_PAYLOAD_ARG1(mbx_mem->payload, command_id);

	scmi_send_sync_command(ch);

	/* Get the return values */
	SCMI_PAYLOAD_RET_VAL2(mbx_mem->payload, ret, *attr);
	assert(mbx_mem->len == SCMI_PROTO_MSG_ATTR_RESP_LEN);
	assert(token == SCMI_MSG_GET_TOKEN(mbx_mem->msg_header));

	scmi_put_channel(ch);

	return ret;
}

/*
 * SCMI Driver initialization API. Returns initialized channel on success
 * or NULL on error. The return type is an opaque void pointer.
 */
void *scmi_init(scmi_channel_t *ch)
{
	uint32_t version;
	int ret;
	int wait_cnt = 0;
#define MAX_WAIT_CNT (1000)

	assert(ch && ch->info);
	assert(ch->info->db_reg_addr);
	assert(ch->info->db_modify_mask);
	assert(ch->info->db_preserve_mask);
	assert(ch->info->ring_doorbell != NULL);

	assert(ch->lock);

	scmi_lock_init(ch->lock);

	ch->is_initialized = 1;

	while (wait_cnt < MAX_WAIT_CNT) {
		ret = scmi_proto_version(ch, SCMI_PWR_DMN_PROTO_ID, &version);
		if (ret == SCMI_E_SUCCESS) {
			break;
		} else {
#ifndef CIX_BOARD_EMU
			mdelay(1);
#endif
			wait_cnt++;
			if (wait_cnt % 20 == 0)
				WARN("Waiting for csu_pm...\n");
		}
	}
	if (ret != SCMI_E_SUCCESS) {
		WARN("SCMI power domain protocol version message failed\n");
		goto error;
	}

	if (!is_scmi_version_compatible(SCMI_PWR_DMN_PROTO_VER, version)) {
		WARN("SCMI power domain protocol version 0x%x incompatible with driver version 0x%x\n",
			version, SCMI_PWR_DMN_PROTO_VER);
		goto error;
	}

	VERBOSE("SCMI power domain protocol version 0x%x detected\n", version);

	ret = scmi_proto_version(ch, SCMI_SYS_PWR_PROTO_ID, &version);
	if ((ret != SCMI_E_SUCCESS)) {
		WARN("SCMI system power protocol version message failed\n");
		goto error;
	}

	if (!is_scmi_version_compatible(SCMI_SYS_PWR_PROTO_VER, version)) {
		WARN("SCMI system power management protocol version 0x%x incompatible with driver version 0x%x\n",
			version, SCMI_SYS_PWR_PROTO_VER);
		goto error;
	}

	VERBOSE("SCMI system power management protocol version 0x%x detected\n",
						version);

	INFO("SCMI driver initialized\n");

	return (void *)ch;

error:
	ch->is_initialized = 0;
	return NULL;
}

int scmi_send_msg_to_pm(void *p, scp_pm_msg *msg)
{
	scmi_mem_ext *mbx_mem;
	int ret = SCMI_E_SUCCESS;
	scmi_channel_t *ch = (scmi_channel_t *)p;
	int i;

	ret = validate_scmi_channel(ch);
	if (ret != SCMI_E_SUCCESS)
		return ret;

	ret = scmi_get_channel(ch);
	if (ret != SCMI_E_SUCCESS)
		return ret;

	if (msg->tx_len > PM_MSG_MAX_LEN_TX)
		return SCMI_E_INVALID_PARAM;

	mbx_mem = (scmi_mem_ext *)(ch->info->scmi_mbx_mem);
	mbx_mem->message_header &= ~0xFFFFL;
	mbx_mem->message_header |= msg->msg_id;
	mbx_mem->msgAttr = 0;
	mbx_mem->msgAttr |= (SCP_PM_MSG_LEN & 0x3fL);
	mbx_mem->msgAttr |= ((SCP_PM_MSG_TYPE & 0xffL) << SCP_PM_MSG_OFFSET);
	mbx_mem->flags = SCMI_FLAG_RESP_POLL;
	mbx_mem->length = msg->tx_len *sizeof(uint32_t) +
		sizeof(mbx_mem->pm_header);
	for (i = 0; i < msg->tx_len; i++) {
		mbx_mem->payload[i] = msg->tx_data[i];
	}
	scmi_send_sync_command(ch);

	/* Get the return values */
	if (mbx_mem->statusCode != SCMI_E_SUCCESS)
		return mbx_mem->statusCode;

	for (i = 0; i < PM_MSG_MAX_LEN_RX; i++) {
		msg->rx_data[i] = mbx_mem->payload[i+PM_MSG_MAX_LEN_TX];
	}
	scmi_put_channel(ch);

	return ret;
}
