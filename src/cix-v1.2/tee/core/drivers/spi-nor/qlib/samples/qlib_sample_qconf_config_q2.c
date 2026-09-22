#ifndef __QLIB_SAMPLE_QCONF_CONFIG_Q2_H__
#define __QLIB_SAMPLE_QCONF_CONFIG_Q2_H__
#endif //__QLIB_SAMPLE_QCONF_CONFIG_Q2_H__

/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_qconf_config_q2.c
* @brief      This file contains the QLIB QCONF structure for Q2_API
*
* ### project qlib_samples
*
************************************************************************************************************/
#define Q2_API 1
#ifdef Q2_API
#define QLIB_NUM_OF_DIES 1
#include "qlib_targets.h"
#if (QLIB_TARGET == all_targets)
#undef QLIB_TARGET
#ifdef QCONF_TARGET
#define QLIB_TARGET QCONF_TARGET
#else
// #error "QCONF_TARGET must be defined with QLIB_TARGET == all_targets"
#endif
#endif

#include "qlib.h"
#include "qlib_sample_qconf.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                            DEFINITIONS                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

// FLASH START ADDRESS on the microcontroller
#ifndef FLASH_BASE_ADDR
#define FLASH_BASE_ADDR (0x0)
#endif

#ifndef QCONF_STRUCT_ON_RAM
const
#endif
    QCONF_T qconf_g =
        {QCONF_MAGIC_WORD, // magicWord
#ifndef QCONF_STRUCT_ON_RAM
        FLASH_BASE_ADDR, // flashBaseAddr
#endif                    // QCONF_STRUCT_ON_RAM
        //otc:
        {
            QCONF_KD,  //deviceMasterKey
            QCONF_KDS, //deviceSecretKey
            // restrictedKeys:
            {
                QCONF_RESTRICTED_K_0,
                QCONF_RESTRICTED_K_1,
                QCONF_RESTRICTED_K_2,
                QCONF_RESTRICTED_K_3,
                QCONF_RESTRICTED_K_4,
                QCONF_RESTRICTED_K_5,
                QCONF_RESTRICTED_K_6,
                QCONF_RESTRICTED_K_7,
            },
            // fullAccessKeys:
            {
                QCONF_FULL_ACCESS_K_0,
                QCONF_FULL_ACCESS_K_1,
                QCONF_FULL_ACCESS_K_2,
                QCONF_FULL_ACCESS_K_3,
                QCONF_FULL_ACCESS_K_4,
                QCONF_FULL_ACCESS_K_5,
                QCONF_FULL_ACCESS_K_6,
                QCONF_FULL_ACCESS_K_7,
            },
            QCONF_SUID, //suid
        },
#if QCONF_TARGET == w77q128jw_revA
// sectionTable:
         {
             {
                 // Section 0 is FW section - holds the boot code
                 0, //baseAddr
                 _8MB_, //size
                 // Section policy structure QLIB_POLICY_T
                 {
                 0, // digestIntegrity: If 1, the section configuration is protected by digest integrity
                 0, // checksumIntegrity: If 1, the section is protected by CRC integrity. Needed for secure boot feature
                     0, // writeProt: If 1, the section is write protected
                     0, // rollbackProt: If 1, the section is rollback protected. Needed for secure boot feature
                     1, // plainAccessWriteEnable: If 1, the section has plain write access
                     1, // plainAccessReadEnable: If 1, the section has plain read access
                     0, // authPlainAccess: If 1, using plain access requires to use @ref QLIB_OpenSession
                     0, // digestIntegrityOnAccess:  If 1, the section is protected by digest integrity
                     0, // slog: If 1, the section is defined as secure log
                 },
             0, // crc place-holder. it is computed in post-build script according to the firmware code written to the section
             0, // digest is pre-calculated according to expected section content
             },
             // Section 1: Plain Access section with write protection
             {
                 _8MB_, //baseAddr
                 _8MB_, //size
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
                 0, // digest is pre-calculated according to expected section content (erased section)
             },
             // Section 2: calculate CDI section
             {
                 0, //baseAddr
                 0, //size
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
                 0,                  // crc
                 0, // pre-calculated digest for empty section
             },
             // section 3: security storage section
             {
                 0, //baseAddr
                 0, //size
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
             // section 4 : Secure data section
             {0},
             {0},
             {0},
             {0},
         },
#elif QCONF_TARGET == w77q64jw_revA
         // sectionTable:
        {
            {
                // Section 0 is FW section - holds the boot code
                0, //baseAddr
                _2MB_, //size
                // Section policy structure QLIB_POLICY_T
                {
                0, // digestIntegrity: If 1, the section configuration is protected by digest integrity
                 0, // checksumIntegrity: If 1, the section is protected by CRC integrity. Needed for secure boot feature
                    0, // writeProt: If 1, the section is write protected
                    0, // rollbackProt: If 1, the section is rollback protected. Needed for secure boot feature
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
                _2MB_, //baseAddr
                _2MB_, //size
                //policy
                {
                    0, //digestIntegrity
                    0, //checksumIntegrity
                    0, //writeProt
                    0, //rollbackProt
                    1, //plainAccessWriteEnable
                    1, //plainAccessReadEnable
                    0, //authPlainAccess
#if Q2_POLICY_AUTH_PROT_AC_BIT(NULL)
                    1, // digestIntegrityOnAccess
#else
                0,
#endif
                    0, // slog
                },
                0,                   // crc
                0, // digest is pre-calculated according to expected section content (erased section)
            },
            // Section 2: calculate CDI section
            {
                _4MB_, //baseAddr
                _2MB_, //size
                //policy
                {
                    0, //digestIntegrity
                    0, //checksumIntegrity
                    0, //writeProt
                    0, //rollbackProt
                    1, //plainAccessWriteEnable
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
            // section 3: security storage section
            {
                (_2MB_ + _4MB_), //baseAddr
                _2MB_, //size
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
            // section 4 : Secure data section
            {0},
            {0},
            {0},
            {0},
        },
#else
#error "No config"
#endif //QCONF_TARGET 64/128
        //watchdogDefault
        {
            FALSE,                //enable
            TRUE,                 //lfOscEn
            FALSE,                //swResetEn
            FALSE,                //resetInDedicatedEn
            0,                    // sectionID
            QLIB_AWDT_TH_12_DAYS, //threshold
            FALSE,                //lock
            0,                    //oscRateHz - Set to 0 if non-applicable
            FALSE,                //fallbackEn (in flash that supports Q2_SUPPORT_AWDTCFG_FALLBACK)
        },
        //deviceConf
        {
            {{0}, {0}}, // resetResp
            FALSE, // safeFB: if TRUE, When section 0 integrity fails, W77Q/T jumps to section 7. Needed for secure boot feature
            FALSE, // Speculative Cypher Key Generation is disabled
            TRUE, // nonSecureFormatEn: if TRUE, non-secure FORMAT command is accepted else,
            // must use SFORMAT with device master key to formats the flash
            {
                QLIB_IO23_MODE__QUAD, // IO2/IO3 pin muxing
                TRUE,                 // dedicated RESET_IN pin enable
            },
            {
#if QCONF_TARGET == w77q128jw_revA
    QLIB_STD_ADDR_LEN__26_BIT,
#elif QCONF_TARGET == w77q64jw_revA
    QLIB_STD_ADDR_LEN__24_BIT,
#else
    QLIB_STD_ADDR_LEN__27_BIT,
#endif

#ifdef SPI_INIT_ADDRESS_MODE_4_BYTES
                QLIB_STD_ADDR_MODE__4_BYTE,
#else
            QLIB_STD_ADDR_MODE__3_BYTE,
#endif //SPI_INIT_ADDRESS_MODE_4_BYTES
            },
            FALSE, // RNG Plain access enabled (in case flash supports W77Q_RNG_FEATURE)
// #if Q2_DEVCFG_CTAG_MODE(NULL)
            TRUE, // OP1 instruction is sent with all available SPI IO pins (in case flash supports Q2_DEVCFG_CTAG_MODE)
// #else
//         FALSE,
// #endif
            0,                               // lock
            0,                               // bootFailReset
#if QCONF_TARGET == w77q128jw_revA
             {{1, 0, 0, 0, 0, 0, 0, 0}},      // resetPA
#elif QCONF_TARGET == w77q64jw_revA
             {{1, 1, 1, 0, 0, 0, 0, 0}},      // resetPA
#else
#error "No config"
#endif // QLIB_TARGET
            {(QLIB_VAULT_RPMC_CONFIG_T)(0)}, // vaultSize
            0,                               // fastReadDummyCycles
        }};
#else

// QLIB_STATUS_T QLIB_SAMPLE_QconfFetch(QCONF_T* qlibTable){
//     qlibTable = &qconf_g;
// }

//This is a dummy function for suppressing the error: ISO C forbids an empty translation unit.
void foo_qlib_sample_qconf_config_q2(void)
{
}
#endif //Q2_API

