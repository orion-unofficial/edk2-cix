/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_lms_attestation.h
* @brief      This file contains lms attestation example
*
* ### project qlib_sample
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_LMS_ATTEST__H_
#define _QLIB_SAMPLE_LMS_ATTEST__H_

#if !defined(EXCLUDE_LMS_ATTESTATION) && !defined(Q2_API)

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************************************************
 * @brief       This routine demonstrates LMS OTS sign & verify functionality with initialization sequence
 *
 * @param[in]   userData      Pointer to data associated with this qlib instance.
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_LmsAttestRun(void* userData);

/************************************************************************************************************
 * @brief       This routine demonstrates LMS OTS sign & verify functionality
 *
 * @param[out]  qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_LmsAttest(QLIB_CONTEXT_T* qlibContext);

#ifdef __cplusplus
}
#endif

#endif // !defined(EXCLUDE_LMS_ATTESTATION) && !defined(Q2_API)
#endif // _QLIB_SAMPLE_LMS_ATTEST__H_
