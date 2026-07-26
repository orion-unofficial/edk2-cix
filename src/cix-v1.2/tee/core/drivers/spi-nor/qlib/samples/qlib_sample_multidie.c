/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_multidie.c
* @brief      This file contains QLIB sample code for multi die.
*
* @example    qlib_sample_multidie.c
*
* @page       multidie Multi Die sample code
* This sample code shows how to use several flash dies .\n
*
* @include    samples/qlib_sample_multidie.c
*
************************************************************************************************************/

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  INCLUDES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#include <stdio.h>

#include "qlib.h"

#if QLIB_NUM_OF_DIES > 1

#include "qlib_sample_multidie.h"
#include "qlib_sample.h"
#include "qlib_sample_qconf.h"

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             LOCAL FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_SAMPLE_SecureReaddWriteErase_L(QLIB_CONTEXT_T* qlibContext);

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_SAMPLE_MultiDie(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;
    uint_fast8_t  die    = 0;
    QLIB_ID_T     flashIds;

    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
     After QLIB_Connect the flash active die is die 0
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));

    for (die = 0; die < QLIB_NUM_OF_DIES; die++)
    {
        /*---------------------------------------------------------------------------------------------------
         Get Active Die flash IDs
        ---------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_GetId(qlibContext, &flashIds), status, disconnect);
        QLIB_SAMPLE_PRINTF("Die %u Flash Unique ID = 0x%lx%08lx%08lx%08lx\r\n",
                           qlibContext->activeDie,
                           (unsigned long)flashIds.std.uniqueID[3],
                           (unsigned long)flashIds.std.uniqueID[2],
                           (unsigned long)flashIds.std.uniqueID[1],
                           (unsigned long)flashIds.std.uniqueID[0]);
        QLIB_SAMPLE_PRINTF("Die %u Secure user ID = 0x%lx%08lx%08lx%08lx\r\n",
                           qlibContext->activeDie,
                           (unsigned long)flashIds.sec.suid[3],
                           (unsigned long)flashIds.sec.suid[2],
                           (unsigned long)flashIds.sec.suid[1],
                           (unsigned long)flashIds.sec.suid[0]);

        /*---------------------------------------------------------------------------------------------------
         Perform some read, write, erase on active die
        ---------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_SecureReaddWriteErase_L(qlibContext), status, disconnect);

        /*---------------------------------------------------------------------------------------------------
         Set next die
        ---------------------------------------------------------------------------------------------------*/
        if (die < (QLIB_GET_NUM_OF_DIES(qlibContext) - 1u))
        {
            QLIB_STATUS_RET_CHECK_GOTO(QLIB_SetActiveDie(qlibContext, die + 1), status, disconnect);
        }
    }
disconnect:
    (void)QLIB_Disconnect(qlibContext);

    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_MultiDieRun(void* userData)
{
    QLIB_CONTEXT_T qlibContext;
    QLIB_STATUS_T  status = QLIB_STATUS__OK;

    /*-------------------------------------------------------------------------------------------------------
     Init QLIB  - needed when running QLIB either on a device or on a remote server.
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_InitLib(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Set user data - store a pointer to data the user might need for its platform
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
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_MultiDie(&qlibContext));

    return QLIB_STATUS__OK;

disconnect:
    (void)QLIB_Disconnect(&qlibContext);
    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_SecureReaddWriteErase_L(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;
    U32   section = PA_WRITE_PROTECT_SECTION_INDEX; //section (1) is pre-configured as secure section with plain read access
    KEY_T key     = QCONF_FULL_ACCESS_K_1;
    U8    writeBuf[FLASH_PAGE_SIZE];
    U8    readBuf[FLASH_PAGE_SIZE];
    U8    eraseBuf[FLASH_PAGE_SIZE];

    (void)memset(readBuf, 0, FLASH_PAGE_SIZE);
    (void)memset(writeBuf, 0xa5, FLASH_PAGE_SIZE);
    (void)memset(eraseBuf, 0xFF, FLASH_PAGE_SIZE);

    /*-------------------------------------------------------------------------------------------------------
     Load the key for the secure write operations
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_LoadKey(qlibContext, section, key, TRUE));

    /*-------------------------------------------------------------------------------------------------------
     Open full access secure session for the secure commands
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(qlibContext, section, QLIB_SESSION_ACCESS_FULL), status, remove_key);

    /*-------------------------------------------------------------------------------------------------------
     Secure erase
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Erase(qlibContext, section, 0, FLASH_SECTOR_SIZE, TRUE), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Secure read
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Read(qlibContext, readBuf, section, 0, FLASH_PAGE_SIZE, TRUE, FALSE), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Verify data
    -------------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO(0 == memcmp(readBuf, eraseBuf, FLASH_PAGE_SIZE), QLIB_STATUS__TEST_FAIL, status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Secure write
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Write(qlibContext, writeBuf, section, 0, FLASH_PAGE_SIZE, TRUE), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Secure read
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Read(qlibContext, readBuf, section, 0, FLASH_PAGE_SIZE, TRUE, FALSE), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Verify data
    -------------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO(0 == memcmp(readBuf, writeBuf, FLASH_PAGE_SIZE), QLIB_STATUS__TEST_FAIL, status, close_session);

close_session:
    (void)QLIB_CloseSession(qlibContext, section);

remove_key:
    (void)QLIB_RemoveKey(qlibContext, section, TRUE);

    return status;
}
#endif // QLIB_NUM_OF_DIES > 1
