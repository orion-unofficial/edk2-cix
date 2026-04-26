/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qconf.c
* @brief      This file contains (QCONF)[qconf.html] code
*
* ### project qlib
*
-----------------------------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  INCLUDES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#define NO_Q2_API_H
#include "qconf.h"
#include "qlib_utils_crc.h"


/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  DEFINITIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#ifdef INIT_FAST_READ_DUMMY_CYCLES
#define QCONF_FAST_READ_DUMMY INIT_FAST_READ_DUMMY_CYCLES
#else
#define QCONF_FAST_READ_DUMMY DEFAULT_FAST_READ_DUMMY
#endif

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QCONF_Config(QLIB_CONTEXT_T* qlibContext, const QCONF_T* configHeader)
{
    QLIB_STATUS_T ret;
    QCONF_T       ramConfigHeader;
    U32           section;
    U32           dieId;
    /*-------------------------------------------------------------------------------------------------------
     Verify the header
    -------------------------------------------------------------------------------------------------------*/
    if (QCONF_MAGIC_WORD != configHeader->magicWord)
    {
        return QLIB_STATUS__OK;
    }

    /*-------------------------------------------------------------------------------------------------------
     Read the flash configuration header and save in RAM
    -------------------------------------------------------------------------------------------------------*/
    (void)memcpy(&ramConfigHeader, configHeader, sizeof(QCONF_T));

    /*-------------------------------------------------------------------------------------------------------
     Variables initialization
    -------------------------------------------------------------------------------------------------------*/

    /*-------------------------------------------------------------------------------------------------------
     Load all keys
    -------------------------------------------------------------------------------------------------------*/
    for (dieId = QLIB_NUM_OF_DIES; 0u != dieId--;)
    {
#if QLIB_NUM_OF_DIES > 1
        if ((W77Q_MULTI_DIE(qlibContext) != 0u) && (QLIB_GET_NUM_OF_DIES(qlibContext) > dieId))
        {
            QLIB_STATUS_RET_CHECK(QLIB_SetActiveDie(qlibContext, dieId));
        }
#endif
        for (section = 0; section < ((W77Q_VAULT(qlibContext) != 0u) ? QLIB_NUM_OF_SECTIONS : QLIB_NUM_OF_MAIN_SECTIONS);
             section++)
        {
#ifdef DEPRECATED_CONFIGURATION_API
#ifndef Q2_API
            if (0u != ramConfigHeader.sectionTable[dieId][section].size)
            {
                QLIB_ASSERT_RET(qlibContext->activeDie == dieId, QLIB_STATUS__INVALID_PARAMETER);
                QLIB_STATUS_RET_CHECK_GOTO(QLIB_LoadKey(qlibContext,
                                                        section,
                                                        ramConfigHeader.otc.fullAccessKeys[dieId][section],
                                                        TRUE),
                                           ret,
                                           exit);
            }
#else  // Q2_API
            if (0u != ramConfigHeader.sectionTable[section].size)
            {
                QLIB_STATUS_RET_CHECK_GOTO(QLIB_LoadKey(qlibContext, section, ramConfigHeader.otc.fullAccessKeys[section], TRUE),
                                           ret,
                                           exit);
            }
#endif // Q2_API
#else  // DEPRECATED_CONFIGURATION_API
            if (0u != ramConfigHeader.cfgSectionArray[dieId].sectionConfigTable[section].size)
            {
                QLIB_ASSERT_RET(qlibContext->activeDie == dieId, QLIB_STATUS__INVALID_PARAMETER);
                QLIB_STATUS_RET_CHECK_GOTO(QLIB_LoadKey(qlibContext,
                                                        section,
                                                        ramConfigHeader.cfgSectionArray[dieId]
                                                            .sectionConfigTable[section]
                                                            .fullAccessKey,
                                                        TRUE),
                                           ret,
                                           exit);
            }
#endif // DEPRECATED_CONFIGURATION_API
        }
    }
#ifndef QCONF_STRUCT_ON_RAM
    /*-------------------------------------------------------------------------------------------------------
     Wipe out all the keys and the magic number in the header. This makes the header sealed.
    -------------------------------------------------------------------------------------------------------*/
    {
        _128BIT wipedConfigurationsBuffer = {0};
        _128BIT verifyConfigurationsBuffer;
        U32     headerOffsetInFlash;
        U32     offset;
        U32     bufferSize;

        QLIB_ASSERT_RET(MAX_U32 > (((UPTR)configHeader) - ramConfigHeader.flashBaseAddr), QLIB_STATUS__INVALID_PARAMETER);
        headerOffsetInFlash = (U32)(((UPTR)configHeader) - ramConfigHeader.flashBaseAddr);

        for (offset = 0; offset < sizeof(QCONF_T); offset += sizeof(_128BIT))
        {
            bufferSize = MIN(sizeof(_128BIT), sizeof(QCONF_T) - offset);
            QLIB_STATUS_RET_CHECK_GOTO(QLIB_Write(qlibContext,
                                                  (U8*)wipedConfigurationsBuffer,
                                                  0,
                                                  headerOffsetInFlash + offset,
                                                  bufferSize,
                                                  FALSE),
                                       ret,
                                       exit);

            /*-----------------------------------------------------------------------------------------------
            Verify that the keys were removed
            -----------------------------------------------------------------------------------------------*/
            QLIB_STATUS_RET_CHECK_GOTO(QLIB_Read(qlibContext,
                                                 (U8*)verifyConfigurationsBuffer,
                                                 0,
                                                 headerOffsetInFlash + offset,
                                                 bufferSize,
                                                 FALSE,
                                                 FALSE),
                                       ret,
                                       exit);

            if (0 != memcmp((U8*)wipedConfigurationsBuffer, (U8*)verifyConfigurationsBuffer, bufferSize))
            {
                ret = QLIB_STATUS__COMMAND_FAIL;
                goto exit;
            }
        }
    }
#endif // QCONF_STRUCT_ON_RAM

    /*-------------------------------------------------------------------------------------------------------
     Write the configuration to the flash device
    -------------------------------------------------------------------------------------------------------*/

    {
#ifdef DEPRECATED_CONFIGURATION_API
        const QLIB_LMS_KEY_ARRAY_T* pLmsKeys;
        U32*                        preProvisionedMasterKey;
#ifndef Q2_API
        pLmsKeys = (W77Q_SUPPORT_LMS(qlibContext) != 0u) ? (const QLIB_LMS_KEY_ARRAY_T*)(ramConfigHeader.otc.lmsKeys) : NULL;
        preProvisionedMasterKey =
            (W77Q_PRE_PROV_MASTER_KEY(qlibContext) != 0u) ? (ramConfigHeader.otc.preProvisionedMasterKey) : NULL;
#else
        pLmsKeys                = NULL;
        preProvisionedMasterKey = NULL;
#endif
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_ConfigDevice(qlibContext,
                                                     ramConfigHeader.otc.deviceMasterKey,
                                                     ramConfigHeader.otc.deviceSecretKey,
                                                     (const QLIB_SECTION_CONFIG_TABLE_T*)(ramConfigHeader.sectionTable),
                                                     (const KEY_ARRAY_T*)(ramConfigHeader.otc.restrictedKeys),
                                                     (const KEY_ARRAY_T*)(ramConfigHeader.otc.fullAccessKeys),
                                                     pLmsKeys,
                                                     preProvisionedMasterKey,
                                                     &(ramConfigHeader.watchdogDefault),
                                                     &(ramConfigHeader.deviceConf),
                                                     ramConfigHeader.otc.suid),
                                   ret,
                                   exit);
#else  // DEPRECATED_CONFIGURATION_API
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_ConfigDeviceMultiDie(qlibContext,
                                                             &ramConfigHeader.flashConfig,
                                                             &ramConfigHeader.dieConfig,
                                                             ramConfigHeader.cfgSectionArray,
                                                             QLIB_GET_NUM_OF_DIES(qlibContext)),
                                   ret,
                                   exit);
#endif // DEPRECATED_CONFIGURATION_API
    }

exit:
    /*-------------------------------------------------------------------------------------------------------
     Close the session on error
    -------------------------------------------------------------------------------------------------------*/
    if (QLIB_STATUS__OK != ret)
    {
        (void)QLIB_CloseSession(qlibContext, section);
    }

    /*-------------------------------------------------------------------------------------------------------
     Remove all keys
    -------------------------------------------------------------------------------------------------------*/
    (void)memset(&ramConfigHeader, 0x0, sizeof(QCONF_T));
    for (dieId = QLIB_NUM_OF_DIES; 0u != dieId--;)
    {
#if QLIB_NUM_OF_DIES > 1
        if ((W77Q_MULTI_DIE(qlibContext) != 0u) && (QLIB_GET_NUM_OF_DIES(qlibContext) > dieId))
        {
            (void)QLIB_SetActiveDie(qlibContext, dieId);
        }
#endif
        for (section = 0; section < ((W77Q_VAULT(qlibContext) != 0u) ? QLIB_NUM_OF_SECTIONS : QLIB_NUM_OF_MAIN_SECTIONS);
             section++)
        {
            (void)QLIB_RemoveKey(qlibContext, section, TRUE);
        }
    }
    return ret;
}

#ifdef DEPRECATED_CONFIGURATION_API
/************************************************************************************************************
 * @brief       This routine initializes the sectionTable and device configuration for legacy flash.
 *
 * @param[in]   qlibContext   [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[out]  sectionTable  pointer to the flash section configuration table
 * @param[out]  deviceConf    pointer to the device configuration
 *
************************************************************************************************************/
static void QCONF_initLegacyConfig_L(QLIB_CONTEXT_T*             qlibContext,
                                     QLIB_SECTION_CONFIG_TABLE_T sectionTable[QLIB_NUM_OF_DIES],
                                     QLIB_DEVICE_CONF_T*         deviceConf)
{
    U32 dieId = 0;

    // map section 0,1 for legacy access to all the flash size
#define FLASH_DIE_LEGACY_SECTION_TABLE               \
    {                                                \
        /* Section 0 */                              \
        {                                            \
            0, /* baseAddr */                        \
            0, /* size */                            \
            {                                        \
                /* policy */                         \
                0, /* digestIntegrity */             \
                0, /* checksumIntegrity */           \
                0, /* writeProt */                   \
                0, /* rollbackProt */                \
                1, /* plainAccessWriteEnable */      \
                1, /* plainAccessReadEnable */       \
                0, /* authPlainAccess */             \
                0, /* digestIntegrityOnAccess */     \
                0, /* slog */                        \
            },                                       \
            0, /* section crc */                     \
            0, /* section digest */                  \
        },     /* Section 1 */                       \
            {                                        \
                0, /* baseAddr */                    \
                0, /* size */                        \
                {                                    \
                    /* policy */                     \
                    0, /* digestIntegrity */         \
                    0, /* checksumIntegrity */       \
                    0, /* writeProt */               \
                    0, /* rollbackProt */            \
                    1, /* plainAccessWriteEnable */  \
                    1, /* plainAccessReadEnable */   \
                    0, /* authPlainAccess */         \
                    0, /* digestIntegrityOnAccess */ \
                    0, /* slog */                    \
                },                                   \
                0, /* section crc */                 \
                0, /* section digest */              \
            },                                       \
            {0}, /* Section 2 */                     \
            {0}, /* Section 3 */                     \
            {0}, /* Section 4 */                     \
            {0}, /* Section 5 */                     \
            {0}, /* Section 6 */                     \
            {0}, /* Section 7 */                     \
        /* Section 8 (vault) */                      \
        {                                            \
            MAX_U32, /* baseAddr */                  \
                0,   /* size */                      \
                {0}, /* policy */                    \
                0,   /* section crc */               \
                0,   /* section digest */            \
        }                                            \
    }

    QLIB_SECTION_CONFIG_TABLE_T sectionTable_l[QLIB_NUM_OF_DIES] =
    { FLASH_DIE_LEGACY_SECTION_TABLE, // die 0
#if QLIB_NUM_OF_DIES > 1
      FLASH_DIE_LEGACY_SECTION_TABLE, //die 1
#endif
#if QLIB_NUM_OF_DIES == 4
      FLASH_DIE_LEGACY_SECTION_TABLE, //die 2
      FLASH_DIE_LEGACY_SECTION_TABLE, //die 3
#endif
    };

    QLIB_DEVICE_CONF_T deviceConf_l = {
        {{0}, {0}}, // resetResp
        FALSE,      // safeFB
        FALSE,      // speculCK
        TRUE,       // nonSecureFormatEn
        {
            QLIB_IO23_MODE__QUAD, // IO2 and IO3 mux
            TRUE,                 // Dedicated reset-in mux
        },
        {
            QLIB_STD_ADDR_LEN__LAST, // addrLen
#ifdef SPI_INIT_ADDRESS_MODE_4_BYTES
            QLIB_STD_ADDR_MODE__4_BYTE,
#else
            QLIB_STD_ADDR_MODE__3_BYTE,
#endif
        },
        FALSE, // RNG Plain access enabled (in case flash supports W77Q_RNG_FEATURE)
        FALSE, // ctagModeMulti (relevant if the flash supports Q2_DEVCFG_CTAG_MODE)
        FALSE, // lock
        FALSE, // bootFailReset
               // resetPA
        {
            {TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
#if QLIB_NUM_OF_DIES > 1
            {TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
#endif
#if QLIB_NUM_OF_DIES == 4
            {TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
            {TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
#endif
        },
        // vaultSize
        {
            QLIB_VAULT_DISABLED_RPMC_8_COUNTERS,
#if QLIB_NUM_OF_DIES > 1
            QLIB_VAULT_DISABLED_RPMC_8_COUNTERS,
#endif
#if QLIB_NUM_OF_DIES == 4
            QLIB_VAULT_DISABLED_RPMC_8_COUNTERS,
            QLIB_VAULT_DISABLED_RPMC_8_COUNTERS,
#endif
        },
        QCONF_FAST_READ_DUMMY,
#ifndef Q2_API
        FALSE // dqsDisable
#endif
    };

    TOUCH(qlibContext);

    if (sectionTable != NULL)
    {
        (void)memcpy((U8*)sectionTable, (U8*)sectionTable_l, sizeof(sectionTable_l));
        for (dieId = 0; (U8)dieId < QLIB_GET_NUM_OF_DIES(qlibContext); dieId++)
        {
            sectionTable[dieId][0].size     = MIN(QLIB_GET_FLASH_SIZE(qlibContext), QLIB_MAX_SECTION_SIZE);
            sectionTable[dieId][1].size     = (sectionTable[dieId][0].size == QLIB_GET_FLASH_SIZE(qlibContext))
                                                  ? 0u
                                                  : (QLIB_GET_FLASH_SIZE(qlibContext) - QLIB_MAX_SECTION_SIZE);
            sectionTable[dieId][1].baseAddr = (sectionTable[dieId][1].size > 0u ? QLIB_MAX_SECTION_SIZE : 0u);

            if (W77Q_VAULT(qlibContext) != 0u)
            {
                sectionTable[dieId][QLIB_SECTION_ID_VAULT].size = _128KB_;
            }
        }
    }

    (void)memcpy(deviceConf, &deviceConf_l, sizeof(QLIB_DEVICE_CONF_T));
    for (dieId = 0; (U8)dieId < QLIB_GET_NUM_OF_DIES(qlibContext); dieId++)
    {
        if (W77Q_VAULT(qlibContext) != 0u)
        {
            deviceConf->vaultSize[dieId] = QLIB_VAULT_128KB_RPMC_DISABLED;
        }
        if (W77Q_DEVCFG_BOOT_FAIL_RST(qlibContext) != 0u)
        {
            deviceConf->bootFailReset = TRUE;
        }
    }
    deviceConf->stdAddrSize.addrLen =
        (LOG2(QLIB_GET_FLASH_SIZE(qlibContext)) >= 24u
             ? QLIB_STD_ADDR_LEN__27_BIT
             : (LOG2(QLIB_GET_FLASH_SIZE(qlibContext)) == 23u ? QLIB_STD_ADDR_LEN__26_BIT : QLIB_STD_ADDR_LEN__25_BIT));
}
#else // DEPRECATED_CONFIGURATION_API
static void QCONF_initLegacyConfig_L(QLIB_CONTEXT_T* qlibContext,
                                     QLIB_FLASH_CONFIG_T* flashCfg,
                                     QLIB_DIE_CONFIG_T* dieCfg,
                                     QLIB_SECTIONS_CONF_TABLE_T* cfgSectionArray,
                                     U32 numOfDies)
{
    U32 dieId;

    // map section 0,1 for legacy access to all the flash size
#define FLASH_DIE_LEGACY_SECTION_ARRAY                       \
    {                                                        \
        {                                                    \
            {                                                \
                /* Section 0 */                              \
                0, /* baseAddr */                            \
                0, /* size */                                \
                {                                            \
                    {                                        \
                        /* policy */                         \
                        0, /* digestIntegrity */             \
                        0, /* checksumIntegrity */           \
                        0, /* writeProt */                   \
                        0, /* rollbackProt */                \
                        1, /* plainAccessWriteEnable */      \
                        1, /* plainAccessReadEnable */       \
                        0, /* authPlainAccess */             \
                        0, /* digestIntegrityOnAccess */     \
                        0, /* slog */                        \
                    },                                       \
                    0,  /* section crc */                    \
                    0,  /* section digest */                 \
                    0,  /* version */                        \
                    {0} /* postActions */                    \
                },                                           \
                1,   /* resetPA */                           \
                {0}, /* restrictedKey */                     \
                {0}, /* fullAccessKey */                     \
                {0}, /* lmsKey */                            \
            },                                               \
                {                                            \
                    /* Section 1 */                          \
                    0, /* baseAddr */                        \
                    0, /* size */                            \
                    {                                        \
                        {                                    \
                            /* policy */                     \
                            0, /* digestIntegrity */         \
                            0, /* checksumIntegrity */       \
                            0, /* writeProt */               \
                            0, /* rollbackProt */            \
                            1, /* plainAccessWriteEnable */  \
                            1, /* plainAccessReadEnable */   \
                            0, /* authPlainAccess */         \
                            0, /* digestIntegrityOnAccess */ \
                            0, /* slog */                    \
                        },                                   \
                        0,  /* section crc */                \
                        0,  /* section digest */             \
                        0,  /* version */                    \
                        {0} /* postActions */                \
                    },                                       \
                    0,   /* resetPA */                       \
                    {0}, /* restrictedKey */                 \
                    {0}, /* fullAccessKey */                 \
                    {0}, /* lmsKey */                        \
                },                                           \
                {0}, /* Section 2 */                         \
                {0}, /* Section 3 */                         \
                {0}, /* Section 4 */                         \
                {0}, /* Section 5 */                         \
                {0}, /* Section 6 */                         \
                {0}, /* Section 7 */                         \
            {                                                \
                0                                            \
            } /* Section 8 (vault) */                        \
        }                                                    \
    }

    QLIB_SECTIONS_CONF_TABLE_T cfgSectionArray_l[QLIB_NUM_OF_DIES] =
    { FLASH_DIE_LEGACY_SECTION_ARRAY, // die 0
#if QLIB_NUM_OF_DIES > 1
      FLASH_DIE_LEGACY_SECTION_ARRAY, //die 1
#endif
#if QLIB_NUM_OF_DIES == 4
      FLASH_DIE_LEGACY_SECTION_ARRAY, //die 2
      FLASH_DIE_LEGACY_SECTION_ARRAY, //die 3
#endif
    };

    QLIB_FLASH_CONFIG_T flashCfg_l = {
        {0}, // watchdogDefault
        {
            // pinMux
            QLIB_IO23_MODE__QUAD, // IO2 and IO3 mux
            TRUE,                 // Dedicated reset-in mux
        },
        {
            QLIB_STD_ADDR_LEN__27_BIT,  // addrLen
#ifdef SPI_INIT_ADDRESS_MODE_4_BYTES
            QLIB_STD_ADDR_MODE__4_BYTE, // addrMode
#else
            QLIB_STD_ADDR_MODE__3_BYTE, // addrMode
#endif
        },
        QCONF_FAST_READ_DUMMY, // fastReadDummyCycles
        FALSE                  // dqsDisable
    };

    QLIB_DIE_CONFIG_T dieCfg_l = {
        {0},        // deviceMasterKey
        {0},        // deviceSecretKey
        {0},        // preProvisionedMasterKey
        {0},        // suid
        {{0}, {0}}, // resetResp
        FALSE,      // safeFB
        FALSE,      // speculCK
        TRUE,       // nonSecureFormatEn
        FALSE,      // rngPlainAccessEn
        FALSE,      // ctagModeMulti
        FALSE,      // devcfgLock
        FALSE       // bootFailReset
    };

    TOUCH(qlibContext);

    if (NULL != flashCfg)
    {
        (void)memcpy((U8*)flashCfg, (U8*)&flashCfg_l, sizeof(QLIB_FLASH_CONFIG_T));
    }

    if (NULL != dieCfg)
    {
        (void)memcpy((U8*)dieCfg, (U8*)&dieCfg_l, sizeof(QLIB_DIE_CONFIG_T));
    }

    if (NULL != cfgSectionArray)
    {
        (void)memcpy((U8*)cfgSectionArray, (U8*)cfgSectionArray_l, sizeof(cfgSectionArray_l));

        for (dieId = 0; dieId < numOfDies; ++dieId)
        {
            cfgSectionArray[dieId].sectionConfigTable[0].size = MIN(QLIB_GET_FLASH_SIZE(qlibContext), QLIB_MAX_SECTION_SIZE);
            cfgSectionArray[dieId].sectionConfigTable[1].size =
                (cfgSectionArray[dieId].sectionConfigTable[0].size == QLIB_GET_FLASH_SIZE(qlibContext))
                    ? 0u
                    : MIN((QLIB_GET_FLASH_SIZE(qlibContext) - QLIB_MAX_SECTION_SIZE), QLIB_MAX_SECTION_SIZE);
            cfgSectionArray[dieId].sectionConfigTable[1].baseAddr =
                (cfgSectionArray[dieId].sectionConfigTable[1].size > 0u) ? QLIB_MAX_SECTION_SIZE : 0u;

            if (W77Q_VAULT(qlibContext) != 0u)
            {
                cfgSectionArray[dieId].sectionConfigTable[QLIB_SECTION_ID_VAULT].size = _128KB_;
            }
        }
    }
}
#endif // DEPRECATED_CONFIGURATION_API

QLIB_STATUS_T QCONF_Recovery(QLIB_CONTEXT_T* qlibContext, const QCONF_OTC_T* otc)
{
    QCONF_OTC_T ramOtc;
    GMC_T       GMC;
    BOOL        nonSecureFormatEn;

#ifdef DEPRECATED_CONFIGURATION_API
    QLIB_SECTION_CONFIG_TABLE_T sectionTable[QLIB_NUM_OF_DIES];
    QLIB_DEVICE_CONF_T          deviceConf = {0};
#else  // DEPRECATED_CONFIGURATION_API
    QLIB_FLASH_CONFIG_T flashCfg;
    QLIB_DIE_CONFIG_T dieCfg;
    QLIB_SECTIONS_CONF_TABLE_T cfgSectionArray[QLIB_NUM_OF_DIES];
    U32 dieId;
    U32 sectionId;
#endif // DEPRECATED_CONFIGURATION_API

    QLIB_WATCHDOG_CONF_T watchdogDefault = {
        FALSE,                // enable
        FALSE,                // lfOscEn
        FALSE,                // swResetEn
        FALSE,                // authenticated
        0,                    // sectionID
        QLIB_AWDT_TH_12_DAYS, // threshold
        FALSE,                // lock
        0,                    // oscRateHz
        FALSE,                // fallbackEn (in flash that supports Q2_SUPPORT_AWDTCFG_FALLBACK)
    };
    U32* preProvisionedMasterKey;

#ifdef DEPRECATED_CONFIGURATION_API
    QCONF_initLegacyConfig_L(qlibContext, (W77Q_FORMAT_MODE(qlibContext) == 1u) ? NULL : sectionTable, &deviceConf);
#else  // DEPRECATED_CONFIGURATION_API
    QCONF_initLegacyConfig_L(qlibContext, &flashCfg, &dieCfg, cfgSectionArray, QLIB_GET_NUM_OF_DIES(qlibContext));
#endif // DEPRECATED_CONFIGURATION_API

    /*-------------------------------------------------------------------------------------------------------
     make a local copy of the OTC (in case that configurations are in flash)
    -------------------------------------------------------------------------------------------------------*/
    (void)memcpy(&ramOtc, otc, sizeof(QCONF_OTC_T));

#ifndef Q2_API
    preProvisionedMasterKey = (W77Q_PRE_PROV_MASTER_KEY(qlibContext) != 0u) ? (ramOtc.preProvisionedMasterKey) : NULL;
#else
    preProvisionedMasterKey = NULL;
#endif

    /*-------------------------------------------------------------------------------------------------------
     Ensure device can be formatted and configure if not
     ------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SEC__get_GMC(qlibContext, GMC));
    nonSecureFormatEn = (READ_VAR_FIELD(QLIB_REG_GMC_GET_DEVCFG(GMC), QLIB_REG_DEVCFG__FORMAT_EN) == 1u) ? TRUE : FALSE;
    if (FALSE == nonSecureFormatEn)
    {
#ifdef DEPRECATED_CONFIGURATION_API
        QLIB_STATUS_RET_CHECK(QLIB_ConfigDevice(qlibContext,
                                                ramOtc.deviceMasterKey,
                                                NULL,
                                                NULL,
                                                NULL,
                                                NULL,
                                                NULL,
                                                preProvisionedMasterKey,
                                                NULL,
                                                &(deviceConf),
                                                NULL));
#else  // DEPRECATED_CONFIGURATION_API
        (void)memcpy(&flashCfg.watchdogDefault, &watchdogDefault, sizeof(QLIB_WATCHDOG_CONF_T));
        (void)memcpy(dieCfg.deviceMasterKey, ramOtc.deviceMasterKey, sizeof(KEY_T));
        (void)memcpy(dieCfg.preProvisionedMasterKey, preProvisionedMasterKey, sizeof(KEY_T));
        QLIB_STATUS_RET_CHECK(
            QLIB_ConfigDeviceMultiDie(qlibContext, &flashCfg, &dieCfg, NULL, QLIB_GET_NUM_OF_DIES(qlibContext)));
#endif // DEPRECATED_CONFIGURATION_API
    }

    /*-------------------------------------------------------------------------------------------------------
     Format the flash - clean from all it's data and configurations
    -------------------------------------------------------------------------------------------------------*/
    if (W77Q_FORMAT_MODE(qlibContext) != 0u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_Format(qlibContext, NULL, FALSE, TRUE));

#ifndef EXCLUDE_Q2_4_BYTES_ADDRESS_MODE
        if (Q2_4_BYTES_ADDRESS_MODE(qlibContext) == 1u)
        {
#ifdef SPI_INIT_ADDRESS_MODE_4_BYTES
            QLIB_STATUS_RET_CHECK(QLIB_SetPowerUpAddressMode(qlibContext, QLIB_STD_ADDR_MODE__4_BYTE));
#else
            QLIB_STATUS_RET_CHECK(QLIB_SetPowerUpAddressMode(qlibContext, QLIB_STD_ADDR_MODE__3_BYTE));
#endif
        }
#endif

#ifndef EXCLUDE_FAST_READ_DUMMY_CONFIG
        if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
        {
            QLIB_STATUS_RET_CHECK(QLIB_SetPowerUpFastReadDummyCycles(qlibContext, QCONF_FAST_READ_DUMMY));
        }
#endif
    }
    else
    {
        QLIB_STATUS_RET_CHECK(QLIB_Format(qlibContext, NULL, FALSE, FALSE));

        /*---------------------------------------------------------------------------------------------------
         Write the configuration to the flash device
        ---------------------------------------------------------------------------------------------------*/
#ifdef DEPRECATED_CONFIGURATION_API
        QLIB_STATUS_RET_CHECK(QLIB_ConfigDevice(qlibContext,
                                                ramOtc.deviceMasterKey,
                                                ramOtc.deviceSecretKey,
                                                (const QLIB_SECTION_CONFIG_TABLE_T*)(sectionTable),
                                                (const KEY_ARRAY_T*)(ramOtc.restrictedKeys),
                                                (const KEY_ARRAY_T*)(ramOtc.fullAccessKeys),
                                                NULL,
                                                preProvisionedMasterKey,
                                                &(watchdogDefault),
                                                &(deviceConf),
                                                ramOtc.suid));
#else  // DEPRECATED_CONFIGURATION_API
        (void)memcpy(&flashCfg.watchdogDefault, &watchdogDefault, sizeof(QLIB_WATCHDOG_CONF_T));
        (void)memcpy(dieCfg.deviceMasterKey, ramOtc.deviceMasterKey, sizeof(KEY_T));
        (void)memcpy(dieCfg.deviceSecretKey, ramOtc.deviceSecretKey, sizeof(KEY_T));
        (void)memcpy(dieCfg.preProvisionedMasterKey, preProvisionedMasterKey, sizeof(KEY_T));
        (void)memcpy(dieCfg.suid, ramOtc.suid, sizeof(_128BIT));

        for (dieId = 0; dieId < QLIB_GET_NUM_OF_DIES(qlibContext); ++dieId)
        {
            for (sectionId = 0; sectionId < QLIB_NUM_OF_SECTIONS; ++sectionId)
                if (cfgSectionArray[dieId].sectionConfigTable[sectionId].size > 0)
                {
                    (void)memcpy(cfgSectionArray[dieId].sectionConfigTable[sectionId].restrictedKey,
                                 ramOtc.restrictedKeys[dieId][sectionId],
                                 sizeof(KEY_T));
                    (void)memcpy(cfgSectionArray[dieId].sectionConfigTable[sectionId].fullAccessKey,
                                 ramOtc.fullAccessKeys[dieId][sectionId],
                                 sizeof(KEY_T));
                }
        }
        QLIB_STATUS_RET_CHECK(
            QLIB_ConfigDeviceMultiDie(qlibContext, &flashCfg, &dieCfg, cfgSectionArray, QLIB_GET_NUM_OF_DIES(qlibContext)));
#endif // DEPRECATED_CONFIGURATION_API
    }

#ifndef EXCLUDE_Q2_4_BYTES_ADDRESS_MODE
    if (Q2_4_BYTES_ADDRESS_MODE(qlibContext) != 0u)
    {
#ifdef SPI_INIT_ADDRESS_MODE_4_BYTES
        QLIB_STATUS_RET_CHECK(QLIB_SetAddressMode(qlibContext, QLIB_STD_ADDR_MODE__4_BYTE));
#else
        QLIB_STATUS_RET_CHECK(QLIB_SetAddressMode(qlibContext, QLIB_STD_ADDR_MODE__3_BYTE));
#endif
    }
#endif

#ifndef EXCLUDE_FAST_READ_DUMMY_CONFIG
    if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_SetFastReadDummyCycles(qlibContext, QCONF_FAST_READ_DUMMY));
    }
#endif

    return QLIB_STATUS__OK;
}

