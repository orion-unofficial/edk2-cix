/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_secure_log.h
* @brief      This file contains QLIB secure log sample definitions
*
* ### project qlib_sample
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_SECURE_LOG__H_
#define _QLIB_SAMPLE_SECURE_LOG__H_

#if !defined(EXCLUDE_SECURE_LOG) && !defined(Q2_API)

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************************************************
 * @brief       This function demonstrates secure log functionality with initialization sequence
 *              It shows read/write/erase commands to vault section with different access privileges.
 *
 * @param[in]   userData      Pointer to data associated with this qlib instance.
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SecureLogRun(void* userData);

/************************************************************************************************************
 * @brief       This function demonstrates secure log functionality
 *              It shows read/write/erase commands to vault section with different access privileges.
 *
 * @param[in]   qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * 
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SecureLog(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
* @brief      This function shows secure log commands to secure section - secure commands allowed with full key, plain commands are not allowed.
*             This function assumes the QLIB library and flash device are already initialized.
*
* @param[out]  qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
*
* @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SecureLogWithFullAccess(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
* @brief      This function shows secure log commands to secure section with plain read & write allowed.
*             This function assumes the QLIB library and flash device are already initialized.
*
* @param[out]  qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
*
* @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SecureLogWithPlainAccess(QLIB_CONTEXT_T* qlibContext);

#ifdef __cplusplus
}
#endif

#endif // !defined(EXCLUDE_SECURE_LOG) && !defined(Q2_API)
#endif // _QLIB_SAMPLE_SECURE_LOG__H_
