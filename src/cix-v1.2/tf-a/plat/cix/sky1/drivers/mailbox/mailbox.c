/*
 * Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <lib/mmio.h>
#include <stdbool.h>
#include <lib/spinlock.h>
#include <lib/utils_def.h>
#include <common/debug.h>
#include "mailbox.h"
#include <assert.h>
#include <drivers/delay_timer.h>
#include <common/debug.h>

#define MBOX_DEMO_ENABLE 0

#define CIX_MBOX_MSG_LEN (32)
#define MBOX_2_BASE (0x05080000)
#define MBOX_2_DB_ADDR (MBOX_2_BASE + CIX_MBOX_MSG_LEN * 0x4)
#define MBOX_2_STAS_ADDR (MBOX_2_DB_ADDR + 0x4)

#define PUB_APB_REG_BASE 0x05040000
#define CLK_EN (PUB_APB_REG_BASE + 0x0)
#define MBOX_2_EN BIT(6)

#define CH_STATUS_BUSY BIT(0)
#define DB_INT_BIT BIT(0)
#define MBOX_CMD_MSG_LEN (1)
#define WAIT_TIMEOUT (2000)

#if MBOX_DEMO_ENABLE
#define GROUP_INDEX_NONE 0x00000001
#define REPAIR_ALL_DONE_PASS_REG 0x10
#define GROUP_REPAIR_EN 0x14
#define GROUP_REPAIR_DONE_BUSY_REG 0x18
#define MEMORY_REAPIR_CNT (1000 * 10000)

static uint8_t test_msg[] = { 0x61, 0x62, 0x63 };
static uint8_t sha256_digest[] = {
	0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
	0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
};
#endif

struct mbox_context {
	uint32_t reg_data_addr;
	uint32_t reg_db_addr;
	uint32_t reg_stas_addr;
	spinlock_t *lock;
	int is_initialized;
	uint32_t clk_en_mask;
};

static spinlock_t mbox_2_lock;

struct mbox_context s_mbox_ctx[] = {
	/* mailbox context of ap2se_tee */
	{
		.reg_data_addr = MBOX_2_BASE,
		.reg_db_addr = MBOX_2_DB_ADDR,
		.reg_stas_addr = MBOX_2_STAS_ADDR,
		.lock = &mbox_2_lock,
		.is_initialized = false,
		.clk_en_mask = MBOX_2_EN,
	}
};

struct mbox_context *get_mbox_context(int index)
{
	if (index < (sizeof(s_mbox_ctx) / sizeof(s_mbox_ctx[0])))
		return &s_mbox_ctx[index];

	return NULL;
}

static void mbox_ring_doorbell(struct mbox_context *mbox_ctx)
{
	uint32_t val32;

	val32 = mmio_read_32(mbox_ctx->reg_db_addr);
	val32 |= DB_INT_BIT;
	mmio_write_32(mbox_ctx->reg_db_addr, val32);
}

static void mbox_set_channel_busy(struct mbox_context *mbox_ctx, bool busy)
{
	uint32_t val32 = mmio_read_32(mbox_ctx->reg_stas_addr);

	if (busy == true) {
		val32 &= ~CH_STATUS_BUSY;
	} else {
		val32 |= CH_STATUS_BUSY;
	}

	mmio_write_32(mbox_ctx->reg_stas_addr, val32);
}

static bool mbox_is_channel_busy(struct mbox_context *mbox_ctx)
{
	uint32_t val32 = mmio_read_32(mbox_ctx->reg_stas_addr);

	return (val32 && CH_STATUS_BUSY) ? false : true;
}

int mbox_send_data_sync(struct mbox_context *mbox_ctx, struct mbox_msg_t *msg, struct mbox_msg_t *msg_rsp)
{
	uint32_t *msg_raw_buf = (uint32_t *)msg;
	uint32_t len, *rsp_buf;
	int i;
	int wait_cnt = 0;

	len = ROUNDUP(msg->size, 4);
	for (i = 0; i < len; i++)
		mmio_write_32(mbox_ctx->reg_data_addr + 0x4 * i, msg_raw_buf[i]);

	/* set chn busy */
	mbox_set_channel_busy(mbox_ctx, true);

	mbox_ring_doorbell(mbox_ctx);

	while (mbox_is_channel_busy(mbox_ctx)) {
		if (wait_cnt++ > WAIT_TIMEOUT) {
			mbox_set_channel_busy(mbox_ctx, false);
			return -1;
		}
		mdelay(1);
	};

	if (msg_rsp) {
		/* Get the return values */
		rsp_buf = (uint32_t *)msg_rsp;
		len = mmio_read_32(mbox_ctx->reg_data_addr) & MBOX_MSG_LEN_MSK;
		len = ROUNDUP(len, 4);
		for (i = 0; i < len; i++)
			rsp_buf[i] = mmio_read_32(mbox_ctx->reg_data_addr + 0x4 * i);
	}

	return 0;
}

void mbox_get_channel(struct mbox_context *mbox_ctx)
{
	spin_lock(mbox_ctx->lock);
	/* Make sure any previous command has finished */
	assert(!mbox_is_channel_busy(mbox_ctx));
}

void mbox_put_channel(struct mbox_context *mbox_ctx)
{
	/* Make sure any previous command has finished */
	assert(!mbox_is_channel_busy(mbox_ctx));
	spin_unlock(mbox_ctx->lock);
}

int mbox_send_cmd_sync(enum mbox_cmd cmd_id, void *para, uint32_t para_size, struct mbox_msg_t *msg_rsp)
{
	struct mbox_msg_t msg = { 0 };
	struct mbox_context *mbox_ctx = &s_mbox_ctx[0];
	int ret;
	int i;

#if ENABLE_TF_A_FIXES
	if (cmd_id != FFA_GET_FUSE_BY_ID)
#endif
		NOTICE("Send CMD: 0x%x\n", cmd_id);

	if (!mbox_ctx->is_initialized)
		return -1;

	msg.cmd_id = cmd_id;
	msg.size = MBOX_HEADER_SIZE + para_size;
	memcpy((void *)msg.data, para, para_size);

	mbox_get_channel(mbox_ctx);

	ret = mbox_send_data_sync(mbox_ctx, &msg, msg_rsp);
	if (0 != ret) {
		INFO("Dump mbox regs:\n");
		for (i = 0; i < 13; i++) {
			INFO("%08x: %08x %08x %08x %08x\n", i * 16,
			     mmio_read_32(mbox_ctx->reg_data_addr + 16 * i + 0x0),
			     mmio_read_32(mbox_ctx->reg_data_addr + 16 * i + 0x4),
			     mmio_read_32(mbox_ctx->reg_data_addr + 16 * i + 0x8),
			     mmio_read_32(mbox_ctx->reg_data_addr + 16 * i + 0xc));
		}
	}

	mbox_put_channel(mbox_ctx);

	return ret;
}

static void mbox_echo_request(void)
{
	INFO("Send CMD: ECHO Request\n");

	if (mbox_send_cmd_sync(FFA_ECHO_REQUEST, NULL, 0, NULL)) {
		INFO("Send command failed!\n");
		return;
	}
	INFO("Got Echo RSP, Mbox channel is ready...\n");
}

/* Here is just examples of implementation.
 * You may place such APIs into other file rather than mailbox.c.
 */
void mbox_get_fuse(void *fuse_info, void *fuse_rsp)
{
	struct mbox_msg_t buf_rsp = { 0 };

	INFO("Send CMD: FFA_GET_FUSE_BY_ADDR\n");

	if (mbox_send_cmd_sync(FFA_GET_FUSE_BY_ADDR, fuse_info, sizeof(efuse_parameter), &buf_rsp)) {
		WARN("Send command failed!\n");
		return;
	}

	if (buf_rsp.cmd_id == FFA_GET_FUSE_BY_ADDR) {
		memcpy(fuse_rsp, buf_rsp.data, sizeof(efuse_respon));
	}

	INFO("Got Fuse RSP, size %d\n", buf_rsp.size);
	for (int i = 0; i < ROUNDUP(buf_rsp.size, 4); i++)
		INFO("0x%08x\n", buf_rsp.data[i]);
}

void mbox_crypto_hash(void *hash_info, void *hash_rsp)
{
	struct mbox_msg_t buf_rsp = { 0 };

	INFO("Send CMD: FFA_CRYPTO_HASH\n");

	if (mbox_send_cmd_sync(FFA_CRYPTO_HASH, hash_info, sizeof(ipc_hash_parameter), &buf_rsp)) {
		WARN("Send command failed!\n");
		return;
	}

	if (buf_rsp.cmd_id == FFA_CRYPTO_HASH) {
		memcpy(hash_rsp, buf_rsp.data, sizeof(ipc_hash_respon));
	}

	INFO("Got Hash RSP, size %d\n", buf_rsp.size);
	for (int i = 0; i < ROUNDUP(buf_rsp.size, 4); i++)
		INFO("0x%08x\n", buf_rsp.data[i]);
}

void mbox_get_fw_version(void *fw_version_rsp)
{
	struct mbox_msg_t buf_rsp = { 0 };

	INFO("Send CMD: FFA_GET_FW_VERSION\n");

	if (mbox_send_cmd_sync(FFA_GET_FW_VERSION, NULL, 0, &buf_rsp)) {
		WARN("Send command failed!\n");
		return;
	}

	if (buf_rsp.cmd_id == FFA_GET_FW_VERSION) {
		memcpy(fw_version_rsp, buf_rsp.data, sizeof(ipc_fw_version_respon));
	}

	INFO("Got fw version RSP, size %d\n", buf_rsp.size);
	for (int i = 0; i < ROUNDUP(buf_rsp.size, 4); i++)
		INFO("0x%08x\n", buf_rsp.data[i]);
}

void mbox_get_trng(void *trng_info, void *trng_rsp)
{
	struct mbox_msg_t buf_rsp = { 0 };

	INFO("Send CMD: FFA_GET_TRNG_DATA\n");

	if (mbox_send_cmd_sync(FFA_GET_TRNG, trng_info, sizeof(ipc_trng_parameter), &buf_rsp)) {
		WARN("Send command failed!\n");
		return;
	}

	if (buf_rsp.cmd_id == FFA_GET_TRNG) {
		memcpy(trng_rsp, buf_rsp.data, sizeof(ipc_trng_respon));
	}

	INFO("Got TRNG RSP, size %d\n", buf_rsp.size);
	for (int i = 0; i < ROUNDUP(buf_rsp.size, 4); i++)
		INFO("0x%08x\n", buf_rsp.data[i]);
}

void mbox_memrepair_distribute(void *memr_info, void *memr_rsp)
{
	struct mbox_msg_t buf_rsp = { 0 };

	INFO("Send CMD: FFA_CRYPTO_HASH\n");

	if (mbox_send_cmd_sync(FFA_FUSE_DIS_MEM_REPAIR, memr_info, sizeof(ipc_dis_memr_parameter), &buf_rsp)) {
		WARN("Send command failed!\n");
		return;
	}

	if (buf_rsp.cmd_id == FFA_FUSE_DIS_MEM_REPAIR) {
		memcpy(memr_rsp, buf_rsp.data, sizeof(ipc_dis_memr_parameter));
	}

	INFO("Got Memrepair RSP, size %d\n", buf_rsp.size);
	for (int i = 0; i < ROUNDUP(buf_rsp.size, 4); i++)
		INFO("0x%08x\n", buf_rsp.data[i]);
}

void mbox_set_power(void *power_info)
{
	struct mbox_msg_t buf_rsp = { 0 };

	INFO("Send CMD: FFA_SET_POWER\n");

	if (mbox_send_cmd_sync(FFA_SET_POWER, power_info, 0, &buf_rsp)) {
		WARN("Send command failed!\n");
		return;
	}

	INFO("Got POWER RSP, size %d\n", buf_rsp.size);
}

void mbox_set_suspend(void *suspend_info)
{
	struct mbox_msg_t buf_rsp = { 0 };

	NOTICE("Send CMD: FFA_SUSPEND\n");

	if (mbox_send_cmd_sync(FFA_SUSPEND, suspend_info, sizeof(uint32_t), &buf_rsp)) {
		WARN("Send command failed!\n");
		return;
	}

	NOTICE("Got SUSPEND RSP, size %d\n", buf_rsp.size);
}

#if MBOX_DEMO_ENABLE
void mbox_get_fuse_demo(void)
{
	efuse_parameter fuse_para;
	efuse_respon fuse_res;

	/* chip id addr as example */
	fuse_para.addr = 456;
	fuse_para.offset = 0;
	fuse_para.size = 64;

	mbox_get_fuse(&fuse_para, &fuse_res);

	if (fuse_res.errcode == 0) {
		hexdump("fuse chip id", (uint8_t *)fuse_res.data, fuse_para.size / 8);
	} else {
		WARN("get fuse failed:%d\n", fuse_res.errcode);
		return;
	}

	INFO("CMD: FFA_GET_FUSE_BY_ADDR pass!\n");
}

void mbox_crypto_hash_demo(void)
{
	int32_t ret;
	ipc_hash_parameter hash_para;
	ipc_hash_respon hash_res;

	hash_para.dma_in_addr = (uint32_t)((uint64_t)test_msg);
	hash_para.dma_in_data_len = sizeof(test_msg);
	hash_para.algo = SEC_SHA_MODE_256;
	hash_para.mode = IPC_HASH_ONETIME;

	mbox_crypto_hash(&hash_para, &hash_res);

	if (hash_res.errcode == 0) {
		ret = memcmp(sha256_digest, hash_res.digest, hash_res.digest_len);
		if (ret != 0) {
			WARN("digest error!\n");
			return;
		}
	} else {
		WARN("crypto hash failed:%d\n", hash_res.errcode);
		return;
	}

	INFO("CMD: FFA_CRYPTO_HASH pass!\n");
}

void mbox_get_fw_version_demo(void)
{
	ipc_fw_version_respon fw_ver_res;

	mbox_get_fw_version(&fw_ver_res);

	if (fw_ver_res.errcode == 0) {
		INFO("#CiX SE firmware Sky1 Alpha%d.%d\n", fw_ver_res.fw_ver_maj, fw_ver_res.fw_ver_min);
	} else {
		WARN("get fw version failed:%d\n", fw_ver_res.errcode);
		return;
	}

	INFO("CMD: FFA_GET_FW_VERSION pass!\n");
}

void mbox_get_trng_demo(void)
{
	ipc_trng_parameter trng_para;
	ipc_trng_respon trng_res;

	trng_para.trng_len = 64;

	mbox_get_trng(&trng_para, &trng_res);

	if (trng_res.errcode == 0) {
		hexdump("trng data", (uint8_t *)trng_res.trng, trng_res.trng_len);
	} else {
		WARN("get trng failed:%d\n", trng_res.errcode);
		return;
	}

	INFO("CMD: FFA_GET_TRNG pass!\n");
}

uint32_t mbox_memrepair_distribute_demo(void)
{
	ipc_dis_memr_parameter memr_para;
	ipc_dis_memr_respon memr_res;
	uint32_t index, done_status, count = MEMORY_REAPIR_CNT;
	uint32_t rcsu_addr;

	memr_para.group_id = MEMR_GROUP_ID_AUDIO;
	memr_para.index = GROUP_INDEX_NONE;
	index = GROUP_INDEX_NONE;
	rcsu_addr = 0x07000000;

	mbox_memrepair_distribute(&memr_para, &memr_res);

	if (memr_res.errcode == 0) {
		if (memr_res.dis_memr_done == 1) {
			/* TODO trigger memrepair or just use the return value */
			INFO("trigger memrepair, group id:%d!\n", memr_para.group_id);
			mmio_write_32((rcsu_addr + GROUP_REPAIR_EN), index);

			do {
				done_status = mmio_read_32((rcsu_addr + REPAIR_ALL_DONE_PASS_REG));
				done_status = (done_status >> 0x1) & 0x3;

				count--;
				if (count == 0x0)
					INFO("%s, %d, done and pass failed, status = %d!\n", __func__, __LINE__,
					     done_status);
			} while (done_status != 0x3 && count != 0);

			return MEMR_DIS_DONE;
		} else {
			INFO("no need to do memrepair, group id:%d!\n", memr_para.group_id);
			return MEMR_DIS_NO_NEED;
		}
	} else {
		WARN("memrepair distribute failed:%d\n", memr_res.errcode);
		return MEMR_DIS_ERR;
	}

	INFO("CMD: FFA_FUSE_DIS_MEM_REPAIR pass!\n");
}
#endif

void mbox_clk_enable(struct mbox_context *mbox_ctx)
{
	uint32_t val32;

	val32 = mmio_read_32(CLK_EN);
	val32 |= mbox_ctx->clk_en_mask;
	mmio_write_32(CLK_EN, val32);
}

void mbox_clk_disable(struct mbox_context *mbox_ctx)
{
	uint32_t val32;

	val32 = mmio_read_32(CLK_EN);
	val32 &= ~mbox_ctx->clk_en_mask;
	mmio_write_32(CLK_EN, val32);
}

void plat_cix_mbox_init(void)
{
	struct mbox_context *mbox_ctx;

	INFO("drv_mbox_init\n");

	mbox_ctx = get_mbox_context(CH_NUM_AP2SE);
	if (!mbox_ctx) {
		WARN("mbox_ctx is null!\n");
		return;
	}
	/* enable mbox clk */
	mbox_clk_enable(mbox_ctx);
	/* set chn free */
	mbox_set_channel_busy(mbox_ctx, false);
	mbox_ctx->is_initialized = true;

	mbox_echo_request();

#if MBOX_DEMO_ENABLE
	/* TBD. place these user APIs into their own file */
	mbox_set_power(NULL);
	mbox_get_fuse_demo();
	mbox_get_fw_version_demo();
	mbox_get_trng_demo();
	mbox_crypto_hash_demo();
	mbox_memrepair_distribute_demo();
#endif
}

void plat_cix_mbox_deinit(void)
{
	struct mbox_context *mbox_ctx;
	INFO("drv_mbox_deinit\n");

	mbox_ctx = get_mbox_context(CH_NUM_AP2SE);
	if (!mbox_ctx) {
		WARN("mbox_ctx is null!\n");
		return;
	}
	/* set chn free */
	mbox_set_channel_busy(mbox_ctx, false);
	mbox_ctx->is_initialized = false;

	/* disable mbox clk */
	mbox_clk_disable(mbox_ctx);
}

void plat_cix_mbox_resume(void)
{
	struct mbox_context *mbox_ctx;

	mbox_ctx = get_mbox_context(CH_NUM_AP2SE);
	if (!mbox_ctx) {
		WARN("mbox_ctx is null!\n");
		return;
	}

	/* enable mbox clk */
	mbox_clk_enable(mbox_ctx);

	/* set chn free */
	mbox_set_channel_busy(mbox_ctx, false);
	mbox_ctx->is_initialized = true;

	INFO("drv_mbox_resume success\n");
}
