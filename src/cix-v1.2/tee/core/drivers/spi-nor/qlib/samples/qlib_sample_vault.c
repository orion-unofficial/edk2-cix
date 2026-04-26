/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_vault.c
* @brief      This file contains QLIB vault sample code
*
* @example    qlib_sample_vault.c
*
* @page       Vault sample code
* This sample code shows how to use the vault.\n
* The first function shows erase/write/read from W77Q/T's vault section as secure flash using full key.\n
* The second function shows erase/write/read from W77Q/T's vault section as secure flash using restricted key.\n
*
* @include    samples/qlib_sample_vault.c
*
************************************************************************************************************/

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  INCLUDES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#include <stdio.h>

#include "qlib.h"
#include "qlib_sample.h"
#include "qlib_sample_vault.h"
#include "qlib_sample_qconf.h"

#ifndef Q2_API

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_SAMPLE_VaultRun(void* userData)
{
    QLIB_CONTEXT_T    qlibContext;
    QLIB_STATUS_T     status    = QLIB_STATUS__OK;
    QLIB_BUS_FORMAT_T busFormat = QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE);

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
     Release the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Disconnect(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Run sample code
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_Vault(&qlibContext));

    goto exit;

disconnect:
    (void)QLIB_Disconnect(&qlibContext);
exit:
    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_Vault(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;

    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Run sample code
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_VaultFullKey(qlibContext), status, disconnect);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_VaultRestrictedKey(qlibContext), status, disconnect);

disconnect:
    (void)QLIB_Disconnect(qlibContext);
    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_VaultFullKey(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T status  = QLIB_STATUS__OK;
    U32           section = QLIB_SECTION_ID_VAULT;
    KEY_T         key     = QCONF_VAULT_FULL_ACCESS;
    U8            writeBuf[FLASH_PAGE_SIZE];
    U8            readBuf[FLASH_PAGE_SIZE];
    U8            eraseBuf[FLASH_PAGE_SIZE];

    (void)memset(readBuf, 0, sizeof(readBuf));
    (void)memset(writeBuf, 0xa5, sizeof(writeBuf));
    (void)memset(eraseBuf, 0xFF, sizeof(eraseBuf));

    /*-------------------------------------------------------------------------------------------------------
     Load the key for the secure write operations
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_LoadKey(qlibContext, section, key, TRUE));

    /*-------------------------------------------------------------------------------------------------------
     Open secure session for the secure commands
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(qlibContext, section, QLIB_SESSION_ACCESS_FULL), status, remove_key);

    /*-------------------------------------------------------------------------------------------------------
     Secure erase
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Erase(qlibContext, section, 0, FLASH_SECTOR_SIZE, TRUE), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Secure write
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Write(qlibContext, writeBuf, section, 0, sizeof(writeBuf), TRUE), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Non-secure read is not allowed
    -------------------------------------------------------------------------------------------------------*/
    QLIB_SAMPLE_ALLOW_TO_FAIL__START();
    status = QLIB_Read(qlibContext, readBuf, section, 0, sizeof(readBuf), FALSE, FALSE);
    QLIB_SAMPLE_ALLOW_TO_FAIL__END();
    QLIB_ASSERT_WITH_ERROR_GOTO(status == QLIB_STATUS__INVALID_PARAMETER, QLIB_STATUS__TEST_FAIL, status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Verify data
    -------------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO(0 != memcmp(readBuf, writeBuf, sizeof(writeBuf)), QLIB_STATUS__TEST_FAIL, status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Non-secure erase is not allowed
    -------------------------------------------------------------------------------------------------------*/
    QLIB_SAMPLE_ALLOW_TO_FAIL__START();
    status = QLIB_Erase(qlibContext, section, 0, FLASH_SECTOR_SIZE, FALSE);
    QLIB_SAMPLE_ALLOW_TO_FAIL__END();
    QLIB_ASSERT_WITH_ERROR_GOTO(status == QLIB_STATUS__INVALID_PARAMETER, QLIB_STATUS__TEST_FAIL, status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Secure read
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Read(qlibContext, readBuf, section, 0, sizeof(readBuf), TRUE, FALSE), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Verify data
    -------------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO(0 == memcmp(readBuf, writeBuf, sizeof(writeBuf)), QLIB_STATUS__TEST_FAIL, status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Note: user can verify section data with integrity check as well by using CALC_SIG command and comparing
     the digest with an expected digest value
    -------------------------------------------------------------------------------------------------------*/

    status = QLIB_STATUS__OK;

close_session:
    (void)QLIB_CloseSession(qlibContext, section);

remove_key:
    (void)QLIB_RemoveKey(qlibContext, section, TRUE);

    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_VaultRestrictedKey(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T status              = QLIB_STATUS__OK;
    U32           section             = QLIB_SECTION_ID_VAULT;
    KEY_T         fullAccessKey       = QCONF_VAULT_FULL_ACCESS;
    KEY_T         restrictedAccessKey = QCONF_VAULT_RESTRICTED;
    U8            writeBuf[FLASH_PAGE_SIZE];
    U8            readBuf[FLASH_PAGE_SIZE];
    U8            eraseBuf[FLASH_PAGE_SIZE];
    BOOL          isFullAccess = TRUE;

    (void)memset(readBuf, 0, sizeof(readBuf));
    (void)memset(writeBuf, 0xa5, sizeof(writeBuf));
    (void)memset(eraseBuf, 0xFF, sizeof(eraseBuf));

    /*-------------------------------------------------------------------------------------------------------
     Set flash data at start of test with full key
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_LoadKey(qlibContext, section, fullAccessKey, TRUE));
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(qlibContext, section, QLIB_SESSION_ACCESS_FULL), status, remove_key);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Erase(qlibContext, section, 0, FLASH_SECTOR_SIZE, TRUE), status, close_session);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Write(qlibContext, writeBuf, section, 0, sizeof(writeBuf), TRUE), status, close_session);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_CloseSession(qlibContext, section), status, remove_key);
    QLIB_STATUS_RET_CHECK(QLIB_RemoveKey(qlibContext, section, TRUE));

    /*-------------------------------------------------------------------------------------------------------
     Load the restricted key for the secure read operations
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_LoadKey(qlibContext, section, restrictedAccessKey, FALSE));
    isFullAccess = FALSE;

    /*-------------------------------------------------------------------------------------------------------
     Open secure session for the secure commands
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(qlibContext, section, QLIB_SESSION_ACCESS_RESTRICTED), status, remove_key);

    /*-------------------------------------------------------------------------------------------------------
     Secure read
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Read(qlibContext, readBuf, section, 0, sizeof(readBuf), TRUE, FALSE), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Verify data
    -------------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO(0 == memcmp(readBuf, writeBuf, sizeof(writeBuf)), QLIB_STATUS__TEST_FAIL, status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Secure erase is not allowed
    -------------------------------------------------------------------------------------------------------*/
    QLIB_SAMPLE_ALLOW_TO_FAIL__START();
    status = QLIB_Erase(qlibContext, section, 0, FLASH_SECTOR_SIZE, TRUE);
    QLIB_SAMPLE_ALLOW_TO_FAIL__END();
    QLIB_ASSERT_WITH_ERROR_GOTO(status == QLIB_STATUS__DEVICE_SESSION_ERR, QLIB_STATUS__TEST_FAIL, status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Secure read
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Read(qlibContext, readBuf, section, 0, sizeof(readBuf), TRUE, FALSE), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Verify data
    -------------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO(0 == memcmp(readBuf, writeBuf, sizeof(writeBuf)), QLIB_STATUS__TEST_FAIL, status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Secure write is not allowed
    -------------------------------------------------------------------------------------------------------*/
    QLIB_SAMPLE_ALLOW_TO_FAIL__START();
    status = QLIB_Write(qlibContext, writeBuf, section, 0, sizeof(writeBuf), TRUE);
    QLIB_SAMPLE_ALLOW_TO_FAIL__END();
    QLIB_ASSERT_WITH_ERROR_GOTO(status == QLIB_STATUS__DEVICE_SESSION_ERR, QLIB_STATUS__TEST_FAIL, status, close_session);

    status = QLIB_STATUS__OK;

close_session:
    (void)QLIB_CloseSession(qlibContext, section);

remove_key:
    (void)QLIB_RemoveKey(qlibContext, section, isFullAccess);

    return status;
}

#endif // Q2_API
