/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_lms_attestation.c
* @brief      This file contains QLIB example for LMS attestation
*
* @example    qlib_sample_lms_attestation.c
*
* @page       LMS_attestation LMS attestation example code
* This sample code shows how to perform LMS attestation signature verification.\n
*
* @include    samples/qlib_sample_lms_attestation.c
*
************************************************************************************************************/

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  INCLUDES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#include "qlib.h"
#include "qlib_sample_lms_attestation.h"
#include "qlib_utils_lms.h"

#if !defined(EXCLUDE_LMS_ATTESTATION) && !defined(Q2_API)
#include "lms_api.h"
#include "lms.h"
/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                 DEFINITIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------------------------------
    User defined parameters
-----------------------------------------------------------------------------------------------------------*/
// The message being signed
#define LMS_ATTEST_MSG                                                                 \
    {                                                                                  \
        'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!', '!', '!', '!', '!' \
    }
// Random nonce data
#define LMS_ATTEST_NONCE                                                                                                        \
    {                                                                                                                           \
        0x64, 0xC8, 0x93, 0x25, 0x4A, 0x94, 0x2B, 0x56, 0xAC, 0x5B, 0xB6, 0x6F, 0xDE, 0xBF, 0x7D, 0xFA, 0xF7, 0xED, 0xD9, 0xB1, \
            0x61, 0xC2, 0x87, 0x0D                                                                                              \
    }

/*-----------------------------------------------------------------------------------------------------------
    Parameters from the database, corresponding to the specific chip
-----------------------------------------------------------------------------------------------------------*/
#define LMS_ATTEST_PUBLIC_KEY                                                                                                   \
    {                                                                                                                           \
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x21, 0x22, 0x23, 0x24, \
            0x25, 0x26, 0x27, 0x28                                                                                              \
    }
#define LMS_ATTEST_CACHE_SIZE QLIB_LMS_ATTEST_ALL_NODES
#define LMS_ATTEST_CACHE \
    {                    \
        0                \
    }

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_SAMPLE_LmsAttestRun(void* userData)
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
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_LmsAttest(&qlibContext));

    goto exit;

disconnect:
    (void)QLIB_Disconnect(&qlibContext);
exit:
    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_LmsAttest(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T                status                          = QLIB_STATUS__OK;
    U8                           msg[]                           = LMS_ATTEST_MSG;
    LMS_ATTEST_NONCE_T           nonce                           = LMS_ATTEST_NONCE;
    LMS_ATTEST_CHUNK_T           pubKey                          = LMS_ATTEST_PUBLIC_KEY;
    LMS_ATTEST_CHUNK_T           pubCache[LMS_ATTEST_CACHE_SIZE] = LMS_ATTEST_CACHE;
    QLIB_LMS_ATTEST_SIG_BUFFER_T lmsSig;
    LMS_ATTEST_KEY_ID_T          wKeyId;
    ALG_LMS_T                    lmsAlg = {0};

    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Run sample code
    -------------------------------------------------------------------------------------------------------*/

    /*-------------------------------------------------------------------------------------------------------
     Generate LMS public key and public cache
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_LMS_Attest_GetPublicKey(qlibContext, pubKey, pubCache, LMS_ATTEST_CACHE_SIZE),
                               status,
                               disconnect);

    /*-------------------------------------------------------------------------------------------------------
     Sign using flash internal private key
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_LMS_Attest_Sign(qlibContext,
                                                    msg,
                                                    sizeof(msg),
                                                    nonce,
                                                    pubCache,
                                                    LMS_ATTEST_CACHE_SIZE,
                                                    lmsSig,
                                                    wKeyId),
                               status,
                               disconnect);

    /*-------------------------------------------------------------------------------------------------------
     Release the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Disconnect(qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Verify the signature itself
    -------------------------------------------------------------------------------------------------------*/
    {
        QLIB_ASSERT_RET(ALG_SUCCESS == lmsInit(&lmsAlg), QLIB_STATUS__COMMAND_FAIL);
        LMS_VERIFY_SIGNATURE_OP_T verifyParams =
            {wKeyId, pubKey, sizeof(pubKey), msg, sizeof(msg), lmsSig, sizeof(QLIB_LMS_ATTEST_SIG_BUFFER_T)};
        QLIB_ASSERT_RET(ALG_SUCCESS == lmsVerifySignature(&lmsAlg, &verifyParams), QLIB_STATUS__COMMAND_FAIL);
    }

    goto exit;

disconnect:
    (void)QLIB_Disconnect(qlibContext);
exit:
    return status;
}
#endif //!defined(EXCLUDE_LMS_ATTESTATION) && !defined(Q2_API)
