/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_secure_log.c
* @brief      This file contains QLIB vault sample code
*
* @example    qlib_sample_secure_log.c
*
* @page       Secure log sample code
* This sample code shows how to use a section configured as secure log.\n
*
* @include    samples/qlib_sample_secure_log.h
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
#include "qlib_sample_secure_log.h"
#include "qlib_sample_qconf.h"

#if !defined(EXCLUDE_SECURE_LOG) && !defined(Q2_API)

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                         LOCAL FUNCTION DECLARATIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_SAMPLE_SecureLogConfigureTest_L(QLIB_CONTEXT_T* qlibContext,
                                                   BOOL            secure,
                                                   U32             sectionID,
                                                   KEY_T           key,
                                                   QLIB_POLICY_T*  origPolicy);
QLIB_STATUS_T QLIB_SAMPLE_SecureLogRestoreSectorConfig_L(QLIB_CONTEXT_T* qlibContext, U32 sectionID, QLIB_POLICY_T* origPolicy);

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_SAMPLE_SecureLogRun(void* userData)
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
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_SecureLog(&qlibContext));

    goto exit;

disconnect:
    (void)QLIB_Disconnect(&qlibContext);
exit:
    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_SecureLog(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;

    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Run sample code
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_SecureLogWithFullAccess(qlibContext), status, disconnect);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_SecureLogWithPlainAccess(qlibContext), status, disconnect);

disconnect:
    (void)QLIB_Disconnect(qlibContext);
    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_SecureLogWithFullAccess(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;
#ifdef EXCLUDE_SECURE_LOG
    TOUCH(qlibContext);
#else
    U32           sectionID = SECURE_DATA_SECTION_INDEX; // Section 4 is pre-configured as fully secure section
    KEY_T         key       = QCONF_FULL_ACCESS_K_4;
    BOOL          secure;
    QLIB_POLICY_T policy;
    U32           logEntry;
    U8            writeBuffer[QLIB_SEC_LOG_ENTRY_SIZE];
    U8            readBuffer[QLIB_SEC_LOG_ENTRY_SIZE * 10];
    U32           address;
    U32           expectedAddress;
    U32           numOfEntries = sizeof(readBuffer) / QLIB_SEC_LOG_ENTRY_SIZE;

    secure = TRUE;

    /*-------------------------------------------------------------------------------------------------------
     Configure the section as secure log, with the required privileges
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_SecureLogConfigureTest_L(qlibContext, secure, sectionID, key, &policy));

    /*-------------------------------------------------------------------------------------------------------
     Write several log entries
    -------------------------------------------------------------------------------------------------------*/
    memset(writeBuffer, 0, sizeof(writeBuffer));
    for (logEntry = 0; logEntry < numOfEntries; ++logEntry)
    {
        writeBuffer[0] = (U8)(logEntry & 0xff);
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_SecureLogWrite(qlibContext, writeBuffer, sectionID, sizeof(writeBuffer), secure),
                                   status,
                                   close_session);
    }

    /*-------------------------------------------------------------------------------------------------------
     Read the last log entry
    -------------------------------------------------------------------------------------------------------*/
    memset(readBuffer, 0xff, sizeof(readBuffer));
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SecureLogRead(qlibContext, readBuffer, &address, sectionID, secure), status, close_session);
    QLIB_ASSERT_WITH_ERROR_GOTO(0 == memcmp(readBuffer, writeBuffer, sizeof(writeBuffer)),
                                QLIB_STATUS__TEST_FAIL,
                                status,
                                close_session);
    expectedAddress = (numOfEntries - 1) * QLIB_SEC_LOG_ENTRY_SIZE;
    QLIB_ASSERT_WITH_ERROR_GOTO(address == expectedAddress, QLIB_STATUS__TEST_FAIL, status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Read all the log entries
    -------------------------------------------------------------------------------------------------------*/
    memset(readBuffer, 0xff, sizeof(readBuffer));
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Read(qlibContext, readBuffer, sectionID, 0, sizeof(readBuffer), secure, TRUE),
                               status,
                               close_session);

    /*-------------------------------------------------------------------------------------------------------
     Verify data
    -------------------------------------------------------------------------------------------------------*/
    memset(writeBuffer, 0, sizeof(writeBuffer));
    for (logEntry = 0; logEntry < numOfEntries; ++logEntry)
    {
        writeBuffer[0] = (U8)(logEntry & 0xff);
        QLIB_ASSERT_WITH_ERROR_GOTO(0 ==
                                        memcmp(&readBuffer[logEntry * QLIB_SEC_LOG_ENTRY_SIZE], writeBuffer, sizeof(writeBuffer)),
                                    QLIB_STATUS__TEST_FAIL,
                                    status,
                                    close_session);
    }

    /*-------------------------------------------------------------------------------------------------------
     Close the secure session
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_CloseSession(qlibContext, sectionID), status, remove_key);

    secure = FALSE;

    /*-------------------------------------------------------------------------------------------------------
     Non-secure read and write are not allowed
    -------------------------------------------------------------------------------------------------------*/
    QLIB_SAMPLE_ALLOW_TO_FAIL__START();
    status = QLIB_SecureLogWrite(qlibContext, writeBuffer, sectionID, sizeof(writeBuffer), FALSE);
    QLIB_SAMPLE_ALLOW_TO_FAIL__END();
    QLIB_ASSERT_WITH_ERROR_GOTO(status == QLIB_STATUS__DEVICE_PRIVILEGE_ERR, QLIB_STATUS__TEST_FAIL, status, close_session);

    QLIB_SAMPLE_ALLOW_TO_FAIL__START();
    status = QLIB_SecureLogRead(qlibContext, readBuffer, &address, sectionID, secure);
    QLIB_SAMPLE_ALLOW_TO_FAIL__END();
    QLIB_ASSERT_WITH_ERROR_GOTO(status == QLIB_STATUS__DEVICE_PRIVILEGE_ERR, QLIB_STATUS__TEST_FAIL, status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Restore the configuration of the sector to its original state
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_SecureLogRestoreSectorConfig_L(qlibContext, sectionID, &policy),
                               status,
                               close_session);

    status = QLIB_STATUS__OK;

close_session:
    (void)QLIB_CloseSession(qlibContext, sectionID);

remove_key:
    (void)QLIB_RemoveKey(qlibContext, sectionID, TRUE);

#endif
    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_SecureLogWithPlainAccess(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;
#ifdef EXCLUDE_SECURE_LOG
    TOUCH(qlibContext);
#else
    U32 sectionID =
        SECURE_DATA_SECTION_INDEX; // Section 4 is pre-configured as fully secure section. We change its configuration and restore it at the end of th test
    KEY_T key = QCONF_FULL_ACCESS_K_4;
    BOOL secure;
    QLIB_POLICY_T policy;
    U32 logEntry;
    U8 writeBuffer[QLIB_SEC_LOG_ENTRY_SIZE];
    U8 readBuffer[QLIB_SEC_LOG_ENTRY_SIZE * 10];
    U32 address;
    U32 expectedAddress;
    U32 numOfEntries = sizeof(readBuffer) / QLIB_SEC_LOG_ENTRY_SIZE;

    secure = FALSE;

    /*-------------------------------------------------------------------------------------------------------
     Configure the section as secure log, with the required privileges
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_SecureLogConfigureTest_L(qlibContext, secure, sectionID, key, &policy));

    /*-------------------------------------------------------------------------------------------------------
     Write several log entries
    -------------------------------------------------------------------------------------------------------*/
    memset(writeBuffer, 0, sizeof(writeBuffer));
    for (logEntry = 0; logEntry < numOfEntries; ++logEntry)
    {
        writeBuffer[0] = (U8)(logEntry & 0xff);
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_SecureLogWrite(qlibContext, writeBuffer, sectionID, sizeof(writeBuffer), secure),
                                   status,
                                   remove_key);
    }

    /*-------------------------------------------------------------------------------------------------------
     Read the last log entry
    -------------------------------------------------------------------------------------------------------*/
    memset(readBuffer, 0xff, sizeof(readBuffer));
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SecureLogRead(qlibContext, readBuffer, &address, sectionID, secure), status, remove_key);
    QLIB_ASSERT_WITH_ERROR_GOTO(0 == memcmp(readBuffer, writeBuffer, sizeof(writeBuffer)),
                                QLIB_STATUS__TEST_FAIL,
                                status,
                                remove_key);
    expectedAddress = (numOfEntries - 1) * QLIB_SEC_LOG_ENTRY_SIZE;
    QLIB_ASSERT_WITH_ERROR_GOTO(address == expectedAddress, QLIB_STATUS__TEST_FAIL, status, remove_key);

    /*-------------------------------------------------------------------------------------------------------
     Read all the log entries
    -------------------------------------------------------------------------------------------------------*/
    memset(readBuffer, 0xff, sizeof(readBuffer));
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Read(qlibContext, readBuffer, sectionID, 0, sizeof(readBuffer), secure, FALSE),
                               status,
                               remove_key);

    /*-------------------------------------------------------------------------------------------------------
     Verify data
    -------------------------------------------------------------------------------------------------------*/
    memset(writeBuffer, 0, sizeof(writeBuffer));
    for (logEntry = 0; logEntry < numOfEntries; ++logEntry)
    {
        writeBuffer[0] = (U8)(logEntry & 0xff);
        QLIB_ASSERT_WITH_ERROR_GOTO(0 ==
                                        memcmp(&readBuffer[logEntry * QLIB_SEC_LOG_ENTRY_SIZE], writeBuffer, sizeof(writeBuffer)),
                                    QLIB_STATUS__TEST_FAIL,
                                    status,
                                    remove_key);
    }

    /*-------------------------------------------------------------------------------------------------------
     Restore the configuration of the sector to its original state
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_SecureLogRestoreSectorConfig_L(qlibContext, sectionID, &policy), status, remove_key);

    status = QLIB_STATUS__OK;
    goto remove_key;

remove_key:
    (void)QLIB_RemoveKey(qlibContext, sectionID, TRUE);

#endif
    return status;
}

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                               LOCAL FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_SAMPLE_SecureLogConfigureTest_L(QLIB_CONTEXT_T* qlibContext,
                                                   BOOL            secure,
                                                   U32             sectionID,
                                                   KEY_T           key,
                                                   QLIB_POLICY_T*  origPolicy)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;
    QLIB_POLICY_T policy;
#ifndef Q2_API
    QLIB_SECTION_CONF_T sectionConfig = {0};
#endif // !Q2_API

    /*-------------------------------------------------------------------------------------------------------
     Load the key for the secure erase operation
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_LoadKey(qlibContext, sectionID, key, TRUE));

    /*-------------------------------------------------------------------------------------------------------
     Open full access secure session for the secure commands
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(qlibContext, sectionID, QLIB_SESSION_ACCESS_FULL), status, remove_key);

    /*-------------------------------------------------------------------------------------------------------
     Erase the section before using it as secure log
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_EraseSection(qlibContext, sectionID, TRUE), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Config section as secure log
    -------------------------------------------------------------------------------------------------------*/
#ifdef Q2_API
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_GetSectionConfiguration(qlibContext, sectionID, NULL, NULL, origPolicy, NULL, NULL, NULL),
                               status,
                               close_session);
#else
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_GetSectionConfiguration(qlibContext,
                                                            sectionID,
                                                            NULL,
                                                            NULL,
                                                            origPolicy,
                                                            &sectionConfig.digest,
                                                            &sectionConfig.crc,
                                                            &sectionConfig.version),
                               status,
                               close_session);
#endif

    (void)memcpy(&policy, origPolicy, sizeof(QLIB_POLICY_T));
    policy.slog = 1;
    if (FALSE == secure)
    {
        policy.plainAccessReadEnable  = 1;
        policy.plainAccessWriteEnable = 1;
    }
#ifdef Q2_API
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_ConfigSection(qlibContext, sectionID, &policy, NULL, NULL, NULL, QLIB_SWAP_NO),
                               status,
                               close_session);
#else
    (void)memcpy(&sectionConfig.policy, &policy, sizeof(QLIB_POLICY_T));
    sectionConfig.postActions.reload = 1u;
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_ConfigDeviceSection(qlibContext, sectionID, &sectionConfig), status, close_session);
#endif

    if (FALSE == secure)
    {
        /*---------------------------------------------------------------------------------------------------
        Close the secure session
        ---------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_CloseSession(qlibContext, sectionID), status, remove_key);
    }

    goto exit;

close_session:
    (void)QLIB_CloseSession(qlibContext, sectionID);

remove_key:
    (void)QLIB_RemoveKey(qlibContext, sectionID, TRUE);

exit:
    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_SecureLogRestoreSectorConfig_L(QLIB_CONTEXT_T* qlibContext, U32 sectionID, QLIB_POLICY_T* origPolicy)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;
#ifndef Q2_API
    QLIB_SECTION_CONF_T sectionConfig = {0};
#endif // !Q2_API

    /*-------------------------------------------------------------------------------------------------------
     Open full access secure session for the secure commands
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(qlibContext, sectionID, QLIB_SESSION_ACCESS_FULL), status, close_session);

#ifdef Q2_API
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_ConfigSection(qlibContext, sectionID, origPolicy, NULL, NULL, NULL, QLIB_SWAP_NO),
                               status,
                               close_session);
#else
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_GetSectionConfiguration(qlibContext,
                                                            sectionID,
                                                            NULL,
                                                            NULL,
                                                            NULL,
                                                            &sectionConfig.digest,
                                                            &sectionConfig.crc,
                                                            &sectionConfig.version),
                               status,
                               close_session);
    (void)memcpy((void*)&sectionConfig.policy, (void*)origPolicy, sizeof(QLIB_POLICY_T));
    sectionConfig.postActions.reload = 1u;
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_ConfigDeviceSection(qlibContext, sectionID, &sectionConfig), status, close_session);
#endif

close_session:
    (void)QLIB_CloseSession(qlibContext, sectionID);

    return status;
}
#endif // !defined(EXCLUDE_SECURE_LOG) && !defined(Q2_API)
