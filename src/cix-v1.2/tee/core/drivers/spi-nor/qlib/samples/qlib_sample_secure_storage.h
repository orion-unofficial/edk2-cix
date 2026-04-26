/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2020 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_secure_storage.h
* @brief      This file contains QLIB secure storage sample definitions
*
* ### project qlib_sample
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_SECURE_STORAGE__H_
#define _QLIB_SAMPLE_SECURE_STORAGE__H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************************************************
 * @brief       This function demonstrates secure storage functionality
 *              It shows read/write/erase commands to sections with different access privileges.
 *
 * @param[in]   userData      Pointer to data associated with this qlib instance.
 * 
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SecureStorage(void* userData);

/************************************************************************************************************
* @brief      This function shows storage commands to secure section with plain read allowed (plain write/erase not allowed).
*             This function assumes the QLIB library and flash device are already initialized.
*
* @param[out]  qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
*
* @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SecureSectionWithPlainAccessRead(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
* @brief      This function shows storage commands to secure section - secure commands allowed with full key, plain commands are not allowed.
*             This function assumes the QLIB library and flash device are already initialized.
*
* @param[out]  qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
*
* @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SecureSectionFullKey(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
* @brief      This function shows storage commands to secure section - only read is allowed when using restricted key.
*             This function assumes the QLIB library and flash device are already initialized.
*
* @param[out]  qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
*
* @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SecureSectionRestrictedKey(QLIB_CONTEXT_T* qlibContext);

#ifdef __cplusplus
}
#endif

#endif // _QLIB_SAMPLE_SECURE_STORAGE__H_
