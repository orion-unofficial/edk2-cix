/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_tm.c
* @brief      This file contains QLIB Transaction Manager (TM) implementation
*
* ### project qlib
*
************************************************************************************************************/
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                              DEFINITIONS                                                */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                       LOCAL FUNCTION DECLARATIONS                                       */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_Cfg_Init(struct QLIB_CONTEXT_T* qlibContext, const struct QLIB_HW_VER_T* hwVer)
{
    U32 qlibTarget;

    QLIB_ASSERT_RET(hwVer != NULL, QLIB_STATUS__INVALID_PARAMETER);
    qlibTarget = hwVer->info.target;
    QLIB_ASSERT_RET((QLIB_TARGET_SUPPORTED(qlibTarget) != 0u), QLIB_STATUS__NOT_SUPPORTED);

    (void)memset(qlibContext->cfgBitArr, 0, sizeof(qlibContext->cfgBitArr));

    if ((qlibTarget & all_q2_targets) != 0u)
    {
        // W77Q (16Mb to 128Mb) flash
        QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_263_E);
        QLIB_CFG_ADD(qlibContext, W77Q_SUPPORT_DUAL_SPI_E);
        QLIB_CFG_ADD(qlibContext, W77Q_SUPPORT__1_1_4_SPI_E);
        QLIB_CFG_ADD(qlibContext, W77Q_CMD_MANUFACTURER_AND_DEVICE_ID_E);
        QLIB_CFG_ADD(qlibContext, W77Q_WID_PID_E);
        QLIB_CFG_ADD(qlibContext, W77Q_RST_RESP_E);
        QLIB_CFG_ADD(qlibContext, W77Q_READ_BYPASS_MODE_BYTE_E);
        QLIB_CFG_ADD(qlibContext, Q2_DEVICE_ID_DUMMY_E);
        QLIB_CFG_ADD(qlibContext, W77Q_AWDTCFG_OSC_RATE_FRAC_E);
        QLIB_CFG_ADD(qlibContext, Q2_QPI_SET_READ_PARAMS_E);

        if (qlibTarget == w77q32jw_revB)
        {
            // W77Q (16Mb to 128Mb) MCD (32Mb) features
            QLIB_CFG_ADD(qlibContext, Q2_SESSION_CLOSE_NOT_SUPPORTED_E);
            QLIB_CFG_ADD(qlibContext, Q2_OP0_OP2_DTR_FEATURE_E);
            QLIB_CFG_ADD(qlibContext, Q2_LIMITED_SECT_SEL_MODES_E);
            // W77Q (16Mb to 128Mb) MCD rev B/C features
            QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_23_E);
            QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_49_E);
            QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_60_E);
            QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_96_E);
            QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_98_E);
            QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_105_E);
            QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_294_E);
        }
        else if ((qlibTarget == w77q128jv_revA) || (qlibTarget == w77q64jv_revA) || (qlibTarget == w77q128jw_revA) ||
                 (qlibTarget == w77q64jw_revA))
        {
            // W77Q (16Mb to 128Mb) HCD (64Mb/128Mb) features
            QLIB_CFG_ADD(qlibContext, Q2_RESTRICTED_ACTIVATION_KID_E);
            QLIB_CFG_ADD(qlibContext, Q2_EXTEND_SEC_LEN_BITS_E);
            QLIB_CFG_ADD(qlibContext, Q2_4_BYTES_ADDRESS_MODE_E);
            QLIB_CFG_ADD(qlibContext, Q2_DEVCFG_CTAG_MODE_E);
            QLIB_CFG_ADD(qlibContext, Q2_SUPPORT_AWDTCFG_FALLBACK_E);
            QLIB_CFG_ADD(qlibContext, Q2_TC_30_BIT_E);
            QLIB_CFG_ADD(qlibContext, Q2_ESSR_FEATURE_E);
            QLIB_CFG_ADD(qlibContext, Q2_CMVP_BIST_FEATURE_E);
            QLIB_CFG_ADD(qlibContext, Q2_SPLIT_IBUF_FEATURE_E);
            QLIB_CFG_ADD(qlibContext, W77Q_SPLIT_IBUF_CTAG_ONLY_FIRST_E);
            QLIB_CFG_ADD(qlibContext, W77Q_RNG_FEATURE_E);
            QLIB_CFG_ADD(qlibContext, W77Q_EXTENDED_ADDR_REG_E);
            QLIB_CFG_ADD(qlibContext, Q2_OPEN_CLOSE_SESSION_2ND_GET_SSR_E);

            if ((qlibTarget == w77q128jv_revA) || (qlibTarget == w77q64jv_revA))
            {
                // HCD 3.3 features
                QLIB_CFG_ADD(qlibContext, Q2_VCC_3_3_V_E);
                QLIB_CFG_ADD(qlibContext, Q2_POLICY_AUTH_PROT_AC_BIT_E);
                // HCD 3.3 rev A features

                QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_49_E);
                QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_60_E);
                QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_265_E);
            }
            else
            {
                // HCD 1.8 rev A features
                QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_49_E);
                QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_60_E);
                QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_262_E);
                QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_265_E);
                QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_294_E);
                QLIB_CFG_ADD(qlibContext, Q2_BYPASS_HW_ISSUE_289_E);
            }
        }
        else
        {
            return QLIB_STATUS__NOT_SUPPORTED;
        }
    }
    else if ((qlibTarget & all_q3_targets) != 0u)
    {
        // W77Q/T (256Mb to 1Gb) chip
        QLIB_CFG_ADD(qlibContext, Q2_RESTRICTED_ACTIVATION_KID_E);
        QLIB_CFG_ADD(qlibContext, Q2_EXTEND_SEC_LEN_BITS_E);
        QLIB_CFG_ADD(qlibContext, Q2_4_BYTES_ADDRESS_MODE_E);
        QLIB_CFG_ADD(qlibContext, Q2_SUPPORT_AWDTCFG_FALLBACK_E);
        QLIB_CFG_ADD(qlibContext, Q2_TC_30_BIT_E);
        QLIB_CFG_ADD(qlibContext, Q2_ESSR_FEATURE_E);
        QLIB_CFG_ADD(qlibContext, W77Q_ESSR_KID_MSB_E);
        QLIB_CFG_ADD(qlibContext, Q2_CMVP_BIST_FEATURE_E);
        QLIB_CFG_ADD(qlibContext, Q2_SPLIT_IBUF_FEATURE_E);
        QLIB_CFG_ADD(qlibContext, W77Q_RNG_FEATURE_E);
        QLIB_CFG_ADD(qlibContext, W77Q_EXTENDED_ADDR_REG_E);
        QLIB_CFG_ADD(qlibContext, Q2_POLICY_AUTH_PROT_AC_BIT_E);
        QLIB_CFG_ADD(qlibContext, W77Q_SEC_SIZE_SCALE_E);
        QLIB_CFG_ADD(qlibContext, W77Q_SET_SCR_MODE_E);
        QLIB_CFG_ADD(qlibContext, W77Q_SUPPORT_UNIQUE_ID_QPI_E);
        QLIB_CFG_ADD(qlibContext, W77Q_EXTENDED_UNIQUE_ID_16B_E);
        QLIB_CFG_ADD(qlibContext, W77Q_HW_RPMC_E);
        QLIB_CFG_ADD(qlibContext, W77Q_PRE_PROV_MASTER_KEY_E);
        QLIB_CFG_ADD(qlibContext, W77Q_FAST_READ_DUMMY_CONFIG_E);
        QLIB_CFG_ADD(qlibContext, W77Q_RST_PA_E);
        QLIB_CFG_ADD(qlibContext, W77Q_DEVCFG_LOCK_E);
        QLIB_CFG_ADD(qlibContext, W77Q_VAULT_E);
        QLIB_CFG_ADD(qlibContext, Q3_BYPASS_HW_ISSUE_282_E);
        QLIB_CFG_ADD(qlibContext, W77Q_FORMAT_MODE_E);
        QLIB_CFG_ADD(qlibContext, W77Q_FLAG_REGISTER_E);
        QLIB_CFG_ADD(qlibContext, W77Q_EXTENDED_CONFIG_REGISTER_E);
        QLIB_CFG_ADD(qlibContext, W77Q_CMD_GET_KEYS_STATUS_E);
        QLIB_CFG_ADD(qlibContext, W77Q_CMD_PA_GRANT_REVOKE_E);
        QLIB_CFG_ADD(qlibContext, W77Q_VER_INTG_DIGEST_E);
        QLIB_CFG_ADD(qlibContext, W77Q_AWDT_VAL_SEC_FRACT_E);
        QLIB_CFG_ADD(qlibContext, W77Q_READ_EXTENDED_ADDR_DUMMY_E);
        QLIB_CFG_ADD(qlibContext, W77Q_READ_JEDEC_QPI_DUMMY_E);
        QLIB_CFG_ADD(qlibContext, W77Q_READ_SR_DUMMY_E);
        QLIB_CFG_ADD(qlibContext, W77Q_SUPPORT_LMS_E);
        QLIB_CFG_ADD(qlibContext, W77Q_SUPPORT_LMS_ATTESTATION_E);
        QLIB_CFG_ADD(qlibContext, W77Q_SECURE_LOG_E);
        QLIB_CFG_ADD(qlibContext, W77Q_MEM_CRC_E);
        QLIB_CFG_ADD(qlibContext, W77Q_MEM_COPY_E);
        QLIB_CFG_ADD(qlibContext, W77Q_SIG_MEM_RANGE_E);
        QLIB_CFG_ADD(qlibContext, W77Q_CONFIG_ZERO_SECTION_ALLOWED_E);
        QLIB_CFG_ADD(qlibContext, W77Q_MULTI_DIE_E);
        QLIB_CFG_ADD(qlibContext, W77Q_INTERRUPTS_E);
        QLIB_CFG_ADD(qlibContext, Q3_BYPASS_HW_ISSUE_OPEN_SESS_SYS_ERR_E);
        QLIB_CFG_ADD(qlibContext, Q3_BYPASS_HW_ISSUE_SET_CMD_FAIL_E);
        QLIB_CFG_ADD(qlibContext, Q3_BYPASS_HW_ISSUE_OCTAL_SESSION_OPEN_SECOND_SSR_E);
        QLIB_CFG_ADD(qlibContext, Q3_BYPASS_HW_ISSUE_QPI_POWER_DOWN_RESP_ERR_E);
        QLIB_CFG_ADD(qlibContext, W77Q_DEDICATED_RESET_INPUT_ENABLE_DEFAULT_E);
        if ((qlibTarget == w77t25nw_Ind_revB) || (qlibTarget == w77t25nw_Auto_revB))
        {
            // W77T : Octal SPI secure flash
            QLIB_CFG_ADD(qlibContext, W77Q_SUPPORT_OCTAL_SPI_E);
        }
        if ((qlibTarget == w77q25nw_Auto_revB) || (qlibTarget == w77t25nw_Auto_revB))
        {
            // W77Q/T (256Mb to 1Gb) chip W77Q/T automotive

            QLIB_CFG_ADD(qlibContext, W77Q_ECC_E);
        }
    }
    else
    {
        return QLIB_STATUS__NOT_SUPPORTED;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
#ifdef QLIB_MAX_SPI_OUTPUT_SIZE
    QLIB_ASSERT_RET((W77Q_SPLIT_IBUF_CTAG_ONLY_FIRST(qlibContext) != 0u) ||
                        (((U32)QLIB_MAX_SPI_OUTPUT_SIZE >= (W77Q_CTAG_SIZE_BYTE + 4u) &&
                          (((U32)QLIB_MAX_SPI_OUTPUT_SIZE % 4u) == 0u))),
                    QLIB_STATUS__NOT_SUPPORTED);
#endif

    return QLIB_STATUS__OK;
}
