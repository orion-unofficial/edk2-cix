/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_multidie.h
* @brief      This file contains QLIB sample code definitions for multi-die
*
* ### project qlib_sample
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_MULTI_DIE__H_
#define _QLIB_SAMPLE_MULTI_DIE__H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************************************************
 * @brief       This routine shows how to switch between secure flash dies
 *
 * @param[in]   userData         Pointer to data associated with this qlib instance (NULL if no data required).
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_MultiDieRun(void* userData);

/************************************************************************************************************
 * @brief       This routine shows how to switch between secure flash dies
 *
 * @param[out]  qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_MultiDie(QLIB_CONTEXT_T* qlibContext);

#ifdef __cplusplus
}
#endif

#endif // _QLIB_SAMPLE_MULTI_DIE__H_
