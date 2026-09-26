/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2020 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_cfg.h
* @brief      This file contains specific chip qlib definitions
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_CFG_H__
#define __QLIB_CFG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "qlib_targets.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                    FORWARD DECLARATIONS                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
struct QLIB_CONTEXT_T;
struct QLIB_HW_VER_T;

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                DEFINITIONS                                              */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/************************************************************************************************************
 * This enumeration defines the possible chip specific features
************************************************************************************************************/
typedef enum QLIB_CFG_FEATURES_T
{
    Q2_BYPASS_HW_ISSUE_23_E,  // OP0 & OP1 are not working in QPI mode
    Q2_BYPASS_HW_ISSUE_49_E,  // The swap indication for section 7 integrity, in Fallback mode, is wrongly taken from section 0
    Q2_BYPASS_HW_ISSUE_60_E,  // standard short write or standard erase in QPI mode doesn't return privilege error
    Q2_BYPASS_HW_ISSUE_96_E,  // get_ssr returns false non-busy indication
    Q2_BYPASS_HW_ISSUE_98_E,  // calc sig sets ssr_ready after clearing busy bit
    Q2_BYPASS_HW_ISSUE_105_E, // section size should be in 256KB granularity
    Q2_BYPASS_HW_ISSUE_262_E, // some commands are ignored when CTAG is sent in quad/qpi mode
    Q2_BYPASS_HW_ISSUE_263_E, // Setting chip to 16Mb doesn't affect HW_VER.size field
    Q2_BYPASS_HW_ISSUE_265_E, // Chip erase fail after global unlock
    Q2_BYPASS_HW_ISSUE_294_E, // Quad GET_SSR during Page Program process in repaired devices cause wrong bit to be programmed
    Q2_BYPASS_HW_ISSUE_289_E, // False positive error indication when SET_SCR sent with split OP1 instructions at low VCC
    Q2_RESTRICTED_ACTIVATION_KID_E,   // Restricted Access Activation Key is available only on HCD devices.
    Q2_SESSION_CLOSE_NOT_SUPPORTED_E, // no close session command
    Q2_EXTEND_SEC_LEN_BITS_E,
    Q2_4_BYTES_ADDRESS_MODE_E,
    Q2_DEVCFG_CTAG_MODE_E,
    Q2_SUPPORT_AWDTCFG_FALLBACK_E,
    Q2_TC_30_BIT_E,
    Q2_ESSR_FEATURE_E,
    W77Q_ESSR_KID_MSB_E,
    Q2_CMVP_BIST_FEATURE_E,
    Q2_SPLIT_IBUF_FEATURE_E,
    W77Q_SPLIT_IBUF_CTAG_ONLY_FIRST_E,
    Q2_QPI_SET_READ_PARAMS_E,
    W77Q_RNG_FEATURE_E,
    Q2_OP0_OP2_DTR_FEATURE_E, // OP0 and OP2 support DTR
    W77Q_EXTENDED_ADDR_REG_E,
    Q2_OPEN_CLOSE_SESSION_2ND_GET_SSR_E,
    Q2_POLICY_AUTH_PROT_AC_BIT_E,
    Q2_VCC_3_3_V_E,
    Q2_LIMITED_SECT_SEL_MODES_E, // MCD devices support 22b-25b SECT_SEL modes only
    Q2_HOLD_PIN_E,
    W77Q_CMD_MANUFACTURER_AND_DEVICE_ID_E,
    W77Q_WID_PID_E,
    W77Q_RST_RESP_E,
    W77Q_SUPPORT_DUAL_SPI_E,
    W77Q_SUPPORT__1_1_4_SPI_E,
    W77Q_SUPPORT_OCTAL_SPI_E,
    Q3_BYPASS_HW_ISSUE_282_E, // SET_SCR/SET_SCR_SWAP via LMS ignores RELOAD and RESET MODE bits
    W77Q_SEC_SIZE_SCALE_E,
    W77Q_SET_SCR_MODE_E,
    W77Q_SUPPORT_UNIQUE_ID_QPI_E,
    W77Q_EXTENDED_UNIQUE_ID_16B_E, // W77Q (16Mb to 128Mb) unique ID is 8B while W77Q/T (256Mb to 1Gb) is 16B
    W77Q_HW_RPMC_E,
    W77Q_PRE_PROV_MASTER_KEY_E,
    W77Q_FAST_READ_DUMMY_CONFIG_E,
    W77Q_SUPPORT_SSE_E, // data CRC protection and data extension byte
    W77Q_BOOT_FAIL_RESET_E,
    W77Q_RST_PA_E,
    W77Q_VAULT_E,
    W77Q_FORMAT_MODE_E,
    W77Q_FLAG_REGISTER_E,
    W77Q_EXTENDED_CONFIG_REGISTER_E,
    W77Q_CMD_GET_KEYS_STATUS_E,
    W77Q_CMD_PA_GRANT_REVOKE_E,
    W77Q_VER_INTG_DIGEST_E,
    W77Q_AWDT_VAL_SEC_FRACT_E,
    W77Q_READ_BYPASS_MODE_BYTE_E,
    Q2_DEVICE_ID_DUMMY_E,
    W77Q_READ_EXTENDED_ADDR_DUMMY_E,
    W77Q_READ_SR_DUMMY_E,
    W77Q_READ_JEDEC_QPI_DUMMY_E,
    W77Q_SUPPORT_LMS_E,
    W77Q_SUPPORT_LMS_ATTESTATION_E,
    W77Q_SECURE_LOG_E,
    W77Q_MEM_CRC_E,
    W77Q_MEM_COPY_E,
    W77Q_AWDTCFG_OSC_RATE_FRAC_E,
    W77Q_DEVCFG_LOCK_E,
    W77Q_DEVCFG_BOOT_FAIL_RST_E,
    W77Q_ECC_E,
    W77Q_SIG_MEM_RANGE_E,
    W77Q_CONFIG_ZERO_SECTION_ALLOWED_E,
    Q3_BYPASS_HW_ISSUE_OPEN_SESS_SYS_ERR_E, // Occasional SYS_ERR is seen on SESSION_OPEN even though the command succeeded
    Q3_BYPASS_HW_ISSUE_SET_CMD_FAIL_E, // Occasional failure of SET_SCR/SET_SCR_SWAP/LMS_EXEC/SET_GMC/SET_GMT with SYSTEM_ERROR
    Q3_BYPASS_HW_ISSUE_OCTAL_SESSION_OPEN_SECOND_SSR_E, // get_ssr returns false session ready indication
    W77Q_MULTI_DIE_E,
    W77Q_INTERRUPTS_E,
    Q3_BYPASS_HW_ISSUE_QPI_POWER_DOWN_RESP_ERR_E, // Release Power-Down command returns invalid response while device is powered down
    W77Q_DEDICATED_RESET_INPUT_ENABLE_DEFAULT_E, // Dedicated reset input is enabled by default
    QLIB_CFG_FEATURE_MAX
} QLIB_CFG_FEATURES_T;

#if ((QLIB_TARGET & (QLIB_TARGET - 1)) != 0)
#define QLIB_CFG_ADD(qlibContext, cfg) SET_VAR_BIT_TYPE(U8, (qlibContext)->cfgBitArr[((U8)(cfg)) >> 3u], ((U8)(cfg)) & 0x7u)
#else
#define QLIB_CFG_ADD(qlibContext, cfg) // not needed since the configuration is determined in compilation time
#endif
#define QLIB_CFG_EXIST(qlibContext, cfg) READ_VAR_BIT((qlibContext)->cfgBitArr[((U8)(cfg)) >> 3], ((U8)(cfg)) & 0x7u)

/********************************************************************************************************
 * Default Definitions
********************************************************************************************************/
#ifndef QLIB_NUM_OF_DIES
#define QLIB_NUM_OF_DIES 1u
#endif

/********************************************************************************************************
 * Set constant defines by QLIB target, if QLIB target is known in compile time
********************************************************************************************************/
#if ((QLIB_TARGET & all_q2_targets) && !(QLIB_TARGET & all_q3_targets))
// currently defined for all W77Q (16Mb to 128Mb) targets:
#define Q2_BYPASS_HW_ISSUE_49(qlibContext)               (1u)
#define Q2_BYPASS_HW_ISSUE_60(qlibContext)               (1u)
#define Q2_BYPASS_HW_ISSUE_263(qlibContext)              (1u)
#define W77Q_CMD_MANUFACTURER_AND_DEVICE_ID(qlibContext) (1u)
#define Q2_HOLD_PIN(qlibContext)                         (1u)
#define Q2_QPI_SET_READ_PARAMS(qlibContext)              (1u)
#define W77Q_SUPPORT_DUAL_SPI(qlibContext)               (1u)
#define W77Q_SUPPORT__1_1_4_SPI(qlibContext)             (1u)
#define W77Q_SUPPORT_OCTAL_SPI(qlibContext)              (0u)
#define Q3_BYPASS_HW_ISSUE_282(qlibContext)              (0u)
#define W77Q_SEC_SIZE_SCALE(qlibContext)                 (0u)
#define W77Q_SET_SCR_MODE(qlibContext)                   (0u)
#define W77Q_SUPPORT_UNIQUE_ID_QPI(qlibContext)          (0u)
#define W77Q_EXTENDED_UNIQUE_ID_16B(qlibContext)         (0u)
#define W77Q_HW_RPMC(qlibContext)                        (0u)
#define W77Q_PRE_PROV_MASTER_KEY(qlibContext)            (0u)
#define W77Q_FAST_READ_DUMMY_CONFIG(qlibContext)         (0u)
#define W77Q_SUPPORT_SSE(qlibContext)                    (0u)
#define W77Q_BOOT_FAIL_RESET(qlibContext)                (0u)
#define W77Q_RST_PA(qlibContext)                         (0u)
#define W77Q_DEVCFG_LOCK(qlibContext)                    (0u)
#define W77Q_DEVCFG_BOOT_FAIL_RST(qlibContext)           (0u)
#define W77Q_VAULT(qlibContext)                          (0u)
#define W77Q_FORMAT_MODE(qlibContext)                                 (0u)
#define W77Q_FLAG_REGISTER(qlibContext)                               (0u)
#define W77Q_EXTENDED_CONFIG_REGISTER(qlibContext)                    (0u)
#define W77Q_WID_PID(qlibContext)                                     (1u)
#define W77Q_RST_RESP(qlibContext)                                    (1u)
#define W77Q_CMD_GET_KEYS_STATUS(qlibContext)                         (0u)
#define W77Q_CMD_PA_GRANT_REVOKE(qlibContext)                         (0u)
#define W77Q_VER_INTG_DIGEST(qlibContext)                             (0u)
#define W77Q_AWDT_VAL_SEC_FRACT(qlibContext)                          (0u)
#define W77Q_READ_BYPASS_MODE_BYTE(qlibContext)                       (1u)
#define Q2_DEVICE_ID_DUMMY(qlibContext)                               (1u)
#define W77Q_READ_EXTENDED_ADDR_DUMMY(qlibContext)                    (0u)
#define W77Q_READ_JEDEC_QPI_DUMMY(qlibContext)                        (0u)
#define W77Q_READ_SR_DUMMY(qlibContext)                               (0u)
#define W77Q_SUPPORT_LMS(qlibContext)                                 (0u)
#define W77Q_SUPPORT_LMS_ATTESTATION(qlibContext)                     (0u)
#define W77Q_SECURE_LOG(qlibContext)                                  (0u)
#define W77Q_MEM_CRC(qlibContext)                                     (0u)
#define W77Q_MEM_COPY(qlibContext)                                    (0u)
#define W77Q_AWDTCFG_OSC_RATE_FRAC(qlibContext)                       (1u)
#define W77Q_ECC(qlibContext)                                         (0u)
#define W77Q_SIG_MEM_RANGE(qlibContext)                               (0u)
#define W77Q_ESSR_KID_MSB(qlibContext)                                (0u)
#define W77Q_CONFIG_ZERO_SECTION_ALLOWED(qlibContext)                 (0u)
#define Q3_BYPASS_HW_ISSUE_OPEN_SESS_SYS_ERR(qlibContext)             (0u)
#define Q3_BYPASS_HW_ISSUE_SET_CMD_FAIL(qlibContext)                  (0u)
#define Q3_BYPASS_HW_ISSUE_OCTAL_SESSION_OPEN_SECOND_SSR(qlibContext) (0u)
#define W77Q_MULTI_DIE(qlibContext)                                   (0u)
#define W77Q_INTERRUPTS(qlibContext)                                  (0u)
#define Q3_BYPASS_HW_ISSUE_QPI_POWER_DOWN_RESP_ERR(qlibContext)       (0u)
#define W77Q_DEDICATED_RESET_INPUT_ENABLE_DEFAULT(qlibContext)        (0u)

#elif ((QLIB_TARGET & all_q3_targets) && !(QLIB_TARGET & all_q2_targets))
// for all W77Q/T (256Mb to 1Gb) targets
#define Q2_BYPASS_HW_ISSUE_23(qlibContext)  (0u)
#define Q2_BYPASS_HW_ISSUE_49(qlibContext)  (0u)
#define Q2_BYPASS_HW_ISSUE_60(qlibContext)  (0u)
#define Q2_BYPASS_HW_ISSUE_96(qlibContext)  (0u)
#define Q2_BYPASS_HW_ISSUE_98(qlibContext)  (0u)
#define Q2_BYPASS_HW_ISSUE_105(qlibContext) (0u)
#define Q2_BYPASS_HW_ISSUE_262(qlibContext) (0u)
#define Q2_BYPASS_HW_ISSUE_263(qlibContext) (0u)
#define Q2_BYPASS_HW_ISSUE_265(qlibContext) (0u)
#define Q2_BYPASS_HW_ISSUE_294(qlibContext) (0u)
#define Q2_BYPASS_HW_ISSUE_289(qlibContext) (0u)
#define Q2_RESTRICTED_ACTIVATION_KID(qlibContext) \
    (1u) //todo change name to support both W77Q (16Mb to 128Mb) and W77Q/T (256Mb to 1Gb)
#define Q2_SESSION_CLOSE_NOT_SUPPORTED(qlibContext)      (0u)
#define Q2_EXTEND_SEC_LEN_BITS(qlibContext)              (1u) // todo name change
#define Q2_4_BYTES_ADDRESS_MODE(qlibContext)             (1u) // todo name change
#define Q2_DEVCFG_CTAG_MODE(qlibContext)                 (0u)
#define Q2_SUPPORT_AWDTCFG_FALLBACK(qlibContext)         (1u) // todo name change
#define Q2_TC_30_BIT(qlibContext)                        (1u) // todo name change
#define Q2_ESSR_FEATURE(qlibContext)                     (1u) // todo name change
#define W77Q_ESSR_KID_MSB(qlibContext)                   (1u)
#define Q2_CMVP_BIST_FEATURE(qlibContext)                (1u) // todo name change
#define Q2_SPLIT_IBUF_FEATURE(qlibContext)               (1u) // todo name change - TODO add feature whether all command include ctag
#define W77Q_SPLIT_IBUF_CTAG_ONLY_FIRST(qlibContext)     (0u)
#define W77Q_RNG_FEATURE(qlibContext)                    (1u)
#define Q2_OP0_OP2_DTR_FEATURE(qlibContext)              (0u)
#define W77Q_EXTENDED_ADDR_REG(qlibContext)              (1u)
#define Q2_OPEN_CLOSE_SESSION_2ND_GET_SSR(qlibContext)   (0u)
#define Q2_POLICY_AUTH_PROT_AC_BIT(qlibContext)          (1u) // TODO name change
#define Q2_VCC_3_3_V(qlibContext)                        (0u)
#define Q2_LIMITED_SECT_SEL_MODES(qlibContext)           (0u)
#define W77Q_CMD_MANUFACTURER_AND_DEVICE_ID(qlibContext) (0u)
#define Q2_HOLD_PIN(qlibContext)                         (0u)
#define Q2_QPI_SET_READ_PARAMS(qlibContext)              (0u)
#define W77Q_WID_PID(qlibContext)                        (0u)
#define W77Q_RST_RESP(qlibContext)                       (0u)
#define W77Q_SUPPORT_DUAL_SPI(qlibContext)               (0u)
#define W77Q_SUPPORT__1_1_4_SPI(qlibContext)             (0u)
#define Q3_BYPASS_HW_ISSUE_282(qlibContext)              (1u)
#define W77Q_SEC_SIZE_SCALE(qlibContext)                 (1u)
#define W77Q_SET_SCR_MODE(qlibContext)                   (1u)
#define W77Q_SUPPORT_UNIQUE_ID_QPI(qlibContext)          (1u)
#define W77Q_EXTENDED_UNIQUE_ID_16B(qlibContext)         (1u)
#define W77Q_HW_RPMC(qlibContext)                        (1u)
#define W77Q_PRE_PROV_MASTER_KEY(qlibContext)            (1u)
#define W77Q_FAST_READ_DUMMY_CONFIG(qlibContext)         (1u)
#define W77Q_RST_PA(qlibContext)                         (1u)
#define W77Q_DEVCFG_LOCK(qlibContext)                    (1u)
#define W77Q_DEVCFG_BOOT_FAIL_RST(qlibContext)           (0u)
#define W77Q_VAULT(qlibContext)                          (1u)
#define W77Q_FORMAT_MODE(qlibContext)                                 (1u)
#define W77Q_FLAG_REGISTER(qlibContext)                               (1u)
#define W77Q_EXTENDED_CONFIG_REGISTER(qlibContext)                    (1u)
#define W77Q_CMD_GET_KEYS_STATUS(qlibContext)                         (1u)
#define W77Q_CMD_PA_GRANT_REVOKE(qlibContext)                         (1u)
#define W77Q_VER_INTG_DIGEST(qlibContext)                             (1u)
#define W77Q_AWDT_VAL_SEC_FRACT(qlibContext)                          (1u)
#define W77Q_READ_BYPASS_MODE_BYTE(qlibContext)                       (0u)
#define Q2_DEVICE_ID_DUMMY(qlibContext)                               (0u)
#define W77Q_READ_EXTENDED_ADDR_DUMMY(qlibContext)                    (1u)
#define W77Q_READ_JEDEC_QPI_DUMMY(qlibContext)                        (1u)
#define W77Q_READ_SR_DUMMY(qlibContext)                               (1u)
#define W77Q_SUPPORT_LMS(qlibContext)                                 (1u)
#define W77Q_SUPPORT_LMS_ATTESTATION(qlibContext)                     (1u)
#define W77Q_SECURE_LOG(qlibContext)                                  (1u)
#define W77Q_MEM_CRC(qlibContext)                                     (1u)
#define W77Q_MEM_COPY(qlibContext)                                    (1u)
#define W77Q_AWDTCFG_OSC_RATE_FRAC(qlibContext)                       (0u)
#define W77Q_SIG_MEM_RANGE(qlibContext)                               (1u)
#define W77Q_CONFIG_ZERO_SECTION_ALLOWED(qlibContext)                 (1u)
#define Q3_BYPASS_HW_ISSUE_OCTAL_SESSION_OPEN_SECOND_SSR(qlibContext) (1u)
#define W77Q_MULTI_DIE(qlibContext)                                   (1u)
#define W77Q_INTERRUPTS(qlibContext)                                  (1u)

#if ((QLIB_NUM_OF_DIES != 1ul) && (QLIB_NUM_OF_DIES != 2ul) && (QLIB_NUM_OF_DIES != 4ul))
#error bad QLIB_NUM_OF_DIES definition
#endif
#endif

#if QLIB_TARGET == w77q32jw_revB
// MCD rev B defines:
#define Q2_BYPASS_HW_ISSUE_23(qlibContext)  (1u)
#define Q2_BYPASS_HW_ISSUE_42(qlibContext)  (1u)
#define Q2_BYPASS_HW_ISSUE_96(qlibContext)  (1u)
#define Q2_BYPASS_HW_ISSUE_98(qlibContext)  (1u)
#define Q2_BYPASS_HW_ISSUE_105(qlibContext) (1u)
#define Q2_BYPASS_HW_ISSUE_294(qlibContext) (1u)

// MCD defines:
#define Q2_SESSION_CLOSE_NOT_SUPPORTED(qlibContext) (1u)
#define Q2_OP0_OP2_DTR_FEATURE(qlibContext)         (1u)
#define Q2_LIMITED_SECT_SEL_MODES(qlibContext)      (1u)
#define QLIB_FLASH_SIZE                             _4MB_

// undefined features:
#define Q2_BYPASS_HW_ISSUE_262(qlibContext)            (0u)
#define Q2_BYPASS_HW_ISSUE_265(qlibContext)            (0u)
#define Q2_BYPASS_HW_ISSUE_289(qlibContext)            (0u)
#define Q2_RESTRICTED_ACTIVATION_KID(qlibContext)      (0u)
#define Q2_EXTEND_SEC_LEN_BITS(qlibContext)            (0u)
#define Q2_4_BYTES_ADDRESS_MODE(qlibContext)           (0u)
#define Q2_DEVCFG_CTAG_MODE(qlibContext)               (0u)
#define Q2_SUPPORT_AWDTCFG_FALLBACK(qlibContext)       (0u)
#define Q2_TC_30_BIT(qlibContext)                      (0u)
#define Q2_ESSR_FEATURE(qlibContext)                   (0u)
#define Q2_CMVP_BIST_FEATURE(qlibContext)              (0u)
#define Q2_SPLIT_IBUF_FEATURE(qlibContext)             (0u)
#define W77Q_SPLIT_IBUF_CTAG_ONLY_FIRST(qlibContext)   (0u)
#define W77Q_RNG_FEATURE(qlibContext)                  (0u)
#define W77Q_EXTENDED_ADDR_REG(qlibContext)            (0u)
#define Q2_VCC_3_3_V(qlibContext)                      (0u)
#define Q2_POLICY_AUTH_PROT_AC_BIT(qlibContext)        (0u)
#define Q2_OPEN_CLOSE_SESSION_2ND_GET_SSR(qlibContext) (0u)

#elif ((QLIB_TARGET == w77q64jv_revA) || (QLIB_TARGET == w77q128jv_revA) || (QLIB_TARGET == w77q64jw_revA) ||  (QLIB_TARGET == w77q128jw_revA) )

// HCD defines:
#define Q2_4_BYTES_ADDRESS_MODE(qlibContext)           (1u)
#define Q2_RESTRICTED_ACTIVATION_KID(qlibContext)      (1u)
#define Q2_EXTEND_SEC_LEN_BITS(qlibContext)            (1u)
#define Q2_DEVCFG_CTAG_MODE(qlibContext)               (1u)
#define Q2_SUPPORT_AWDTCFG_FALLBACK(qlibContext)       (1u)
#define Q2_TC_30_BIT(qlibContext)                      (1u)
#define Q2_ESSR_FEATURE(qlibContext)                   (1u)
#define Q2_CMVP_BIST_FEATURE(qlibContext)              (1u)
#define Q2_SPLIT_IBUF_FEATURE(qlibContext)             (1u)
#define W77Q_SPLIT_IBUF_CTAG_ONLY_FIRST(qlibContext)   (1u)
#define W77Q_RNG_FEATURE(qlibContext)                  (1u)
#define W77Q_EXTENDED_ADDR_REG(qlibContext)            (1u)
#define Q2_OPEN_CLOSE_SESSION_2ND_GET_SSR(qlibContext) (1u)
// undefined HCD features:
#define Q2_SESSION_CLOSE_NOT_SUPPORTED(qlibContext)    (0u)
#define Q2_OP0_OP2_DTR_FEATURE(qlibContext)            (0u)
#define Q2_LIMITED_SECT_SEL_MODES(qlibContext)         (0u)

#if ((QLIB_TARGET == w77q64jw_revA) || (QLIB_TARGET == w77q64jv_revA))
#define QLIB_FLASH_SIZE _8MB_
#else
#define QLIB_FLASH_SIZE _16MB_
#endif

#if ((QLIB_TARGET == w77q64jw_revA) || (QLIB_TARGET == w77q128jw_revA) )
// HCD jw (1.8V) rev A defines:
#define Q2_BYPASS_HW_ISSUE_262(qlibContext)     (1u)
#define Q2_BYPASS_HW_ISSUE_265(qlibContext)     (1u)
#define Q2_BYPASS_HW_ISSUE_294(qlibContext)     (1u)
#define Q2_BYPASS_HW_ISSUE_289(qlibContext)     (1u)

// undefined features
#define Q2_BYPASS_HW_ISSUE_23(qlibContext)      (0u)
#define Q2_BYPASS_HW_ISSUE_42(qlibContext)      (0u)
#define Q2_BYPASS_HW_ISSUE_96(qlibContext)      (0u)
#define Q2_BYPASS_HW_ISSUE_98(qlibContext)      (0u)
#define Q2_BYPASS_HW_ISSUE_105(qlibContext)     (0u)
#define Q2_VCC_3_3_V(qlibContext)               (0u)
#define Q2_POLICY_AUTH_PROT_AC_BIT(qlibContext) (0u)

#elif ((QLIB_TARGET == w77q64jv_revA) || (QLIB_TARGET == w77q128jv_revA))
// HCD jv (3.3V) rev A defines:
#define Q2_BYPASS_HW_ISSUE_265(qlibContext)     (1u)

// HCD jv (3.3V) defines:
#define Q2_VCC_3_3_V(qlibContext)               (1u)
#define Q2_POLICY_AUTH_PROT_AC_BIT(qlibContext) (1u)

// undefined features
#define Q2_BYPASS_HW_ISSUE_23(qlibContext)      (0u)
#define Q2_BYPASS_HW_ISSUE_42(qlibContext)      (0u)
#define Q2_BYPASS_HW_ISSUE_96(qlibContext)      (0u)
#define Q2_BYPASS_HW_ISSUE_98(qlibContext)      (0u)
#define Q2_BYPASS_HW_ISSUE_105(qlibContext)     (0u)
#define Q2_BYPASS_HW_ISSUE_262(qlibContext)     (0u)
#define Q2_BYPASS_HW_ISSUE_294(qlibContext)     (0u)
#define Q2_BYPASS_HW_ISSUE_289(qlibContext)     (0u)
#endif
#elif ((QLIB_TARGET == w77q25nw_Ind_revB) || (QLIB_TARGET == w77q25nw_Auto_revB) || (QLIB_TARGET == w77t25nw_Ind_revB) ||  (QLIB_TARGET == w77t25nw_Auto_revB) )
// W77Q/T (256Mb to 1Gb) secure flash
#define QLIB_FLASH_SIZE _32MB_
#if (QLIB_TARGET == w77q25nw_Ind_revB) || (QLIB_TARGET == w77q25nw_Auto_revB)
#define W77Q_SUPPORT_OCTAL_SPI(qlibContext) (0u)
#elif (QLIB_TARGET == w77t25nw_Ind_revB) || (QLIB_TARGET == w77t25nw_Auto_revB)
#define W77Q_SUPPORT_OCTAL_SPI(qlibContext) (1u)
#endif

#if ((QLIB_TARGET == w77q25nw_Auto_revB) || (QLIB_TARGET == w77t25nw_Auto_revB) )
#define W77Q_ECC(qlibContext) (1u)
#else
#if ((QLIB_TARGET == w77q25nw_Ind_revB) || (QLIB_TARGET == w77t25nw_Ind_revB))
#define W77Q_ECC(qlibContext) (0u)
#endif
#endif

#define W77Q_SUPPORT_SSE(qlibContext)                           (0u)
#define W77Q_BOOT_FAIL_RESET(qlibContext)                       (0u)
#define Q3_BYPASS_HW_ISSUE_OPEN_SESS_SYS_ERR(qlibContext)       (1u)
#define Q3_BYPASS_HW_ISSUE_SET_CMD_FAIL(qlibContext)            (1u)
#define Q3_BYPASS_HW_ISSUE_QPI_POWER_DOWN_RESP_ERR(qlibContext) (1u)
#define W77Q_DEDICATED_RESET_INPUT_ENABLE_DEFAULT(qlibContext)  (1u)
#endif

/********************************************************************************************************
 * Set defines by QLIB target, for use if target is detected in runtime
********************************************************************************************************/
#ifndef Q2_BYPASS_HW_ISSUE_23
#define Q2_BYPASS_HW_ISSUE_23(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_23_E)
#endif
#ifndef Q2_BYPASS_HW_ISSUE_42
#define Q2_BYPASS_HW_ISSUE_42(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_42_E)
#endif
#ifndef Q2_BYPASS_HW_ISSUE_49
#define Q2_BYPASS_HW_ISSUE_49(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_49_E)
#endif
#ifndef Q2_BYPASS_HW_ISSUE_60
#define Q2_BYPASS_HW_ISSUE_60(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_60_E)
#endif
#ifndef Q2_BYPASS_HW_ISSUE_96
#define Q2_BYPASS_HW_ISSUE_96(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_96_E)
#endif
#ifndef Q2_BYPASS_HW_ISSUE_98
#define Q2_BYPASS_HW_ISSUE_98(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_98_E)
#endif
#ifndef Q2_BYPASS_HW_ISSUE_105
#define Q2_BYPASS_HW_ISSUE_105(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_105_E)
#endif
#ifndef Q2_BYPASS_HW_ISSUE_262
#define Q2_BYPASS_HW_ISSUE_262(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_262_E)
#endif
#ifndef Q2_BYPASS_HW_ISSUE_263
#define Q2_BYPASS_HW_ISSUE_263(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_263_E)
#endif
#ifndef Q2_BYPASS_HW_ISSUE_265
#define Q2_BYPASS_HW_ISSUE_265(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_265_E)
#endif
#ifndef Q2_BYPASS_HW_ISSUE_289
#define Q2_BYPASS_HW_ISSUE_289(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_289_E)
#endif
#ifndef Q2_BYPASS_HW_ISSUE_294
#define Q2_BYPASS_HW_ISSUE_294(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_BYPASS_HW_ISSUE_294_E)
#endif
#ifndef Q2_RESTRICTED_ACTIVATION_KID
#define Q2_RESTRICTED_ACTIVATION_KID(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_RESTRICTED_ACTIVATION_KID_E)
#endif
#ifndef Q2_SESSION_CLOSE_NOT_SUPPORTED
#define Q2_SESSION_CLOSE_NOT_SUPPORTED(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_SESSION_CLOSE_NOT_SUPPORTED_E)
#endif
#ifndef Q2_EXTEND_SEC_LEN_BITS
#define Q2_EXTEND_SEC_LEN_BITS(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_EXTEND_SEC_LEN_BITS_E)
#endif
#ifndef Q2_4_BYTES_ADDRESS_MODE
#define Q2_4_BYTES_ADDRESS_MODE(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_4_BYTES_ADDRESS_MODE_E)
#else //not defined Q2_4_BYTES_ADDRESS_MODE
#if (Q2_4_BYTES_ADDRESS_MODE(qlibContext) == 0u)
#define EXCLUDE_Q2_4_BYTES_ADDRESS_MODE
#endif //(Q2_4_BYTES_ADDRESS_MODE(qlibContext) == 0u)
#endif //not defined Q2_4_BYTES_ADDRESS_MODE
#ifndef Q2_DEVCFG_CTAG_MODE
#define Q2_DEVCFG_CTAG_MODE(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_DEVCFG_CTAG_MODE_E)
#else //not defined Q2_DEVCFG_CTAG_MODE
#if (Q2_DEVCFG_CTAG_MODE(qlibContext) == 0u)
#define EXCLUDE_Q2_DEVCFG_CTAG_MODE
#endif //(Q2_DEVCFG_CTAG_MODE(qlibContext) == 0u)
#endif //not defined Q2_DEVCFG_CTAG_MODE
#ifndef Q2_SUPPORT_AWDTCFG_FALLBACK
#define Q2_SUPPORT_AWDTCFG_FALLBACK(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_SUPPORT_AWDTCFG_FALLBACK_E)
#endif
#ifndef Q2_TC_30_BIT
#define Q2_TC_30_BIT(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_TC_30_BIT_E)
#endif
#ifndef Q2_ESSR_FEATURE
#define Q2_ESSR_FEATURE(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_ESSR_FEATURE_E)
#else //not defined Q2_ESSR_FEATURE
#if (Q2_ESSR_FEATURE(qlibContext) == 0u)
#define EXCLUDE_Q2_ESSR_FEATURE
#endif //(Q2_ESSR_FEATURE(qlibContext) == 0u)
#endif //not defined Q2_ESSR_FEATURE
#ifndef W77Q_ESSR_KID_MSB
#define W77Q_ESSR_KID_MSB(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_ESSR_KID_MSB_E)
#endif
#ifndef Q2_CMVP_BIST_FEATURE
#define Q2_CMVP_BIST_FEATURE(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_CMVP_BIST_FEATURE_E)
#else //not defined Q2_CMVP_BIST_FEATURE
#if (Q2_CMVP_BIST_FEATURE(qlibContext) == 0u)
#define EXCLUDE_Q2_CMVP_BIST_FEATURE
#endif //(Q2_CMVP_BIST_FEATURE(qlibContext) == 0u)
#endif //not defined Q2_CMVP_BIST_FEATURE
#ifndef Q2_SPLIT_IBUF_FEATURE
#define Q2_SPLIT_IBUF_FEATURE(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_SPLIT_IBUF_FEATURE_E)
#endif
#ifndef W77Q_SPLIT_IBUF_CTAG_ONLY_FIRST
#define W77Q_SPLIT_IBUF_CTAG_ONLY_FIRST(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SPLIT_IBUF_CTAG_ONLY_FIRST_E)
#else
#if defined QLIB_MAX_SPI_OUTPUT_SIZE && (W77Q_SPLIT_IBUF_CTAG_ONLY_FIRST(qlibContext) == 0u) && \
    (QLIB_MAX_SPI_OUTPUT_SIZE < (W77Q_CTAG_SIZE_BYTE + 4))
#error "QLIB_MAX_SPI_OUTPUT_SIZE should be at least size of ctag and 4B data"
#endif
#endif
#ifndef W77Q_RNG_FEATURE
#define W77Q_RNG_FEATURE(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_RNG_FEATURE_E)
#else //not defined W77Q_RNG_FEATURE
#if (W77Q_RNG_FEATURE(qlibContext) == 0u)
#define EXCLUDE_W77Q_RNG_FEATURE
#endif //(W77Q_RNG_FEATURE(qlibContext) == 0u)
#endif //not defined W77Q_RNG_FEATURE
#ifndef Q2_OP0_OP2_DTR_FEATURE
#define Q2_OP0_OP2_DTR_FEATURE(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_OP0_OP2_DTR_FEATURE_E)
#endif
#ifndef W77Q_EXTENDED_ADDR_REG
#define W77Q_EXTENDED_ADDR_REG(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_EXTENDED_ADDR_REG_E)
#endif
#ifndef Q2_OPEN_CLOSE_SESSION_2ND_GET_SSR
#define Q2_OPEN_CLOSE_SESSION_2ND_GET_SSR(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_OPEN_CLOSE_SESSION_2ND_GET_SSR_E)
#endif
#ifndef Q2_POLICY_AUTH_PROT_AC_BIT
#define Q2_POLICY_AUTH_PROT_AC_BIT(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_POLICY_AUTH_PROT_AC_BIT_E)
#else //not defined Q2_POLICY_AUTH_PROT_AC_BIT
#if (Q2_POLICY_AUTH_PROT_AC_BIT(qlibContext) == 0u)
#define EXCLUDE_Q2_POLICY_AUTH_PROT_AC_BIT
#endif //(Q2_POLICY_AUTH_PROT_AC_BIT(qlibContext) == 0u)
#endif //not defined Q2_POLICY_AUTH_PROT_AC_BIT
#ifndef Q2_VCC_3_3_V
#define Q2_VCC_3_3_V(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_VCC_3_3_V_E)
#endif
#ifndef Q2_LIMITED_SECT_SEL_MODES
#define Q2_LIMITED_SECT_SEL_MODES(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_LIMITED_SECT_SEL_MODES_E)
#endif
#ifdef QLIB_FLASH_SIZE
#define QLIB_GET_FLASH_SIZE(qlibContext) QLIB_FLASH_SIZE
#else
#define QLIB_GET_FLASH_SIZE(qlibContext) (((U32)1 << ((U32)(qlibContext)->detectedDeviceID + (U32)1)))
#endif
#ifndef W77Q_CMD_MANUFACTURER_AND_DEVICE_ID
#define W77Q_CMD_MANUFACTURER_AND_DEVICE_ID(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_CMD_MANUFACTURER_AND_DEVICE_ID_E)
#endif
#ifndef Q2_HOLD_PIN
#define Q2_HOLD_PIN(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_HOLD_PIN_E)
#else //not defined Q2_HOLD_PIN
#if (Q2_HOLD_PIN(qlibContext) == 0u)
#define EXCLUDE_Q2_HOLD_PIN
#endif //(Q2_HOLD_PIN(qlibContext) == 0u)
#endif //not defined Q2_HOLD_PIN
#ifndef Q2_QPI_SET_READ_PARAMS
#define Q2_QPI_SET_READ_PARAMS(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_QPI_SET_READ_PARAMS_E)
#endif
#ifndef W77Q_WID_PID
#define W77Q_WID_PID(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_WID_PID_E)
#endif
#ifndef W77Q_RST_RESP
#define W77Q_RST_RESP(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_RST_RESP_E)
#else //not defined W77Q_RST_RESP
#if (W77Q_RST_RESP(qlibContext) == 0u)
#define EXCLUDE_RST_RESP
#endif //(W77Q_RST_RESP(qlibContext) == 0u)
#endif //not defined W77Q_RST_RESP
#ifndef W77Q_CMD_GET_KEYS_STATUS
#define W77Q_CMD_GET_KEYS_STATUS(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_CMD_GET_KEYS_STATUS_E)
#else //not defined W77Q_CMD_GET_KEYS_STATUS
#if (W77Q_CMD_GET_KEYS_STATUS(qlibContext) == 0u)
#define EXCLUDE_CMD_GET_KEYS_STATUS
#endif //(W77Q_CMD_GET_KEYS_STATUS(qlibContext) == 0u)
#endif //not defined W77Q_CMD_GET_KEYS_STATUS
#ifndef W77Q_CMD_PA_GRANT_REVOKE
#define W77Q_CMD_PA_GRANT_REVOKE(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_CMD_PA_GRANT_REVOKE_E)
#else //not defined W77Q_CMD_PA_GRANT_REVOKE
#if (W77Q_CMD_PA_GRANT_REVOKE(qlibContext) == 0u)
#define EXCLUDE_CMD_PA_GRANT_REVOKE
#else
#define EXCLUDE_CMD_INIT_PA
#endif //(W77Q_CMD_PA_GRANT_REVOKE(qlibContext) == 0u)
#endif //not defined W77Q_CMD_PA_GRANT_REVOKE
#ifndef W77Q_VER_INTG_DIGEST
#define W77Q_VER_INTG_DIGEST(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_VER_INTG_DIGEST_E)
#endif
#ifndef W77Q_SUPPORT_DUAL_SPI
#define W77Q_SUPPORT_DUAL_SPI(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SUPPORT_DUAL_SPI_E)
#endif
#ifndef W77Q_SUPPORT__1_1_4_SPI
#define W77Q_SUPPORT__1_1_4_SPI(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SUPPORT__1_1_4_SPI_E)
#endif
#ifndef Q3_BYPASS_HW_ISSUE_282
#define Q3_BYPASS_HW_ISSUE_282(qlibContext) QLIB_CFG_EXIST((qlibContext), Q3_BYPASS_HW_ISSUE_282_E)
#endif
#ifndef W77Q_SUPPORT_OCTAL_SPI
#define W77Q_SUPPORT_OCTAL_SPI(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SUPPORT_OCTAL_SPI_E)
#endif
#ifndef W77Q_SEC_SIZE_SCALE
#define W77Q_SEC_SIZE_SCALE(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SEC_SIZE_SCALE_E)
#endif
#ifndef W77Q_SET_SCR_MODE
#define W77Q_SET_SCR_MODE(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SET_SCR_MODE_E)
#endif
#ifndef W77Q_SUPPORT_UNIQUE_ID_QPI
#define W77Q_SUPPORT_UNIQUE_ID_QPI(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SUPPORT_UNIQUE_ID_QPI_E)
#endif
#ifndef W77Q_EXTENDED_UNIQUE_ID_16B
#define W77Q_EXTENDED_UNIQUE_ID_16B(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_EXTENDED_UNIQUE_ID_16B_E)
#endif
#ifndef W77Q_HW_RPMC
#define W77Q_HW_RPMC(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_HW_RPMC_E)
#else //not defined W77Q_HW_RPMC
#if (W77Q_HW_RPMC(qlibContext) == 0u)
#define EXCLUDE_HW_RPMC
#endif //(W77Q_HW_RPMC(qlibContext) == 0u)
#endif //not defined W77Q_HW_RPMC
#ifndef W77Q_PRE_PROV_MASTER_KEY
#define W77Q_PRE_PROV_MASTER_KEY(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_PRE_PROV_MASTER_KEY_E)
#endif
#ifndef W77Q_FAST_READ_DUMMY_CONFIG
#define W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_FAST_READ_DUMMY_CONFIG_E)
#else //not defined W77Q_FAST_READ_DUMMY_CONFIG
#if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) == 0u)
#define EXCLUDE_FAST_READ_DUMMY_CONFIG
#endif //(W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) == 0u)
#endif //not defined W77Q_FAST_READ_DUMMY_CONFIG
#ifndef W77Q_SUPPORT_SSE
#define W77Q_SUPPORT_SSE(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SUPPORT_SSE_E)
#endif
#ifndef W77Q_BOOT_FAIL_RESET
#define W77Q_BOOT_FAIL_RESET(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_BOOT_FAIL_RESET_E)
#endif
#ifndef W77Q_RST_PA
#define W77Q_RST_PA(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_RST_PA_E)
#endif
#ifndef W77Q_DEVCFG_LOCK
#define W77Q_DEVCFG_LOCK(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_DEVCFG_LOCK_E)
#endif
#ifndef W77Q_DEVCFG_BOOT_FAIL_RST
#define W77Q_DEVCFG_BOOT_FAIL_RST(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_DEVCFG_BOOT_FAIL_RST_E)
#endif
#ifndef W77Q_VAULT
#define W77Q_VAULT(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_VAULT_E)
#endif
#ifndef W77Q_FORMAT_MODE
#define W77Q_FORMAT_MODE(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_FORMAT_MODE_E)
#endif
#ifndef W77Q_FLAG_REGISTER
#define W77Q_FLAG_REGISTER(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_FLAG_REGISTER_E)
#else //not defined W77Q_FLAG_REGISTER
#if (W77Q_FLAG_REGISTER(qlibContext) == 0u)
#define EXCLUDE_FLAG_REGISTER
#endif //(W77Q_FLAG_REGISTER(qlibContext) == 0u)
#endif //not defined W77Q_FLAG_REGISTER
#ifndef W77Q_EXTENDED_CONFIG_REGISTER
#define W77Q_EXTENDED_CONFIG_REGISTER(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_EXTENDED_CONFIG_REGISTER_E)
#else //not defined W77Q_EXTENDED_CONFIG_REGISTER
#if (W77Q_EXTENDED_CONFIG_REGISTER(qlibContext) == 0u)
#define EXCLUDE_EXTENDED_CONFIG_REGISTER
#endif //(W77Q_EXTENDED_CONFIG_REGISTER(qlibContext) == 0u)
#endif //not defined W77Q_EXTENDED_CONFIG_REGISTER
#ifndef W77Q_AWDT_VAL_SEC_FRACT
#define W77Q_AWDT_VAL_SEC_FRACT(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_AWDT_VAL_SEC_FRACT_E)
#endif
#ifndef W77Q_READ_BYPASS_MODE_BYTE
#define W77Q_READ_BYPASS_MODE_BYTE(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_READ_BYPASS_MODE_BYTE_E)
#endif
#ifndef Q2_DEVICE_ID_DUMMY
#define Q2_DEVICE_ID_DUMMY(qlibContext) QLIB_CFG_EXIST((qlibContext), Q2_DEVICE_ID_DUMMY_E)
#endif
#ifndef W77Q_READ_EXTENDED_ADDR_DUMMY
#define W77Q_READ_EXTENDED_ADDR_DUMMY(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_READ_EXTENDED_ADDR_DUMMY_E)
#endif
#ifndef W77Q_READ_SR_DUMMY
#define W77Q_READ_SR_DUMMY(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_READ_SR_DUMMY_E)
#endif
#ifndef W77Q_READ_JEDEC_QPI_DUMMY
#define W77Q_READ_JEDEC_QPI_DUMMY(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_READ_JEDEC_QPI_DUMMY_E)
#else
#define QLIB_KNOWN_READ_JEDEC_QPI_DUMMY
#endif
#ifndef W77Q_SUPPORT_LMS
#define W77Q_SUPPORT_LMS(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SUPPORT_LMS_E)
#else //not defined W77Q_SUPPORT_LMS
#if (W77Q_SUPPORT_LMS(qlibContext) == 0u)
#define EXCLUDE_LMS
#endif //(W77Q_SUPPORT_LMS(qlibContext) == 0u)
#endif //not defined W77Q_SUPPORT_LMS
#ifndef W77Q_SUPPORT_LMS_ATTESTATION
#define W77Q_SUPPORT_LMS_ATTESTATION(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SUPPORT_LMS_ATTESTATION_E)
#else //not defined W77Q_SUPPORT_LMS_ATTESTATION
#if (W77Q_SUPPORT_LMS_ATTESTATION(qlibContext) == 0u)
#define EXCLUDE_LMS_ATTESTATION
#endif //(W77Q_SUPPORT_LMS_ATTESTATION(qlibContext) == 0u)
#endif //not defined W77Q_SUPPORT_LMS_ATTESTATION
#ifndef W77Q_SECURE_LOG
#define W77Q_SECURE_LOG(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SECURE_LOG_E)
#else //not defined W77Q_SECURE_LOG
#if (W77Q_SECURE_LOG(qlibContext) == 0u)
#define EXCLUDE_SECURE_LOG
#endif //(W77Q_SECURE_LOG(qlibContext) == 0u)
#endif //not defined W77Q_SECURE_LOG
#ifndef W77Q_MEM_CRC
#define W77Q_MEM_CRC(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_MEM_CRC_E)
#else //not defined W77Q_MEM_CRC
#if (W77Q_MEM_CRC(qlibContext) == 0u)
#define EXCLUDE_MEM_CRC
#endif //(W77Q_MEM_CRC(qlibContext) == 0u)
#endif //not defined W77Q_MEM_CRC
#ifndef W77Q_MEM_COPY
#define W77Q_MEM_COPY(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_MEM_COPY_E)
#else //not defined W77Q_MEM_COPY
#if (W77Q_MEM_COPY(qlibContext) == 0u)
#define EXCLUDE_MEM_COPY
#endif //(W77Q_MEM_COPY(qlibContext) == 0u)
#endif //not defined W77Q_MEM_COPY
#ifndef W77Q_AWDTCFG_OSC_RATE_FRAC
#define W77Q_AWDTCFG_OSC_RATE_FRAC(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_AWDTCFG_OSC_RATE_FRAC_E)
#endif
#ifndef W77Q_ECC
#define W77Q_ECC(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_ECC_E)
#else //not defined W77Q_ECC
#if (W77Q_ECC(qlibContext) == 0u)
#define EXCLUDE_W77Q_ECC
#endif //(W77Q_ECC(qlibContext) == 0u)
#endif //not defined W77Q_ECC
#ifndef W77Q_SIG_MEM_RANGE
#define W77Q_SIG_MEM_RANGE(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_SIG_MEM_RANGE_E)
#endif
#ifndef W77Q_CONFIG_ZERO_SECTION_ALLOWED
#define W77Q_CONFIG_ZERO_SECTION_ALLOWED(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_CONFIG_ZERO_SECTION_ALLOWED_E)
#endif
#ifndef Q3_BYPASS_HW_ISSUE_OPEN_SESS_SYS_ERR
#define Q3_BYPASS_HW_ISSUE_OPEN_SESS_SYS_ERR(qlibContext) QLIB_CFG_EXIST((qlibContext), Q3_BYPASS_HW_ISSUE_OPEN_SESS_SYS_ERR_E)
#endif
#ifndef Q3_BYPASS_HW_ISSUE_SET_CMD_FAIL
#define Q3_BYPASS_HW_ISSUE_SET_CMD_FAIL(qlibContext) QLIB_CFG_EXIST((qlibContext), Q3_BYPASS_HW_ISSUE_SET_CMD_FAIL_E)
#endif
#ifndef Q3_BYPASS_HW_ISSUE_OCTAL_SESSION_OPEN_SECOND_SSR
#define Q3_BYPASS_HW_ISSUE_OCTAL_SESSION_OPEN_SECOND_SSR(qlibContext) \
    QLIB_CFG_EXIST((qlibContext), Q3_BYPASS_HW_ISSUE_OCTAL_SESSION_OPEN_SECOND_SSR_E)
#endif
#ifndef W77Q_MULTI_DIE
#define W77Q_MULTI_DIE(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_MULTI_DIE_E)
#endif
#ifndef W77Q_INTERRUPTS
#define W77Q_INTERRUPTS(qlibContext) QLIB_CFG_EXIST((qlibContext), W77Q_INTERRUPTS_E)
#elif (W77Q_INTERRUPTS(qlibContext) == 0u)
#define EXCLUDE_W77Q_INTERRUPTS
#endif
#ifndef Q3_BYPASS_HW_ISSUE_QPI_POWER_DOWN_RESP_ERR
#define Q3_BYPASS_HW_ISSUE_QPI_POWER_DOWN_RESP_ERR(qlibContext) \
    QLIB_CFG_EXIST((qlibContext), Q3_BYPASS_HW_ISSUE_QPI_POWER_DOWN_RESP_ERR_E)
#endif
#ifndef W77Q_DEDICATED_RESET_INPUT_ENABLE_DEFAULT
#define W77Q_DEDICATED_RESET_INPUT_ENABLE_DEFAULT(qlibContext) \
    QLIB_CFG_EXIST(qlibContext, W77Q_DEDICATED_RESET_INPUT_ENABLE_DEFAULT_E)
#endif

/************************************************************************************************************
 * @brief       This function initializes the features bit array according to detected device,
 *              and verify the detected device match the compilation target
 *
 * @param[out]  qlibContext         [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]   hwVer               flash hardware version
 *
 * @return
 * QLIB_STATUS__OK = 0                      - no error occurred\n
 * QLIB_STATUS__INVALID_PARAMETER           - @p qlibContext is NULL\n
 * QLIB_STATUS__NOT_SUPPORTED               - detected device not supported by QLIB\n
 * QLIB_STATUS__(ERROR)                     - Other error
************************************************************************************************************/
QLIB_STATUS_T QLIB_Cfg_Init(struct QLIB_CONTEXT_T* qlibContext, const struct QLIB_HW_VER_T* hwVer);

#ifdef __cplusplus
}
#endif

#endif // __QLIB_CFG_H__
