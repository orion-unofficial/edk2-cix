/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_common.h
* @brief      This file contains QLIB common types and definitions (exported to the user)
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_COMMON_H__
#define __QLIB_COMMON_H__


#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                             ERROR CHECKING                                              */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#if !defined(__QLIB_H__) && !defined(SWIG)
#error "This internal header file should not be included directly. Please include `qlib.h' instead"
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                               DEFINITIONS                                               */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
//#define REMOVE_DEPRECATED_CONFIGURATION_API
#ifndef REMOVE_DEPRECATED_CONFIGURATION_API
#define DEPRECATED_CONFIGURATION_API
#endif

#if defined(Q2_API) && !defined(DEPRECATED_CONFIGURATION_API)
#error "Q2_API requires DEPRECATED_CONFIGURATION_API to be defined"
#endif

/************************************************************************************************************
 * XIP support
************************************************************************************************************/
// clang-format off
#if defined(QLIB_SUPPORT_XIP) && !defined(_MSC_VER)
    #if !defined(__RAM_SECTION)
        #if !defined(QLIB_RAM_SECTION)
            #error "QLIB_RAM_SECTION must be defined in order to support XIP"
        #endif
        #define __RAM_SECTION          __attribute__((noinline))  __attribute__ ((section(QLIB_RAM_SECTION)))
    #endif
#else
    #define __RAM_SECTION
#endif
// clang-format on

#if defined QLIB_SUPPORT_XIP && defined QLIB_NO_DIRECT_FLASH_ACCESS
#error "QLIB must support direct flash access in order to support XIP"
#endif

#define QLIB_INIT_DIE_ID     0u
#define QLIB_WATCHDOG_DIE_ID 0u

#define QLIB_ACTIVE_DIE_STATE(qlibCtx) ((qlibCtx)->dieState[(qlibCtx)->activeDie])

#define QLIB_CALC_SECTION_SIZE(qlibCtx, section)                                                                               \
    ((QLIB_SECTION_ID_VAULT == (section))                                                                                      \
         ? QLIB_VAULT_GET_SIZE((qlibCtx))                                                                                      \
         : ((W77Q_SEC_SIZE_SCALE((qlibCtx)) != 0u)                                                                             \
                ? QLIB_REG_SMRn__LEN_IN_TAG_TO_BYTES(((qlibCtx)),                                                              \
                                                     QLIB_ACTIVE_DIE_STATE((qlibCtx)).sectionsState[(section)].sizeTag,        \
                                                     QLIB_ACTIVE_DIE_STATE((qlibCtx)).sectionsState[(section)].scale)          \
                : (QLIB_ACTIVE_DIE_STATE((qlibCtx)).sectionsState[(section)].enabled == 0u                                     \
                       ? 0u                                                                                                    \
                       : QLIB_REG_SMRn__LEN_IN_TAG_TO_BYTES((qlibCtx),                                                         \
                                                            QLIB_ACTIVE_DIE_STATE((qlibCtx)).sectionsState[(section)].sizeTag, \
                                                            0u))))
#define QLIB_PRNG_RESEED_COUNT 128

/************************************************************************************************************
* Section ID in case of fallback
************************************************************************************************************/
#define W77Q_BOOT_SECTION          0
#define W77Q_BOOT_SECTION_FALLBACK 7

#define QLIB_FALLBACK_SECTION(qlibContext, sectionID)                                                                \
    (((U32)(READ_VAR_FIELD(QLIB_ACTIVE_DIE_STATE(qlibContext).ssr.asUint, QLIB_REG_SSR__FB_REMAP)) == 1u)            \
         ? ((U32)(sectionID) == (U32)W77Q_BOOT_SECTION                                                               \
                ? (U32)W77Q_BOOT_SECTION_FALLBACK                                                                    \
                : ((U32)(sectionID) == (U32)W77Q_BOOT_SECTION_FALLBACK ? (U32)W77Q_BOOT_SECTION : (U32)(sectionID))) \
         : (sectionID))

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                               PURE TYPES                                                */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * Flash power state selector
************************************************************************************************************/
typedef enum
{
    QLIB_POWER_FIRST = 0x1145,

    QLIB_POWER_UP,
    QLIB_POWER_DOWN,

    QLIB_POWER_LAST
} QLIB_POWER_T;

/************************************************************************************************************
 * Flash erase type selector
************************************************************************************************************/
typedef enum
{
    QLIB_ERASE_FIRST = 0x1568,

    QLIB_ERASE_SECTOR_4K,
    QLIB_ERASE_BLOCK_32K,
    QLIB_ERASE_BLOCK_64K,

    QLIB_ERASE_SECTION,
    QLIB_ERASE_CHIP,

    QLIB_ERASE_LAST
} QLIB_ERASE_T;

/************************************************************************************************************
 * This enumeration defines the Authenticated watchdog threshold
************************************************************************************************************/
typedef enum QLIB_AWDT_TH_T
{
    QLIB_AWDT_TH_1_SEC      = 0,
    QLIB_AWDT_TH_2_SEC      = 1,
    QLIB_AWDT_TH_4_SEC      = 2,
    QLIB_AWDT_TH_8_SEC      = 3,
    QLIB_AWDT_TH_16_SEC     = 4,
    QLIB_AWDT_TH_32_SEC     = 5,
    QLIB_AWDT_TH_1_MINUTES  = 6,
    QLIB_AWDT_TH_2_MINUTES  = 7,
    QLIB_AWDT_TH_4_MINUTES  = 8,
    QLIB_AWDT_TH_8_MINUTES  = 9,
    QLIB_AWDT_TH_17_MINUTES = 10,
    QLIB_AWDT_TH_34_MINUTES = 11,
    QLIB_AWDT_TH_1_HOURS    = 12,
    QLIB_AWDT_TH_2_HOURS    = 13,
    QLIB_AWDT_TH_4_HOURS    = 14,
    QLIB_AWDT_TH_9_HOURS    = 15,
    QLIB_AWDT_TH_18_HOURS   = 16,
    QLIB_AWDT_TH_36_HOURS   = 17,
    QLIB_AWDT_TH_72_HOURS   = 18,
    QLIB_AWDT_TH_6_DAYS     = 19,
    QLIB_AWDT_TH_12_DAYS    = 20
} QLIB_AWDT_TH_T;

/************************************************************************************************************
 * This enumeration defines the session integrity check type
************************************************************************************************************/
typedef enum QLIB_INTEGRITY_T
{
    QLIB_INTEGRITY_CRC,   ///< CRC type integrity
    QLIB_INTEGRITY_DIGEST ///< digest type integrity
} QLIB_INTEGRITY_T;

/************************************************************************************************************
 * This enumeration defines the session access type to open
************************************************************************************************************/
typedef enum QLIB_SESSION_ACCESS_T
{
    QLIB_SESSION_ACCESS_RESTRICTED,  ///< Open session using restricted key
    QLIB_SESSION_ACCESS_CONFIG_ONLY, ///< Open session just for configuration - ignoring the validity of SCRi
    QLIB_SESSION_ACCESS_FULL         ///< Open session using full key
} QLIB_SESSION_ACCESS_T;

/************************************************************************************************************
 * Key type
************************************************************************************************************/
typedef enum
{
    QLIB_KID__RESTRICTED_ACCESS_SECTION = 0x00,
    QLIB_KID__FULL_ACCESS_SECTION       = 0x10,
    QLIB_KID__SECTION_PROVISIONING      = 0x20,
    QLIB_KID__SECTION_LMS               = 0x30,
    QLIB_KID__DEVICE_SECRET = 0x8F,
    QLIB_KID__DEVICE_MASTER = 0x9F,
    QLIB_KID__DEVICE_KEY_PROVISIONING = 0xAF,
    QLIB_KID__INVALID                 = 0xFF
} QLIB_KID_TYPE_T;

/************************************************************************************************************
 * Standard commands address mode
************************************************************************************************************/
typedef enum
{
    QLIB_STD_ADDR_MODE__3_BYTE, ///< 3 bytes address mode
    QLIB_STD_ADDR_MODE__4_BYTE  ///< 4 bytes address mode
} QLIB_STD_ADDR_MODE_T;
/*---------------------------------------------------------------------------------------------------------*/
/************************************************************************************************************
*************************************************************************************************************
 *                                             BASIC INCLUDES
*************************************************************************************************************
************************************************************************************************************/
#define __QLIB_PLATFORM_INCLUDED__
#include "qlib_platform.h"
#include <stdint.h>
#include <stdbool.h>
#include "qlib_defs.h"
#include "qlib_errors.h"
#include "qlib_debug.h"
#include "qlib_targets.h"
#include "qlib_cfg.h"
#include "qlib_sec_regs.h"
#include "qlib_sec_cmds.h"
#include "qlib_std_cmds.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                             DEPENDENT TYPES                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * SPI bus format including dtr
************************************************************************************************************/
typedef U32 QLIB_BUS_FORMAT_T;

#define QLIB_BUS_FORMAT(mode, dtr) MAKE_32_BIT((mode), ((dtr) == TRUE ? 1 : 0), 0, 0)

#define QLIB_BUS_FORMAT_GET_MODE(busFormat) ((QLIB_BUS_MODE_T)BYTE((busFormat), 0))

#define QLIB_BUS_FORMAT_GET_DTR(busFormat) (INT_TO_BOOLEAN(BYTE((busFormat), 1)))

/************************************************************************************************************
 * Section configuration actions
************************************************************************************************************/
typedef enum
{
    QLIB_SECTION_CONF_ACTION__NO,     ///< New configuration is not loaded after command execution
    QLIB_SECTION_CONF_ACTION__RELOAD, ///< Automatically load new configuration after command execution
    QLIB_SECTION_CONF_ACTION__RESET   ///< Trigger host and flash hardware reset after command execution
} QLIB_SECTION_CONF_ACTION_T;

/************************************************************************************************************
 * This type contains the value of W77Q/T WID
************************************************************************************************************/
typedef _64BIT QLIB_WID_T;

/************************************************************************************************************
 * Flash ID - Standard part
************************************************************************************************************/
typedef struct
{
#ifdef Q2_API
    U64 uniqueID;
#else
    _128BIT uniqueID;   ///< unique ID
#endif
} QLIB_STD_ID_T;

/************************************************************************************************************
 * Flash ID - secure part
************************************************************************************************************/
typedef struct
{
    _128BIT    suid; ///< secure user ID
    QLIB_WID_T wid;  ///< Winbond ID
} QLIB_SEC_ID_T;

/************************************************************************************************************
 * Flash monotonic counter
************************************************************************************************************/
typedef U32 QLIB_MC_T[2];

/************************************************************************************************************
 * Flash monotonic counter offsets
************************************************************************************************************/
typedef enum
{
    TC  = 0, ///< Offset to TC part in @ref QLIB_MC_T
    DMC = 1  ///< Offset to DMC part in @ref QLIB_MC_T
} QLIB_MC_OFFSET_T;

/************************************************************************************************************
 * Flash ID
************************************************************************************************************/
typedef struct
{
    QLIB_STD_ID_T std; ///< standard ID
    QLIB_SEC_ID_T sec; ///< secure ID
} QLIB_ID_T;

/************************************************************************************************************
 * Hardware Version - Standard part
************************************************************************************************************/
typedef struct
{
    U8 manufacturerID; ///< Manufacture ID
    U8 memoryType;     ///< Memory type
    U8 capacity;       ///< Capacity
    U8 deviceID;       ///< Device ID
} QLIB_STD_HW_VER_T;

/************************************************************************************************************
 * Hardware Version - Secure part
************************************************************************************************************/
typedef struct
{
    U8 flashVersion;    ///< Flash version
    U8 securityVersion; ///< Security Protocol Version
    U8 revision;        ///< Hardware revision
    U8 flashSize;       ///< Flash size
    U8 hashVersion;     ///< Hash function version
} QLIB_SEC_HW_VER_T;

/************************************************************************************************************
 * Target Size
************************************************************************************************************/
typedef enum
{
    QLIB_TARGET_SIZE_UNKNOWN = 0,
    QLIB_TARGET_SIZE_32Mb    = 0x15,
    QLIB_TARGET_SIZE_64Mb,
    QLIB_TARGET_SIZE_128Mb,
    QLIB_TARGET_SIZE_256Mb,
    QLIB_TARGET_SIZE_512Mb,
    QLIB_TARGET_SIZE_1Gb
} QLIB_TARGET_SIZE_T;

#define DEVICE_ID_TO_TARGET_SIZE(devId) ((QLIB_TARGET_SIZE_T)(devId))
/************************************************************************************************************
 * Target Revision
************************************************************************************************************/
typedef enum
{
    QLIB_TARGET_REVISION_UNKNOWN,
    QLIB_TARGET_REVISION_A,
    QLIB_TARGET_REVISION_B,
    QLIB_TARGET_REVISION_C
} QLIB_TARGET_REVISION_T;

/************************************************************************************************************
 * Target Supply Voltage
************************************************************************************************************/
typedef enum
{
    QLIB_TARGET_VOLTAGE_UNKNOWN,
    QLIB_TARGET_VOLTAGE_1_8V,
    QLIB_TARGET_VOLTAGE_3_3V
} QLIB_TARGET_VOLTAGE_T;

/************************************************************************************************************
 * Hardware Version - General Information
************************************************************************************************************/
typedef struct
{
    QLIB_TARGET_SIZE_T     flashSize;   ///< Flash size
    BOOL                   isSingleDie; ///< Single/Multi die
    QLIB_TARGET_VOLTAGE_T  voltage;     ///< Flash voltage
    QLIB_TARGET_REVISION_T revision;    ///< Target revision
    U32                    target;      ///< QLIB target as defined in @ref qlib_targets.h
} QLIB_HW_INFO_T;

/************************************************************************************************************
 * Hardware Version
************************************************************************************************************/
typedef struct QLIB_HW_VER_T
{
    QLIB_STD_HW_VER_T std;  ///< standard HW information
    QLIB_SEC_HW_VER_T sec;  ///< secure HW version
    QLIB_HW_INFO_T    info; ///< HW information
} QLIB_HW_VER_T;

/*---------------------------------------------------------------------------------------------------------*/
/* Keys                                                                                                    */
/*---------------------------------------------------------------------------------------------------------*/
typedef _128BIT        KEY_T;
typedef KEY_T          KEY_ARRAY_T[QLIB_NUM_OF_SECTIONS];
typedef const U32*     CONST_KEY_P_T;
typedef CONST_KEY_P_T  KEY_P_ARRAY_T[QLIB_NUM_OF_SECTIONS];
typedef _320BIT        QLIB_LMS_KEY_T;
typedef QLIB_LMS_KEY_T QLIB_LMS_KEY_ARRAY_T[QLIB_NUM_OF_SECTIONS];

/************************************************************************************************************
 * Key ID
************************************************************************************************************/
typedef U8 QLIB_KID_T;


/************************************************************************************************************
 * Secure read data structure
************************************************************************************************************/
typedef union QLIB_SEC_READ_DATA_T
{
    U32 asArray[(sizeof(U32) + sizeof(_256BIT) + sizeof(_64BIT)) / sizeof(U32)];
    struct
    {
        U32     tc;
        _256BIT data;
        _64BIT  sig;
    } asStruct;
} QLIB_SEC_READ_DATA_T;

/************************************************************************************************************
 * HASH buffer data structure
************************************************************************************************************/
typedef U32 QLIB_HASH_BUF_T[15]; // Add extra U32 for signature of SARD command

#define QLIB_HASH_BUF_OFFSET__KEY       0
#define QLIB_HASH_BUF_OFFSET__CTAG      4
#define QLIB_HASH_BUF_OFFSET__DATA      5
#define QLIB_HASH_BUF_OFFSET__CTRL      13
#define QLIB_HASH_BUF_OFFSET__READ_PAGE (QLIB_HASH_BUF_OFFSET__DATA - 1)
#define QLIB_HASH_BUF_OFFSET__READ_TC   QLIB_HASH_BUF_OFFSET__READ_PAGE
#define QLIB_HASH_BUF_OFFSET__READ_SIG  (QLIB_HASH_BUF_OFFSET__DATA + 8)

#define QLIB_HASH_BUF_GET__KEY(buf)       (&((buf)[QLIB_HASH_BUF_OFFSET__KEY]))
#define QLIB_HASH_BUF_GET__CTAG(buf)      ((buf)[QLIB_HASH_BUF_OFFSET__CTAG])
#define QLIB_HASH_BUF_GET__DATA(buf)      (&((buf)[QLIB_HASH_BUF_OFFSET__DATA]))
#define QLIB_HASH_BUF_GET__CTRL(buf)      ((buf)[QLIB_HASH_BUF_OFFSET__CTRL])
#define QLIB_HASH_BUF_GET__READ_PAGE(buf) (&((buf)[QLIB_HASH_BUF_OFFSET__READ_PAGE]))
#define QLIB_HASH_BUF_GET__READ_TC(buf)   ((buf)[QLIB_HASH_BUF_OFFSET__READ_TC])
#define QLIB_HASH_BUF_GET__READ_SIG(buf)  (&((buf)[QLIB_HASH_BUF_OFFSET__READ_SIG]))

/************************************************************************************************************
 * crypto context of a command
************************************************************************************************************/
typedef struct QLIB_CRYPTO_CONTEXT_T
{
    QLIB_HASH_BUF_T hashBuf;
    _256BIT         cipherKey;
} QLIB_CRYPTO_CONTEXT_T;

/************************************************************************************************************
 * PRNG state
************************************************************************************************************/
typedef struct QLIB_PRNG_STATE_T
{
    U64 state;
    U8  count;
} QLIB_PRNG_STATE_T;

/************************************************************************************************************
 * key manager state
************************************************************************************************************/
typedef struct QLIB_KEY_MNGR_T
{
    KEY_P_ARRAY_T restrictedKeys; ///< Array of restricted keys
    KEY_P_ARRAY_T fullAccessKeys; ///< Array of full access keys
    KEY_T                 sessionKey; ///< Session key
    U8                    kid;        ///< Session KID (key ID)
    U8                    cmdContexIndex;
    QLIB_CRYPTO_CONTEXT_T cmdContexArr[2];
} QLIB_KEY_MNGR_T;

#define QLIB_KEY_MNGR__CMD_CONTEXT_GET(qlibContext) \
    (QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.cmdContexArr[QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.cmdContexIndex])
#define QLIB_KEY_MNGR__CMD_CONTEXT_ADVANCE(qlibContext) (QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.cmdContexIndex ^= 0x1u)

/************************************************************************************************************
 * This type contains section policy configuration
************************************************************************************************************/
typedef struct QLIB_POLICY_T
{
    U32 digestIntegrity : 1;         ///< perform digest integrity check when Section Configuration Register is set
    U32 checksumIntegrity : 1;       ///< perform checksum integrity check
    U32 writeProt : 1;               ///< write protected section
    U32 rollbackProt : 1;            ///< rollback protected section
    U32 plainAccessWriteEnable : 1;  ///< allow plain write access
    U32 plainAccessReadEnable : 1;   ///< allow plain read access
    U32 authPlainAccess : 1;         ///< plain access require @ref QLIB_OpenSession
    U32 digestIntegrityOnAccess : 1; ///< perform digest integrity check when section is accessed
    U32 slog : 1;                    ///< use the section as secure log
} QLIB_POLICY_T;

/************************************************************************************************************
 * This type contains a section configuration
************************************************************************************************************/
typedef struct QLIB_SECTION_CONFIG_T
{
    U32           baseAddr; ///< flash physical address
    U32           size;     ///< section size. When the size equals to 0 the section is disabled.
    QLIB_POLICY_T policy;   ///< section policy
    U32           crc;      ///< section crc
    U64           digest;   ///< section digest
} QLIB_SECTION_CONFIG_T;

/************************************************************************************************************
 * Section configuration table
************************************************************************************************************/
typedef QLIB_SECTION_CONFIG_T QLIB_SECTION_CONFIG_TABLE_T[QLIB_NUM_OF_SECTIONS];

/************************************************************************************************************
 * This type contains Authenticated watchdog configuration
************************************************************************************************************/
typedef struct QLIB_WATCHDOG_CONF_T
{
    BOOL           enable;        ///< Watchdog enable
    BOOL           lfOscEn;       ///< Low frequency oscillator enable
    BOOL           swResetEn;     ///< Software reset enable
    BOOL           authenticated; ///< Requires authentication (signature) for its configurations and reset
    U32            sectionID;     ///< Section ID of the key associated with the AWDT function
    QLIB_AWDT_TH_T threshold;     ///< Watchdog expires when the timer reaches this value
    BOOL           lock;          ///< AWDTCFG register is locked
    U32            oscRateHz;     ///< Frequency of the internal oscillator, used for calibration. Set to 0 if non-applicable
    BOOL fallbackEn; ///< AWDT Fallback function is triggered after the watchdog expires (only AWDTCFG Fallback is supported))
} QLIB_WATCHDOG_CONF_T;

/************************************************************************************************************
 * This type contains IO2 muxing options
************************************************************************************************************/
typedef enum QLIB_IO23_MODE_T
{
    QLIB_IO23_MODE__LEGACY_WP_HOLD,
    QLIB_IO23_MODE__RESET_IN_OUT,
    QLIB_IO23_MODE__QUAD,
    QLIB_IO23_MODE__NONE ///< QLIB does not configure IO23 pins.
} QLIB_IO23_MODE_T;

/************************************************************************************************************
 * This type contains W77Q pin muxing options
************************************************************************************************************/
typedef struct QLIB_PIN_MUX_T
{
    QLIB_IO23_MODE_T io23Mux;            // Set IO2 and IO3 mux type (LEGACY/RESET/QUAD)
    BOOL             dedicatedResetInEn; // Set TRUE to enable dedicated RSTIN#
} QLIB_PIN_MUX_T;

/************************************************************************************************************
* This type contains reset response buffers
************************************************************************************************************/
typedef struct QLIB_RESET_RESPONSE_T
{
    U32 response1[_64B_ / sizeof(U32)];
    U32 response2[_64B_ / sizeof(U32)];
} QLIB_RESET_RESPONSE_T;

/************************************************************************************************************
* Standard logical address length
************************************************************************************************************/
typedef enum QLIB_STD_ADDR_LEN_T
{
    QLIB_STD_ADDR_LEN__22_BIT = 19, ///< address is 2 bits die, 3 bits of SID and offset of 19 bits
    QLIB_STD_ADDR_LEN__23_BIT = 20, ///< address is 2 bits die, 3 bits of SID and offset of 20 bits
    QLIB_STD_ADDR_LEN__24_BIT = 21, ///< address is 2 bits die, 3 bits of SID and offset of 21 bits
    QLIB_STD_ADDR_LEN__25_BIT = 22, ///< address is 2 bits die, 3 bits of SID and offset of 22 bits
    QLIB_STD_ADDR_LEN__26_BIT = 23, ///< address is 2 bits die, 3 bits of SID and offset of 23 bits
    QLIB_STD_ADDR_LEN__27_BIT = 24, ///< address is 2 bits die, 3 bits of SID and offset of 24 bits
    QLIB_STD_ADDR_LEN__LAST
} QLIB_STD_ADDR_LEN_T;
#define QLIB_STD_ADDR_LEN_TO_ADDRESS_OFFSET_SIZE(len) ((U32)(len))

/************************************************************************************************************
* This type contains the standard commands address size
************************************************************************************************************/
typedef struct QLIB_STD_ADDR_SIZE_T
{
    QLIB_STD_ADDR_LEN_T  addrLen;
    QLIB_STD_ADDR_MODE_T addrMode; ///< Power up address mode set in the non-volatile Status Register bit ADP
} QLIB_STD_ADDR_SIZE_T;

/************************************************************************************************************
 * This type determines the memory size allocated to The Vault and to the Replay Protection Monotonic Counter
************************************************************************************************************/
typedef enum
{
    QLIB_VAULT_DISABLED_RPMC_8_COUNTERS = 0, ///< Vault is disabled. RPMC (if supported) implements 8 counters.
    QLIB_VAULT_64KB_RPMC_4_COUNTERS     = 2, ///< Vault uses 64kB. RPMC (if supported) implements 4 counters.
    QLIB_VAULT_128KB_RPMC_DISABLED      = 3  ///< Vault uses 128kB. RPMC is disabled.
} QLIB_VAULT_RPMC_CONFIG_T;

typedef struct QLIB_SECTION_CONF_POST_ACTIONS_T
{
    U32 swap : 1;
    U32 reload : 1;
    U32 reset : 1;
} QLIB_SECTION_CONF_POST_ACTIONS_T;

/************************************************************************************************************
 * This type contains general device configuration
************************************************************************************************************/
typedef struct QLIB_DEVICE_CONF_T
{
    /********************************************************************************************************
     * Reset response (1 & 2) value to be sent following the Flash power on reset.
     * If value is all zeros, reset response is disabled
     *******************************************************************************************************/
    QLIB_RESET_RESPONSE_T resetResp;
    BOOL                  safeFB;            ///< Safe fall back Enabled
    BOOL                  speculCK;          ///< Speculative Cypher Key Generation
    BOOL                  nonSecureFormatEn; ///< Non-secure FORMAT enable
    QLIB_PIN_MUX_T        pinMux;            ///< Pin muxing configuration
    QLIB_STD_ADDR_SIZE_T  stdAddrSize;       ///< Address size configuration
    BOOL                  rngPAEn;           ///< RNG Plain Access Enabled (for devices supporting W77Q_RNG_FEATURE)
    BOOL ctagModeMulti; ///< OP1 instruction is sent with all available SPI IO pins. Relevant only if Q2_SUPPORT_DEVCFG_CTAG_MODE
    BOOL lock;          ///< DEVCFG register is locked
    BOOL bootFailReset; ///< Host is kept in reset if boot sequence fails. (for devices that support W77Q_BOOT_FAIL_RESET)
    BOOL resetPA[QLIB_NUM_OF_DIES][QLIB_NUM_OF_MAIN_SECTIONS]; ///< Plain Access enabled for section after reset
    QLIB_VAULT_RPMC_CONFIG_T vaultSize[QLIB_NUM_OF_DIES];      ///< Vault and RPMC size
    U8 fastReadDummyCycles; ///< Number of dummy cycles for all Fast-Read instructions (only W77Q/T 256Mb to 1Gb flash)
#ifndef Q2_API
    BOOL dqsDisable; ///< Disable the DQS pin
#endif
} QLIB_DEVICE_CONF_T;

typedef struct QLIB_FLASH_CONFIG_T
{
    QLIB_WATCHDOG_CONF_T watchdogDefault; ///< Watchdog and rest configurations. Relevant only for die 0
    QLIB_PIN_MUX_T       pinMux;          ///< Pin muxing configuration. Relevant only for die 0
    QLIB_STD_ADDR_SIZE_T stdAddrSize;     ///< Address size configuration. Relevant only for die 0
    U8                   fastReadDummyCycles; ///< Number of dummy cycles for all Fast-Read instructions (only W77Q/T 256Mb to 1Gb flash). Relevant only for die 0
    BOOL                 dqsDisable; ///< Disable the DQS pin. Relevant only for die 0
} QLIB_FLASH_CONFIG_T;

typedef struct QLIB_DIE_CONFIG_T
{
    KEY_T   deviceMasterKey; ///< Device Master Key value.
    KEY_T   deviceSecretKey; ///< Device Secret Key value. The device secret key is used to configure the Root Of Trust feature.
    KEY_T   preProvisionedMasterKey; ///< Pre-provisioned master key value.
    _128BIT suid;                    ///< Secure User ID
    QLIB_RESET_RESPONSE_T resetResp; ///< Code sequence that keeps the host in reset until the flash completes its reset sequence.
    BOOL                  safeFB;    ///< Safe fall back Enabled
    BOOL                  speculCK;  ///< Speculative Cypher Key Generation
    BOOL                  nonSecureFormatEn; ///< Non-secure FORMAT enable
    BOOL                  rngPAEn;           ///< RNG Plain Access Enabled (for devices supporting W77Q_RNG_FEATURE)
    BOOL ctagModeMulti; ///< OP1 instruction is sent with all available SPI IO pins. Relevant only if Q2_SUPPORT_DEVCFG_CTAG_MODE
    BOOL devcfgLock;    ///< DEVCFG register is locked
    BOOL bootFailReset; ///< Host is kept in reset if boot sequence fails.
} QLIB_DIE_CONFIG_T;

typedef struct QLIB_SECTION_CONF_T
{
    QLIB_POLICY_T                    policy;      ///< Section policy
    U32                              crc;         ///< Section CRC
    U64                              digest;      ///< Section digest
    U32                              version;     ///< Section version
    QLIB_SECTION_CONF_POST_ACTIONS_T postActions; ///< Flags for defining actions after configuration happens
} QLIB_SECTION_CONF_T;

typedef struct QLIB_EXTENDED_SECTION_CONF_T
{
    U32                 baseAddr;      ///< Flash physical address
    U32                 size;          ///< Section size. When the size equals to 0 the section is disabled.
    QLIB_SECTION_CONF_T sectionConfig; ///< Section fields that can be updated with QLIB_ConfigDeviceSection function
    BOOL                resetPA;       ///< Plain Access enabled for section after reset
    KEY_T               restrictedKey; ///< Restricted key for the section
    KEY_T               fullAccessKey; ///< Full access key for the section
    QLIB_LMS_KEY_T      lmsKey;        ///< LMS Root key for the section
} QLIB_EXTENDED_SECTION_CONF_T;

typedef struct QLIB_SECTIONS_CONF_TABLE_T
{
    QLIB_EXTENDED_SECTION_CONF_T sectionConfigTable[QLIB_NUM_OF_SECTIONS];
} QLIB_SECTIONS_CONF_TABLE_T;

/************************************************************************************************************
 * Bus interface
************************************************************************************************************/
PACKED_START
typedef struct QLIB_INTERFACE_T
{
    BOOL            dtr;              ///< Double Transfer Rate enable
    QLIB_BUS_MODE_T busMode;          ///< General bus mode to use, not all the commands support all the bus modes
    QLIB_BUS_MODE_T secureCmdsFormat; ///< Secure commands transfer format
    U8              op0;              ///< Opcode for secure "get SSR"
    U8              op1;              ///< Opcode for secure "write input buffer"
    U8              op2;              ///< Opcode for secure "read output buffer"
    U8              padding;          ///
    BOOL            busIsLocked;      ///< Mutex - Marks the physical link to the flash is in use
} PACKED QLIB_INTERFACE_T;
PACKED_END

/************************************************************************************************************
 * Section plain access state - Defines whether section was granted plain access by GRANT or SESSION_OPEN.
 * The actual plain access privilege also depends on the Section Security Policy register.
************************************************************************************************************/
#define QLIB_SECTION_PLAIN_EN_NO 0u ///< section plain access was not granted
#define QLIB_SECTION_PLAIN_EN_RD 1u ///< section read plain access was granted
#define QLIB_SECTION_PLAIN_EN_WR 2u ///< section write plain access was granted

/************************************************************************************************************
 * Session state
************************************************************************************************************/
PACKED_START
typedef struct QLIB_SECTION_STATE_T
{
    U8 sizeTag : 4;      ///< Max Section size tag representing the section size
    U8 scale : 1;        ///< Scale representing the section block size (in W77Q/T 256Mb to 1Gb only)
    U8 enabled : 1;      ///< Section is enabled
    U8 plainEnabled : 2; ///< Section is plain read or plain write enabled
} PACKED QLIB_SECTION_STATE_T;
PACKED_END

/************************************************************************************************************
 * QLIB Reset Status
************************************************************************************************************/
PACKED_START
typedef struct QLIB_RESET_STATUS_T
{
    U32 powerOnReset : 1;                     // 1==HW reset, 0==SW reset (by software command)
    U32 fallbackRemapping : QLIB_NUM_OF_DIES; // A bit per die, 1==Remapping occurred in die
    U32 watchdogReset : 1;                    // 1==Watchdog reset occurred
    U32 reserved : (30u - QLIB_NUM_OF_DIES);
} PACKED QLIB_RESET_STATUS_T;
PACKED_END


/************************************************************************************************************
 * QLIB DIE state
************************************************************************************************************/
typedef struct QLIB_ASYNC_HASH_STATE_T
{
    void* context;
    U32*  outputData;
} QLIB_ASYNC_HASH_STATE_T;

/************************************************************************************************************
 * This type contains QLIB configuration features according to flash type
************************************************************************************************************/
typedef U8 QLIB_CFG_T[((U32)QLIB_CFG_FEATURE_MAX / BYTES_TO_BITS(sizeof(U8))) + 1u];

/************************************************************************************************************
 * QLIB DIE state
************************************************************************************************************/
typedef struct QLIB_DIE_STATE_T
{
    QLIB_WID_T               wid;                                      ///< Winbond ID
    QLIB_MC_T                mc;                                       ///< Monotonic counter
    U32                      mcInSync : 1;                             ///< Monotonic counter is synced indication
    U32                      isPoweredDown : 1;                        ///< The flash is in powered-down state indication
    U32                      isSuspended : 1;                          ///< The flash is in suspended state indication
    QLIB_KEY_MNGR_T          keyMngr;                                  ///< Key manager
    QLIB_REG_SSR_T           ssr;                                      ///< Last SSR
    QLIB_VAULT_RPMC_CONFIG_T vaultSize;                                ///< Vault and RPMC size
    QLIB_SECTION_STATE_T     sectionsState[QLIB_NUM_OF_MAIN_SECTIONS]; ///< section state and configuration (not including vault)
} QLIB_DIE_STATE_T;

/************************************************************************************************************
 * QLIB context structure\n
 * [QLIB internal state](md_definitions.html#DEF_CONTEXT)
************************************************************************************************************/
typedef struct QLIB_CONTEXT_T
{
    QLIB_INTERFACE_T     busInterface;            ///< Bus interface configuration
    U32                  watchdogIsSecure : 1;    ///< Watchdog is secure indication
    U32                  suspendDie : 2;          ///< The die that was active before last suspend operation
    U32                  multiTransactionCmd : 1; ///< Qlib runs SecureRead/Write commands which can cause many hw transactions
    U32                  ctagMode : 1;            ///< current CTAG mode (relevant in case Q2_SUPPORT_DEVCFG_CTAG_MODE)
    U32                  MaxDieId : 2;            ///< maximal Die Id (0-3, limited by QLIB_NUM_OF_DIES)
    U32                  watchdogSectionId;       ///< Section key used for Secure Watchdog
    U32                  addrSize;                ///< Standard address size
    QLIB_STD_ADDR_MODE_T addrMode;                ///< Standard address mode
    U8                   extendedAddr;            ///<extended address register value
    void*                userData;                ///< Saved user data pointer
    QLIB_PRNG_STATE_T    prng;                    ///< PRNG state
    QLIB_RESET_STATUS_T  resetStatus;             ///< Last Reset status
    U8 activeDie; ///< The currently active die
    U8 detectedDeviceID;
    U8 fastReadDummy; ///< Number of dummy cycles for all Fast-Read instructions in case this is configurable in current device
    QLIB_DIE_STATE_T        dieState[QLIB_NUM_OF_DIES];
    QLIB_ASYNC_HASH_STATE_T hashState;
    QLIB_CFG_T cfgBitArr;
} QLIB_CONTEXT_T;

/************************************************************************************************************
 * Synchronization object
************************************************************************************************************/
PACKED_START
typedef struct QLIB_SYNC_OBJ_T
{
    QLIB_INTERFACE_T         busInterface;
    QLIB_WID_T               wid[QLIB_NUM_OF_DIES];
    QLIB_RESET_STATUS_T      resetStatus;
    U32                      addrSize;
    QLIB_STD_ADDR_MODE_T     addrMode; ///< Standard address mode
    QLIB_SECTION_STATE_T     sectionsState[QLIB_NUM_OF_DIES][QLIB_NUM_OF_MAIN_SECTIONS];
    QLIB_VAULT_RPMC_CONFIG_T vaultSize[QLIB_NUM_OF_DIES];
    QLIB_CFG_T               cfgBitArr;
    U8                       detectedDeviceID;
    U8                       fastReadDummy; ///< Number of dummy cycles for all Fast-Read instructions (QW77Q/T 256Mb to 1Gb3)
} PACKED QLIB_SYNC_OBJ_T;
PACKED_END


/************************************************************************************************************
 * QLIB notifications
************************************************************************************************************/
typedef struct QLIB_NOTIFICATIONS_T
{
    U8 mcMaintenance : 1; ///< Monotonic Counter maintenance is needed
    U8 resetDevice : 1;   ///< Device reset is needed since Transaction Counter is close to its max value
    U8 replaceDevice : 1; ///< Device replacement is needed since DMC is close to its max value
} QLIB_NOTIFICATIONS_T;

/************************************************************************************************************
 * QLIB software version information
************************************************************************************************************/
#ifdef Q2_API
/************************************************************************************************************
 * Target for which QLIB was compiled as defined in old QLIB API
************************************************************************************************************/
typedef struct
{
    QLIB_TARGET_SIZE_T     flashSize;   ///< Flash size
    BOOL                   isSingleDie; ///< Single/Multi die
    QLIB_TARGET_REVISION_T revision;    ///< Target revision
    QLIB_TARGET_VOLTAGE_T  voltage;     ///< Voltage
} QLIB_COMPILED_TARGET_T;
#endif

typedef struct
{
    U32 qlibVersion; ///< SW version
#ifdef Q2_API
    QLIB_COMPILED_TARGET_T qlibTarget; ///< obsolete. defined for old QLIB
#else
    U32     qlibTarget; ///< Target information for which QLIB was compiled, as defined in @ref qlib_targets.h
#endif
} QLIB_SW_VERSION_T;

/************************************************************************************************************
 * Revoke plain access types
************************************************************************************************************/
typedef enum
{
    QLIB_PA_REVOKE_WRITE_ACCESS = 2, ///< revoke plain write access
    QLIB_PA_REVOKE_ALL_ACCESS   = 3, ///< revoke both plain read and plain write access
} QLIB_PA_REVOKE_TYPE_T;

/************************************************************************************************************
 * LMS
************************************************************************************************************/
#if (!defined EXCLUDE_LMS || !defined EXCLUDE_LMS_ATTESTATION)
#define QLIB_LMS_SIGNATURE_SIZE(n, p, h)     \
    (sizeof(U32) + /* otsIdxQ */             \
     sizeof(U32) + /* lmotsParameterSetId */ \
     (n) +         /* randomString */        \
     ((p) * (n)) + /* otsSignature */        \
     sizeof(U32) + /*lmsParameterSetId */    \
     ((h) * (n)))  /* path */
#endif

/************************************************************************************************************
 * LMS Attestation Types
************************************************************************************************************/
#define LMS_ATTEST_PARAM_P 51u // Maximum number of chains (including checksum)
#define LMS_ATTEST_PARAM_N 24u // Hash output size in bytes

typedef U8                 LMS_ATTEST_PRIVATE_SEED_T[LMS_ATTEST_PARAM_N]; // Private key seed
typedef U8                 LMS_ATTEST_KEY_ID_T[16];                       // key ID
typedef U8                 LMS_ATTEST_CHUNK_T[LMS_ATTEST_PARAM_N];        // output of the HASH function
typedef LMS_ATTEST_CHUNK_T LMS_ATTEST_OTS_SIG_T[LMS_ATTEST_PARAM_P]; // The OTS signature holds the single signature of OTS leaf
typedef U8                 LMS_ATTEST_NONCE_T[LMS_ATTEST_PARAM_N];   // Nonce value used for the signature

#if !defined EXCLUDE_LMS_ATTESTATION && !defined Q2_API
typedef struct
{
    LMS_ATTEST_KEY_ID_T  keyId;
    U32                  leafNum;
    LMS_ATTEST_OTS_SIG_T otsSig;
    LMS_ATTEST_CHUNK_T   path[QLIB_LMS_ATTEST_TREE_HEIGHT];
} QLIB_OTS_SIG_T;

typedef U8
    QLIB_LMS_ATTEST_SIG_BUFFER_T[QLIB_LMS_SIGNATURE_SIZE(LMS_ATTEST_PARAM_N, LMS_ATTEST_PARAM_P, QLIB_LMS_ATTEST_TREE_HEIGHT)];

#endif

/************************************************************************************************************
 * LMS Types
************************************************************************************************************/
#if !defined EXCLUDE_LMS

#define QLIB_LMS_PARAM_P     51u // Maximum number of chains (including checksum)
#define QLIB_LMS_PARAM_N     24u // Hash output size in bytes
#define QLIB_LMS_TREE_HEIGHT 20u

typedef U8 QLIB_LMS_KEY_ID_T[16]; // key ID
typedef U8 QLIB_LMS_MSG_DATA_STRUCT_T[64];

typedef U8 QLIB_LMS_SIG_BUFFER_T[QLIB_LMS_SIGNATURE_SIZE(QLIB_LMS_PARAM_N, QLIB_LMS_PARAM_P, QLIB_LMS_TREE_HEIGHT)];
#endif

#ifndef Q2_API
#ifndef EXCLUDE_W77Q_INTERRUPTS
/************************************************************************************************************
 * Interrupt types
************************************************************************************************************/
typedef struct QLIB_INTERRUPT
{
    U32 securityViolationInt : 1; ///< Security Violation Interrupt
    U32 watchdogTimerInt : 1;     ///< Watchdog Timer Interrupt
    U32 spiIntegrityInt : 1;      ///< SPI Integrity Interrupt
    U32 eccSingleErrInt : 1;      ///< ECC Single Error Correction Interrupt
    U32 eccDoubleErrInt : 1;      ///< ECC Double Error Detection Interrupt
    U32 eccMultipleProgInt : 1;   ///< ECC Multiple-programming Interrupt
} QLIB_INTERRUPT_T;
#endif

#ifndef EXCLUDE_W77Q_ECC
typedef struct
{
    BOOL singleErr;
    BOOL doubleErr;
    BOOL multipleProg;
    BOOL erasedLine;
    BOOL eccEnabled;
    BOOL eccOff;
} QLIB_ECC_STATUS_T;

typedef struct
{
    BOOL sacvf; // TRUE if sra is valid
    U32  sra;   // Physical Address (28 lsb) of the first SEC event from previous memory read operations
    U8   sc;    // SEC Counter
    BOOL dacvf; // TRUE if dra is valid
    U32  dra;   // Physical Address (28 lsb) of the first DED event from previous memory read operations
    U8   dc;    // DED counter
} QLIB_ADVANCED_ECC_T;
#endif
#endif
/************************************************************************************************************
*************************************************************************************************************
 *                                            FUNCTION DECLARATIONS
*************************************************************************************************************
************************************************************************************************************/

/************************************************************************************************************
 * @brief       This function performs hash of given data\n
 *
 * @param[out]  output     digest
 * @param[in]   data       Input data
 * @param[in]   dataSize   Input data size in bytes
 ************************************************************************************************************/
QLIB_STATUS_T QLIB_HASH(U32* output, const void* data, U32 dataSize);

#ifdef QLIB_HASH_OPTIMIZATION_ENABLED
/************************************************************************************************************
 * @brief The function starts asynchronous HASH calculation
 *
 * @param[in,out]  qlibContext      QLIB state object
 * @param[out]     output           Pointer to HASH output
 * @param[in]      data             Input data
 * @param[in]      dataSize         Input data size in bytes
************************************************************************************************************/
QLIB_STATUS_T QLIB_HASH_Async(QLIB_CONTEXT_T* qlibContext, U32* output, const void* data, U32 dataSize);

/************************************************************************************************************
 * @brief The function waits for previous asynchronous HASH calculation to finish.
 *
 * @param[in,out]  qlibContext      QLIB state object
************************************************************************************************************/
QLIB_STATUS_T QLIB_HASH_Async_WaitWhileBusy(QLIB_CONTEXT_T* qlibContext);
#endif // QLIB_HASH_OPTIMIZATION_ENABLED

/************************************************************************************************************
*************************************************************************************************************
 *                                            DEPENDENT INCLUDES
*************************************************************************************************************
************************************************************************************************************/
#include "qlib_std.h"
#include "qlib_sec.h"
#include "qlib_cmd_proc.h"
#include "qlib_tm.h"
#include "qlib_crypto.h"
#include "qlib_version.h"
#include "qlib_key_mngr.h"
#include "qlib.h"

#ifdef __cplusplus
}
#endif

#endif // __QLIB_COMMON_H__
