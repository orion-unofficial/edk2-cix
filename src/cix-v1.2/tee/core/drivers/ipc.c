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
 *                 ipc.c
 *
 * Filename      : ipc.c
 * Programmer(s) : China Security team
 * Author        : Shuai Fengyun
 * Mail          : Abel.Shuai@cixcomputing.com
 * Create Time   : 2023-11-09 16:13:58
 **************************************************************************************
 */

#define MOUDLE_CIX_IPC_C_





/*
 *******************************************************************************
 *                                INCLUDE FILES
 *******************************************************************************
*/
#include "drivers/ipc.h"
#include <kernel/thread.h>
#include <mm/generic_ram_layout.h>
#include <mm/core_memprot.h>
#include <cix_ipc_cmd.h>





/*
 *******************************************************************************
 *                         FUNCTIONS SUPPLIED BY THIS MODULE
 *******************************************************************************
*/





/*
 *******************************************************************************
 *                          VARIABLES SUPPLIED BY THIS MODULE
 *******************************************************************************
*/





/*
 *******************************************************************************
 *                          FUNCTIONS USED ONLY BY THIS MODULE
 *******************************************************************************
*/
TEE_Result cix_get_efuse(uint32_t offset, uint32_t len, uint8_t* fuse_data, uint32_t* fuse_len);





/*
 *******************************************************************************
 *                          VARIABLES USED ONLY BY THIS MODULE
 *******************************************************************************
*/




/*
 *******************************************************************************
 *                               FUNCTIONS IMPLEMENT
 *******************************************************************************
*/
/*
 *- #Description  Get efuse data by ID.
 * @param   fuse_id           [IN] Fuse ID
 *                               - Type: uint32_t
 *                               - Range: N/A.
 * @param   fuse_data         [OUT] Point to fuse data
 *                               - Type: uint8_t*
 *                               - Range: N/A.
 * @param   fuse_len          [OUT] Point to length of fuse data
 *                               - Type: uint32_t*
 *                               - Range: N/A.
 *
 * @return     TEE_Result
 * @retval     resultval
 *
 *
 */

TEE_Result cix_get_key_info(csec_meta_key_id_t id, uint32_t req_len, uint8_t* output, uint32_t* rsp_len)
{
	csec_km_meta_t tee_key_meta;
	void *point_shm = NULL;
	TEE_Result ret = TEE_ERROR_GENERIC;

	point_shm = phys_to_virt(TEE_KEY_META_SHM_BASE, MEM_AREA_RAM_SEC, 1);
	memcpy(&tee_key_meta, point_shm, sizeof(csec_km_meta_t));
	switch (id) {
	case KEY_ID_DEVICE_KEY:
		if (req_len <= KM_DEVICE_KEY_SIZE) {
			memcpy(output, &(tee_key_meta.key_cfg[KM_DEVICE_KEY_OFFSET]), req_len);
			*rsp_len = req_len;
			EMSG("Get device key:");
			DHEXDUMP(output, *rsp_len);
			ret = TEE_SUCCESS;
		} else {
			EMSG("Wrong req size for device key\n");
			ret = TEE_ERROR_GENERIC;
		}
		break;
	case KEY_ID_MODEL_KEY:
		if (req_len <= KM_MODEL_KEY_SIZE) {
			memcpy(output, &(tee_key_meta.key_cfg[KM_MODEL_KEY_OFFSET]), req_len);
			*rsp_len = req_len;
			EMSG("Get model key:");
			DHEXDUMP(output, *rsp_len);
			ret = TEE_SUCCESS;
		} else {
			EMSG("Wrong req size for model key\n");
			ret = TEE_ERROR_GENERIC;
		}
		break;
	case KEY_ID_TA_CONFIG_ENABLE:
		if (req_len == TA_ENC_CONFIG_SIZE) {
			memcpy(output, &(tee_key_meta.config[TA_ENC_ENABLE_OFFSET]), TA_ENC_CONFIG_SIZE);
			*rsp_len = req_len;
			ret = TEE_SUCCESS;
		} else {
			EMSG("Wrong req size for TA config\n");
			ret = TEE_ERROR_GENERIC;
		}
		break;
	default:
		ret = TEE_ERROR_GENERIC;
		EMSG("Invalid key_id\n");
		break;
	}
}

TEE_Result cix_get_efuse(uint32_t offset, uint32_t len, uint8_t* fuse_data, uint32_t* fuse_len)
{
	void *point_shm = NULL;
	TEE_Result ret = TEE_ERROR_GENERIC;

	cix_tee_req_info* req_info = NULL;
	uint8_t *rsp_data = NULL;

	/** 1) Get address of share buffer */
	point_shm = phys_to_virt(TZ_TFA_SHM_BASE, MEM_AREA_RAM_SEC, 1);
	req_info = (cix_tee_req_info*)point_shm;

	/** 2) Package request */
	req_info->offset = offset;
	req_info->len = len;
	req_info->result = IPC_RESULT_FAIL;

	/** 3) Send request to TF-A */
	request_to_tf_a();

	/** 4) Check return result */
	if (IPC_RESULT_FAIL == req_info->result) {
		ret = TEE_ERROR_IPC_FAIL;
	}

	/** 5) Get respond data */
	rsp_data = (uint8_t *)point_shm + sizeof(cix_tee_req_info);
	*fuse_len = req_info->data_len;
	memcpy(fuse_data, rsp_data, *fuse_len);
	ret = TEE_SUCCESS;
	return ret;
}



