/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_wd.h
* @brief      This file contains QLIB WD related headers
*
* ### project qlib_samples
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_WD__H_
#define _QLIB_SAMPLE_WD__H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************************************************
 * @brief   This macro holds execution for @p mSec milliseconds.
 *          This is required for WD calibration, if not defined by the user WD calibration will fail.
 * @param   mSec   time to hold
************************************************************************************************************/
#ifndef QLIB_SAMPLE_DELAY_MSEC
#define QLIB_SAMPLE_DELAY_MSEC(mSec)
#endif

/************************************************************************************************************
 * @brief       This routine shows WD flow with initialization sequence
 *
 * @param[in]   userData      Pointer to data associated with this qlib instance (NULL if no data required).
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_WatchDogRun(void* userData);

/************************************************************************************************************
 * @brief       This routine shows example of using QLIB_Watchdog_Config function
 *
 * @param[out]  qlibContext     [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]   enable          TRUE to enable WD, FALSE to disable
 * @param[in]   secure          TRUE if WD is secured, FALSE if non-secured
 * @param[in]   threshold       WD counter limit enumeration
 * @param[in]   section         Number of section, its key is used for secured WD
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_WatchDogConfig(QLIB_CONTEXT_T* qlibContext,
                                         BOOL            enable,
                                         BOOL            secure,
                                         QLIB_AWDT_TH_T  threshold,
                                         U32             section);

/************************************************************************************************************
 * @brief       This routine shows an example of how to use QLIB_Watchdog_Touch function
 *
 * @param[out]  qlibContext     [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]   secure          TRUE if WD is secured, FALSE if non-secured
 * @param[in]   section         [Section index](md_definitions.html#DEF_SECTION). The section key is used for secured WD
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_WatchDogTouch(QLIB_CONTEXT_T* qlibContext, BOOL secure, U32 section);

/************************************************************************************************************
 * @brief       This routine shows example of using QLIB_Watchdog_Get function
 *
 * @param[out]  qlibContext     [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]   secure          TRUE if WD is secured, FALSE if non-secured
 * @param[in]   section         [Section index](md_definitions.html#DEF_SECTION). The section key is used for secured WD
 * @param[out]  time            WD counter value (if Q2_API is defined the time is in seconds, otherwise in milliseconds)
 * @param[out]  expired         if TRUE WD is expired (in this case time value irrelevant)
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_WatchDogGet(QLIB_CONTEXT_T* qlibContext, BOOL secure, U32 section, U32* time, BOOL* expired);

/************************************************************************************************************
 * @brief       This routine shows WD calibration flow with initialization sequence
 *
 * @param[in]   userData        Pointer to data associated with this qlib instance (NULL if no data required).
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_WatchDogCalibrateRun(void* userData);

/************************************************************************************************************
 * @brief       This routine calibrates the SF oscillator of the WD
 *
 * @param[out]  qlibContext     [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]   deviceMasterKey device master key, if available, also update the default WD configuration
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_WatchDogCalibrate(QLIB_CONTEXT_T* qlibContext, const KEY_T deviceMasterKey);

#ifdef __cplusplus
}
#endif

#endif // _QLIB_SAMPLE_WD__H_
