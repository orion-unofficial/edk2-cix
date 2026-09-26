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
 *                 tee_cix_ipc.c
 *
 * Filename      : tee_cix_ipc.c
 * Programmer(s) : China Security team
 * Author        : Shuai Fengyun
 * Mail          : Abel.Shuai@cixcomputing.com
 * Create Time   : 2023-11-09 16:32:02
 **************************************************************************************
 */

#define MOUDLE_CIX_IPC_C_





/*
 *******************************************************************************
 *                                INCLUDE FILES
 *******************************************************************************
*/
#include <stdlib.h>
#include <string.h>
#include <string_ext.h>
#include <tee_api.h>
#include <tee_internal_api_extensions.h>
#include <types_ext.h>
#include <user_ta_header.h>
#include <utee_syscalls.h>
#include "tee_api_private.h"
#include "tee_cix_ipc.h"



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
TEE_Result TEE_GetCixEfuse(uint32_t offset, uint32_t len, uint8_t* fuse_data, uint32_t* fuse_len);





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
TEE_Result TEE_GetCixEfuse(uint32_t offset, uint32_t len, uint8_t* fuse_data, uint32_t* fuse_len)
{
	TEE_Result ret = TEE_SUCCESS;
	/** 1) Check input parameter */
	if ((NULL == fuse_data) || (NULL == fuse_len)) {
		EMSG("Invalid Input data");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/** 2) Call system call API to get */
	ret = _utee_get_cix_efuse(offset, len, fuse_data, fuse_len);
	return ret;
}





