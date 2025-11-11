/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_lms_fw_update.h
* @brief      This file contains QLIB LMS FW update sample definitions
*
* ### project qlib_sample
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_LMS_FW_UPDATE__H_
#define _QLIB_SAMPLE_LMS_FW_UPDATE__H_

#include "qlib.h"

#if !defined(EXCLUDE_LMS) && !defined(Q2_API)

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************************************************
 * @brief       This function demonstrates LMS FW update usage with initialization sequence.
 *              It is dependent on the results of the lms_tool.py Python script, as the buffer generate by
 *              this script is send to the flash in this sample.
 *
 * @param[in]   userData      Pointer to data associated with this qlib instance.
 * 
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_LmsFwUpdateRun(void* userData);

/************************************************************************************************************
 * @brief       This function demonstrates LMS FW update usage in Qlib
 *              It is dependent on the results of the lms_tool.py Python script, as the buffer generate by
 *              this script is send to the flash in this sample.
 *
 * @param[in]   qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * 
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_LmsFwUpdate(QLIB_CONTEXT_T* qlibContext);

#ifdef __cplusplus
}
#endif

#endif // !defined(EXCLUDE_LMS) && !defined(Q2_API)
#endif // _QLIB_SAMPLE_LMS_FW_UPDATE__H_
