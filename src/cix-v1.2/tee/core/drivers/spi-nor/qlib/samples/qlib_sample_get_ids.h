/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2022 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_get_ids.h
* @brief      This file contains QLIB sample code definitions for getting Flash IDs
*
* ### project qlib_sample
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_GET_IDS__H_
#define _QLIB_SAMPLE_GET_IDS__H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************************************************
 * @brief       This routine shows how to fetch flash IDs including initialization sequence
 *
 * @param[in]   userData      Pointer to data associated with this qlib instance.
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_GetIDsRun(void* userData);

/************************************************************************************************************
 * @brief       This function demonstrates how to fetch the flash IDs.
 *
 * @param[out]  qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_GetIDs(QLIB_CONTEXT_T* qlibContext);

#ifdef __cplusplus
}
#endif

#endif // _QLIB_SAMPLE_GET_IDS__H_
