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
 *                 cix_tee_err.c
 *
 * Filename      : cix_tee_err.c
 * Programmer(s) : China Security team
 * Author        : Shuai Fengyun
 * Mail          : Abel.Shuai@cixcomputing.com
 * Create Time   : 2024-01-15 14:03:05
 **************************************************************************************
 */

#define MOUDLE_TEE_CIX_C_





/*
 *******************************************************************************
 *                                INCLUDE FILES
 *******************************************************************************
*/
#include <stdint.h>
#include <errno.h>

#include <plat_cix.h>
#include <plat/common/platform.h>
#include "plat_cix.h"
#include <common/debug.h>
#include <lib/rlog.h>
#include <services/sdei.h>
#include <context.h>
#include <setjmp.h>





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





/*
 *******************************************************************************
 *                          VARIABLES USED ONLY BY THIS MODULE
 *******************************************************************************
*/
static uintptr_t g_tee_dump_data_addr;

/*
 *******************************************************************************
 *                               FUNCTIONS IMPLEMENT
 *******************************************************************************
*/
/*
 *- #Description  This function for handle command.
 * @param   pMsg           [IN] The received request message
 *                               - Type: MBX_Msg *
 *                               - Range: N/A.
 *
 * @return     void
 * @retval     void
 *
 *
 */

int set_tee_dump_data_address(uint64_t addr, uint64_t arg1, uint64_t arg2)
{
	g_tee_dump_data_addr = addr;
	return 0;
}

static void dump_tee_panic_data(char* panic_data, uint32_t data_len)
{
	memcpy((char*)g_tee_dump_data_addr, panic_data, data_len);
	flush_dcache_range((uintptr_t)g_tee_dump_data_addr, data_len);
}

/* SDEI main interrupt handler */
int sdei_tee_exception_handler(uint32_t data_len)
{
	char* share_buf = (char*)(SKY1_ATF_TEE_SHM_BASE);

	dump_tee_panic_data(share_buf, data_len);
	sdei_dispatch_event(CIX_TEE_EXCEPTION_EVENT);
	ERROR("%s,%d Event%d ... \n", __func__, __LINE__, CIX_TEE_EXCEPTION_EVENT);
	return 0;
}
