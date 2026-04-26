/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2022 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_sign_verify.c
* @brief      This file contains QLIB data signing and signature verification sample code.
*
* @example    qlib_sample_sign_verify.c
*
* @page       sign_verify sign and verify sample code
* This sample code shows how to use qlib for data signing and signature verification .\n
*
* @include    samples/qlib_sample_sign_verify.c
*
************************************************************************************************************/

#ifdef QLIB_SIGN_DATA_BY_FLASH
/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  INCLUDES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#include <stdio.h>

#include "qlib.h"
#include "qlib_sample.h"
#include "qlib_sample_sign_verify.h"
#include "qlib_sample_qconf.h"

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             DEFINITIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#define SIGN_SECTION                SECURE_DATA_SECTION_INDEX //section (4) is pre-configured as fully secure section
#define SIGN_SECTION_FULL_KEY       QCONF_FULL_ACCESS_K_4
#define SIGN_SECTION_RESTRICTED_KEY QCONF_RESTRICTED_K_4
#define SIGN_SECTION_SIZE           SECURE_DATA_SECTION_SIZE

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_SAMPLE_SignVerify(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;
    _256BIT       signature;
    BOOL          isFullAccess        = TRUE;
    U32           section             = SIGN_SECTION;
    KEY_T         fullAccessKey       = SIGN_SECTION_FULL_KEY;
    KEY_T         restrictedAccessKey = SIGN_SECTION_RESTRICTED_KEY;
    U8            data[FLASH_PAGE_SIZE];

    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Erase flash data, since the sign and verify operations expect an empty section
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_LoadKey(qlibContext, section, fullAccessKey, TRUE), status, disconnect);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(qlibContext, section, QLIB_SESSION_ACCESS_FULL), status, remove_key);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Erase(qlibContext, section, 0, SIGN_SECTION_SIZE, TRUE), status, close_session);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_CloseSession(qlibContext, section), status, remove_key);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_RemoveKey(qlibContext, section, TRUE), status, disconnect);

    /*-------------------------------------------------------------------------------------------------------
     Load the restricted key for the sign/verify operations
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_LoadKey(qlibContext, section, restrictedAccessKey, FALSE), status, disconnect);
    isFullAccess = FALSE;

    /*-------------------------------------------------------------------------------------------------------
     Open secure session for the secure commands
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(qlibContext, section, QLIB_SESSION_ACCESS_RESTRICTED), status, remove_key);

    /*-------------------------------------------------------------------------------------------------------
     Calculate signature for given data using the open section key
    -------------------------------------------------------------------------------------------------------*/
    (void)memset(data, 0xa5, sizeof(data));
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Sign(qlibContext, section, data, sizeof(data), signature), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     verify the signature
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Verify(qlibContext, section, data, sizeof(data), signature), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Demonstrate failure scenario where data and signature don't match
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Sign(qlibContext, section, data, sizeof(data), signature), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     verification of signature should fail for wrong data
    -------------------------------------------------------------------------------------------------------*/
    data[0]++;
    QLIB_SAMPLE_ALLOW_TO_FAIL__START();
    status = QLIB_Verify(qlibContext, section, data, sizeof(data), signature);
    QLIB_SAMPLE_ALLOW_TO_FAIL__END();
    QLIB_ASSERT_WITH_ERROR_GOTO(status == QLIB_STATUS__DEVICE_AUTHENTICATION_ERR, QLIB_STATUS__TEST_FAIL, status, close_session);
    status = QLIB_STATUS__OK;

close_session:
    (void)QLIB_CloseSession(qlibContext, section);

remove_key:
    (void)QLIB_RemoveKey(qlibContext, section, isFullAccess);

disconnect:
    (void)QLIB_Disconnect(qlibContext);

    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_SignVerifyRun(void* userData)
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
     to retrieve first CDI in the fw trust chain, section 0 is used
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_SignVerify(&qlibContext));

    return QLIB_STATUS__OK;

disconnect:
    (void)QLIB_Disconnect(&qlibContext);
    return status;
}
#endif // QLIB_SIGN_DATA_BY_FLASH
