/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_utils_lms.c
* @brief      This file contains LMS utility functions
*
* ### project qlib
*
************************************************************************************************************/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib_utils_lms.h"

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                TYPES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/

#ifndef EXCLUDE_LMS_ATTESTATION
/************************************************************************************************************
 * @brief Buffer used for chain hashing
************************************************************************************************************/
PACKED_START
typedef struct
{
    LMS_ATTEST_KEY_ID_T keyId;
    U32                 q;
    U16                 index;
    U8                  iter;
    LMS_ATTEST_CHUNK_T  prevChunk;
} PACKED QLIB_LMS_Attest_HASH_CHAIN_BUF_T;
PACKED_END
#endif

#ifndef EXCLUDE_LMS
typedef struct
{
    U32     section;
    SSPRn_T sspr;
    U64     digest;
    U32     crc;
    U32     newVersion;
    BOOL    swapSection;
    U8      mode;
} QLIB_LMS_GEN_SET_SCR_CMD_INPUT_PARAMS_T;
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           LOCAL FUNCTIONS                                               */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#ifndef EXCLUDE_LMS
static void QLIB_UTILS_LMS_CreateSetScrMsg_L(QLIB_LMS_GEN_SET_SCR_CMD_INPUT_PARAMS_T* params, QLIB_LMS_MSG_DATA_STRUCT_T lmsMsg);
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#ifndef EXCLUDE_LMS_ATTESTATION
QLIB_STATUS_T QLIB_LMS_Attest_HashChain(const LMS_ATTEST_CHUNK_T  input,
                                        const LMS_ATTEST_KEY_ID_T keyId,
                                        U32                       q,
                                        U16                       chainIndex,
                                        U8                        iterStart,
                                        U8                        iter,
                                        LMS_ATTEST_CHUNK_T        output)
{
    QLIB_LMS_Attest_HASH_CHAIN_BUF_T buf;
    _256BIT                          hash;
    U8                               j;

    // Set buffer static parameters
    // I
    (void)memcpy((U8*)buf.keyId, (const U8*)keyId, sizeof(LMS_ATTEST_KEY_ID_T));
    // ots index
    buf.q = REVERSE_BYTES_32_BIT(q);
    // chain index
    buf.index = REVERSE_BYTES_16_BIT(chainIndex);

    // Set OTS private key (Xi) as initial prev chunk
    (void)memcpy(buf.prevChunk, input, sizeof(LMS_ATTEST_CHUNK_T));

    // Perform iterations
    for (j = 0; j < iter; j++)
    {
        buf.iter = j + iterStart;
        QLIB_STATUS_RET_CHECK(QLIB_HASH(hash, &buf, sizeof(QLIB_LMS_Attest_HASH_CHAIN_BUF_T)));
        (void)memcpy((U8*)buf.prevChunk, (U8*)hash, sizeof(LMS_ATTEST_CHUNK_T));
    }

    // signed digit result
    (void)memcpy(output, buf.prevChunk, sizeof(LMS_ATTEST_CHUNK_T));

    return QLIB_STATUS__OK;
}
#endif // EXCLUDE_LMS_ATTESTATION

#ifndef EXCLUDE_LMS
QLIB_STATUS_T QLIB_UTILS_LMS_CreateSetScrMsg(U32                        sectionId,
                                             const QLIB_POLICY_T*       policy,
                                             const U64*                 digest,
                                             const U32*                 crc,
                                             U32                        version,
                                             BOOL                       swap,
                                             QLIB_SECTION_CONF_ACTION_T action,
                                             QLIB_LMS_MSG_DATA_STRUCT_T lmsMsg)
{
    QLIB_LMS_GEN_SET_SCR_CMD_INPUT_PARAMS_T genScrParams = {0};
    SSPRn_T                                 sspr         = 0;

    QLIB_ASSERT_RET(policy != NULL, QLIB_STATUS__INVALID_PARAMETER);

    /*-------------------------------------------------------------------------------------------------------
     Create SET_SCR message
    -------------------------------------------------------------------------------------------------------*/
    SET_VAR_FIELD_32(sspr, QLIB_REG_SSPRn__AUTH_CFG, policy->digestIntegrity);
    SET_VAR_FIELD_32(sspr, QLIB_REG_SSPRn__AUTH_AC, policy->digestIntegrityOnAccess);
    SET_VAR_FIELD_32(sspr, QLIB_REG_SSPRn__INTEGRITY_AC, policy->checksumIntegrity);
    SET_VAR_FIELD_32(sspr, QLIB_REG_SSPRn__WP_EN, policy->writeProt);
    SET_VAR_FIELD_32(sspr, QLIB_REG_SSPRn__ROLLBACK_EN, policy->rollbackProt);
    SET_VAR_FIELD_32(sspr, QLIB_REG_SSPRn__PA_RD_EN, policy->plainAccessReadEnable);
    SET_VAR_FIELD_32(sspr, QLIB_REG_SSPRn__PA_WR_EN, policy->plainAccessWriteEnable);
    SET_VAR_FIELD_32(sspr, QLIB_REG_SSPRn__AUTH_PA, policy->authPlainAccess);
    SET_VAR_FIELD_32(sspr, QLIB_REG_SSPRn__SLOG, policy->slog);

    genScrParams.crc    = (NULL != crc ? *crc : 0u);
    genScrParams.digest = (NULL != digest ? *digest : 0u);

    QLIB_SEC_CMD_SET_SCR_MODE_SET(genScrParams.mode,
                                  action == QLIB_SECTION_CONF_ACTION__RESET ? TRUE : FALSE,
                                  action == QLIB_SECTION_CONF_ACTION__RELOAD ? TRUE : FALSE);

    genScrParams.newVersion  = version;
    genScrParams.section     = sectionId;
    genScrParams.sspr        = sspr;
    genScrParams.swapSection = swap;

    QLIB_UTILS_LMS_CreateSetScrMsg_L(&genScrParams, lmsMsg);
    return QLIB_STATUS__OK;
}
#endif // EXCLUDE_LMS

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           LOCAL FUNCTIONS                                               */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#ifndef EXCLUDE_LMS
static void QLIB_UTILS_LMS_CreateSetScrMsg_L(QLIB_LMS_GEN_SET_SCR_CMD_INPUT_PARAMS_T* params, QLIB_LMS_MSG_DATA_STRUCT_T lmsMsg)
{
    U8* pMsg = lmsMsg;
    U8  cmd;
    U32 ctag;

    cmd  = params->swapSection == FALSE ? (U8)QLIB_CMD_SEC_SET_SCR : (U8)QLIB_CMD_SEC_SET_SCR_SWAP;
    ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(cmd, params->section, params->mode, 0);
    ctag = REVERSE_BYTES_32_BIT(ctag);

    (void)memcpy(pMsg, (const U8*)(&ctag), sizeof(U32));
    pMsg += sizeof(U32);
    (void)memset(pMsg, 0, 12);
    pMsg += 12;
    (void)memcpy(pMsg, (const U8*)(&params->sspr), sizeof(U32));
    pMsg += sizeof(U32);
    (void)memcpy(pMsg, (const U8*)(&params->crc), sizeof(U32));
    pMsg += sizeof(U32);
    (void)memcpy(pMsg, (const U8*)(&params->digest), sizeof(U64));
    pMsg += sizeof(U64);
    (void)memcpy(pMsg, (const U8*)(&params->newVersion), sizeof(U32));
    pMsg += sizeof(U32);
    (void)memset(pMsg, 0, 12);
    pMsg += 12;
    (void)memset(pMsg, 0, 16);
}
#endif // EXCLUDE_LMS
