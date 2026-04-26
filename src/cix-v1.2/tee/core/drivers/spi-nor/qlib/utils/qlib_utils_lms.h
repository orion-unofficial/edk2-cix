/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_utils_lms.h
* @brief      This file contains LMS utility functions
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_UTILS_LMS_H__
#define __QLIB_UTILS_LMS_H__

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib.h"

#ifndef EXCLUDE_LMS_ATTESTATION
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief This function computes hash-chain
 *
 * @param[in]      input        Input buffer for 1st iteration
 * @param[in]      keyId        key ID
 * @param[in]      q            Leaf index
 * @param[in]      chainIndex   Chain index
 * @param[in]      iterStart    Iteration start index
 * @param[in]      iter         Num of iterations
 * @param[out]     output       Hash chain output
 *
 * @return
 * QLIB_STATUS__OK = 0                 - no error occurred\n
 * QLIB_STATUS__(ERROR)                - Other error
************************************************************************************************************/
QLIB_STATUS_T QLIB_LMS_Attest_HashChain(const LMS_ATTEST_CHUNK_T  input,
                                        const LMS_ATTEST_KEY_ID_T keyId,
                                        U32                       q,
                                        U16                       chainIndex,
                                        U8                        iterStart,
                                        U8                        iter,
                                        LMS_ATTEST_CHUNK_T        output);

#endif // EXCLUDE_LMS_ATTESTATION

#ifndef EXCLUDE_LMS
/************************************************************************************************************
 * @brief       This routine creates a set_scr LMS command
 *
 * @param[in]  sectionId         section
 * @param[in]  policy            section policy
 * @param[in]  digest            digest of section data (or NULL if not needed)
 * @param[in]  crc               crc of section data (or NULL if not needed)
 * @param[in]  version           section version to set in the set_scr command
 * @param[in]  swap              TRUE for swap of rollback protected section, FALSE otherwise
 * @param[in]  action            whether to reload new section configuration or reset flash after section configuration
 * @param[out] lmsMsg            LMS message data structure
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_UTILS_LMS_CreateSetScrMsg(U32                        sectionId,
                                             const QLIB_POLICY_T*       policy,
                                             const U64*                 digest,
                                             const U32*                 crc,
                                             U32                        version,
                                             BOOL                       swap,
                                             QLIB_SECTION_CONF_ACTION_T action,
                                             QLIB_LMS_MSG_DATA_STRUCT_T lmsMsg);
#endif // EXCLUDE_LMS
#ifdef __cplusplus
}
#endif
#endif // __QLIB_UTILS_LMS_H__
