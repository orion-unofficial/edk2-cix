/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_qconf_config.c
* @brief      This file contains the QLIB QCONF structure
*
* ### project qlib_samples
*
************************************************************************************************************/
#define Q2_API 1

#ifndef Q2_API

#include "qlib_targets.h"
#if (QLIB_TARGET == all_targets)
#undef QLIB_TARGET
#ifdef QCONF_TARGET
#define QLIB_TARGET QCONF_TARGET
#else
// #error "QCONF_TARGET must be defined with QLIB_TARGET == all_targets"
#define QLIB_TARGET all_targets
#endif
#endif

#include "qlib.h"
#include "qlib_sample_qconf.h"
#include "qlib_common.h"
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                            DEFINITIONS                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
// FLASH START ADDRESS on the microcontroller
#ifndef FLASH_BASE_ADDR
#define FLASH_BASE_ADDR (0x0)
#endif

#define QCONF_WATCHDOG_DEFAULT                                                                          \
    {                                                                                                   \
        FALSE,                    /* enable */                                                          \
            TRUE,                 /* lfOscEn */                                                         \
            FALSE,                /* swResetEn */                                                       \
            FALSE,                /* resetInDedicatedEn */                                              \
            0,                    /* sectionID */                                                       \
            QLIB_AWDT_TH_12_DAYS, /* threshold */                                                       \
            FALSE,                /* lock */                                                            \
            0,                    /* oscRateHz - Set to 0 if non-applicable */                          \
            FALSE,                /* fallbackEn (in flash that supports Q2_SUPPORT_AWDTCFG_FALLBACK) */ \
    }

#ifndef QCONF_STRUCT_ON_RAM
const
#endif
    QCONF_T qconf_g = {
        QCONF_MAGIC_WORD, // magicWord
#ifndef QCONF_STRUCT_ON_RAM
        FLASH_BASE_ADDR, // flashBaseAddr
#endif                   // QCONF_STRUCT_ON_RAM
#ifdef DEPRECATED_CONFIGURATION_API
        //otc:
        {
            QCONF_KD,  //deviceMasterKey
            QCONF_KDS, //deviceSecretKey
            QCONF_KD1, //preProvisionedMasterKey
            // restrictedKeys:
            {
                // die 0
                {QCONF_RESTRICTED_K_0,
                 QCONF_RESTRICTED_K_1,
                 QCONF_RESTRICTED_K_2,
                 QCONF_RESTRICTED_K_3,
                 QCONF_RESTRICTED_K_4,
                 QCONF_RESTRICTED_K_5,
                 QCONF_RESTRICTED_K_6,
                 QCONF_RESTRICTED_K_7,
#if W77Q_VAULT(NULL)
                 QCONF_VAULT_RESTRICTED
#else
                 {0}
#endif
                },
#if QLIB_NUM_OF_DIES > 1
                //die 1
                {
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                },
#endif
#if QLIB_NUM_OF_DIES == 4
                //die 2
                {
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                },
                //die 3
                {
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                },
#endif
            },
            // fullAccessKeys:
            {
                // die 0
                {QCONF_FULL_ACCESS_K_0,
                 QCONF_FULL_ACCESS_K_1,
                 QCONF_FULL_ACCESS_K_2,
                 QCONF_FULL_ACCESS_K_3,
                 QCONF_FULL_ACCESS_K_4,
                 QCONF_FULL_ACCESS_K_5,
                 QCONF_FULL_ACCESS_K_6,
                 QCONF_FULL_ACCESS_K_7,
#if W77Q_VAULT(NULL)
                 QCONF_VAULT_FULL_ACCESS
#else
                 {0}
#endif
                },
#if QLIB_NUM_OF_DIES > 1
                {QCONF_FULL_ACCESS_K_0, QCONF_FULL_ACCESS_K_1, {0}, {0}, {0}, {0}, {0}, {0}, QCONF_VAULT_FULL_ACCESS}, // die 1
#endif
#if QLIB_NUM_OF_DIES == 4
                {QCONF_FULL_ACCESS_K_0, QCONF_FULL_ACCESS_K_1, {0}, {0}, {0}, {0}, {0}, {0}, QCONF_VAULT_FULL_ACCESS}, //die 2,
                {QCONF_FULL_ACCESS_K_0, QCONF_FULL_ACCESS_K_1, {0}, {0}, {0}, {0}, {0}, {0}, QCONF_VAULT_FULL_ACCESS}, //die 3,
#endif
            },
            // lmsKeys
            {
                // die 0
                {
#if W77Q_SUPPORT_LMS(NULL)
                    QCONF_LMS_K_0,
                    QCONF_LMS_K_1,
                    QCONF_LMS_K_2,
                    QCONF_LMS_K_3,
                    QCONF_LMS_K_4,
                    QCONF_LMS_K_5,
                    QCONF_LMS_K_6,
                    QCONF_LMS_K_7,
#if W77Q_VAULT(NULL)
                    QCONF_LMS_K_8,
#endif
#else
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},

#endif
                },
#if QLIB_NUM_OF_DIES > 1
                //die 1
                {
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                },
#endif
#if QLIB_NUM_OF_DIES == 4
                //die 2
                {
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                },
                //die 3
                {
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                    {0},
                },
#endif
            },
            QCONF_SUID, //suid
        },

        // sectionTable:
        {
            {// Section 0 is FW section - holds the boot code
             {
                 BOOT_SECTION_BASE, //baseAddr
                 BOOT_SECTION_SIZE, //size
                 // Section policy structure QLIB_POLICY_T
                 {
                     1, // digestIntegrity: If 1, the section configuration is protected by digest integrity

                     0, // checksumIntegrity: If 1, the section is protected by CRC integrity. Needed for secure boot feature
                     0, // writeProt: If 1, the section is write protected
                     1, // rollbackProt: If 1, the section is rollback protected. Needed for secure boot feature
                     1, // plainAccessWriteEnable: If 1, the section has plain write access
                     1, // plainAccessReadEnable: If 1, the section has plain read access
                     0, // authPlainAccess: If 1, using plain access requires to use @ref QLIB_OpenSession
#if !Q2_POLICY_AUTH_PROT_AC_BIT(NULL)
                     0, // digestIntegrityOnAccess:  If 1, the section is protected by digest integrity
#else                   // QLIB_SAMPLE_UNKNOWN_FLASH_DIGEST
                     1, // digestIntegrityOnAccess:  If 1, the section is protected by digest integrity (if device supports Q2_POLICY_AUTH_PROT_AC_BIT)
#endif                  // QLIB_SAMPLE_UNKNOWN_FLASH_DIGEST
                     0, // slog: If 1, the section is defined as secure log
                 },
                 0, // crc place-holder. it is computed in post-build script according to the firmware code written to the section
                 0x9f58675d7251e2edu, // digest is pre-calculated according to expected section content
             },
             // Section 1: Plain Access section with write protection
             {
                 PA_WRITE_PROTECT_SECTION_BASE, //baseAddr
                 PA_WRITE_PROTECT_SECTION_SIZE, //size
                 //policy
                 {
                     1, //digestIntegrity
                     0, //checksumIntegrity
                     0, //writeProt
                     0, //rollbackProt
                     0, //plainAccessWriteEnable
                     1, //plainAccessReadEnable
                     1, //authPlainAccess
#if Q2_POLICY_AUTH_PROT_AC_BIT(NULL)
                     1, // digestIntegrityOnAccess
#else
                     0,
#endif
                     0, // slog
                 },
                 0,                   // crc
                 0x9f58675d7251e2edu, // digest is pre-calculated according to expected section content (erased section)
             },
             // Section 2: calculate CDI section
             {
                 DIGEST_WRITE_PROTECTED_SECTION_BASE, //baseAddr
                 DIGEST_WRITE_PROTECTED_SECTION_SIZE, //size
                 //policy
                 {
                     1, //digestIntegrity
                     0, //checksumIntegrity
                     0, //writeProt
                     1, //rollbackProt
                     0, //plainAccessWriteEnable
                     1, //plainAccessReadEnable
                     0, //authPlainAccess
#if Q2_POLICY_AUTH_PROT_AC_BIT(NULL)
                     1, // digestIntegrityOnAccess
#else
                     0,
#endif
                     0, // slog
                 },
                 0,                  // crc
                 0x6022e0b9e4f995a6, // pre-calculated digest for empty section
             },
             // section 3: Rollback protected section
             {
                 FW_UPDATE_SECTION_BASE, //baseAddr
                 FW_UPDATE_SECTION_SIZE, //size
                 //policy
                 {
                     0, //digestIntegrity
                     0, //checksumIntegrity
                     0, //writeProt
                     1, //rollbackProt
                     0, //plainAccessWriteEnable
                     1, //plainAccessReadEnable
                     0, //authPlainAccess
                     0, // digestIntegrityOnAccess
                     0, // slog
                 },
                 0, // crc
                 0, // digest
             },
             // section 4 : Secure data section
             {
                 SECURE_DATA_SECTION_BASE, //baseAddr
                 SECURE_DATA_SECTION_SIZE, //size
                 //policy
                 {
                     0, //digestIntegrity
                     0, //checksumIntegrity
                     0, //writeProt
                     0, //rollbackProt
                     0, //plainAccessWriteEnable
                     0, //plainAccessReadEnable
                     0, //authPlainAccess
                     0, // digestIntegrityOnAccess
                     0, // slog
                 },
                 0, // crc
                 0, // digest
             },
             // section 5: A shadow of section 0
             {
                 BOOT_SECTION_BASE, //baseAddr
                 BOOT_SECTION_SIZE, //size
                 //policy
                 {
                     0, // digestIntegrity
                     0, // checksumIntegrity
                     0, // writeProt
                     0, // rollbackProt
                     0, // plainAccessWriteEnable
                     1, // plainAccessReadEnable
                     0, // authPlainAccess
                     0, // digestIntegrityOnAccess
                     0, // slog
                 },
                 0, // crc
                 0, // digest
             },
             // section 6
             {
                 FW_STORAGE_SECTION_BASE, //baseAddr
                 FW_STORAGE_SECTION_SIZE, //size
                 //policy
                 {
                     0, //digestIntegrity
                     0, //checksumIntegrity
                     0, //writeProt
                     0, //rollbackProt
                     0, //plainAccessWriteEnable
                     1, //plainAccessReadEnable
                     0, //authPlainAccess
                     0, // digestIntegrityOnAccess
                     0, // slog
                 },
                 0, // crc
                 0, // digest
             },
             // Section 7 is defined as a fallback section.
             // When section 0 (boot section) integrity fails, W77Q jumps to the fallback section (section 7). In such case, it is important to define the same access policy (write protection / plain access / plain read / auth plain access) both in section 0 and section 7.
             // This is part of the secure boot feature.
             {
                 RECOVERY_SECTION_BASE, //baseAddr
                 RECOVERY_SECTION_SIZE, //size
                 //policy
                 {
                     0, //digestIntegrity
                     0, //checksumIntegrity
                     0, //writeProt
                     0, //rollbackProt
                     1, //plainAccessWriteEnable
                     1, //plainAccessReadEnable
                     0, //authPlainAccess
                     0, // digestIntegrityOnAccess
                     0, // slog
                 },
                 0, // crc
                 0, // digest
             },
             // Section 8 is the vault. Secure access only.
             {
                 MAX_U32, //baseAddr
#if W77Q_VAULT(NULL)
                 _128KB_, //size
#else
                 0,
#endif
                 //policy
                 {
                     0, //digestIntegrity
                     0, //checksumIntegrity
                     0, //writeProt
                     0, //rollbackProt
                     0, //plainAccessWriteEnable
                     0, //plainAccessReadEnable
                     0, //authPlainAccess
                     0, // digestIntegrityOnAccess
                     0, // slog
                 },
                 0, // crc
                 0, // digest
             }},
#if QLIB_NUM_OF_DIES > 1
            //die 1
            {
                {0},
                // Section 1: Plain Access section with write protection
                {
                    0,                //baseAddr
                    FLASH_BLOCK_SIZE, //size
                    //policy
                    {
                        0, //digestIntegrity
                        0, //checksumIntegrity
                        0, //writeProt
                        0, //rollbackProt
                        0, //plainAccessWriteEnable
                        1, //plainAccessReadEnable
                        1, //authPlainAccess
                        0, // digestIntegrityOnAccess
                        0, // slog
                    },
                    0,                  // crc
                    0x9f58675d7251e2ed, // digest is pre-calculated according to expected section content (erased section)
                },
                {0},
                {0},
                {0},
                {0},
                {0},
                {0},
                {0},
            },
#endif
#if QLIB_NUM_OF_DIES == 4
            //die 2
            {
                {0},
                // Section 1: Plain Access section with write protection
                {
                    0,                //baseAddr
                    FLASH_BLOCK_SIZE, //size
                    //policy
                    {
                        0, //digestIntegrity
                        0, //checksumIntegrity
                        0, //writeProt
                        0, //rollbackProt
                        0, //plainAccessWriteEnable
                        1, //plainAccessReadEnable
                        1, //authPlainAccess
                        0, // digestIntegrityOnAccess
                        0, // slog
                    },
                    0,                  // crc
                    0x9f58675d7251e2ed, // digest is pre-calculated according to expected section content (erased section)
                },
                {0},
                {0},
                {0},
                {0},
                {0},
                {0},
                {0},
            },
            //die 3
            {
                {0},
                // Section 1: Plain Access section with write protection
                {
                    0,                //baseAddr
                    FLASH_BLOCK_SIZE, //size
                    //policy
                    {
                        0, //digestIntegrity
                        0, //checksumIntegrity
                        0, //writeProt
                        0, //rollbackProt
                        0, //plainAccessWriteEnable
                        1, //plainAccessReadEnable
                        1, //authPlainAccess
                        0, // digestIntegrityOnAccess
                        0, // slog
                    },
                    0,                  // crc
                    0x9f58675d7251e2ed, // digest is pre-calculated according to expected section content (erased section)
                },
                {0},
                {0},
                {0},
                {0},
                {0},
                {0},
                {0},
            },
#endif
        },
        QCONF_WATCHDOG_DEFAULT,
        //deviceConf
        {
            {{0}, {0}}, // resetResp
            TRUE, // safeFB: if TRUE, When section 0 integrity fails, W77Q jumps to section 7. Needed for secure boot feature
            TRUE, // Speculative Cypher Key Generation is disabled
            TRUE, // nonSecureFormatEn: if TRUE, non-secure FORMAT command is accepted else,
            // must use SFORMAT with device master key to formats the flash
            {
                QLIB_IO23_MODE__QUAD, // IO2/IO3 pin muxing
                TRUE,                 // dedicated RESET_IN pin enable
            },
            {
                QLIB_STD_ADDR_LEN__24_BIT,
#ifdef SPI_INIT_ADDRESS_MODE_4_BYTES
                QLIB_STD_ADDR_MODE__4_BYTE,
#else
                QLIB_STD_ADDR_MODE__3_BYTE,
#endif //SPI_INIT_ADDRESS_MODE_4_BYTES
            },
            FALSE, // RNG Plain access enabled (in case flash supports Q2_RNG_FEATURE)
#if Q2_DEVCFG_CTAG_MODE(NULL)
            TRUE, // OP1 instruction is sent with all available SPI IO pins (in case flash supports Q2_DEVCFG_CTAG_MODE)
#else
            FALSE,
#endif
            FALSE, // lock
#if W77Q_DEVCFG_BOOT_FAIL_RST(NULL)
            TRUE, // bootFailReset
#else
            FALSE,
#endif
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
#if W77Q_VAULT(NULL)
                QLIB_VAULT_128KB_RPMC_DISABLED,
#else
                QLIB_VAULT_DISABLED_RPMC_8_COUNTERS,
#endif
#if QLIB_NUM_OF_DIES > 1
                QLIB_VAULT_DISABLED_RPMC_8_COUNTERS,
#endif
#if QLIB_NUM_OF_DIES == 4
                QLIB_VAULT_DISABLED_RPMC_8_COUNTERS,
                QLIB_VAULT_DISABLED_RPMC_8_COUNTERS,
#endif
            },
#ifdef INIT_FAST_READ_DUMMY_CYCLES
            INIT_FAST_READ_DUMMY_CYCLES,
#else
            DEFAULT_FAST_READ_DUMMY,
#endif
#ifndef Q2_API
            FALSE, // dqsDisable
#endif
        },
#else // DEPRECATED_CONFIGURATION_API
    QLIB_NUM_OF_DIES,
    // flashConfig
    {
        QCONF_WATCHDOG_DEFAULT,
        // must use SFORMAT with device master key to formats the flash
        {
            QLIB_IO23_MODE__QUAD, // IO2/IO3 pin muxing
            TRUE,                 // dedicated RESET_IN pin enable
        },
        {
            QLIB_STD_ADDR_LEN__24_BIT,
#ifdef SPI_INIT_ADDRESS_MODE_4_BYTES
            QLIB_STD_ADDR_MODE__4_BYTE,
#else
                    QLIB_STD_ADDR_MODE__3_BYTE,
#endif //SPI_INIT_ADDRESS_MODE_4_BYTES
        },
#ifdef INIT_FAST_READ_DUMMY_CYCLES
        INIT_FAST_READ_DUMMY_CYCLES,
#else
                DEFAULT_FAST_READ_DUMMY,
#endif
        FALSE, // dqsDisable
    },
    // dieConfig
    {
        QCONF_KD,                   // deviceMasterKey
        QCONF_KDS,                  //deviceSecretKey
        QCONF_KD1,                  //preProvisionedMasterKey
        QCONF_SUID,                 //suid
        {{0}, {0}},                 // resetResp
        TRUE,  // safeFB: if TRUE, When section 0 integrity fails, W77Q jumps to section 7. Needed for secure boot feature
        TRUE,  // Speculative Cypher Key Generation is disabled
        TRUE,  // nonSecureFormatEn: if TRUE, non-secure FORMAT command is accepted else,
        FALSE, // RNG Plain access enabled (in case flash supports Q2_RNG_FEATURE)
#if Q2_DEVCFG_CTAG_MODE(NULL)
        TRUE,  // OP1 instruction is sent with all available SPI IO pins (in case flash supports Q2_DEVCFG_CTAG_MODE)
#else
                FALSE,
#endif
        FALSE, // devcfgLock
#if W77Q_DEVCFG_BOOT_FAIL_RST(NULL)
        TRUE,  // bootFailReset
#else
                FALSE,
#endif
    },
    // cfgSectionArray
    {// die 0
      // sectionConfigTable
      {// section 0
       {
           BOOT_SECTION_BASE, //baseAddr
           BOOT_SECTION_SIZE, //size
           // sectionConfig
           {
               // Section policy structure QLIB_POLICY_T
               {
                          1, // digestIntegrity: If 1, the section configuration is protected by digest integrity
                          0, // checksumIntegrity: If 1, the section is protected by CRC integrity. Needed for secure boot feature
                   0, // writeProt: If 1, the section is write protected
                   1, // rollbackProt: If 1, the section is rollback protected. Needed for secure boot feature
                   1, // plainAccessWriteEnable: If 1, the section has plain write access
                   1, // plainAccessReadEnable: If 1, the section has plain read access
                   0, // authPlainAccess: If 1, using plain access requires to use @ref QLIB_OpenSession
#if !Q2_POLICY_AUTH_PROT_AC_BIT(NULL)
                   0, // digestIntegrityOnAccess:  If 1, the section is protected by digest integrity
#else  // QLIB_SAMPLE_UNKNOWN_FLASH_DIGEST
                          1, // digestIntegrityOnAccess:  If 1, the section is protected by digest integrity (if device supports Q2_POLICY_AUTH_PROT_AC_BIT)
#endif // QLIB_SAMPLE_UNKNOWN_FLASH_DIGEST
                   0, // slog: If 1, the section is defined as secure log
               },
                      0, // crc place-holder. it is computed in post-build script according to the firmware code written to the section
                      0x9f58675d7251e2edu, // digest is pre-calculated according to expected section content
               0,  // version
               {0} // postActions
           },
           TRUE,                  // resetPA
           QCONF_RESTRICTED_K_0,  // restrictedKey
           QCONF_FULL_ACCESS_K_0, // fullAccessKey
           QCONF_LMS_K_0          // lmsKey
       },
       // Section 1: Plain Access section with write protection
       {
           PA_WRITE_PROTECT_SECTION_BASE, //baseAddr
           PA_WRITE_PROTECT_SECTION_SIZE, //size
           // sectionConfig
           {
               // Section policy structure QLIB_POLICY_T
               {
                   1, //digestIntegrity
                   0, //checksumIntegrity
                   0, //writeProt
                   0, //rollbackProt
                   0, //plainAccessWriteEnable
                   1, //plainAccessReadEnable
                   1, //authPlainAccess
#if Q2_POLICY_AUTH_PROT_AC_BIT(NULL)
                   1, // digestIntegrityOnAccess
#else
                          0,
#endif
                   0, // slog
               },
               0,                   // crc
               0x9f58675d7251e2edu, // digest is pre-calculated according to expected section content (erased section)
               0,                   // version
               {0}                  // postActions
           },
           FALSE,                 // resetPA
           QCONF_RESTRICTED_K_1,  // restrictedKey
           QCONF_FULL_ACCESS_K_1, // fullAccessKey
#if W77Q_SUPPORT_LMS(NULL)
           QCONF_LMS_K_1          // lmsKey
#else
                  {0}
#endif //W77Q_SUPPORT_LMS
       },
       // Section 2: calculate CDI section
       {
           DIGEST_WRITE_PROTECTED_SECTION_BASE, //baseAddr
           DIGEST_WRITE_PROTECTED_SECTION_SIZE, //size
           // sectionConfig
           {
               // Section policy structure QLIB_POLICY_T
               {
                   1, //digestIntegrity
                   0, //checksumIntegrity
                   0, //writeProt
                   1, //rollbackProt
                   0, //plainAccessWriteEnable
                   1, //plainAccessReadEnable
                   0, //authPlainAccess
#if Q2_POLICY_AUTH_PROT_AC_BIT(NULL)
                   1, // digestIntegrityOnAccess
#else
                          0,
#endif
                   0, // slog
               },
               0,                  // crc
               0x6022e0b9e4f995a6, // pre-calculated digest for empty section
               0,                  // version
               {0}                 // postActions
           },
           FALSE,                 // resetPA
           QCONF_RESTRICTED_K_2,  // restrictedKey
           QCONF_FULL_ACCESS_K_2, // fullAccessKey
           QCONF_LMS_K_2          // lmsKey
       },
       // Section 3: Rollback protected section
       {
           FW_UPDATE_SECTION_BASE, // baseAddr
           FW_UPDATE_SECTION_SIZE, // size
           // sectionConfig
           {
               // Section policy structure QLIB_POLICY_T
               {
                   0, //digestIntegrity
                   0, //checksumIntegrity
                   0, //writeProt
                   1, //rollbackProt
                   0, //plainAccessWriteEnable
                   1, //plainAccessReadEnable
                   0, //authPlainAccess
                   0, // digestIntegrityOnAccess
                   0, // slog
               },
               0,  // crc
               0,  // digest
               0,  // version
               {0} // postActions
           },
           FALSE,                 // resetPA
           QCONF_RESTRICTED_K_3,  // restrictedKey
           QCONF_FULL_ACCESS_K_3, // fullAccessKey
           QCONF_LMS_K_3          // lmsKey
       },
       // Section 4 : Secure data section
       {
           SECURE_DATA_SECTION_BASE, // baseAddr
           SECURE_DATA_SECTION_SIZE, // size
           // sectionConfig
           {
               // Section policy structure QLIB_POLICY_T
               {
                   0, //digestIntegrity
                   0, //checksumIntegrity
                   0, //writeProt
                   0, //rollbackProt
                   0, //plainAccessWriteEnable
                   0, //plainAccessReadEnable
                   0, //authPlainAccess
                   0, // digestIntegrityOnAccess
                   0, // slog
               },
               0,  // crc
               0,  // digest
               0,  // version
               {0} // postActions
           },
           FALSE,                 // resetPA
           QCONF_RESTRICTED_K_4,  // restrictedKey
           QCONF_FULL_ACCESS_K_4, // fullAccessKey
           QCONF_LMS_K_4          // lmsKey
       },
       // Section 5: A shadow of section 0
       {
           BOOT_SECTION_BASE, // baseAddr
           BOOT_SECTION_SIZE, // size
           // sectionConfig
           {
               // Section policy structure QLIB_POLICY_T
               {
                   0, // digestIntegrity
                   0, // checksumIntegrity
                   0, // writeProt
                   0, // rollbackProt
                   0, // plainAccessWriteEnable
                   1, // plainAccessReadEnable
                   0, // authPlainAccess
                   0, // digestIntegrityOnAccess
                   0, // slog
               },
               0,  // crc
               0,  // digest
               0,  // version
               {0} // postActions
           },
           FALSE,                 // resetPA
           QCONF_RESTRICTED_K_5,  // restrictedKey
           QCONF_FULL_ACCESS_K_5, // fullAccessKey
           QCONF_LMS_K_5          // lmsKey
       },
       // Section 6
       {
           FW_STORAGE_SECTION_BASE, // baseAddr
           FW_STORAGE_SECTION_SIZE, // size
           // sectionConfig
           {
               // Section policy structure QLIB_POLICY_T
               {
                   0, //digestIntegrity
                   0, //checksumIntegrity
                   0, //writeProt
                   0, //rollbackProt
                   0, //plainAccessWriteEnable
                   1, //plainAccessReadEnable
                   0, //authPlainAccess
                   0, // digestIntegrityOnAccess
                   0, // slog
               },
               0,  // crc
               0,  // digest
               0,  // version
               {0} // postActions
           },
           FALSE,                 // resetPA
           QCONF_RESTRICTED_K_6,  // restrictedKey
           QCONF_FULL_ACCESS_K_6, // fullAccessKey
           QCONF_LMS_K_6          // lmsKey
       },
       // Section 7 is defined as a fallback section.
       // When section 0 (boot section) integrity fails, W77Q jumps to the fallback section (section 7). In such case, it is important to define the same access policy (write protection / plain access / plain read / auth plain access) both in section 0 and section 7.
       // This is part of the secure boot feature.
       {
           RECOVERY_SECTION_BASE, // baseAddr
           RECOVERY_SECTION_SIZE, // size
           // sectionConfig
           {
               // Section policy structure QLIB_POLICY_T
               {
                   0, //digestIntegrity
                   0, //checksumIntegrity
                   0, //writeProt
                   0, //rollbackProt
                   1, //plainAccessWriteEnable
                   1, //plainAccessReadEnable
                   0, //authPlainAccess
                   0, // digestIntegrityOnAccess
                   0, // slog
               },
               0,  // crc
               0,  // digest
               0,  // version
               {0} // postActions
           },
           FALSE,                 // resetPA
           QCONF_RESTRICTED_K_7,  // restrictedKey
           QCONF_FULL_ACCESS_K_7, // fullAccessKey
           QCONF_LMS_K_7          // lmsKey
       },
       // Section 8 is the vault. Secure access only.
       {
           MAX_U32, // baseAddr
#if W77Q_VAULT(NULL)
           _128KB_, // size
#else
                  0,
#endif
           // sectionConfig
           {
               // Section policy structure QLIB_POLICY_T
               {
                   0, //digestIntegrity
                   0, //checksumIntegrity
                   0, //writeProt
                   0, //rollbackProt
                   0, //plainAccessWriteEnable
                   0, //plainAccessReadEnable
                   0, //authPlainAccess
                   0, // digestIntegrityOnAccess
                   0, // slog
               },
               0,  // crc
               0,  // digest
               0,  // version
               {0} // postActions
           },
           FALSE,                   // resetPA

#if W77Q_VAULT(NULL)
           QCONF_VAULT_RESTRICTED,  // restrictedKey
           QCONF_VAULT_FULL_ACCESS, // fullAccessKey
           QCONF_LMS_K_8            // lmsKey
#else
                  {0},
                  {0},
                  {0}
#endif
       }}},
#if QLIB_NUM_OF_DIES > 1
     // Die 1
// sectionConfigTable
{
    {0},
        // Section 1: Plain Access section with write protection
        {
            0,                // baseAddr
            FLASH_BLOCK_SIZE, // size
            // sectionConfig
            {
                // Section policy structure QLIB_POLICY_T
                {
                    1, //digestIntegrity
                    0, //checksumIntegrity
                    0, //writeProt
                    0, //rollbackProt
                    0, //plainAccessWriteEnable
                    1, //plainAccessReadEnable
                    1, //authPlainAccess
#if Q2_POLICY_AUTH_PROT_AC_BIT(NULL)
                    1, // digestIntegrityOnAccess
#else
                          0,
#endif
                    0, // slog
                },
                0,                  // crc
                0x9f58675d7251e2ed, // digest is pre-calculated according to expected section content (erased section)
                0,                  // version
                {0}                 // postActions
            },
            FALSE, // resetPA
            {0},   // restrictedKey
            {0},   // fullAccessKey
            {0}    // lmsKey
        },
        {0}, {0}, {0}, {0}, {0}, {0},
    {
        0
    }
}
}
,
#if QLIB_NUM_OF_DIES == 4
// Die 2
{
    // sectionConfigTable
    {{0},
     // Section 1: Plain Access section with write protection
     {
         0,                // baseAddr
         FLASH_BLOCK_SIZE, // size
         // sectionConfig
         {
             // Section policy structure QLIB_POLICY_T
             {
                 1, //digestIntegrity
                 0, //checksumIntegrity
                 0, //writeProt
                 0, //rollbackProt
                 0, //plainAccessWriteEnable
                 1, //plainAccessReadEnable
                 1, //authPlainAccess
#if Q2_POLICY_AUTH_PROT_AC_BIT(NULL)
                 1, // digestIntegrityOnAccess
#else
                 0,
#endif
                 0, // slog
             },
             0,                  // crc
             0x9f58675d7251e2ed, // digest is pre-calculated according to expected section content (erased section)
             0,                  // version
             {0}                 // postActions
         },
         FALSE, // resetPA
         {0},   // restrictedKey
         {0},   // fullAccessKey
         {0}    // lmsKey
     },
     {0},
     {0},
     {0},
     {0},
     {0},
     {0},
     {0}},
}
// Die 3
{
    // sectionConfigTable
    {
        {0},
            // Section 1: Plain Access section with write protection
            {
                0,                // baseAddr
                FLASH_BLOCK_SIZE, // size
                // sectionConfig
                {
                    // Section policy structure QLIB_POLICY_T
                    {
                        1, //digestIntegrity
                        0, //checksumIntegrity
                        0, //writeProt
                        0, //rollbackProt
                        0, //plainAccessWriteEnable
                        1, //plainAccessReadEnable
                        1, //authPlainAccess
#if Q2_POLICY_AUTH_PROT_AC_BIT(NULL)
                        1, // digestIntegrityOnAccess
#else
                        0,
#endif
                        0, // slog
                    },
                    0,                  // crc
                    0x9f58675d7251e2ed, // digest is pre-calculated according to expected section content (erased section)
                    0,                  // version
                    {0}                 // postActions
                },
                FALSE, // resetPA
                {0},   // restrictedKey
                {0},   // fullAccessKey
                {0}    // lmsKey
            },
            {0}, {0}, {0}, {0}, {0}, {0},
        {
            0
        }
    }
}
#endif // QLIB_NUM_OF_DIES == 4
#endif // QLIB_NUM_OF_DIES > 1
}
#endif // DEPRECATED_CONFIGURATION_API
};
#else
//This is a dummy function for suppressing the error: ISO C forbids an empty translation unit.
void foo_qlib_sample_qconf_config(void)
{
}
#endif
