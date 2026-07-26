/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2022 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_sign_verify.h
* @brief      This file contains QLIB data signing sample definitions
*
* ### project qlib_sample
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_SIGN_VERIFY__H_
#define _QLIB_SAMPLE_SIGN_VERIFY__H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef QLIB_SIGN_DATA_BY_FLASH
/************************************************************************************************************
 * @brief       This routine shows data signing and verification with initialization sequence
 *
 * @param[in]   userData      Pointer to data associated with this qlib instance.
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SignVerifyRun(void* userData);

/************************************************************************************************************
 * @brief       This function demonstrates data signing and signature verification using given section keys.
 *
 * @param[out]  qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SignVerify(QLIB_CONTEXT_T* qlibContext);

#endif // QLIB_SIGN_DATA_BY_FLASH

#ifdef __cplusplus
}
#endif

#endif // _QLIB_SAMPLE_SIGN_VERIFY__H_
