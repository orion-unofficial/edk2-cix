/*
 * Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _MAILBOX_H
#define _MAILBOX_H

#include <stdint.h>

#define CIX_MBOX_MSG_LEN	(32)
#define CH_NUM_AP2SE        (0)
#define MBOX_MSG_LEN_MSK    (0x7fL)
#define ROUNDUP(x, y) (((x) + (y) -1)/y)
#define MBOX_HEADER_SIZE	(sizeof(uint32_t) * 2)

/**
 * struct mbox_msg_t -  msg struct of a mailbox.
 * @size:               the size of msg.
 * @type:               type of the msg.
 * @reserve1:           reserve for externsion
 * @cmd_id:           reserve for externsion..
 * @data:               the point of raw data.
 */
struct mbox_msg_t {
	uint32_t size : 7;
	uint32_t type : 3;
	uint32_t reserve1 : 22;
	uint32_t cmd_id;
	uint32_t data[CIX_MBOX_MSG_LEN - 2];
};

enum mbox_cmd {
	FFA_ECHO_REQUEST = 0x82000001,
	FFA_GET_FUSE_BY_ID = 0x82000002,
	FFA_DEBUG = 0x82000003,
	FFA_GET_POWER = 0x82000004,
	FFA_SET_POWER = 0x82000005,
	FFA_FUSE_DIS_MEM_REPAIR = 0x82000006,
	FFA_SUSPEND = 0x82000007,
	FFA_RESUME = 0x82000008,
	FFA_CRYPTO_RSA = 0x82000009,
	FFA_CRYPTO_SM2 = 0x8200000A,
	FFA_CRYPTO_SCA = 0x8200000B,
	FFA_CRYPTO_HASH = 0x8200000C,
	FFA_CRYPTO_HMAC = 0x8200000D,
	FFA_GET_FUSE_BY_ADDR = 0x8200000E,
	FFA_GET_FW_VERSION = 0x8200000F,
	FFA_GET_TRNG = 0x82000010,
};

#define MEMR_DIS_DONE 0x1
#define MEMR_DIS_NO_NEED 0x2
#define MEMR_DIS_ERR 0x3

/* Comment unexport group id */
typedef enum {
	MEMR_GROUP_ID_CPU = 1,
	// MEMR_GROUP_ID_CI700 = 2,
	MEMR_GROUP_ID_DPU = 3,
	MEMR_GROUP_ID_GPU = 4,
	MEMR_GROUP_ID_NPU = 5,
	MEMR_GROUP_ID_AUDIO = 6,
	MEMR_GROUP_ID_ISP = 7,
	MEMR_GROUP_ID_VPU = 8,
	// MEMR_GROUP_ID_CSU_PM = 9,
	// MEMR_GROUP_ID_CSU_SE = 10,
	 MEMR_GROUP_ID_MMHUB = 11,
	MEMR_GROUP_ID_PCIE = 12,
	// MEMR_GROUP_ID_SENSOR = 13,
	// MEMR_GROUP_ID_DP = 14,
	// MEMR_GROUP_ID_GMAC = 15,
	// MEMR_GROUP_ID_MIPI = 16,
	 MEMR_GROUP_ID_SMMU_MM = 17,
	 MEMR_GROUP_ID_SMMU_PCIE = 18,
	// MEMR_GROUP_ID_SMMU_SYS = 19,
	// MEMR_GROUP_ID_USB = 20,
	// MEMR_GROUP_ID_FCH = 21,
	// MEMR_GROUP_ID_MAX = MEMR_GROUP_ID_FCH,
} MEMR_GROUP_ID;

typedef enum {
	SEC_SHA_MODE_256,
	SEC_SHA_MODE_384,
	SEC_SHA_MODE_512,
	SEC_SM3_MODE,
} sec_sha_mode_t;

typedef enum {
	IPC_HASH_ONETIME = 0x0,
	IPC_HASH_FINAL = 0x1,
	IPC_HASH_INITIAL = 0x2,
	IPC_HASH_UPDATE = 0x3,
} ipc_hash_mode;

typedef struct _efuse_parameter {
	uint32_t addr;
	uint32_t offset;
	uint32_t size;
} efuse_parameter;

typedef struct _efuse_respon {
	int32_t errcode;
	uint32_t data[CIX_MBOX_MSG_LEN - 3];
} efuse_respon;

typedef struct _ipc_hash_parameter {
	uint32_t idx;
	uint32_t dma_in_addr;
	uint32_t dma_in_data_len;
	uint32_t algo; // sec_sha_mode_t
	uint32_t mode; // ipc_hash_mode
	uint32_t digest[16];
} ipc_hash_parameter;

typedef struct _ipc_hash_respon {
	int32_t errcode;
	uint32_t digest_len;
	uint32_t digest[16];
} ipc_hash_respon;

typedef struct _ipc_fw_version_respon {
	int32_t errcode;
	uint32_t fw_ver_maj;
	uint32_t fw_ver_min;
} ipc_fw_version_respon;

typedef struct _ipc_trng_parameter {
	uint32_t trng_len;
} ipc_trng_parameter;

typedef struct _ipc_trng_respon {
	int32_t errcode;
	uint32_t trng_len;
	uint8_t trng[64];
} ipc_trng_respon;

typedef struct _ipc_dis_memr_parameter {
	uint32_t group_id;
	uint32_t index;
} ipc_dis_memr_parameter;

typedef struct _ipc_dis_memr_respon {
	int32_t errcode;
	uint32_t dis_memr_done;
} ipc_dis_memr_respon;

void plat_cix_mbox_init(void);
void plat_cix_mbox_resume(void);
int mbox_send_cmd_sync(enum mbox_cmd cmd_id,
                       void *para,
                       uint32_t para_size,
                       struct mbox_msg_t *msg_rsp);
void mbox_memrepair_distribute(void *memr_info, void *memr_rsp);

#endif
