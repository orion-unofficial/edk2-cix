/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2020 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_cdi.c
* @brief      This file contains QLIB cdi sample code
*
* @example    qlib_sample_cdi.c
*
* @page       cdi_attestation cdi attestation sample code
* This sample code shows how to use cdi feature for secure boot implementation .\n
*
* @include    samples/qlib_sample_cdi.c
*
************************************************************************************************************/

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  INCLUDES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#include <stdio.h>

#include "qlib.h"
#include "qlib_sample_cdi.h"
#include "qlib_sample_qconf.h"

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_SAMPLE_DiceAttestationFlow(void* userData)
{
    QLIB_CONTEXT_T    qlibContext;
    QLIB_STATUS_T     status    = QLIB_STATUS__OK;
    QLIB_BUS_FORMAT_T busFormat = QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE);
    _256BIT           nextCdi;
    _256BIT           prevCdi;
    KEY_T             sectionFullAccessKeys[] = {QCONF_FULL_ACCESS_K_0,
                                                 QCONF_FULL_ACCESS_K_1,
                                                 QCONF_FULL_ACCESS_K_2,
                                                 QCONF_FULL_ACCESS_K_3,
                                                 QCONF_FULL_ACCESS_K_4,
                                                 QCONF_FULL_ACCESS_K_5,
                                                 QCONF_FULL_ACCESS_K_6,
                                                 QCONF_FULL_ACCESS_K_7};

    /*-------------------------------------------------------------------------------------------------------
     Init QLIB  - needed when running QLIB either on a device or on a remote server.
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_InitLib(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Set the user data into Qlib's context
    -------------------------------------------------------------------------------------------------------*/
    QLIB_SetUserData(&qlibContext, userData);

    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Init Flash Device - not needed when using QLIB on a remote server
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_InitDevice(&qlibContext, busFormat), status, disconnect);

    /*-------------------------------------------------------------------------------------------------------
     to retrieve first CDI in the fw trust chain, section 0 is used
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_GetCdi(&qlibContext,
                                                  nextCdi,
                                                  NULL,
                                                  BOOT_SECTION_INDEX,
                                                  sectionFullAccessKeys[BOOT_SECTION_INDEX]),
                               status,
                               disconnect);

    /*-------------------------------------------------------------------------------------------------------
      assuming the second firmware resides in section 1 and its policy has digest enabled we use previous(first)
      cdi to obtain next(second) value
     ------------------------------------------------------------------------------------------------------*/
    (void)memcpy(prevCdi, nextCdi, sizeof(_256BIT));
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_GetCdi(&qlibContext,
                                                  nextCdi,
                                                  prevCdi,
                                                  PA_WRITE_PROTECT_SECTION_INDEX,
                                                  sectionFullAccessKeys[PA_WRITE_PROTECT_SECTION_INDEX]),
                               status,
                               disconnect);

    /*-------------------------------------------------------------------------------------------------------
      assuming the third firmware resides in section 2 and its policy has digest enabled we use previous(second)
      cdi to obtain next (third) value
     ------------------------------------------------------------------------------------------------------*/
    (void)memcpy(prevCdi, nextCdi, sizeof(_256BIT));
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_GetCdi(&qlibContext,
                                                  nextCdi,
                                                  prevCdi,
                                                  DIGEST_WRITE_PROTECTED_SECTION_INDEX,
                                                  sectionFullAccessKeys[DIGEST_WRITE_PROTECTED_SECTION_INDEX]),
                               status,
                               disconnect);

    /*-------------------------------------------------------------------------------------------------------
      At this point nextCdi contains last CDI that was derived from three stage boot chain. it can be used further
      in any attestation mechanism. (for instance server ...)
     ------------------------------------------------------------------------------------------------------*/

disconnect:
    (void)QLIB_Disconnect(&qlibContext);
    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_GetCdi(QLIB_CONTEXT_T* qlibContext,
                                 _256BIT         nextCdi,
                                 _256BIT         prevCdi,
                                 U32             sectionId,
                                 KEY_T           sectionFullAccessKey)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;

    /*-------------------------------------------------------------------------------------------------------
     Load the key for the secure write operations
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_LoadKey(qlibContext, sectionId, sectionFullAccessKey, TRUE));

    /*-------------------------------------------------------------------------------------------------------
     Open secure session for the secure commands
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(qlibContext, sectionId, QLIB_SESSION_ACCESS_FULL), status, remove_key);

    /*-------------------------------------------------------------------------------------------------------
     Retrieving cdi value
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_CalcCDI(qlibContext, nextCdi, prevCdi, sectionId), status, close_session);

close_session:
    (void)QLIB_CloseSession(qlibContext, sectionId);

remove_key:
    (void)QLIB_RemoveKey(qlibContext, sectionId, TRUE);

    return status;
}
