/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2022 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_get_ids.c
* @brief      This file contains QLIB sample code for initialization and getting Flash IDs.
*
* @example    qlib_sample_get_ids.c
*
* @page       get_ids get IDs sample code
* This sample code shows how to use qlib for fetching flash IDs .\n
*
* @include    samples/qlib_sample_get_ids.c
*
************************************************************************************************************/

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  INCLUDES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#include <stdio.h>

#include "qlib.h"
#include "qlib_sample_get_ids.h"
#include "qlib_sample.h"

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_SAMPLE_GetIDs(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;
    QLIB_ID_T     flashIds;

    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Get Flash IDs
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_GetId(qlibContext, &flashIds), status, disconnect);
#ifdef Q2_API
    QLIB_SAMPLE_PRINTF("Flash Unique ID = 0x%llx\r\n", (unsigned long long)flashIds.std.uniqueID);
#else
    QLIB_SAMPLE_PRINTF("Flash Unique ID = 0x%lx%08lx%08lx%08lx\r\n",
                       (unsigned long)flashIds.std.uniqueID[3],
                       (unsigned long)flashIds.std.uniqueID[2],
                       (unsigned long)flashIds.std.uniqueID[1],
                       (unsigned long)flashIds.std.uniqueID[0]);
#endif
    QLIB_SAMPLE_PRINTF("Secure user ID = 0x%lx%08lx%08lx%08lx\r\n",
                       (unsigned long)flashIds.sec.suid[3],
                       (unsigned long)flashIds.sec.suid[2],
                       (unsigned long)flashIds.sec.suid[1],
                       (unsigned long)flashIds.sec.suid[0]);
    QLIB_SAMPLE_PRINTF("Winbond ID = 0x%lx%08lx\r\n", (unsigned long)flashIds.sec.wid[1], (unsigned long)flashIds.sec.wid[0]);

disconnect:
    (void)QLIB_Disconnect(qlibContext);

    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_GetIDsRun(void* userData)
{
    QLIB_CONTEXT_T qlibContext;
    QLIB_STATUS_T  status = QLIB_STATUS__OK;

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
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_InitDevice(&qlibContext, QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE)), status, disconnect);

    /*-------------------------------------------------------------------------------------------------------
     Release the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Disconnect(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Sample for getting Flash IDs
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_GetIDs(&qlibContext));

    return QLIB_STATUS__OK;

disconnect:
    (void)QLIB_Disconnect(&qlibContext);
    return status;
}
