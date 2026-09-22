/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sec.h
* @brief      This file contains security features definitions
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_SEC_H__
#define __QLIB_SEC_H__

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 DEFINES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

#define QLIB_EXECUTE_SIGNED_GET(contextP)                                                                                     \
    (((U8)QLIB_KID__RESTRICTED_ACCESS_SECTION == QLIB_KEY_MNGR__GET_KEY_TYPE(QLIB_ACTIVE_DIE_STATE(contextP).keyMngr.kid)) || \
     ((U8)QLIB_KID__FULL_ACCESS_SECTION == QLIB_KEY_MNGR__GET_KEY_TYPE(QLIB_ACTIVE_DIE_STATE(contextP).keyMngr.kid)))

#define QLIB_SEC__get_GMT(contextP, gmt)                                                  \
    (QLIB_EXECUTE_SIGNED_GET(contextP) ? QLIB_CMD_PROC__get_GMT_SIGNED((contextP), (gmt)) \
                                       : QLIB_CMD_PROC__get_GMT_UNSIGNED((contextP), (gmt)))

#define QLIB_SEC__get_SCRn(contextP, sec, scrn)                                                    \
    (QLIB_EXECUTE_SIGNED_GET(contextP) ? QLIB_CMD_PROC__get_SCRn_SIGNED((contextP), (sec), (scrn)) \
                                       : QLIB_CMD_PROC__get_SCRn_UNSIGNED((contextP), (sec), (scrn)))

#define QLIB_SEC__get_SUID(contextP, suid)                                                  \
    (QLIB_EXECUTE_SIGNED_GET(contextP) ? QLIB_CMD_PROC__get_SUID_SIGNED((contextP), (suid)) \
                                       : QLIB_CMD_PROC__get_SUID_UNSIGNED((contextP), (suid)))

#define QLIB_SEC__get_GMC(contextP, gmc)                                                  \
    (QLIB_EXECUTE_SIGNED_GET(contextP) ? QLIB_CMD_PROC__get_GMC_SIGNED((contextP), (gmc)) \
                                       : QLIB_CMD_PROC__get_GMC_UNSIGNED((contextP), (gmc)))

#define QLIB_SEC__get_HW_VER(contextP, hwVer)                                                  \
    (QLIB_EXECUTE_SIGNED_GET(contextP) ? QLIB_CMD_PROC__get_HW_VER_SIGNED((contextP), (hwVer)) \
                                       : QLIB_CMD_PROC__get_HW_VER_UNSIGNED((contextP), (hwVer)))

#define QLIB_SEC__get_SSR(contextP, ssr, mask)                                                    \
    (QLIB_EXECUTE_SIGNED_GET(contextP) ? QLIB_CMD_PROC__get_SSR_SIGNED((contextP), (ssr), (mask)) \
                                       : QLIB_CMD_PROC__get_SSR_UNSIGNED((contextP), (ssr), (mask)))

#define QLIB_SEC__get_ESSR(contextP, essr)                                                  \
    (QLIB_EXECUTE_SIGNED_GET(contextP) ? QLIB_CMD_PROC__get_ESSR_SIGNED((contextP), (essr)) \
                                       : QLIB_CMD_PROC__get_ESSR_UNSIGNED((contextP), (essr)))

#define QLIB_SEC__get_MC(contextP, mc)                                                  \
    (QLIB_EXECUTE_SIGNED_GET(contextP) ? QLIB_CMD_PROC__get_MC_SIGNED((contextP), (mc)) \
                                       : QLIB_CMD_PROC__get_MC_UNSIGNED((contextP), (mc)))

#define QLIB_SEC__get_WID(contextP, wid)                                                  \
    (QLIB_EXECUTE_SIGNED_GET(contextP) ? QLIB_CMD_PROC__get_WID_SIGNED((contextP), (wid)) \
                                       : QLIB_CMD_PROC__get_WID_UNSIGNED((contextP), (wid)))

#define QLIB_SEC__get_AWDTCNFG(contextP, awdtCnfg)                                              \
    (QLIB_EXECUTE_SIGNED_GET(contextP) ? QLIB_CMD_PROC__get_AWDT_SIGNED((contextP), (awdtCnfg)) \
                                       : QLIB_CMD_PROC__get_AWDT_UNSIGNED((contextP), (awdtCnfg)))

#define QLIB_SEC__get_RNGR(contextP, rngr)                                               \
    (QLIB_EXECUTE_SIGNED_GET(contextP) ? QLIB_CMD_PROC__get_RNGR_SEC((contextP), (rngr)) \
                                       : QLIB_CMD_PROC__get_RNGR_PLAIN((contextP), (rngr)))

#define QLIB_SEC_FLASH_IS_AFTER_FORMAT(qlibContext, gmc)                                      \
    ((QLIB_ACTIVE_DIE_STATE(qlibContext).ssr.asStruct.STATE == QLIB_REG_SSR__STATE_LOCKED) || \
     ((QLIB_REG_GMC_GET_DEVCFG(gmc) & QLIB_REG_DEVCFG_RESERVED_ZERO_MASK) != 0u))

/*---------------------------------------------------------------------------------------------------------*/
/* Every time Transaction counter is USED it is incremented. Should be used once for transaction           */
/* Check TC did not reach its maximal value. Otherwise, the device should be reset before any secure       */
/* function can be used                                                                                    */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_INCREMENT_TRANSACTION_CNTR(context_p)                      \
    if (Q2_TC_30_BIT(context_p) != 0u)                                  \
    {                                                                   \
        SET_VAR_FIELD_32(QLIB_ACTIVE_DIE_STATE(context_p).mc[TC],       \
                         QLIB_REG_TC__CNTR,                             \
                         QLIB_ACTIVE_DIE_STATE(context_p).mc[TC] + 1u); \
    }                                                                   \
    else                                                                \
    {                                                                   \
        QLIB_ACTIVE_DIE_STATE(context_p).mc[TC] += 1u;                  \
    }

#define QLIB_MAX_TRANSACTION_CNTR(context_p) (Q2_TC_30_BIT(context_p) != 0u ? MAX_FIELD_VAL(QLIB_REG_TC__CNTR) : 0xFFFFFFFFu)

/*---------------------------------------------------------------------------------------------------------*/
/* Use current value (if macro is at right side of assignment) and then increment                          */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_TRANSACTION_CNTR_USE(context_p)                                                         \
    QLIB_ACTIVE_DIE_STATE(context_p).mc[TC] += 0u;                                                   \
    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(context_p).mc[TC] != QLIB_MAX_TRANSACTION_CNTR(context_p), \
                    QLIB_STATUS__DEVICE_MC_ERR);                                                     \
    QLIB_INCREMENT_TRANSACTION_CNTR(context_p)

/************************************************************************************************************
 * Get vault size, from qlibContext or from configuration enum
************************************************************************************************************/
#define QLIB_VAULT_CFG_TO_SIZE(cfg) \
    (((cfg) == QLIB_VAULT_64KB_RPMC_4_COUNTERS) ? _64KB_ : (((cfg) == QLIB_VAULT_128KB_RPMC_DISABLED) ? _128KB_ : 0UL))
#define QLIB_VAULT_GET_SIZE(context) QLIB_VAULT_CFG_TO_SIZE(QLIB_ACTIVE_DIE_STATE((context)).vaultSize)
#define QLIB_VAULT_SIZE_TO_CFG(size)                      \
    (((size) == _64KB_) ? QLIB_VAULT_64KB_RPMC_4_COUNTERS \
                        : (((size) == _128KB_) ? QLIB_VAULT_128KB_RPMC_DISABLED : QLIB_VAULT_DISABLED_RPMC_8_COUNTERS))

/************************************************************************************************************
 * LMS Attestation definitions
************************************************************************************************************/
#define QLIB_LMS_COMMAND_SIZE (1812u + 12u) // expect padding for 16 bytes alignment

/************************************************************************************************************
 * LMS Attestation definitions
************************************************************************************************************/
#define LMS_ATTEST_NUM_OF_CHAINS 48u // Number of chains (only for input)
#define LMS_ATTEST_PARAM_W       4u  // Input size in bits
#define LMS_ATTEST_MAX_DIGIT     ((1U << LMS_ATTEST_PARAM_W) - 1u)

#define LMS_ATTEST_D_LEAF 0x8282u // LMS hashing constant for LMS leaf
#define LMS_ATTEST_D_PBLC 0x8080u // LMS hashing constant for OTS public key
#define LMS_ATTEST_D_MESG 0x8181u // LMS hashing constant for OTS message
#define LMS_ATTEST_D_INTR 0x8383u // LMS hashing constant for LMS node

// macro to extract digit i from input message
#define QLIB_LMS_ATTEST_COEF(input, i)                                   \
    (LMS_ATTEST_MAX_DIGIT & (((input)[((i)*LMS_ATTEST_PARAM_W) / 8u]) >> \
                             (8u - (LMS_ATTEST_PARAM_W * ((i) % (8u / LMS_ATTEST_PARAM_W)) + LMS_ATTEST_PARAM_W))))

#define QLIB_LMS_ATTEST_LEAF_NODES_START_INDEX ((U32)1 << (QLIB_LMS_ATTEST_TREE_HEIGHT))
#define QLIB_LMS_ATTEST_NUM_OF_LEAVES          QLIB_LMS_ATTEST_LEAF_NODES_START_INDEX
#define QLIB_LMS_ATTEST_ALL_NODES              ((U32)1 << ((QLIB_LMS_ATTEST_TREE_HEIGHT) + 1u))
#define QLIB_LMS_ATTEST_FIRST_CACHED_INDEX(cacheSize)                                                               \
    (((cacheSize) >= QLIB_LMS_ATTEST_ALL_NODES)                                                                     \
         ? (1u)                                                                                                     \
         : (((cacheSize) > 0u)                                                                                      \
                ? (((cacheSize) & ((cacheSize)-1u)) == 0u ? (cacheSize) : ((1UL << LOG2(cacheSize)) - (cacheSize))) \
                : QLIB_LMS_ATTEST_ALL_NODES))

/************************************************************************************************************
 * @brief Buffer used for LMS OTS node hashing
************************************************************************************************************/
PACKED_START
typedef struct
{
    LMS_ATTEST_KEY_ID_T keyId;
    U32                 q;
    U16                 d_intr;
    LMS_ATTEST_CHUNK_T  left;
    LMS_ATTEST_CHUNK_T  right;
} PACKED QLIB_LMS_ATTEST_NODE_T;
PACKED_END
/************************************************************************************************************
 * @brief Buffer used for LMS OTS leaf hashing
************************************************************************************************************/
PACKED_START
typedef struct
{
    LMS_ATTEST_KEY_ID_T keyId;
    U32                 nodeIndex;
    U16                 d_leaf;
    LMS_ATTEST_CHUNK_T  otsPub;
} PACKED QLIB_LMS_ATTEST_LEAF_T;
PACKED_END

/************************************************************************************************************
 * @brief Buffer used for OTS public key hashing
************************************************************************************************************/
PACKED_START
typedef struct
{
    LMS_ATTEST_KEY_ID_T keyId;
    U32                 q;
    U16                 d_pblc;
    LMS_ATTEST_CHUNK_T  pubChunks[LMS_ATTEST_PARAM_P];
} PACKED QLIB_LMS_ATTEST_PUB_BUF_T;
PACKED_END

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This function initiate the globals of the secure module.
 *              This function should be called once at the beginning of every host (local or remote)
 *
 * @param[in,out]   qlibContext   qlib context object
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_InitLib(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This function synchronize the Qlib state with the flash secure module state,
 *              both Qlib and the flash state may change during this function
 *
 * @param[in,out]   qlibContext   qlib context object
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_SyncState(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This function formats the flash device, including keys and sections configurations.
 *              If deviceMasterKey is available then secure format (SFORMAT) command is used,
 *              otherwise, if deviceMasterKey is NULL, non-secure format (FORMAT) command is used
 *              In order to use non-secure format, FORMAT_EN must be set in DEVCFG.
 *              If configuration registers are deleted, the flash is reset after command execution to load new configuration
 *              Note that this function sets active DIE to 0.
 *
 * @param[in,out]   qlibContext      qlib context object
 * @param[in]       deviceMasterKey  Device Master Key value, or NULL for non-secure format
 * @param[in]       eraseDataOnly    If TRUE, only the data is erased, else also all the configurations will be erased and flash is reset
 * @param[in]       factoryDefault   If set, after the format, the device is returned to its Factory-default configurations
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_Format(QLIB_CONTEXT_T* qlibContext, const KEY_T deviceMasterKey, BOOL eraseDataOnly, BOOL factoryDefault);

/************************************************************************************************************
 * @brief       This function sets the opcodes for secure operations
 *
 * @param       qlibContext   QLIB state object
************************************************************************************************************/
void QLIB_SEC_configOPS(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This function check if there are notifications from the device.
 *
 * @param[in,out]   qlibContext   qlib context object
 * @param[out]      notifs        Bitmap of notifications. A bit is set to 1 if the notification exists.
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_GetNotifications(QLIB_CONTEXT_T* qlibContext, QLIB_NOTIFICATIONS_T* notifs);

/************************************************************************************************************
 * @brief       This function perform one monotonic counter maintenance iteration.
 *              To speed-up the time-critical boot sequence and Session Open process,
 *              it is recommended to call this function when the SW is in idle while
 *              \ref QLIB_GetNotifications returns mcMaintenance value f 1.
 *              This function is part of the Qlib maintenance flow perform by \ref QLIB_PerformMaintenance
 *
 * @param[in,out]   qlibContext   qlib context object
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_PerformMCMaint(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This function re-configures the flash. It assume clean/formatted flash if keys are provided.
 *              The 'restrictedKeys' and 'fullAccessKeys' can be NULL or contain invalid (zero) key elements
 *              in the array. In such case all or the appropriate key locations will not be programmed to
 *              the chip, and remain un-programmed. Note: un-programmed keys have security implications.
 *              The function can receive optional 'suid' (Secure User ID) parameter, which can add
 *              additional user-specific unique ID, in addition to Winbond WID.
 *
 * @param[in,out]   qlibContext                QLIB state object
 * @param[in]       flashCfg                   Flash configuration
 * @param[in]       dieCfg                     Die configuration
 * @param[in]       cfgSectionArray            Sections configuration
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_ConfigDevice(QLIB_CONTEXT_T*                   qlibContext,
                                    const QLIB_FLASH_CONFIG_T*        flashCfg,
                                    const QLIB_DIE_CONFIG_T*          dieCfg,
                                    const QLIB_SECTIONS_CONF_TABLE_T* cfgSectionArray);

/************************************************************************************************************
 * @brief       This function retrieve the section configuration.
 *              All configuration parameters are optional (can be NULL).
 *
 * @param       qlibContext   QLIB state object
 * @param       sectionID     Section number to configure
 * @param       policy        section configuration policy
 * @param       baseAddr      section base address
 * @param       size          section size in bytes
 * @param       digest        section digest
 * @param       crc           section CRC
 * @param       version       Section version value
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_GetSectionConfiguration(QLIB_CONTEXT_T* qlibContext,
                                               U32             sectionID,
                                               U32*            baseAddr,
                                               U32*            size,
                                               QLIB_POLICY_T*  policy,
                                               U64*            digest,
                                               U32*            crc,
                                               U32*            version);

/************************************************************************************************************
 * @brief       This function updates section configuration.
 *              All configuration parameters ('policy','digest','crc', 'newVersion') are optional
 *              (can be NULL). This function requires the session to be opened to the relevant section
 *              with Full Access
 *
 * @param[in,out]   qlibContext     qlib context object
 * @param[in]       sectionID       Section number to configure
 * @param[in]       policy          New section configuration policy
 * @param[in]       digest          New section digest, or NULL if not digest available
 * @param[in]       crc             New section CRC, or NULL if no CRC available
 * @param[in]       newVersion      new section version value, or NULL if version not changed
 * @param[in]       swap            swap the code
 * @param[in]       action          whether to load new configuration or reset after command execution
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_ConfigSection(QLIB_CONTEXT_T*            qlibContext,
                                     U32                        sectionID,
                                     const QLIB_POLICY_T*       policy,
                                     const U64*                 digest,
                                     const U32*                 crc,
                                     const U32*                 newVersion,
                                     BOOL                       swap,
                                     QLIB_SECTION_CONF_ACTION_T action);

/************************************************************************************************************
 * @brief       This function opens a session to a given section
 *
 * @param       qlibContext     QLIB state object
 * @param       sectionID       Section index
 * @param       sessionAccess   Session access type
 * @param       includeWID      If TRUE, the WID is used to calculate the session key (personalized session key)
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_OpenSession(QLIB_CONTEXT_T*       qlibContext,
                                   U32                   sectionID,
                                   QLIB_SESSION_ACCESS_T sessionAccess,
                                   BOOL                  includeWID);

/************************************************************************************************************
 * @brief       This function closes a session to a given section
 *
 * @param       qlibContext QLIB state object
 * @param       sectionID   Section index
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_CloseSession(QLIB_CONTEXT_T* qlibContext, U32 sectionID);

/************************************************************************************************************
 * @brief       This function grants plain access to a authenticated plain access section
 *
 * @param       qlibContext     QLIB state object
 * @param       sectionID       Section index
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_AuthPlainAccess_Grant(QLIB_CONTEXT_T* qlibContext, U32 sectionID);

/************************************************************************************************************
 * @brief       This function revokes plain access from a section
 *
 * @param       qlibContext QLIB state object
 * @param       sectionID   Section index
 * @param[in]   revokeType  Type of plain access to revoke
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_PlainAccess_Revoke(QLIB_CONTEXT_T* qlibContext, U32 sectionID, QLIB_PA_REVOKE_TYPE_T revokeType);

/************************************************************************************************************
 * @brief       This function perform secure reads data from the flash.
 *              It can generate Secure and Secure + Authenticated reads. A session must be open first
 *
 * @param       qlibContext-   QLIB state object
 * @param       buf            Pointer to output buffer
 * @param       sectionID      Section index
 * @param       offset         Section offset
 * @param       size           Size of read data
 * @param       auth           if TRUE, read operation will be authenticated
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_Read(QLIB_CONTEXT_T* qlibContext, U8* buf, U32 sectionID, U32 offset, U32 size, BOOL auth);

/************************************************************************************************************
 * @brief       This function perform secure write data to the flash.
 *
 * @param       qlibContext   QLIB state object
 * @param       buf           Pointer to input buffer
 * @param       sectionID     Section index
 * @param       offset        Section offset
 * @param       size          Data size
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_Write(QLIB_CONTEXT_T* qlibContext, const U8* buf, U32 sectionID, U32 offset, U32 size);

/************************************************************************************************************
 * @brief       This function erases data from the flash
 *
 * @param       qlibContext-   QLIB state object
 * @param       sectionID      Section index
 * @param       offset         Section offset
 * @param       size           Data size
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_Erase(QLIB_CONTEXT_T* qlibContext, U32 sectionID, U32 offset, U32 size);

/************************************************************************************************************
 * @brief       This function copy a range of memory within a single Section
 *
 * @param       qlibContext-   QLIB state object
 * @param       dest           Starting address offset of destination memory range to copy to
 * @param       src            Starting address offset of source memory range to copy from
 * @param       len            Length of memory range to copy
 * @param       sectionID      section
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_MemCopy(QLIB_CONTEXT_T* qlibContext, U32 dest, U32 src, U32 len, U32 sectionID);

/************************************************************************************************************
 * @brief       This function calculate the CRC-32 checksum of a memory Section or part of
 *
 * @param       qlibContext-   QLIB state object
 * @param       crc32         Pointer to the variable where the CRC-32 checksum will be stored.
 * @param       sectionID     Section index
 * @param       offset        Starting offset of the memory section
 * @param       size          Size of the memory section
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_MemCrc(QLIB_CONTEXT_T* qlibContext, U32* crc32, U32 sectionID, U32 offset, U32 size);

/************************************************************************************************************
 * @brief       This function erases the entire section with either plain-text or secure command
 *
 * @param[in,out]   qlibContext   qlib context object
 * @param[in]       sectionID     Section ID
 * @param[in]       secure        if TRUE, secure erase is performed
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_EraseSection(QLIB_CONTEXT_T* qlibContext, U32 sectionID, BOOL secure);

/************************************************************************************************************
 * @brief This function configures volatile access permissions to given section.
 *
 * @param qlibContext   qlib context object
 * @param sectionID     Section index
 * @param readEnable    if TRUE, read will be enabled for this section
 * @param writeEnable   if TRUE, write will be enabled for this section
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_ConfigAccess(QLIB_CONTEXT_T* qlibContext, U32 sectionID, BOOL readEnable, BOOL writeEnable);

/************************************************************************************************************
 * @brief       This function allows loading of the section keys
 *
 * @param[in,out]   qlibContext     qlib context object
 * @param[in]       sectionID       Section index
 * @param[in]       key             Section key value
 * @param[in]       fullAccess      If TRUE, the given section key is full access key. Otherwise the
 *                                  given key considered a restricted key
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_LoadKey(QLIB_CONTEXT_T* qlibContext, U32 sectionID, const KEY_T key, BOOL fullAccess);

/************************************************************************************************************
 * @brief       The function allows removal of keys from QLib
 *
 * @param       qlibContext  QLIB state object
 * @param       sectionID    Section index
 * @param       fullAccess   If TRUE, the given section key is full access key. Otherwise the given
 *                           key considered a restricted key
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_RemoveKey(QLIB_CONTEXT_T* qlibContext, U32 sectionID, BOOL fullAccess);

/************************************************************************************************************
 * @brief       This function forces an integrity check on specific section
 *
 * @param       qlibContext     QLIB state object
 * @param       sectionID       Section index
 * @param       integrityType   The integrity type to perform
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_CheckIntegrity(QLIB_CONTEXT_T* qlibContext, U32 sectionID, QLIB_INTEGRITY_T integrityType);

/************************************************************************************************************
 * @brief       This function returns CDI value
 *
 * @param       qlibContext     QLIB state object
 * @param[out]  nextCdi         CDI value produced by this module
 * @param[in]   prevCdi         CDI value obtained from previous module
 * @param[in]   sectionId       section number CDI associated with
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_CalcCDI(QLIB_CONTEXT_T* qlibContext, _256BIT nextCdi, const _256BIT prevCdi, U32 sectionId);

/************************************************************************************************************
 * @brief       This function configures the Secure Watchdog functionality.
 *              The key used for the Watchdog functionality, is the key used in currently open session.
 *
 * @param       qlibContext   QLIB state object
 * @param       watchdogCFG   Watchdog and rest configurations
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_Watchdog_ConfigSet(QLIB_CONTEXT_T* qlibContext, const QLIB_WATCHDOG_CONF_T* watchdogCFG);

/************************************************************************************************************
 * @brief       This function read the Secure Watchdog configuration.
 *
 * @param       qlibContext   QLIB state object
 * @param       watchdogCFG   Watchdog and rest configurations
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_Watchdog_ConfigGet(QLIB_CONTEXT_T* qlibContext, QLIB_WATCHDOG_CONF_T* watchdogCFG);

/************************************************************************************************************
 * @brief       This function touches (resets) the Secure Watchdog. A session must be open with
 *              appropriate key to allow successful execution of this function.
 *              They key should have same 'sectionID' as key used for watchdog configuration.
 *              It can be either full-access or restricted key.
 *
 * @param       qlibContext   QLIB state object
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_Watchdog_Touch(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This function triggers (force expire) the Secure Watchdog.
 *
 * @param       qlibContext   QLIB state object
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_Watchdog_Trigger(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This function read the current status of the Secure watchdog timer.
 *              All output parameters can be NULL
 *
 * @param[in,out]   qlibContext        qlib context object
 * @param[out]      milliSecondsPassed The current value of the AWD timer (in milliseconds)
 * @param[out]      expired            TRUE if AWD timer has expired
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_Watchdog_Get(QLIB_CONTEXT_T* qlibContext, U32* milliSecondsPassed, BOOL* expired);

/************************************************************************************************************
 * @brief       This function updates the RAM copy of WID and optionally returns it
 *
 * @param       qlibContext   QLIB state object
 * @param[out]  id            Winbond ID
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_GetWID(QLIB_CONTEXT_T* qlibContext, QLIB_WID_T id);

/************************************************************************************************************
 * @brief       This routine returns the Hardware version of the flash
 *
 * @param[in,out]   qlibContext   qlib context object
 * @param[out]      hwVer         pointer to struct to be filled with the output
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_GetHWVersion(QLIB_CONTEXT_T* qlibContext, QLIB_SEC_HW_VER_T* hwVer);

/************************************************************************************************************
 * @brief       This routine returns the WID and SUID of the flash
 *
 * @param[in,out]   qlibContext   qlib context object
 * @param[out]      id_p          pointer to struct to be filled with the output
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_GetId(QLIB_CONTEXT_T* qlibContext, QLIB_SEC_ID_T* id_p);

/************************************************************************************************************
 * @brief       This routine reads the secure status register and returns QLIB_STATUS__OK if no error occur,
 *              or the corresponding QLIB_STATUS__[ERROR] if an error occur
 *
 * @param       qlibContext   qlib context object
 *
 * @return      returns QLIB_STATUS__OK if no error occur found or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_GetStatus(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This routine configures Flash interface configuration for secure commands.
 *              This function do not handle switching from or to QPI mode,
 *              this is assumed to be completed by the standard module, see @ref QLIB_STD_SetInterface.
 *
 * @param       qlibContext   qlib context object
 * @param[in]   busFormat     SPI bus format
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_SetInterface(QLIB_CONTEXT_T* qlibContext, QLIB_BUS_FORMAT_T busFormat);

/************************************************************************************************************
 * @brief       This function synchronize the host monotonic counter with the flash monotonic counter,
 *              and refresh the out-dated information in the lib context.
 *              This function mast be called after every reset or if secure flash commands has
 *              executed without using this lib.
 *
 * @param       qlibContext   QLIB state object
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_SyncAfterFlashReset(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This function enables plain access to the given section
 *
 * @param       qlibContext     QLIB state object
 * @param       sectionID       Section index
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_EnablePlainAccess(QLIB_CONTEXT_T* qlibContext, U32 sectionID);


#ifdef QLIB_SIGN_DATA_BY_FLASH
/************************************************************************************************************
 * @brief           This function signs (or verifies) the given data using flash commands
 *
 * @param[out]      qlibContext     [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]       sectionID       [Section index](md_definitions.html#DEF_SECTION)
 * @param[in]       dataIn          Input data
 * @param[in]       dataSize        Input data size (in bytes)
 * @param[in,out]   signature       Signature buffer
 * @param[in]       verify          if verify == FALSE, the signature is generated and copied to the 'signature' buffer\n
 *                                  if verify == TRUE, the signature from 'signature' buffer is verified
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_SignVerify(QLIB_CONTEXT_T* qlibContext,
                                  U32             sectionID,
                                  U8*             dataIn,
                                  U32             dataSize,
                                  _256BIT         signature,
                                  BOOL            verify);
#endif

/************************************************************************************************************
 * @brief       This routine Initializes the CTAG mode
 *
 * @param       qlibContext         qlib context object
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_InitCtagMode(QLIB_CONTEXT_T* qlibContext);

#ifndef EXCLUDE_LMS
/************************************************************************************************************
 * @brief       This function writes LMS data structure to the flash and executes the command
 *
 * @param       qlibContext   QLIB state object
 * @param       cmd           Pointer to LMS Command
 * @param       cmdSize       LMS command size in bytes
 * @param       sectionID     Section index
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_SendLMSCommand(QLIB_CONTEXT_T* qlibContext, const U32* cmd, U32 cmdSize, U32 sectionID);
#endif

/************************************************************************************************************
 * @brief       This routine returns the secure instruction lines nibble
 *
 * @param       busMode         SPI bus mode
 *
 * @return      The secure command lines nibble
************************************************************************************************************/
U8 QLIB_SEC_BusModeToSecInstLines(QLIB_BUS_MODE_T busMode) __RAM_SECTION;


/************************************************************************************************************
 * @brief       This function reads the last entry of the log (16B), and the current address of the head of the log.
 *
 * @param       qlibContext-   QLIB state object
 * @param       buf            Pointer to output buffer for the entry data
 * @param       addr           Pointer to output buffer for the address of the head of the log
 * @param       sectionID      Section index
 * @param       secure         If TRUE then secure read, else standard read.
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_SecureLogRead(QLIB_CONTEXT_T* qlibContext, U8* buf, U32* addr, U32 sectionID, BOOL secure);

/************************************************************************************************************
 * @brief       This function appends new entries to the log section in the flash.
 *
 * @param       qlibContext   QLIB state object
 * @param       buf           Pointer to input buffer
 * @param       sectionID     Section index
 * @param       size          Data size
 * @param       secure         If TRUE then secure write, else standard write.
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_SecureLogWrite(QLIB_CONTEXT_T* qlibContext, const U8* buf, U32 sectionID, U32 size, BOOL secure);

/************************************************************************************************************
 * @brief       This function returns an arbitrary number of random bytes generated by the Secure Flash
 *
 *  This function can be used with or without an open session. If a session is open, secure commands will be
 *  used to retrieve the random data from the flash
 *
 * @param[in,out]  qlibContext     [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[out]     random          A pointer to a memory location that will be filled with random data
 * @param[in]      randomSize      Number of random bytes to read
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_GetRandom(QLIB_CONTEXT_T* qlibContext, void* random, U32 randomSize);

#if !defined EXCLUDE_LMS_ATTESTATION && !defined Q2_API
/************************************************************************************************************
 * @brief This function programs the private key seed and key ID into the flash
 *
 * @param[in,out]  qlibContext  QLIB state object
 * @param[in]      seed         Private key seed
 * @param[in]      keyId        Key ID used in all LMS hash calculations
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_LMS_Attest_SetPrivateKey(QLIB_CONTEXT_T*                 qlibContext,
                                                const LMS_ATTEST_PRIVATE_SEED_T seed,
                                                const LMS_ATTEST_KEY_ID_T       keyId);

/************************************************************************************************************
 * @brief This function calculates OTS message signature
 *
 * @param[in,out]  qlibContext      QLIB state object
 * @param[in]      msg              The message data
 * @param[in]      msgSize          The message size in bytes
 * @param[in]      nonce            Random nonce data
 * @param[in, out] pubCache         Optional Merkle tree cache that can be used to speed-up signing.
 * @param[in]      pubCacheLen      Number of array elements in the tree cache
 * @param[out]     sig              Output signature
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_LMS_Attest_Sign(QLIB_CONTEXT_T*          qlibContext,
                                       const U8*                msg,
                                       U32                      msgSize,
                                       const LMS_ATTEST_NONCE_T nonce,
                                       LMS_ATTEST_CHUNK_T       pubCache[],
                                       U32                      pubCacheLen,
                                       QLIB_OTS_SIG_T*          sig);

/************************************************************************************************************
 * @brief This function calculates the LMS public key using flash commands
 *
 * @param[in,out]  qlibContext       QLIB state object
 * @param[out]     pubKey            The calculated public key (the root of the Merkle tree)
 * @param[out]     pubCache          Optional Merkle tree cache that can be used to speed-up signing.
 * @param[in]      pubCacheLen       Number of array elements in the tree cache
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_LMS_Attest_GetPublicKey(QLIB_CONTEXT_T*    qlibContext,
                                               LMS_ATTEST_CHUNK_T pubKey,
                                               LMS_ATTEST_CHUNK_T pubCache[],
                                               U32                pubCacheLen);

/************************************************************************************************************
 * @brief This function calculates OTS message hash
 *
 * @param[in]   keyId               key id
 * @param[in]   leafIndex           Leaf index
 * @param[in]   nonce               Random nonce buffer
 * @param[in]   msg                 Message
 * @param[in]   msgSize             Message size
 * @param[out]  msgHash             Output message hash
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_LMS_Attest_CalcMsgHash(const LMS_ATTEST_KEY_ID_T keyId,
                                              U32                       leafIndex,
                                              const LMS_ATTEST_NONCE_T  nonce,
                                              const U8*                 msg,
                                              U32                       msgSize,
                                              LMS_ATTEST_CHUNK_T        msgHash);
#endif

/************************************************************************************************************
 * @brief       This routine clears any error indication bits from SSR
 *
 * @param       qlibContext   qlib context object
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_ClearSSR(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief This function checks if a key is provisioned
 *
 * @param[in,out]  qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]      keyIdType        The type of key requested
 * @param[in]      sectionID        Section index for the key (if relevant)
 * @param[out]     isProvisioned    TRUE if the key is provisioned, FALSE otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SEC_IsKeyProvisioned(QLIB_CONTEXT_T* qlibContext,
                                        QLIB_KID_TYPE_T keyIdType,
                                        U32             sectionID,
                                        BOOL*           isProvisioned);

#ifdef __cplusplus
}
#endif

#endif // __QLIB_SEC_H__
