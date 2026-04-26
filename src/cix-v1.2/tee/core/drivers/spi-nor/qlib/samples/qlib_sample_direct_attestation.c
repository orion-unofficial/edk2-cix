/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2020 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_direct_attestation.c
* @brief      This file contains QLIB direct attestation sample code
*
* @example    qlib_sample_direct_attestation.c
*
* @page       direct_attestation Remote attestation sample code
* This sample code shows how direct attestation feature can be implemented using w77q/t.\n
*
* @include    samples/qlib_sample_direct_attestation.c
*
************************************************************************************************************/

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  INCLUDES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#include "qlib.h"
#include "qlib_sample_direct_attestation.h"
#include "qlib_sample_qconf.h"

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_SAMPLE_DirectAttestationDigestGet(QLIB_CONTEXT_T* qlibContext, U32 sectionId, U64* digest)
{
    U32           baseAddr;
    U32           size;
    U32           crc;
    U32           version;
    QLIB_POLICY_T policy;

    /*-------------------------------------------------------------------------------------------------------
    Get configuration securely
    This routine returns section `digest` which can be further used to ensure particular flash section content
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(
        QLIB_GetSectionConfiguration(qlibContext, sectionId, &baseAddr, &size, &policy, digest, &crc, &version));

    /*-------------------------------------------------------------------------------------------------------
    Ensure that the retrieval was secure
    -------------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET((policy.digestIntegrity == 1u) && ((policy.writeProt == 1u) || (policy.rollbackProt == 1u)),
                    QLIB_STATUS__SECURITY_ERR);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_SAMPLE_DirectAttestationFlow(void* userData, U64 digestExp)
{
    QLIB_CONTEXT_T    qlibContext;
    QLIB_STATUS_T     status     = QLIB_STATUS__OK;
    QLIB_BUS_FORMAT_T busFormat  = QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE);
    U32               sectionId  = DIGEST_WRITE_PROTECTED_SECTION_INDEX;
    KEY_T             sectionKey = QCONF_FULL_ACCESS_K_2;
    U64               digest;

    /*-------------------------------------------------------------------------------------------------------
     Initialize QLIB  - needed when running QLIB either on a device or on a remote server.
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
     Load the key for the secure operations
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_LoadKey(&qlibContext, sectionId, sectionKey, TRUE), status, disconnect);

    /*-------------------------------------------------------------------------------------------------------
     Open secure session for the secure commands
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(&qlibContext, sectionId, QLIB_SESSION_ACCESS_FULL), status, remove_key);

    /*-------------------------------------------------------------------------------------------------------
     Get configuration securely
     This routine returns section `digest` which can be further used to ensure particular flash section content
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_DirectAttestationDigestGet(&qlibContext, sectionId, &digest), status, close_session);

    /*-------------------------------------------------------------------------------------------------------
     Compare to expected value
    -------------------------------------------------------------------------------------------------------*/
    if (digestExp != digest)
    {
        status = QLIB_STATUS__SECURITY_ERR;
    }

close_session:
    (void)QLIB_CloseSession(&qlibContext, sectionId);

remove_key:
    (void)QLIB_RemoveKey(&qlibContext, sectionId, TRUE);

disconnect:
    (void)QLIB_Disconnect(&qlibContext);
    return status;
}
