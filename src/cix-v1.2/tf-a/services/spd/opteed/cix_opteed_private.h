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
 *                 cix_opteed_private.h
 *
 * Filename      : cix_opteed_private.h
 * Programmer(s) : China Security team
 * Author        : Shuai Fengyun
 * Mail          : Abel.Shuai@cixcomputing.com
 * Create Time   : 2023-11-29 10:55:27
 **************************************************************************************
 */

#ifndef MOUDLE_CIX_OPTEED_PRIVATE_H_
#define MOUDLE_CIX_OPTEED_PRIVATE_H_




/*
 *******************************************************************************
 *                                INCLUDE FILES
 *******************************************************************************
*/





/*
 *******************************************************************************
 *                  MACRO DEFINITION USED ONLY BY THIS MODULE
 *******************************************************************************
*/
#define IPC_RESULT_SUCCESS	0x00
#define IPC_RESULT_FAIL	0x01


/*
 *******************************************************************************
 *                STRUCTRUE DEFINITION USED ONLY BY THIS MODULE
 *******************************************************************************
*/
typedef struct {
	unsigned long x0;
	unsigned long x1;
	unsigned long x2;
	unsigned long x3;
}optee_ser_num_result;

typedef struct {
	uint32_t offset;
	uint32_t len; /* Length of fuse data */
	uint32_t result;
	uint32_t data_len; /* Length of fuse data */
} cix_tee_req_info;

typedef struct {
	uint32_t secure_flag; /* Secure flag */
	uint32_t offset;
	uint32_t len; /* Length of fuse data */
} cix_req_data_info;

typedef struct _cix_ipc_respon {
	int32_t errcode;
	uint32_t data[CIX_MBOX_MSG_LEN - 3];
} cix_ipc_respon;


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


















#endif  /* MOUDLE_NAME_H*/
