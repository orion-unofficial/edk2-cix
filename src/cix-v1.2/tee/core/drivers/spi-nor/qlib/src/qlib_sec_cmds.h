/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sec_cmds.h
* @brief      This file contains secure command definitions
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_SEC_CMDS_H__
#define __QLIB_SEC_CMDS_H__

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
/*                                               DEFINITIONS                                               */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------*/
/* Secure Instructions                                                                                     */
/*---------------------------------------------------------------------------------------------------------*/
#define W77Q_SEC_INST__INVALID    0x00u
#define W77Q_SEC_INST__SINGLE     0xA0u
#define W77Q_SEC_INST__DUAL       0xB0u
#define W77Q_SEC_INST__QUAD       0xD0u
#define W77Q_SEC_INST__OCTAL      0xF0u
#define W77Q_SEC_INST__LINES_MASK 0xF0u
#define W77Q_SEC_INST__OP0        0x0u
#define W77Q_SEC_INST__OP1        0x1u
#define W77Q_SEC_INST__OP2        0x2u

#define W77Q_SEC_INST__MAKE(inst, format, dtr_bit)             \
    (((U8)(inst)) | (QLIB_SEC_BusModeToSecInstLines(format)) | \
     ((((U8)(dtr_bit)) & (((format) != QLIB_BUS_MODE_8_8_8) ? 1u : 0u)) << 2))
#define W77Q_SEC_INST__SET_FORMAT(inst, format) \
    ((inst) = (((inst) & ((U8)~W77Q_SEC_INST__LINES_MASK)) | (QLIB_SEC_BusModeToSecInstLines(format))))

#define W77Q_SEC_INST__WR_IBUF_START 0x80
#define W77Q_SEC_INST__WR_IBUF_END   0x91

/*---------------------------------------------------------------------------------------------------------*/
/* Dummy cycles                                                                                            */
/*---------------------------------------------------------------------------------------------------------*/
#define W77Q_SEC_INST_DUMMY_CYCLES_BY_DEV_ID__OP0(devId) \
    ((devId) <= 0x15u ? 32 : 8) // number of dummy cycles for OP0 (no dtr) according to flash type
#define Q2_OP0_DUMMY_CYCLES_DTR \
    16 // Relevant only for W77Q (16Mb to 128Mb) MCD device. Others don't support DTR for secure commands

// we assume that DTR is not used when QLIB is compiled to different flavor then the chip
// or before initialization of feature configuration bitmap
#define W77Q_SEC_INST_DUMMY_CYCLES__OP0(qlibContext, dtr, devId)                                \
    (((Q2_OP0_OP2_DTR_FEATURE(qlibContext) != 0u) && ((dtr) == TRUE)) ? Q2_OP0_DUMMY_CYCLES_DTR \
                                                                      : W77Q_SEC_INST_DUMMY_CYCLES_BY_DEV_ID__OP0(devId))

#define W77Q_SEC_INST_DUMMY_CYCLES__OP2 8

/*---------------------------------------------------------------------------------------------------------*/
/* Sizes                                                                                                   */
/*---------------------------------------------------------------------------------------------------------*/
#define W77Q_CTAG_SIZE_BYTE     (4u)
#define W77Q_MAX_IBUF_SIZE_BYTE (_256B_)

#ifdef QLIB_SUPPORT_QPI
#define Q2_SEC_INST_SUPPORTED_IN_QPI(deviceId) ((deviceId) <= 0x15u ? FALSE : TRUE) // run time version of Q2_BYPASS_HW_ISSUE_23
#define Q2_SEC_HW_VER_INST_SUPPORTED_IN_QPI(deviceId) \
    (((deviceId) == 0x16u) || ((deviceId) == 0x17u) ? FALSE : TRUE) // run time replacement for Q2_BYPASS_HW_ISSUE_262
#endif

#define Q2_SEC_INST_BYPASS_CTAG_MODE(deviceId) ((deviceId) <= 0x15u || (deviceId) >= 0x18u ? FALSE : TRUE)
#define Q2_SEC_INST_SUPPORTED_IN_1_1_4(deviceId) \
    ((deviceId) <= 0x17u ? TRUE : FALSE) // run time version of W77Q_SUPPORT__1_1_4_SPI
/************************************************************************************************************
 * Secure commands
************************************************************************************************************/
typedef enum
{
    QLIB_CMD_SEC_NONE       = 0x00,
    QLIB_CMD_SEC_GET_ESSR   = 0x11,
    QLIB_CMD_SEC_GET_WID    = 0x14,
    QLIB_CMD_SEC_GET_SUID   = 0x15,
    QLIB_CMD_SEC_GET_AWDTSR = 0x18,
    QLIB_CMD_SEC_SFORMAT          = 0x20,
    QLIB_CMD_SEC_FORMAT           = 0x30,
    QLIB_CMD_SEC_SET_KEY          = 0x21,
    QLIB_CMD_SEC_SET_KEY_LMS      = 0x2A,
    QLIB_CMD_SEC_SET_SUID         = 0x22,
    QLIB_CMD_SEC_SET_GMC          = 0x24,
    QLIB_CMD_SEC_GET_GMC          = 0x34,
    QLIB_CMD_SEC_SET_GMT          = 0x25,
    QLIB_CMD_SEC_GET_GMT          = 0x35,
    QLIB_CMD_SEC_SET_AWDT         = 0x26,
    QLIB_CMD_SEC_GET_AWDT         = 0x36,
    QLIB_CMD_SEC_AWDT_TOUCH       = 0x27,
    QLIB_CMD_SEC_SET_AWDT_PLAIN   = 0x2D,
    QLIB_CMD_SEC_AWDT_TOUCH_PLAIN = 0x2E,
    QLIB_CMD_SEC_SET_SCR          = 0x28,
    QLIB_CMD_SEC_SET_SCR_SWAP     = 0x29,
    QLIB_CMD_SEC_GET_KEYS_STATUS  = 0x31,
    QLIB_CMD_SEC_GET_SCR          = 0x38,
    QLIB_CMD_SEC_SET_RST_RESP     = 0x2B,
    QLIB_CMD_SEC_GET_RST_RESP     = 0x3B,
    QLIB_CMD_SEC_SET_ACLR         = 0x2C,
    QLIB_CMD_SEC_GET_ACLR         = 0x3C,
    QLIB_CMD_SEC_GET_MC           = 0x40,
    QLIB_CMD_SEC_MC_MAINT         = 0x41,
    QLIB_CMD_SEC_SESSION_OPEN     = 0x44,
    QLIB_CMD_SEC_SESSION_CLOSE    = 0x45,
    QLIB_CMD_SEC_INIT_SECTION_PA  = 0x47,
    QLIB_CMD_SEC_CALC_CDI         = 0x48,
    QLIB_CMD_SEC_VER_INTG         = 0x49,
    QLIB_CMD_SEC_PA_GRANT         = 0x4C,
    QLIB_CMD_SEC_PA_GRANT_PLAIN   = 0x4D,
    QLIB_CMD_SEC_PA_REVOKE        = 0x4E,
    QLIB_CMD_SEC_GET_TC           = 0x50,
    QLIB_CMD_SEC_CALC_SIG         = 0x52,
    QLIB_CMD_SEC_MEM_CRC          = 0x54,
    QLIB_CMD_SEC_GET_RNGR         = 0x58,
    QLIB_CMD_SEC_GET_RNGR_PLAIN   = 0x59,
    QLIB_CMD_SEC_GET_RNGR_COUNTER = 0x5A,
    QLIB_CMD_SEC_SRD              = 0x60,
    QLIB_CMD_SEC_SARD             = 0x61,
    QLIB_CMD_SEC_LOG_SRD          = 0x62,
    QLIB_CMD_SEC_LOG_PRD          = 0x63,
    QLIB_CMD_SEC_SAWR             = 0x64,
    QLIB_CMD_SEC_MEM_COPY         = 0x65,
    QLIB_CMD_SEC_LOG_SAWR         = 0x66,
    QLIB_CMD_SEC_LOG_PWR          = 0x67,
    QLIB_CMD_SEC_SERASE_4         = 0x68,
    QLIB_CMD_SEC_SERASE_32        = 0x69,
    QLIB_CMD_SEC_SERASE_64        = 0x6A,
    QLIB_CMD_SEC_SERASE_SEC       = 0x6B,
    QLIB_CMD_SEC_SERASE_ALL       = 0x6C,
    QLIB_CMD_SEC_ERASE_SECT_PLAIN = 0x6F,
    QLIB_CMD_SEC_LMS_LOAD         = 0x70,
    QLIB_CMD_SEC_LMS_EXEC         = 0x71,
    QLIB_CMD_SEC_OTS_SET_KEY      = 0x78,
    QLIB_CMD_SEC_OTS_INIT         = 0x79,
    QLIB_CMD_SEC_OTS_SIGN_DIGIT   = 0x7A,
    QLIB_CMD_SEC_OTS_PUB_CHAIN    = 0x7B,
    QLIB_CMD_SEC_OTS_GET_ID       = 0x7C,
    QLIB_CMD_SEC_GET_VERSION      = 0xF0,
    QLIB_CMD_SEC_AWDT_EXPIRE      = 0xFE,
    QLIB_CMD_SEC_SLEEP            = 0xF8
} QLIB_SEC_CMD_T;

/************************************************************************************************************
 * Register data types
************************************************************************************************************/

typedef enum
{
    QLIB_SIGNED_DATA_ID_SECTION_DIGEST = 0x00u,
    QLIB_SIGNED_DATA_ID_WID            = 0x10u,
    QLIB_SIGNED_DATA_ID_SUID           = 0x14u,
    QLIB_SIGNED_DATA_ID_HW_VER         = 0x18u,
    QLIB_SIGNED_DATA_ID_MEMORY_RANGE   = 0X1Cu,
    QLIB_SIGNED_DATA_ID_SSR            = 0x20u,
    QLIB_SIGNED_DATA_ID_ESSR           = 0x21u,
    QLIB_SIGNED_DATA_ID_AWDTCFG        = 0x24u,
    QLIB_SIGNED_DATA_ID_AWDTSR         = 0x25u,
    QLIB_SIGNED_DATA_ID_MC             = 0x28u,
    QLIB_SIGNED_DATA_ID_GMC            = 0x30u,
    QLIB_SIGNED_DATA_ID_GMT            = 0x32u,
    QLIB_SIGNED_DATA_ID_SECTION_CONFIG = 0x40u

} QLIB_SIGNED_DATA_ID_T;

typedef enum
{
    /*-----------------------------------------------------------------------------------------------------*/
    /*                                                      ID     SIZE                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_SIGNED_DATA_TYPE_SECTION_DIGEST = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_SECTION_DIGEST, BITS_TO_BYTES(64u)),
    QLIB_SIGNED_DATA_TYPE_WID            = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_WID, BITS_TO_BYTES(64u)),
    QLIB_SIGNED_DATA_TYPE_SUID           = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_SUID, BITS_TO_BYTES(128u)),
    QLIB_SIGNED_DATA_TYPE_HW_VER         = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_HW_VER, BITS_TO_BYTES(32u)),
    QLIB_SIGNED_DATA_TYPE_MEMORY_RANGE   = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_MEMORY_RANGE, BITS_TO_BYTES(64u)),
    QLIB_SIGNED_DATA_TYPE_SSR            = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_SSR, BITS_TO_BYTES(32u)),
    QLIB_SIGNED_DATA_TYPE_ESSR           = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_ESSR, BITS_TO_BYTES(64u)),
    QLIB_SIGNED_DATA_TYPE_AWDTCFG        = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_AWDTCFG, BITS_TO_BYTES(32u)),
    QLIB_SIGNED_DATA_TYPE_AWDTSR         = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_AWDTSR, BITS_TO_BYTES(32u)),
    QLIB_SIGNED_DATA_TYPE_MC             = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_MC, BITS_TO_BYTES(64u)),
    QLIB_SIGNED_DATA_TYPE_GMC            = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_GMC, BITS_TO_BYTES(160u)),
    QLIB_SIGNED_DATA_TYPE_GMT            = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_GMT, BITS_TO_BYTES(160u)),
    QLIB_SIGNED_DATA_TYPE_SECTION_CONFIG = MAKE_16_BIT(QLIB_SIGNED_DATA_ID_SECTION_CONFIG, BITS_TO_BYTES(160u))
} QLIB_SIGNED_DATA_TYPE_T;

/************************************************************************************************************
 *  Maximum data size
************************************************************************************************************/
#define QLIB_SIGNED_DATA_MAX_SIZE (U8) BITS_TO_BYTES(160u)

/************************************************************************************************************
 * Maximum calc_sig params size
************************************************************************************************************/
#define QLIB_SIGNED_CALC_SIG_PARAMS_MAX_SIZE BITS_TO_BYTES(64u)

/*---------------------------------------------------------------------------------------------------------*/
/* Get data size                                                                                           */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_SIGNED_DATA_TYPE_GET_SIZE(data_type) BYTE(data_type, 1)

/*---------------------------------------------------------------------------------------------------------*/
/* Get data ID                                                                                             */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_SIGNED_DATA_TYPE_GET_ID(data_type, section)                                                               \
    (BYTE(data_type, 0) +                                                                                              \
     ((((data_type) == QLIB_SIGNED_DATA_TYPE_SECTION_DIGEST) || ((data_type) == QLIB_SIGNED_DATA_TYPE_SECTION_CONFIG)) \
          ? ((U8)(0x0fu & (section)))                                                                                  \
          : 0u))

/*---------------------------------------------------------------------------------------------------------*/
/* open session command mode field                                                                         */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_SEC_CMD_OPEN_MODE_FIELD_INC_WID 0u, 1u
#define QLIB_SEC_CMD_OPEN_MODE_FIELD_IGN_SCR 1u, 1u

#define QLIB_SEC_OPEN_CMD_MODE_SET(mode, includeWID, ignoreScrValidity)                                           \
    {                                                                                                             \
        SET_VAR_FIELD_8(mode, QLIB_SEC_CMD_OPEN_MODE_FIELD_INC_WID, ((includeWID) == TRUE) ? 0x1UL : 0UL);        \
        SET_VAR_FIELD_8(mode, QLIB_SEC_CMD_OPEN_MODE_FIELD_IGN_SCR, ((ignoreScrValidity) == TRUE) ? 0x1UL : 0UL); \
    }

/*---------------------------------------------------------------------------------------------------------*/
/* close session command mode field                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_SEC_CMD_CLOSE_MODE_FIELD_REVOKE_PA 0u, 1u

#define QLIB_SEC_CLOSE_CMD_MODE_SET(mode, revokePlainAccess)                                                         \
    {                                                                                                                \
        SET_VAR_FIELD_8((mode), QLIB_SEC_CMD_CLOSE_MODE_FIELD_REVOKE_PA, ((revokePlainAccess) == TRUE) ? 0x1u : 0u); \
    }

/************************************************************************************************************
 * SET_SCR command mode field
************************************************************************************************************/
#define QLIB_SEC_CMD_SET_SCR_MODE_FIELD_RESET  0u, 1u
#define QLIB_SEC_CMD_SET_SCR_MODE_FIELD_RELOAD 1u, 1u

#define QLIB_SEC_CMD_SET_SCR_MODE_SET(mode, reset, reload)                                               \
    {                                                                                                    \
        SET_VAR_FIELD_8((mode), QLIB_SEC_CMD_SET_SCR_MODE_FIELD_RESET, ((reset) == TRUE) ? 0x1u : 0u);   \
        SET_VAR_FIELD_8((mode), QLIB_SEC_CMD_SET_SCR_MODE_FIELD_RELOAD, ((reload) == TRUE) ? 0x1u : 0u); \
    }

/************************************************************************************************************
 * FORMAT / SFORMAT command mode field
************************************************************************************************************/
#define QLIB_SEC_CMD_FORMAT_MODE_FIELD_RESET   0u, 1u
#define QLIB_SEC_CMD_FORMAT_MODE_FIELD_DEFAULT 1u, 1u
#define QLIB_SEC_CMD_FORMAT_MODE_FIELD_INIT    2u, 1u

#define QLIB_SEC_CMD_FORMAT_MODE_SET(mode, reset, default, init)                                          \
    {                                                                                                     \
        SET_VAR_FIELD_8((mode), QLIB_SEC_CMD_FORMAT_MODE_FIELD_RESET, ((reset) == TRUE) ? 0x1u : 0u);     \
        SET_VAR_FIELD_8((mode), QLIB_SEC_CMD_FORMAT_MODE_FIELD_DEFAULT, ((default) == TRUE) ? 0x1u : 0u); \
        SET_VAR_FIELD_8((mode), QLIB_SEC_CMD_FORMAT_MODE_FIELD_INIT, ((init) == TRUE) ? 0x1u : 0u);       \
    }

#ifdef __cplusplus
}
#endif

#endif //__QLIB_SEC_CMDS_H__
