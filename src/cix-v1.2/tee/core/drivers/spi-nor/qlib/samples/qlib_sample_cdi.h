/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2020 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_cdi.h
* @brief      This file contains QLIB cdi sample definitions
*
* ### project qlib_sample
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_CDI__H_
#define _QLIB_SAMPLE_CDI__H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************************************************
 * @brief       This function demonstrates cdi value retrieval through the boot chain consisted of 3 boot stages.
 *              The sample demonstrates the "Extending Chain Of Trust" secure flow described in W77Q AS v0.93
 *              revA section from January 17,2021 (section 8.6.3)
 *
 * @param[in]   userData      Pointer to data associated with this qlib instance.
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_DiceAttestationFlow(void* userData);

/************************************************************************************************************
* @brief      This function shows how CDI value is retrieved.
*             This function assumes the QLIB library and flash device are already initialized.
*             This sample demonstrates the "Generating Root Of Trust" secure flow described in W77Q AS v0.93
*             revA section from January 17,2021 (section 8.6.2)
*
* @param[out]  qlibContext          [QLIB internal state](md_definitions.html#DEF_CONTEXT)
* @param[out]  nextCdi              CDI value produced by this module
* @param[in]   prevCdi              CDI value obtained from previous module
* @param[in]   sectionId            section number CDI associated with
* @param[in]   sectionFullAccessKey section full access key
*
* @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_GetCdi(QLIB_CONTEXT_T* qlibContext,
                                 _256BIT         nextCdi,
                                 _256BIT         prevCdi,
                                 U32             sectionId,
                                 KEY_T           sectionFullAccessKey);

#ifdef __cplusplus
}
#endif

#endif // _QLIB_SAMPLE_CDI__H_
