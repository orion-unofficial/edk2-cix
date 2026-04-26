/**************************************************************************************/
/*                          COPYRIGHT INFORMATION                                     */
/*     Copyright 2024 Cix Technology Group Co., Ltd.                             */
/*     All Rights Reserved.                                                           */
/*                                                                                    */
/*     The following programs are the sole property of Cix Technology Group      */
/*     Co., Ltd., and contain its proprietary and confidential information.           */
/*                                                                                    */
/*                                                                                    */
/**************************************************************************************/
/*
 **************************************************************************************
 *
 *                 ipc.h
 *
 * Filename      : ipc.h
 * Programmer(s) : China Security team
 * Author        : Shuai Fengyun
 * Mail          : Abel.Shuai@cixcomputing.com
 * Create Time   : 2023-11-09 16:13:33
 **************************************************************************************
 */

#ifndef MOUDLE_CIX_IPC_H_
#define MOUDLE_CIX_IPC_H_




/*
 *******************************************************************************
 *                                INCLUDE FILES
 *******************************************************************************
*/
#include <tee_api_types.h>
#include <string.h>





/*
 *******************************************************************************
 *                  MACRO DEFINITION USED ONLY BY THIS MODULE
 *******************************************************************************
*/
#define IPC_RESULT_SUCCESS	0x00
#define IPC_RESULT_FAIL	0x01

#define FUSE_HWUK_OFFSET	(2880U)
#define FUSE_HWUK_SIZE	(128U)

/* version 0.1 */

/* define same as fuse table this version */
/* common */
#define KM_CFG_VERSION 0
#define KM_CFG_LC 1
#define KM_CFG_ASYM_ALG_TYPE 2
#define KM_CFG_SEC_ENABLE 3
#define KM_CFG_ENC_ENABLE 4
/* 64 bits serial num + 64 bits chip ip in fuse, 4 word*/
#define KM_CFG_SERIAL_NUM 5
#define KM_CFG_MAX 16

/* 128 bits for oem model key, 4 word */
/* config region */
/* 1. common config for all */
/* version 0.2 */
/* define same as fuse table this version */
#define KM_CFG_VERSION 0
#define KM_CFG_LC 1
#define KM_CFG_ASYM_ALG_TYPE 2
#define KM_CFG_SEC_ENABLE 3
#define KM_CFG_ENC_ENABLE 4
/* 64 bits serial num + 64 bits chip ip in fuse, 4 word*/
#define KM_CFG_SERIAL_NUM 5

/* 2. pbl config */

/* 3. TEE config */
/* 128 bits for tee huk, 4 word */
#define TA_ENC_ENABLE_OFFSET 9

/* Device key info config */
#define KM_CFG_TEE_HUK 0
#define KEY_TA_ENC_KEY 16



/* TEE key offset */
#define KM_DEVICE_KEY_OFFSET 0
#define KM_MODEL_KEY_OFFSET 16

/* TEE config size */
#define TA_ENC_CONFIG_SIZE   4

/* TEE key size */
#define KM_DEVICE_KEY_SIZE   16
#define KM_MODEL_KEY_SIZE   32




/* boot time config*/
#define BOOT_CFG_MAX 256
/* key config*/
#define KEY_CFG_MAX 1024 * 2


/*
 *******************************************************************************
 *                STRUCTRUE DEFINITION USED ONLY BY THIS MODULE
 *******************************************************************************
*/


typedef struct {
	uint32_t offset;
	uint32_t len; /* Length of fuse data */
	uint32_t result;
	uint32_t data_len; /* Length of fuse data */
} cix_tee_req_info;

/* max size 4k */
typedef struct _km_meta_tag {
	uint8_t hpubk[32];
	uint8_t rng[64];
	uint32_t config[KM_CFG_MAX];
	uint8_t boot_cfg[BOOT_CFG_MAX];
	uint8_t key_cfg[KEY_CFG_MAX];
} csec_km_meta_t;


typedef enum _kid_tag {
	KEY_ID_TA_CONFIG_ENABLE,
	KEY_ID_DEVICE_KEY,
	KEY_ID_MODEL_KEY,
	KID_MAX,
} csec_meta_key_id_t;



#ifndef MOUDLE_CIX_IPC_C_


/*
 *******************************************************************************
 *                      VARIABLES SUPPLIED BY THIS MODULE
 *******************************************************************************
*/





/*
 *******************************************************************************
 *                      FUNCTIONS SUPPLIED BY THIS MODULE
 *******************************************************************************
*/
extern TEE_Result cix_get_efuse(uint32_t offset, uint32_t len, uint8_t* fuse_data, uint32_t* fuse_len);
extern TEE_Result cix_get_key_info(csec_meta_key_id_t id, uint32_t req_len, uint8_t* output, uint32_t* rsp_len);

#endif

#endif  /* MOUDLE_NAME_H*/
