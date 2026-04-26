/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_cmd_proc.c
* @brief      Command processor: This file handles processing of flash API commands
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
#include "qlib_platform.h"
#include <trace.h>
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 DEFINE                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_SEC_DMC_MAX_VAL             0x3FFFFFFFu
#define QLIB_SEC_TC_MAX_VAL(qlibContext) ((Q2_TC_30_BIT(qlibContext) != 0u) ? 0x3FFFFFFFu : 0xFFFFFFFFu)
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 MACROS                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

#define QLIB_CMD_PROC_build_encryption_key(qlibContext_p, cipherContext)         \
    QLIB_CRYPTO_BuildCipherKey((cipherContext)->hashBuf,                         \
                               QLIB_ACTIVE_DIE_STATE(qlibContext_p).keyMngr.kid, \
                               DECRYPTION_OF_INPUT_DIR_CODE,                     \
                               (cipherContext)->cipherKey);

#define QLIB_CMD_PROC_build_decryption_key(qlibContext_p, cipherContext)         \
    QLIB_CRYPTO_BuildCipherKey((cipherContext)->hashBuf,                         \
                               QLIB_ACTIVE_DIE_STATE(qlibContext_p).keyMngr.kid, \
                               ENCRYPTION_OF_OUTPUT_DIR_CODE,                    \
                               (cipherContext)->cipherKey);

#ifdef QLIB_HASH_OPTIMIZATION_ENABLED
#define QLIB_CMD_PROC_build_decryption_key_async(qlibContext_p, cipherContext)         \
    QLIB_CRYPTO_BuildCipherKey_Async(qlibContext_p,                                    \
                                     (cipherContext)->hashBuf,                         \
                                     QLIB_ACTIVE_DIE_STATE(qlibContext_p).keyMngr.kid, \
                                     ENCRYPTION_OF_OUTPUT_DIR_CODE,                    \
                                     (cipherContext)->cipherKey);
#else
#define QLIB_CMD_PROC_build_decryption_key_async(qlibContext_p, cipherContext) \
    QLIB_CMD_PROC_build_decryption_key(qlibContext_p, cipherContext)
#endif

#define QLIB_CMD_PROC_initialize_decryption_key(qlibContext_p, cipherContext) \
    {                                                                         \
        (cipherContext) = &QLIB_KEY_MNGR__CMD_CONTEXT_GET(qlibContext_p);     \
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_refresh_ssk_L(qlibContext_p));    \
        QLIB_CMD_PROC_build_decryption_key(qlibContext_p, (cipherContext));   \
    }


#define QLIB_CMD_PROC_update_decryption_key_async(qlibContext_p, cipherContext)                       \
    {                                                                                                 \
        QLIB_CRYPTO_put_salt_on_session_key(QLIB_ACTIVE_DIE_STATE(qlibContext_p).mc[TC],              \
                                            QLIB_HASH_BUF_GET__KEY((cipherContext)->hashBuf),         \
                                            QLIB_ACTIVE_DIE_STATE(qlibContext_p).keyMngr.sessionKey); \
        QLIB_CMD_PROC_build_decryption_key_async(qlibContext_p, (cipherContext));                     \
    }

#ifdef QLIB_HASH_OPTIMIZATION_ENABLED
#define QLIB_CMD_PROC_update_decryption_key_async_wait_till_ready(qlibContext_p) QLIB_HASH_Async_WaitWhileBusy(qlibContext_p)
#else
#define QLIB_CMD_PROC_update_decryption_key_async_wait_till_ready(qlibContext_p) (QLIB_STATUS__OK)
#endif

#define QLIB_CMD_PROC_encrypt_address(enc_addr, addr, key)         \
    {                                                              \
        (enc_addr) = (addr) ^ (QLIB_CRYPTO_CreateAddressKey(key)); \
        (enc_addr) = Q2_SWAP_24BIT_ADDR_FOR_SPI(enc_addr);         \
    }

#ifdef QLIB_SIGN_DATA_BY_FLASH
#define QLIB_CMD_PROC_decrypt_address(addr, enc_addr, key)     \
    {                                                          \
        (addr) = Q2_SWAP_24BIT_ADDR_FOR_SPI(enc_addr);         \
        (addr) = (addr) ^ (QLIB_CRYPTO_CreateAddressKey(key)); \
        (addr) = (addr)&0xFFFFFFu;                             \
    }
#endif

#define QLIB_CMD_PROC_execute_sec_cmd(qlibContext, ctag, writeData, writeDataSize, readData, readDataSize, ssr) \
    QLIB_TM_Secure(qlibContext, ctag, writeData, writeDataSize, readData, readDataSize, ssr)

#define QLIB_CMD_PROC_execute_sec_cmd_write_read(qlibContext, ctag, writeData, writeDataSize, readData, readDataSize) \
    QLIB_CMD_PROC_execute_sec_cmd(qlibContext,                                                                        \
                                  ctag,                                                                               \
                                  writeData,                                                                          \
                                  writeDataSize,                                                                      \
                                  readData,                                                                           \
                                  readDataSize,                                                                       \
                                  &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr)

#define QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext, ctag) \
    QLIB_CMD_PROC_execute_sec_cmd_write_read(qlibContext, ctag, NULL, 0, NULL, 0)

#define QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext, ctag, writeData, writeDataSize) \
    QLIB_CMD_PROC_execute_sec_cmd_write_read(qlibContext, ctag, writeData, writeDataSize, NULL, 0)

#define QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, ctag, readData, readDataSize) \
    QLIB_CMD_PROC_execute_sec_cmd_write_read(qlibContext, ctag, NULL, 0, readData, readDataSize)

#define QLIB_CMD_PROC__OP1_only(qlibContext, ctag)                QLIB_CMD_PROC_execute_sec_cmd(qlibContext, ctag, NULL, 0, NULL, 0, NULL)
#define QLIB_CMD_PROC__OP0_busy_wait(qlibContext)                 QLIB_CMD_PROC_execute_sec_cmd_write_read(qlibContext, 0, NULL, 0, NULL, 0)
#define QLIB_CMD_PROC__OP0_busy_wait_OP2(qlibContext, data, size) QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, 0, data, size)

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                       LOCAL FUNCTION DECLARATIONS                                       */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
static QLIB_STATUS_T QLIB_CMD_PROC_use_mc_L(QLIB_CONTEXT_T* qlibContext, QLIB_MC_T mc);
static QLIB_STATUS_T QLIB_CMD_PROC_refresh_ssk_L(QLIB_CONTEXT_T* qlibContext);

static QLIB_STATUS_T QLIB_CMD_PROC__sign_data_L(QLIB_CONTEXT_T* qlibContext,
                                                U32             plain_ctag,
                                                const U32*      data_up_to256bit,
                                                U32             data_size,
                                                U32*            signature,
                                                BOOL            refresh_ssk);

static QLIB_STATUS_T QLIB_CMD_PROC__prepare_signed_setter_cmd_L(QLIB_CONTEXT_T* qlibContext,
                                                                U32*            ctag,
                                                                const U32*      data,
                                                                U32             data_size,
                                                                U32*            signature,
                                                                U32*            encrypted_data,
                                                                U32*            addr);

static QLIB_STATUS_T QLIB_CMD_PROC__execute_signed_setter_L(QLIB_CONTEXT_T* qlibContext,
                                                            U32             ctag,
                                                            const U32*      data,
                                                            U32             data_size,
                                                            BOOL            encrypt_data,
                                                            U32*            addr);

static QLIB_STATUS_T QLIB_CMD_PROC__Session_Close_L(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, BOOL revokePA);
static QLIB_STATUS_T QLIB_CMD_PROC__Session_Close_Bypass_L(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, BOOL revokePA);

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------*/
/*                                            STATUS COMMANDS                                              */
/*---------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_CMD_PROC__get_SSR_UNSIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_REG_SSR_T* ssr, U32 mask)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_NONE)));

    if (ssr != NULL)
    {
        ssr->asUint = QLIB_GET_LAST_SEC_STATUS_FIELD(qlibContext);
    }
    return QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, mask);
}

QLIB_STATUS_T QLIB_CMD_PROC__get_SSR_SIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_REG_SSR_T* ssr, U32 mask)
{
    QLIB_REG_SSR_T ssrHw;
    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC__CALC_SIG(qlibContext, QLIB_SIGNED_DATA_TYPE_SSR, 0, NULL, 0, &ssrHw.asUint, sizeof(QLIB_REG_SSR_T), NULL));
    if (ssr != NULL)
    {
        ssr->asUint = ssrHw.asUint;
    }
    return QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, mask);
}

QLIB_STATUS_T QLIB_CMD_PROC__get_ESSR_UNSIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_REG_ESSR_T* essr)
{
    QLIB_STATUS_T ret;

    if (Q2_ESSR_FEATURE(qlibContext) != 0u)
    {
        QLIB_ASSERT_RET(essr != NULL, QLIB_STATUS__INVALID_PARAMETER);
        ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                                 QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_ESSR),
                                                 &essr->asUint32,
                                                 sizeof(QLIB_REG_ESSR_T));
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

        return ret;
    }
    else
    {
        return QLIB_STATUS__NOT_SUPPORTED;
    }
}

QLIB_STATUS_T QLIB_CMD_PROC__get_ESSR_SIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_REG_ESSR_T* essr)
{
    QLIB_REG_ESSR_T essrHw;

    if (Q2_ESSR_FEATURE(qlibContext) != 0u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__CALC_SIG(qlibContext,
                                                      QLIB_SIGNED_DATA_TYPE_ESSR,
                                                      0,
                                                      NULL,
                                                      0,
                                                      &essrHw.asUint64,
                                                      sizeof(QLIB_REG_ESSR_T),
                                                      NULL));
        if (essr != NULL)
        {
            essr->asUint64 = essrHw.asUint64;
        }
        return QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS);
    }
    else
    {
        return QLIB_STATUS__NOT_SUPPORTED;
    }
}

QLIB_STATUS_T QLIB_CMD_PROC__get_WID_UNSIGNED(QLIB_CONTEXT_T* qlibContext, _64BIT WID)
{
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;

    QLIB_ASSERT_RET(WID != NULL, QLIB_STATUS__INVALID_PARAMETER);
    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_WID), WID, sizeof(_64BIT));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_WID_SIGNED(QLIB_CONTEXT_T* qlibContext, _64BIT WID)
{
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;
    QLIB_ASSERT_RET(WID != NULL, QLIB_STATUS__INVALID_PARAMETER);
    ret = QLIB_CMD_PROC__CALC_SIG(qlibContext, QLIB_SIGNED_DATA_TYPE_WID, 0, NULL, 0, WID, sizeof(_64BIT), NULL);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}


QLIB_STATUS_T QLIB_CMD_PROC__get_SUID_UNSIGNED(QLIB_CONTEXT_T* qlibContext, _128BIT SUID)
{
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;

    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_SUID), SUID, sizeof(_128BIT));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_SUID_SIGNED(QLIB_CONTEXT_T* qlibContext, _128BIT SUID)
{
    QLIB_STATUS_T ret = QLIB_CMD_PROC__CALC_SIG(qlibContext, QLIB_SIGNED_DATA_TYPE_SUID, 0, NULL, 0, SUID, sizeof(_128BIT), NULL);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_AWDTSR(QLIB_CONTEXT_T* qlibContext, AWDTSR_T* AWDTSR)
{
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;

    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                             QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_AWDTSR),
                                             AWDTSR,
                                             sizeof(AWDTSR_T));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}


/*---------------------------------------------------------------------------------------------------------*/
/*                                         CONFIGURATION COMMANDS                                          */
/*---------------------------------------------------------------------------------------------------------*/


QLIB_STATUS_T QLIB_CMD_PROC__SFORMAT(QLIB_CONTEXT_T* qlibContext, BOOL modeReset, BOOL modeDefault, BOOL modeInit)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* OP1, CTAG (32b), SIG (64b)                                                                          */
    /* CTAG = CMD (8b),MODE(8b), 16'b0                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    U8 mode = 0;
    QLIB_SEC_CMD_FORMAT_MODE_SET(mode, modeReset, modeDefault, modeInit);

    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid == (QLIB_KID_T)QLIB_KID__DEVICE_MASTER,
                    QLIB_STATUS__COMMAND_IGNORED);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext,
                                                                 QLIB_CMD_PROC__MAKE_CTAG_MODE(QLIB_CMD_SEC_SFORMAT, mode),
                                                                 NULL,
                                                                 0,
                                                                 FALSE,
                                                                 NULL));
    // SYS_ERR is set when configuration registers are invalid after HW reset
    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC__checkLastSsrErrors(qlibContext,
                                          (modeReset == TRUE && modeDefault == FALSE)
                                              ? (SSR_MASK__ALL_ERRORS & ((U32)(~MASK_FIELD(QLIB_REG_SSR__SYS_ERR_S))))
                                              : SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__FORMAT(QLIB_CONTEXT_T* qlibContext, BOOL modeReset, BOOL modeDefault)
{
    U32 ver  = 0xA5A5A5A5u;
    U8  mode = 0;
    QLIB_SEC_CMD_FORMAT_MODE_SET(mode, modeReset, modeDefault, 0);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext,
                                                              QLIB_CMD_PROC__MAKE_CTAG_MODE(QLIB_CMD_SEC_FORMAT, mode),
                                                              &ver,
                                                              sizeof(U32)));

    // SYS_ERR is set when configuration registers are invalid after HW reset
    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC__checkLastSsrErrors(qlibContext,
                                          (modeReset == TRUE && modeDefault == FALSE)
                                              ? (SSR_MASK__ALL_ERRORS & ((U32)(~MASK_FIELD(QLIB_REG_SSR__SYS_ERR_S))))
                                              : SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_KEY(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, const KEY_T key_buff)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* OP1, CTAG (32b), Encrypted-Key (128b), SIG (64b)                                                    */
    /* CTAG = CMD (8b), KID (8b), 16'b0                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    U32 ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_SET_KEY, kid, 0, 0);

    /*-----------------------------------------------------------------------------------------------------*/
    /* set key is never done on provisioning key id                                                        */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(!QLIB_KEY_ID_IS_PROVISIONING(kid), QLIB_STATUS__COMMAND_IGNORED);

    /*-----------------------------------------------------------------------------------------------------*/
    /* check if the session opened is provisioning session                                                 */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(QLIB_KEY_ID_IS_PROVISIONING(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid), QLIB_STATUS__COMMAND_IGNORED);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Execute command                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext, ctag, key_buff, sizeof(_128BIT), TRUE, NULL));
    if (W77Q_CMD_GET_KEYS_STATUS(qlibContext) == 1u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    }
    else
    {
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, (U32)SSR_MASK__IGNORE_IGNORE_ERR));
    }
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_SUID(QLIB_CONTEXT_T* qlibContext, const _128BIT SUID)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext,
                                                                 QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_SET_SUID),
                                                                 SUID,
                                                                 sizeof(_128BIT),
                                                                 FALSE,
                                                                 NULL));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_GMC(QLIB_CONTEXT_T* qlibContext, const GMC_T GMC)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* OP1, CTAG (32b), GMC (160b), SIG (64b                                                               */
    /* CTAG = CMD (8b), 24'b0                                                                              */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid == (QLIB_KID_T)QLIB_KID__DEVICE_MASTER,
                    QLIB_STATUS__COMMAND_IGNORED);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext,
                                                                 QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_SET_GMC),
                                                                 GMC,
                                                                 sizeof(GMC_T),
                                                                 FALSE,
                                                                 NULL));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_GMC_UNSIGNED(QLIB_CONTEXT_T* qlibContext, GMC_T GMC)
{
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;

    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_GMC), GMC, sizeof(GMC_T));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_GMC_SIGNED(QLIB_CONTEXT_T* qlibContext, GMC_T GMC)
{
    QLIB_STATUS_T ret = QLIB_CMD_PROC__CALC_SIG(qlibContext, QLIB_SIGNED_DATA_TYPE_GMC, 0, NULL, 0, GMC, sizeof(GMC_T), NULL);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_GMT(QLIB_CONTEXT_T* qlibContext, const GMT_T* pGMT)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* OP1, CTAG (32b), GMT (160b), SIG (64b)                                                              */
    /* CTAG = CMD (8b), 24'b0                                                                              */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid == (QLIB_KID_T)QLIB_KID__DEVICE_MASTER,
                    QLIB_STATUS__COMMAND_IGNORED);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext,
                                                                 QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_SET_GMT),
                                                                 pGMT->asArray,
                                                                 sizeof(GMT_T),
                                                                 FALSE,
                                                                 NULL));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_GMT_UNSIGNED(QLIB_CONTEXT_T* qlibContext, GMT_T* pGMT)
{
    QLIB_STATUS_T ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                                           QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_GMT),
                                                           pGMT->asArray,
                                                           sizeof(GMT_T));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_GMT_SIGNED(QLIB_CONTEXT_T* qlibContext, GMT_T* pGMT)
{
    QLIB_STATUS_T ret =
        QLIB_CMD_PROC__CALC_SIG(qlibContext, QLIB_SIGNED_DATA_TYPE_GMT, 0, NULL, 0, pGMT->asArray, sizeof(GMT_T), NULL);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_AWDT(QLIB_CONTEXT_T* qlibContext, AWDTCFG_T AWDTCFG)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* OP1, CTAG (32b), AWDTCFG (32b), SIG                                                                 */
    /* CMD (8b), 24'b0                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext,
                                                                 QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_SET_AWDT),
                                                                 &AWDTCFG,
                                                                 sizeof(AWDTCFG_T),
                                                                 FALSE,
                                                                 NULL));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_AWDT_UNSIGNED(QLIB_CONTEXT_T* qlibContext, AWDTCFG_T* AWDTCFG)
{
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;

    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                             QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_AWDT),
                                             AWDTCFG,
                                             sizeof(AWDTCFG_T));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_AWDT_SIGNED(QLIB_CONTEXT_T* qlibContext, AWDTCFG_T* AWDTCFG)
{
    QLIB_STATUS_T ret =
        QLIB_CMD_PROC__CALC_SIG(qlibContext, QLIB_SIGNED_DATA_TYPE_AWDTCFG, 0, NULL, 0, AWDTCFG, sizeof(AWDTCFG_T), NULL);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__AWDT_TOUCH(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext,
                                                                 QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_AWDT_TOUCH),
                                                                 NULL,
                                                                 0,
                                                                 FALSE,
                                                                 NULL));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_AWDT_PLAIN(QLIB_CONTEXT_T* qlibContext, AWDTCFG_T AWDTCFG)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext,
                                                              QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_SET_AWDT_PLAIN),
                                                              &AWDTCFG,
                                                              sizeof(AWDTCFG_T)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__AWDT_TOUCH_PLAIN(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_AWDT_TOUCH_PLAIN)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_SCRn(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex, const SCRn_T SCRn, BOOL reset, BOOL reload)
{
    U8  mode = 0;
    U32 ctag;

    QLIB_SEC_CMD_SET_SCR_MODE_SET(mode, reset, reload);

    ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_SET_SCR, (U8)sectionIndex, mode, 0);

    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid ==
                        QLIB_KEY_MNGR__KID_WITH_SECTION(QLIB_KID__FULL_ACCESS_SECTION, sectionIndex),
                    QLIB_STATUS__COMMAND_IGNORED);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext, ctag, SCRn, sizeof(SCRn_T), FALSE, NULL));

#if defined(QLIB_MAX_SPI_OUTPUT_SIZE)
    if ((Q2_BYPASS_HW_ISSUE_289(qlibContext) != 0u) && (Q2_SPLIT_IBUF_FEATURE(qlibContext) != 0u))
    {
        SCRn_T        testSCRn;
        QLIB_STATUS_T err = QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS);
        if (err == QLIB_STATUS__DEVICE_SESSION_ERR)
        {
            QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_SCRn_UNSIGNED(qlibContext, sectionIndex, testSCRn));
            QLIB_ASSERT_RET(memcmp(SCRn, testSCRn, sizeof(SCRn_T)) == 0, err);
        }
        else
        {
            QLIB_STATUS_RET_CHECK(err);
        }
    }
#endif
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_SCRn_swap(QLIB_CONTEXT_T* qlibContext,
                                           U32             sectionIndex,
                                           const SCRn_T    SCRn,
                                           BOOL            reset,
                                           BOOL            reload)
{
    U8  mode = 0;
    U32 ctag;

    QLIB_SEC_CMD_SET_SCR_MODE_SET(mode, reset, reload);
    ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_SET_SCR_SWAP, (U8)sectionIndex, mode, 0);

    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid ==
                        QLIB_KEY_MNGR__KID_WITH_SECTION(QLIB_KID__FULL_ACCESS_SECTION, sectionIndex),
                    QLIB_STATUS__COMMAND_IGNORED);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext, ctag, SCRn, sizeof(SCRn_T), FALSE, NULL));

#if defined(QLIB_MAX_SPI_OUTPUT_SIZE)
    if ((Q2_BYPASS_HW_ISSUE_289(qlibContext) != 0u) && (Q2_SPLIT_IBUF_FEATURE(qlibContext) != 0u))
    {
        SCRn_T        testSCRn;
        QLIB_STATUS_T err = QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS);
        if (err == QLIB_STATUS__DEVICE_SESSION_ERR)
        {
            QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_SCRn_UNSIGNED(qlibContext, sectionIndex, testSCRn));
            QLIB_ASSERT_RET(memcmp(SCRn, testSCRn, sizeof(SCRn_T)) == 0, err);
        }
        else
        {
            QLIB_STATUS_RET_CHECK(err);
        }
    }
#endif
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_SCRn_UNSIGNED(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex, SCRn_T SCRn)
{
    U32           ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_GET_SCR, (U8)sectionIndex, 0, 0);
    QLIB_STATUS_T ret  = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, ctag, SCRn, sizeof(SCRn_T));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_SCRn_SIGNED(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex, SCRn_T SCRn)
{
    QLIB_STATUS_T ret = QLIB_CMD_PROC__CALC_SIG(qlibContext,
                                                QLIB_SIGNED_DATA_TYPE_SECTION_CONFIG,
                                                sectionIndex,
                                                NULL,
                                                0,
                                                SCRn,
                                                sizeof(SCRn_T),
                                                NULL);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_RST_RESP(QLIB_CONTEXT_T* qlibContext, BOOL is_RST_RESP1, const U32* RST_RESP_half)
{
    _256BIT hash_output;
    U32     dataOutBuf[(_64B_ + sizeof(_64BIT)) / sizeof(U32)]; // RST_RESP_half + signature
    U32     ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_SET_RST_RESP, is_RST_RESP1 == TRUE ? 0 : 1, 0, 0);

    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid == (U8)QLIB_KID__DEVICE_MASTER, QLIB_STATUS__COMMAND_IGNORED);
    QLIB_STATUS_RET_CHECK(QLIB_HASH(hash_output, RST_RESP_half, _64B_));
    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC__sign_data_L(qlibContext, ctag, hash_output, sizeof(_256BIT), &dataOutBuf[_64B_ / sizeof(U32)], TRUE));
    (void)memcpy(dataOutBuf, RST_RESP_half, _64B_);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext, ctag, dataOutBuf, sizeof(dataOutBuf)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_RST_RESP(QLIB_CONTEXT_T* qlibContext, U32* RST_RESP)
{
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;

    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_RST_RESP), RST_RESP, _128B_);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_ACLR(QLIB_CONTEXT_T* qlibContext, ACLR_T ACLR)
{
    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_SET_ACLR), &ACLR, sizeof(ACLR_T)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_ACLR(QLIB_CONTEXT_T* qlibContext, ACLR_T* ACLR)
{
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;

    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_ACLR), ACLR, sizeof(ACLR_T));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}


QLIB_STATUS_T QLIB_CMD_PROC__get_KEYS_STATUS_UNSIGNED(QLIB_CONTEXT_T* qlibContext, U64* status)
{
    QLIB_STATUS_T ret;
    _64BIT        statusBuf;

    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                             QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_KEYS_STATUS),
                                             statusBuf,
                                             sizeof(_64BIT));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    (void)memcpy((U8*)status, (U8*)statusBuf, sizeof(U64));
    return ret;
}

/*---------------------------------------------------------------------------------------------------------*/
/*                                        Session Control Commands                                         */
/*---------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_CMD_PROC__get_MC_UNSIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_MC_T mc)
{
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;
    {
        ret =QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, QLIB_CMD_PROC__MAKE_CTAG \
        (QLIB_CMD_SEC_GET_MC), mc, sizeof(QLIB_MC_T));
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
        QLIB_STATUS_RET_CHECK(ret);
    }
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_MC_SIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_MC_T mc)
{
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;

    {
        ret = QLIB_CMD_PROC__CALC_SIG(qlibContext, QLIB_SIGNED_DATA_TYPE_MC, 0, NULL, 0, mc, sizeof(QLIB_MC_T), NULL);
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
        QLIB_STATUS_RET_CHECK(ret);
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__synch_MC(QLIB_CONTEXT_T* qlibContext)
{
    U32 maxTC = QLIB_SEC_TC_MAX_VAL(qlibContext);
    if (1u == QLIB_ACTIVE_DIE_STATE(qlibContext).mcInSync)
    {
        return QLIB_STATUS__OK;
    }
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__MC_MAINT(qlibContext));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_MC_UNSIGNED(qlibContext, QLIB_ACTIVE_DIE_STATE(qlibContext).mc));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Verify MC                                                                                           */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).mc[TC] >= 0x10u, QLIB_STATUS__DEVICE_MC_ERR);
    if (Q2_TC_30_BIT(qlibContext) != 0u)
    {
        QLIB_ASSERT_RET((QLIB_ACTIVE_DIE_STATE(qlibContext).mc[TC] & 0x3FFFFFFFu) < maxTC, QLIB_STATUS__DEVICE_MC_ERR);
    }
    else
    {
        QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).mc[TC] < maxTC, QLIB_STATUS__DEVICE_MC_ERR);
    }
    QLIB_ASSERT_RET(QLIB_ACTIVE_DIE_STATE(qlibContext).mc[DMC] < QLIB_SEC_DMC_MAX_VAL, QLIB_STATUS__DEVICE_MC_ERR);

    QLIB_ACTIVE_DIE_STATE(qlibContext).mcInSync = TRUE;
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__MC_MAINT(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_MC_MAINT)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__Session_Open(QLIB_CONTEXT_T* qlibContext,
                                          QLIB_KID_T      kid,
                                          const KEY_T     key,
                                          BOOL            includeWID,
                                          BOOL            ignoreScrValidity)
{
    QLIB_WID_T      id;
    QLIB_MC_T       mc;
    U64             nonce = 0;
    QLIB_STATUS_T   ret   = QLIB_STATUS__COMMAND_FAIL;
    U8              mode  = 0;
    U32             ctag  = 0;
    _64BIT          seed;
    QLIB_REG_ESSR_T essr;
    /*-----------------------------------------------------------------------------------------------------*/
    /* OP1, CTAG (32b), NONCE (64b), SIG (64b)                                                             */
    /*-----------------------------------------------------------------------------------------------------*/
    U32 buff[(sizeof(_64BIT) + sizeof(_64BIT)) / sizeof(U32)];

    /*-----------------------------------------------------------------------------------------------------*/
    /* CTAG = CMD (8b), KID (8b), MODE (8b), 8'b0                                                          */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_SEC_OPEN_CMD_MODE_SET(mode, includeWID, ignoreScrValidity);
    ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_SESSION_OPEN, kid, mode, 0);

    /*-----------------------------------------------------------------------------------------------------*/
    /* NONCE (64b)                                                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    {
        nonce = PLAT_GetNONCE();
    }
    (void)memcpy((void*)buff, (void*)&nonce, sizeof(U64));

    /*-----------------------------------------------------------------------------------------------------*/
    /* SIG (64b)                                                                                           */
    /*-----------------------------------------------------------------------------------------------------*/
    if (includeWID == TRUE)
    {
        (void)memcpy((void*)id, (void*)QLIB_ACTIVE_DIE_STATE(qlibContext).wid, sizeof(QLIB_WID_T));
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Key                                                                                                 */
    /*-----------------------------------------------------------------------------------------------------*/
    if (!QLIB_KEY_MNGR__IS_KEY_VALID(key))
    {
        U8   keySection = QLIB_KEY_MNGR__GET_KEY_SECTION(kid);
        U32  sectionID  = (U32)keySection;
        BOOL fullAccess = QLIB_KEY_MNGR__GET_KEY_TYPE(kid) == (QLIB_KID_T)QLIB_KID__FULL_ACCESS_SECTION ? TRUE : FALSE;

        QLIB_ASSERT_RET(sectionID < QLIB_NUM_OF_SECTIONS, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
        QLIB_SEC_KEYMNGR_GetKey(&QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr, &key, sectionID, fullAccess);
        QLIB_ASSERT_RET(QLIB_KEY_MNGR__IS_KEY_VALID(key), QLIB_STATUS__INVALID_PARAMETER);
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Get, sync and advance MC                                                                            */
    /*-----------------------------------------------------------------------------------------------------*/
    {
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_use_mc_L(qlibContext, mc));
    }
    /*-----------------------------------------------------------------------------------------------------*/
    /* now we are ready to create the signature and current session key                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CRYPTO_SessionKeyAndSignature(key,
                                       ctag,
                                       mc,
                                       buff,
                                       (includeWID == TRUE) ? id : NULL,
                                       QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.sessionKey,
                                       &buff[2],
                                       seed);

    (void)memcpy((void*)&qlibContext->prng.state, (void*)seed, sizeof(seed));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set session KID                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid = kid;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform open session command                                                                        */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext, ctag, buff, sizeof(_64BIT) + sizeof(_64BIT)),
                               ret,
                               error);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Verify errors                                                                                       */
    /*-----------------------------------------------------------------------------------------------------*/
    if (Q3_BYPASS_HW_ISSUE_OPEN_SESS_SYS_ERR(qlibContext) == 1u)
    {
        if (READ_VAR_FIELD(QLIB_GET_LAST_SEC_STATUS_FIELD(qlibContext), QLIB_REG_SSR__SYS_ERR_S) == 1u)
        {
            QLIB_GET_LAST_SEC_STATUS_FIELD(qlibContext) &= (U32)(~(MASK_FIELD(QLIB_REG_SSR__SYS_ERR_S)));
        }
    }
    if (QLIB_KEY_MNGR__GET_KEY_TYPE(kid) == (QLIB_KID_T)QLIB_KID__FULL_ACCESS_SECTION)
    {
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, (U32)SSR_MASK__IGNORE_INTEG_ERR), ret, error);
    }
    else
    {
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS), ret, error);
    }
    /*-----------------------------------------------------------------------------------------------------*/
    /* Verify session is open                                                                              */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO(READ_VAR_FIELD(QLIB_GET_LAST_SEC_STATUS_FIELD(qlibContext), QLIB_REG_SSR__SES_READY) == 1u,
                                QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE,
                                ret,
                                error);
    /*-----------------------------------------------------------------------------------------------------*/
    /* Set SSK                                                                                             */
    /*-----------------------------------------------------------------------------------------------------*/
    (void)memcpy((void*)QLIB_HASH_BUF_GET__KEY(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.cmdContexArr[0].hashBuf),
                 (void*)QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.sessionKey,
                 sizeof(KEY_T));
    (void)memcpy((void*)QLIB_HASH_BUF_GET__KEY(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.cmdContexArr[1].hashBuf),
                 (void*)QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.sessionKey,
                 sizeof(KEY_T));

    /********************************************************************************************************
     * Verify session is open with correct KID
    ********************************************************************************************************/
    if (W77Q_ESSR_KID_MSB(qlibContext) != 0u)
    {
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_CMD_PROC__get_ESSR_UNSIGNED(qlibContext, &essr), ret, error);
        QLIB_ASSERT_WITH_ERROR_GOTO(((READ_VAR_FIELD(essr.asUint64, QLIB_REG_ESSR__KID_MSB) << 4) |
                                     READ_VAR_FIELD(QLIB_GET_LAST_SEC_STATUS_FIELD(qlibContext), QLIB_REG_SSR__KID)) == kid,
                                    QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE,
                                    ret,
                                    error);
    }
    else
    {
        QLIB_ASSERT_WITH_ERROR_GOTO((U8)READ_VAR_FIELD(QLIB_GET_LAST_SEC_STATUS_FIELD(qlibContext), QLIB_REG_SSR__KID) ==
                                        QLIB_KEY_MNGR__GET_KEY_SECTION(kid),
                                    QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE,
                                    ret,
                                    error);
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set OK                                                                                              */
    /*-----------------------------------------------------------------------------------------------------*/
    ret = QLIB_STATUS__OK;
    goto exit;

error:
    QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid = (QLIB_KID_T)QLIB_KID__INVALID;
    (void)memset(QLIB_HASH_BUF_GET__KEY(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.cmdContexArr[0].hashBuf), 0xFF, sizeof(KEY_T));
    (void)memset(QLIB_HASH_BUF_GET__KEY(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.cmdContexArr[1].hashBuf), 0xFF, sizeof(KEY_T));

exit:
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__Session_Close(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, BOOL revokePA)
{
    if (Q2_SESSION_CLOSE_NOT_SUPPORTED(qlibContext) == 0u)
    {
        return QLIB_CMD_PROC__Session_Close_L(qlibContext, kid, revokePA);
    }
    else
    {
        return QLIB_CMD_PROC__Session_Close_Bypass_L(qlibContext, kid, revokePA);
    }
}

QLIB_STATUS_T QLIB_CMD_PROC__init_section_PA(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex)
{
    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext,
                                           QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_INIT_SECTION_PA, sectionIndex, 0, 0)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, (U32)SSR_MASK__IGNORE_INTEG_ERR));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__PA_grant_plain(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex)
{
    /********************************************************************************************************
     * OP1, CTAG (32b)
     * CTAG = CMD (8b), SID (8b), 16'b0
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext,
                                           QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_PA_GRANT_PLAIN, sectionIndex, 0, 0)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__PA_grant(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid)
{
    _64BIT        sig;
    QLIB_MC_T     mc;
    CONST_KEY_P_T key;
    _64BIT        zeroNonce = {0};

    /********************************************************************************************************
     * OP1, CTAG (32b), SIG (64b)
     * CTAG = CMD (8b), KID (8b), 16'b0
    ********************************************************************************************************/
    U32 ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_PA_GRANT, kid, 0, 0);

    /********************************************************************************************************
     * Key
    ********************************************************************************************************/
    U8   sectionID  = QLIB_KEY_MNGR__GET_KEY_SECTION(kid);
    BOOL fullAccess = QLIB_KEY_MNGR__GET_KEY_TYPE(kid) == (QLIB_KID_T)QLIB_KID__FULL_ACCESS_SECTION ? TRUE : FALSE;
    QLIB_ASSERT_RET(sectionID < QLIB_NUM_OF_SECTIONS, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
    QLIB_SEC_KEYMNGR_GetKey(&QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr, &key, sectionID, fullAccess);
    QLIB_ASSERT_RET(QLIB_KEY_MNGR__IS_KEY_VALID(key), QLIB_STATUS__INVALID_PARAMETER);

    /********************************************************************************************************
     * Get, sync and advance MC
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_use_mc_L(qlibContext, mc));

    /********************************************************************************************************
     * Create signature
    ********************************************************************************************************/
    QLIB_CRYPTO_SessionKeyAndSignature(key, ctag, mc, zeroNonce, QLIB_ACTIVE_DIE_STATE(qlibContext).wid, NULL, sig, NULL);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext, ctag, sig, sizeof(_64BIT)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__PA_revoke(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex, QLIB_PA_REVOKE_TYPE_T revokeType)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext,
                                                             QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_PA_REVOKE,
                                                                                             sectionIndex,
                                                                                             (U8)revokeType,
                                                                                             0)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__CALC_CDI(QLIB_CONTEXT_T* qlibContext, U32 mode, _256BIT cdi)
{
    U32                    buff[(sizeof(U32) + sizeof(_256BIT)) / sizeof(U32)];
    QLIB_STATUS_T          ret = QLIB_STATUS__SECURITY_ERR;
    QLIB_CRYPTO_CONTEXT_T* cryptContext;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Prepare decryption key                                                                              */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CMD_PROC_initialize_decryption_key(qlibContext, cryptContext);

    /*-----------------------------------------------------------------------------------------------------*/
    /* execute the command                                                                                 */
    /*-----------------------------------------------------------------------------------------------------*/
    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                             QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_CALC_CDI, mode != 0u ? 1 : 0, 0, 0),
                                             buff,
                                             sizeof(buff));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Decrypt the data                                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CRYPTO_EncryptData(cdi, &buff[1], cryptContext->cipherKey, QLIB_SEC_READ_PAGE_SIZE_BYTE);

    /*-----------------------------------------------------------------------------------------------------*/
    /* verify the transaction counter  (HW returns previous transaction number)                            */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO(buff[0] == (QLIB_ACTIVE_DIE_STATE(qlibContext).mc[TC] - 1u),
                                QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE,
                                ret,
                                tc_fail);
    return QLIB_STATUS__OK;

tc_fail:
    QLIB_ACTIVE_DIE_STATE(qlibContext).mcInSync = FALSE;
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__Check_Integrity(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex, BOOL verifyDigest)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext,
                                                             QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_VER_INTG,
                                                                                             sectionIndex,
                                                                                             BOOLEAN_TO_INT(verifyDigest),
                                                                                             0)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

/*---------------------------------------------------------------------------------------------------------*/
/*                                       Secure Transport Commands                                         */
/*---------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_CMD_PROC__get_TC(QLIB_CONTEXT_T* qlibContext, U32* tc)
{
    {
        QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;

        ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_TC), tc, sizeof(U32));
        if (Q2_TC_30_BIT(qlibContext) != 0u)
        {
            *tc = (U32)READ_VAR_FIELD(*tc, QLIB_REG_TC__CNTR);
        }

        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
        QLIB_STATUS_RET_CHECK(ret);
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__CALC_SIG(QLIB_CONTEXT_T*         qlibContext,
                                      QLIB_SIGNED_DATA_TYPE_T dataType,
                                      U32                     section,
                                      U32*                    params,
                                      U32                     paramsSize,
                                      void*                   data,
                                      U32                     size,
                                      _64BIT                  signature)
{
    U32 buff[(sizeof(U32) + QLIB_SIGNED_DATA_MAX_SIZE + QLIB_SIGNED_CALC_SIG_PARAMS_MAX_SIZE) / sizeof(U32)];
    U32 tc_counter_HW = 0;
    U32 tc_counter_SW = 0;

    U32 data_size = QLIB_SIGNED_DATA_TYPE_GET_SIZE(dataType);
    U8  data_id   = QLIB_SIGNED_DATA_TYPE_GET_ID(dataType, section);

    U32           ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_CALC_SIG, data_id, 0, 0);
    _64BIT        calculated_signature;
    QLIB_STATUS_T ret = QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE;
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_RET(size >= data_size, QLIB_STATUS__INVALID_PARAMETER);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Make sure MC is in sync before CALC_SIG command since MC is used in the sign stage                  */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__synch_MC(qlibContext));

    ret = QLIB_CMD_PROC_execute_sec_cmd_write_read(qlibContext, ctag, params, paramsSize, buff, sizeof(buff));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);

    /*-----------------------------------------------------------------------------------------------------*/
    /* verify the signature                                                                                */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__sign_data_L(qlibContext, ctag, &buff[1], data_size, calculated_signature, TRUE));

    /*-----------------------------------------------------------------------------------------------------*/
    /* verify the transaction counter (HW returns previous transaction number)                             */
    /*-----------------------------------------------------------------------------------------------------*/
    tc_counter_SW = QLIB_ACTIVE_DIE_STATE(qlibContext).mc[TC];
    (void)memcpy((void*)&tc_counter_HW, (void*)buff, sizeof(U32));
    QLIB_ASSERT_WITH_ERROR_GOTO(tc_counter_HW == (tc_counter_SW - 1u), QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE, ret, tc_fail);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Check signature                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    if (memcmp((U8*)calculated_signature, (U8*)buff + sizeof(U32) + data_size, sizeof(_64BIT)) != 0)
    {
        ret = QLIB_STATUS__SECURITY_ERR;
    }
    else
    {
        ret = QLIB_STATUS__OK;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Parameters back to user                                                                             */
    /*-----------------------------------------------------------------------------------------------------*/
    if (signature != NULL)
    {
        (void)memcpy((void*)signature, (void*)((U8*)buff + sizeof(U32) + data_size), sizeof(_64BIT));
    }
    if (data != NULL)
    {
        (void)memcpy((void*)data, (void*)((U8*)buff + sizeof(U32)), data_size);
    }

    return ret;

tc_fail:
    QLIB_ACTIVE_DIE_STATE(qlibContext).mcInSync = FALSE;
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_RNGR_SEC(QLIB_CONTEXT_T* qlibContext, RNGR_T rngr)
{
    U32                    buff[(sizeof(U32) + sizeof(RNGR_T) + sizeof(_64BIT)) / sizeof(U32)]; // TC + DATA + Signature
    QLIB_CRYPTO_CONTEXT_T* pCryptContext;
    _64BIT                 calculated_signature;
    U32                    ctag;
    QLIB_STATUS_T          ret;

    if (W77Q_RNG_FEATURE(qlibContext) == 0u)
    {
        return QLIB_STATUS__NOT_SUPPORTED;
    }
    else
    {
        /*-------------------------------------------------------------------------------------------------*/
        /* Prepare decryption key                                                                          */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_CMD_PROC_initialize_decryption_key(qlibContext, pCryptContext);

        /*-------------------------------------------------------------------------------------------------*/
        /* Make sure MC is in sync since MC is used in the sign stage                                      */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__synch_MC(qlibContext));

        /*-------------------------------------------------------------------------------------------------*/
        /* execute the command                                                                             */
        /*-------------------------------------------------------------------------------------------------*/
        ctag = QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_RNGR);
        ret  = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, ctag, buff, sizeof(buff));
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
        QLIB_STATUS_RET_CHECK(ret);

        /*-------------------------------------------------------------------------------------------------*/
        /* Decrypt the data                                                                                */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_CRYPTO_EncryptData(rngr, &buff[1], pCryptContext->cipherKey, sizeof(RNGR_T));

        /*-------------------------------------------------------------------------------------------------*/
        /* verify the transaction counter (HW returns previous transaction number)                         */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_ASSERT_WITH_ERROR_GOTO((buff[0] + 1u) == QLIB_ACTIVE_DIE_STATE(qlibContext).mc[TC],
                                    QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE,
                                    ret,
                                    tc_fail);

        /*-------------------------------------------------------------------------------------------------*/
        /* verify the signature                                                                            */
        /*-------------------------------------------------------------------------------------------------*/
        (void)QLIB_CMD_PROC__sign_data_L(qlibContext, ctag, rngr, sizeof(RNGR_T), calculated_signature, FALSE);
        QLIB_ASSERT_RET(memcmp((U8*)calculated_signature, (U8*)buff + sizeof(U32) + sizeof(RNGR_T), sizeof(_64BIT)) == 0,
                        QLIB_STATUS__SECURITY_ERR);

        return QLIB_STATUS__OK;
    }

tc_fail:
    QLIB_ACTIVE_DIE_STATE(qlibContext).mcInSync = FALSE;
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_RNGR_PLAIN(QLIB_CONTEXT_T* qlibContext, RNGR_T rngr)
{
    if (W77Q_RNG_FEATURE(qlibContext) == 0u)
    {
        return QLIB_STATUS__NOT_SUPPORTED;
    }
    else
    {
        QLIB_STATUS_T ret;
        QLIB_ASSERT_RET(rngr != NULL, QLIB_STATUS__INVALID_PARAMETER);
        ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                                 QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_RNGR_PLAIN),
                                                 (U32*)rngr,
                                                 sizeof(RNGR_T));
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

        return ret;
    }
}

QLIB_STATUS_T QLIB_CMD_PROC__get_RNGR_COUNTER(QLIB_CONTEXT_T* qlibContext, RNGR_CNT_T rngr_cnt)
{
    if (W77Q_RNG_FEATURE(qlibContext) == 0u)
    {
        return QLIB_STATUS__NOT_SUPPORTED;
    }
    else
    {
        QLIB_STATUS_T ret;
        QLIB_ASSERT_RET(rngr_cnt != NULL, QLIB_STATUS__INVALID_PARAMETER);
        ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                                 QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_RNGR_COUNTER),
                                                 (U32*)rngr_cnt,
                                                 sizeof(RNGR_CNT_T));
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

        return ret;
    }
}

QLIB_STATUS_T QLIB_CMD_PROC__SRD(QLIB_CONTEXT_T* qlibContext, U32 addr, U32* data32B)
{
    QLIB_CRYPTO_CONTEXT_T* cryptContext;
    QLIB_STATUS_T          ret = QLIB_STATUS__SECURITY_ERR;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Build decryption cipher key                                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CMD_PROC_initialize_decryption_key(qlibContext, cryptContext);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Randomize address                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    addr = (U32)(addr ^ QLIB_CRYPTO_GetRandBits(&qlibContext->prng, LOG2(QLIB_SEC_READ_PAGE_SIZE_BYTE)));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Encrypt address                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CMD_PROC_encrypt_address(addr, addr, cryptContext->cipherKey);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform read                                                                                        */
    /*-----------------------------------------------------------------------------------------------------*/
    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                             QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SRD, addr),
                                             QLIB_HASH_BUF_GET__READ_PAGE(cryptContext->hashBuf),
                                             sizeof(U32) + QLIB_SEC_READ_PAGE_SIZE_BYTE);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);

    /*-----------------------------------------------------------------------------------------------------*/
    /* verify the transaction counter (HW returns previous transaction number)                             */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO((QLIB_HASH_BUF_GET__READ_TC(cryptContext->hashBuf) + 1u) ==
                                    QLIB_ACTIVE_DIE_STATE(qlibContext).mc[TC],
                                QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE,
                                ret,
                                tc_fail);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Decrypt the data                                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CRYPTO_EncryptData(data32B,
                            QLIB_HASH_BUF_GET__DATA(cryptContext->hashBuf),
                            cryptContext->cipherKey,
                            QLIB_SEC_READ_PAGE_SIZE_BYTE);

    return QLIB_STATUS__OK;

tc_fail:
    QLIB_ACTIVE_DIE_STATE(qlibContext).mcInSync = FALSE;
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__SARD(QLIB_CONTEXT_T* qlibContext, U32 addr, U32* data32B)
{
    QLIB_CRYPTO_CONTEXT_T* cryptContext;
    U32                    enc_addr;

    _64BIT        received_signature;
    _64BIT        calculated_signature;
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;


    /*-----------------------------------------------------------------------------------------------------*/
    /* Build decryption cipher key                                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CMD_PROC_initialize_decryption_key(qlibContext, cryptContext);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Randomize address                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    addr = (U32)(addr ^ QLIB_CRYPTO_GetRandBits(&qlibContext->prng, LOG2(QLIB_SEC_READ_PAGE_SIZE_BYTE)));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Encrypt address                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CMD_PROC_encrypt_address(enc_addr, addr, cryptContext->cipherKey);

    /*-----------------------------------------------------------------------------------------------------*/
    /* execute the command                                                                                 */
    /*-----------------------------------------------------------------------------------------------------*/
    ret =
        QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                           QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SARD, enc_addr),
                                           QLIB_HASH_BUF_GET__READ_PAGE(cryptContext->hashBuf),
                                           sizeof(U32) + QLIB_SEC_READ_PAGE_SIZE_BYTE + sizeof(_64BIT)); // TC + DATA + Signature
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Decrypt the data                                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CRYPTO_EncryptData(QLIB_HASH_BUF_GET__DATA(cryptContext->hashBuf),
                            QLIB_HASH_BUF_GET__DATA(cryptContext->hashBuf),
                            cryptContext->cipherKey,
                            QLIB_SEC_READ_PAGE_SIZE_BYTE);
    (void)memcpy(data32B, QLIB_HASH_BUF_GET__DATA(cryptContext->hashBuf), QLIB_SEC_READ_PAGE_SIZE_BYTE);

    /*-----------------------------------------------------------------------------------------------------*/
    /* verify the transaction counter (HW returns previous transaction number)                             */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO((QLIB_HASH_BUF_GET__READ_TC(cryptContext->hashBuf) + 1u) ==
                                    QLIB_ACTIVE_DIE_STATE(qlibContext).mc[TC],
                                QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE,
                                ret,
                                tc_fail);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Backup the signature so we can reuse the data buffer                                                */
    /*-----------------------------------------------------------------------------------------------------*/
    received_signature[0] = QLIB_HASH_BUF_GET__READ_SIG(cryptContext->hashBuf)[0];
    received_signature[1] = QLIB_HASH_BUF_GET__READ_SIG(cryptContext->hashBuf)[1];

    /*-----------------------------------------------------------------------------------------------------*/
    /* calculate the signature                                                                             */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_HASH_BUF_GET__CTAG(cryptContext->hashBuf) = QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SARD, addr);
    QLIB_CRYPTO_CalcAuthSignature(cryptContext->hashBuf, QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid, calculated_signature);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Signature check                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    if ((calculated_signature[0] == received_signature[0]) && (calculated_signature[1] == received_signature[1]))
    {
        ret = QLIB_STATUS__OK;
    }
    else
    {
        (void)memset(data32B, 0, QLIB_SEC_READ_PAGE_SIZE_BYTE);
        ret = QLIB_STATUS__SECURITY_ERR;
    }

    return ret;

tc_fail:
    QLIB_ACTIVE_DIE_STATE(qlibContext).mcInSync = FALSE;
    return ret;
}

#ifdef QLIB_SIGN_DATA_BY_FLASH
QLIB_STATUS_T QLIB_CMD_PROC__SARD_Sign(QLIB_CONTEXT_T* qlibContext, U32 enc_addr, _64BIT signature)
{
    QLIB_HASH_BUF_T dataOut;
    QLIB_STATUS_T   ret = QLIB_STATUS__SECURITY_ERR;

    /*-----------------------------------------------------------------------------------------------------*/
    /* execute the command                                                                                 */
    /*-----------------------------------------------------------------------------------------------------*/
    ret =
        QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                           QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SARD, enc_addr),
                                           QLIB_HASH_BUF_GET__READ_PAGE(dataOut),
                                           sizeof(U32) + QLIB_SEC_READ_PAGE_SIZE_BYTE + sizeof(_64BIT)); // TC + DATA + Signature
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Return the signature                                                                                */
    /*-----------------------------------------------------------------------------------------------------*/
    signature[0] = QLIB_HASH_BUF_GET__READ_SIG(dataOut)[0];
    signature[1] = QLIB_HASH_BUF_GET__READ_SIG(dataOut)[1];

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__SARD_Verify(QLIB_CONTEXT_T* qlibContext, U32 enc_addr, _64BIT signature)
{
    QLIB_CRYPTO_CONTEXT_T* cryptContext = NULL;
    U32                    addr         = 0;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Build decryption cipher key                                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CMD_PROC_initialize_decryption_key(qlibContext, cryptContext);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set data 0xFF                                                                                       */
    /*-----------------------------------------------------------------------------------------------------*/
    (void)memset(QLIB_HASH_BUF_GET__READ_PAGE(cryptContext->hashBuf),
                 0xFF,
                 sizeof(U32) + QLIB_SEC_READ_PAGE_SIZE_BYTE + sizeof(_64BIT));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set address                                                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CMD_PROC_decrypt_address(addr, enc_addr, cryptContext->cipherKey);
    QLIB_HASH_BUF_GET__CTAG(cryptContext->hashBuf) = QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SARD, addr);

    /*-----------------------------------------------------------------------------------------------------*/
    /* calculate the signature                                                                             */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CRYPTO_CalcAuthSignature(cryptContext->hashBuf, QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid, signature);

    return QLIB_STATUS__OK;
}
#endif

#ifndef QLIB_SUPPORT_XIP
QLIB_STATUS_T QLIB_CMD_PROC__SRD_Multi(QLIB_CONTEXT_T* qlibContext, U32 addr, U32* data, U32 size)
{
    QLIB_CRYPTO_CONTEXT_T* cryptContext_old = NULL;
    QLIB_CRYPTO_CONTEXT_T* cryptContext_new = NULL;
    QLIB_STATUS_T          ret              = QLIB_STATUS__SECURITY_ERR;
    U32                    enc_addr         = 0;
    U32                    rand             = 0;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Build decryption cipher key                                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CMD_PROC_initialize_decryption_key(qlibContext, cryptContext_old);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Randomize address                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    rand = QLIB_CRYPTO_GetRandBits(&qlibContext->prng, LOG2(QLIB_SEC_READ_PAGE_SIZE_BYTE));
    addr = addr ^ rand;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Encrypt address                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CMD_PROC_encrypt_address(enc_addr, addr, cryptContext_old->cipherKey);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Start read transaction (non-blocking)                                                               */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__OP1_only(qlibContext, QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SRD, enc_addr)));

    do
    {
        if (size > QLIB_SEC_READ_PAGE_SIZE_BYTE)
        {
            /*---------------------------------------------------------------------------------------------*/
            /* Build next cipher without advancing TC (in case it is not used)                             */
            /*---------------------------------------------------------------------------------------------*/
            QLIB_KEY_MNGR__CMD_CONTEXT_ADVANCE(qlibContext);
            cryptContext_new = &QLIB_KEY_MNGR__CMD_CONTEXT_GET(qlibContext);
            QLIB_CMD_PROC_update_decryption_key_async(qlibContext, cryptContext_new);

            /*---------------------------------------------------------------------------------------------*/
            /* Generate random                                                                             */
            /*---------------------------------------------------------------------------------------------*/
            rand = QLIB_CRYPTO_GetRandBits(&qlibContext->prng, LOG2(QLIB_SEC_READ_PAGE_SIZE_BYTE));

            /*---------------------------------------------------------------------------------------------*/
            /* Calculate next address                                                                      */
            /*---------------------------------------------------------------------------------------------*/
            addr += QLIB_SEC_READ_PAGE_SIZE_BYTE;
            addr = addr ^ rand;

            /*---------------------------------------------------------------------------------------------*/
            /* Increment TC                                                                                */
            /*---------------------------------------------------------------------------------------------*/
            QLIB_TRANSACTION_CNTR_USE(qlibContext);
        }

        /*-------------------------------------------------------------------------------------------------*/
        /* Wait while busy, check for errors and read data                                                 */
        /*-------------------------------------------------------------------------------------------------*/
        ret = QLIB_CMD_PROC__OP0_busy_wait_OP2(qlibContext,
                                               QLIB_HASH_BUF_GET__READ_PAGE(cryptContext_old->hashBuf),
                                               sizeof(U32) + QLIB_SEC_READ_PAGE_SIZE_BYTE);

        /*-------------------------------------------------------------------------------------------------*/
        /* Start next command                                                                              */
        /*-------------------------------------------------------------------------------------------------*/
        if (size > QLIB_SEC_READ_PAGE_SIZE_BYTE)
        {
            /*---------------------------------------------------------------------------------------------*/
            /* Wait till cipher is ready                                                                   */
            /*---------------------------------------------------------------------------------------------*/
            QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_update_decryption_key_async_wait_till_ready(qlibContext));

            /*---------------------------------------------------------------------------------------------*/
            /* Encrypt next address                                                                        */
            /*---------------------------------------------------------------------------------------------*/
            QLIB_CMD_PROC_encrypt_address(enc_addr, addr, cryptContext_new->cipherKey);

            /*---------------------------------------------------------------------------------------------*/
            /* Start next read transaction (non-blocking)                                                  */
            /*---------------------------------------------------------------------------------------------*/
            QLIB_STATUS_RET_CHECK(
                QLIB_CMD_PROC__OP1_only(qlibContext, QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SRD, enc_addr)));
        }

        /*-------------------------------------------------------------------------------------------------*/
        /* Check errors after starting new command                                                         */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
        QLIB_STATUS_RET_CHECK(ret);

        /*-------------------------------------------------------------------------------------------------*/
        /* Decrypt with old cipher                                                                         */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_CRYPTO_EncryptData_INLINE(data, QLIB_HASH_BUF_GET__DATA(cryptContext_old->hashBuf), cryptContext_old->cipherKey, 8);

        /*-------------------------------------------------------------------------------------------------*/
        /* Update variables                                                                                */
        /*-------------------------------------------------------------------------------------------------*/
        if (size > QLIB_SEC_READ_PAGE_SIZE_BYTE)
        {
            data += QLIB_SEC_READ_PAGE_SIZE_BYTE / sizeof(U32);
            size -= QLIB_SEC_READ_PAGE_SIZE_BYTE;
            cryptContext_old = cryptContext_new;
        }
        else
        {
            size = 0; // exit loop
        }
    } while (size != 0u);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__SARD_Multi(QLIB_CONTEXT_T* qlibContext, U32 addr, U32* data, U32 size)
{
    QLIB_CRYPTO_CONTEXT_T* cryptContext_old = NULL;
    QLIB_CRYPTO_CONTEXT_T* cryptContext_new = NULL;
    _64BIT                 received_signature;
    _64BIT                 calculated_signature;
    QLIB_STATUS_T          ret      = QLIB_STATUS__SECURITY_ERR;
    U32                    enc_addr = 0;
    U32                    rand     = 0;
    U32                    new_addr = 0;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Build decryption cipher key                                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_CMD_PROC_initialize_decryption_key(qlibContext, cryptContext_old);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Encrypt address                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    rand = QLIB_CRYPTO_GetRandBits(&qlibContext->prng, LOG2(QLIB_SEC_READ_PAGE_SIZE_BYTE));
    addr = addr ^ rand;
    QLIB_CMD_PROC_encrypt_address(enc_addr, addr, cryptContext_old->cipherKey);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Start read transaction (non-blocking)                                                               */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__OP1_only(qlibContext, QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SARD, enc_addr)));

    do
    {
        if (size > QLIB_SEC_READ_PAGE_SIZE_BYTE)
        {
            /*---------------------------------------------------------------------------------------------*/
            /* Build next cipher without advancing TC (in case it is not used)                             */
            /*---------------------------------------------------------------------------------------------*/
            QLIB_KEY_MNGR__CMD_CONTEXT_ADVANCE(qlibContext);
            cryptContext_new = &QLIB_KEY_MNGR__CMD_CONTEXT_GET(qlibContext);
            QLIB_CMD_PROC_update_decryption_key_async(qlibContext, cryptContext_new);

            /*---------------------------------------------------------------------------------------------*/
            /* Generate random                                                                             */
            /*---------------------------------------------------------------------------------------------*/
            rand = QLIB_CRYPTO_GetRandBits(&qlibContext->prng, LOG2(QLIB_SEC_READ_PAGE_SIZE_BYTE));

            /*---------------------------------------------------------------------------------------------*/
            /* Calculate next address                                                                      */
            /*---------------------------------------------------------------------------------------------*/
            new_addr = addr + QLIB_SEC_READ_PAGE_SIZE_BYTE;
            new_addr = new_addr ^ rand;

            /*---------------------------------------------------------------------------------------------*/
            /* Increment TC                                                                                */
            /*---------------------------------------------------------------------------------------------*/
            QLIB_TRANSACTION_CNTR_USE(qlibContext);
        }

        /*-------------------------------------------------------------------------------------------------*/
        /* Wait while busy, check for errors and read data                                                 */
        /*-------------------------------------------------------------------------------------------------*/
        ret = QLIB_CMD_PROC__OP0_busy_wait_OP2(qlibContext,
                                               QLIB_HASH_BUF_GET__READ_PAGE(cryptContext_old->hashBuf),
                                               sizeof(U32) + QLIB_SEC_READ_PAGE_SIZE_BYTE + sizeof(U64));

        /*-------------------------------------------------------------------------------------------------*/
        /* Start next command                                                                              */
        /*-------------------------------------------------------------------------------------------------*/
        if (size > QLIB_SEC_READ_PAGE_SIZE_BYTE)
        {
            /*---------------------------------------------------------------------------------------------*/
            /* Wait till cipher is ready                                                                   */
            /*---------------------------------------------------------------------------------------------*/
            QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_update_decryption_key_async_wait_till_ready(qlibContext));

            /*---------------------------------------------------------------------------------------------*/
            /* Encrypt next address                                                                        */
            /*---------------------------------------------------------------------------------------------*/
            QLIB_CMD_PROC_encrypt_address(enc_addr, new_addr, cryptContext_new->cipherKey);

            /*---------------------------------------------------------------------------------------------*/
            /* Start next read transaction (non-blocking)                                                  */
            /*---------------------------------------------------------------------------------------------*/
            QLIB_STATUS_RET_CHECK(
                QLIB_CMD_PROC__OP1_only(qlibContext, QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SARD, enc_addr)));
        }

        /*-------------------------------------------------------------------------------------------------*/
        /* Check errors after starting new command                                                         */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
        QLIB_STATUS_RET_CHECK(ret);

        /*-------------------------------------------------------------------------------------------------*/
        /* Decrypt with old cipher                                                                         */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_CRYPTO_EncryptData_INLINE(QLIB_HASH_BUF_GET__DATA(cryptContext_old->hashBuf),
                                       QLIB_HASH_BUF_GET__DATA(cryptContext_old->hashBuf),
                                       cryptContext_old->cipherKey,
                                       8);

        ARRAY_COPY_INLINE(data, QLIB_HASH_BUF_GET__DATA(cryptContext_old->hashBuf), 8);

        /*-------------------------------------------------------------------------------------------------*/
        /* Verify signature                                                                                */
        /*-------------------------------------------------------------------------------------------------*/
        received_signature[0] = QLIB_HASH_BUF_GET__READ_SIG(cryptContext_old->hashBuf)[0];
        received_signature[1] = QLIB_HASH_BUF_GET__READ_SIG(cryptContext_old->hashBuf)[1];

        /*-------------------------------------------------------------------------------------------------*/
        /* calculate the signature                                                                         */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_HASH_BUF_GET__CTAG(cryptContext_old->hashBuf) = QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SARD, addr);
        QLIB_CRYPTO_CalcAuthSignature(cryptContext_old->hashBuf,
                                      QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid,
                                      calculated_signature);
        /* Signature check                                                                                 */
        /*-------------------------------------------------------------------------------------------------*/
        if ((calculated_signature[0] != received_signature[0]) || (calculated_signature[1] != received_signature[1]))
        {
            /*---------------------------------------------------------------------------------------------*/
            /*                               Signature verification failed!                                */
            /*---------------------------------------------------------------------------------------------*/

            /*---------------------------------------------------------------------------------------------*/
            /* Wait till transaction is finished                                                           */
            /*---------------------------------------------------------------------------------------------*/
            (void)QLIB_CMD_PROC__OP0_busy_wait(qlibContext);

            /*---------------------------------------------------------------------------------------------*/
            /* Clear data                                                                                  */
            /*---------------------------------------------------------------------------------------------*/
            (void)memset(data, 0, size);

            return QLIB_STATUS__SECURITY_ERR;
        }

        /*-------------------------------------------------------------------------------------------------*/
        /* Update variables                                                                                */
        /*-------------------------------------------------------------------------------------------------*/
        if (size > QLIB_SEC_READ_PAGE_SIZE_BYTE)
        {
            data += QLIB_SEC_READ_PAGE_SIZE_BYTE / sizeof(U32);
            size -= QLIB_SEC_READ_PAGE_SIZE_BYTE;
            addr             = new_addr;
            cryptContext_old = cryptContext_new;
        }
        else
        {
            size = 0; // exit loop
        }
    } while (size != 0u);

    return QLIB_STATUS__OK;
}
#endif // QLIB_SUPPORT_XIP

QLIB_STATUS_T QLIB_CMD_PROC__SAWR(QLIB_CONTEXT_T* qlibContext, U32 addr, const U32* data)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Error checking                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/

    /*-----------------------------------------------------------------------------------------------------*/
    /* Randomize 5-LS bits to prevent zero bits encryption                                                 */
    /*-----------------------------------------------------------------------------------------------------*/
    addr = (U32)(addr ^ QLIB_CRYPTO_GetRandBits(&qlibContext->prng, LOG2(QLIB_SEC_WRITE_PAGE_SIZE_BYTE)));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Send the command                                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext,
                                                                 QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SAWR, addr),
                                                                 data,
                                                                 QLIB_SEC_WRITE_PAGE_SIZE_BYTE,
                                                                 TRUE,
                                                                 &addr));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__LOG_SRD(QLIB_CONTEXT_T* qlibContext, U32* addr, U32* data16B)
{
    QLIB_CRYPTO_CONTEXT_T* cryptContext;
    QLIB_STATUS_T          ret;

    /********************************************************************************************************
     * Build decryption cipher key
    ********************************************************************************************************/
    QLIB_CMD_PROC_initialize_decryption_key(qlibContext, cryptContext);

    /********************************************************************************************************
     * execute the command
    ********************************************************************************************************/
    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                             QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_LOG_SRD),
                                             QLIB_HASH_BUF_GET__READ_PAGE(cryptContext->hashBuf),
                                             sizeof(U32) + sizeof(U32) + QLIB_SEC_LOG_ENTRY_SIZE); // TC + Head address + data
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);

    /********************************************************************************************************
     * verify the transaction counter (HW returns previous transaction number)
    ********************************************************************************************************/
    QLIB_ASSERT_WITH_ERROR_GOTO((QLIB_HASH_BUF_GET__READ_TC(cryptContext->hashBuf) + 1u) ==
                                    QLIB_ACTIVE_DIE_STATE(qlibContext).mc[TC],
                                QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE,
                                ret,
                                tc_fail);

    /********************************************************************************************************
     * Decrypt the data (log entry at the head of the log)
    ********************************************************************************************************/
    // TODO: After the FPGA is updated and address is also encrypted, use a buffer to decrypted bot data & address here,
    // and then copy the result to each output buffer
    if (NULL != data16B)
    {
        QLIB_CRYPTO_EncryptData(data16B,
                                &(QLIB_HASH_BUF_GET__DATA(cryptContext->hashBuf)[1]),
                                cryptContext->cipherKey,
                                QLIB_SEC_LOG_ENTRY_SIZE);
    }

    /********************************************************************************************************
     * Copy the address of the head of the log
    ********************************************************************************************************/
    if (NULL != addr)
    {
        (void)memcpy(addr, QLIB_HASH_BUF_GET__DATA(cryptContext->hashBuf), sizeof(U32));
    }

    return QLIB_STATUS__OK;

tc_fail:
    QLIB_ACTIVE_DIE_STATE(qlibContext).mcInSync = FALSE;
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__LOG_PRD(QLIB_CONTEXT_T* qlibContext, U32 section, U32* addr, U32* data16B)
{
    U32           buff[(sizeof(U32) + QLIB_SEC_LOG_ENTRY_SIZE) / sizeof(U32)]; // Head address + data
    U32           ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_LOG_PRD, (U8)section, 0, 0);
    QLIB_STATUS_T ret;

    /********************************************************************************************************
     * execute the command
    ********************************************************************************************************/
    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, ctag, buff, sizeof(buff));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);

    if (NULL != addr)
    {
        (void)memcpy(addr, &buff[0], sizeof(U32));
    }
    if (NULL != data16B)
    {
        (void)memcpy(data16B, &buff[1], QLIB_SEC_LOG_ENTRY_SIZE);
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__LOG_SAWR(QLIB_CONTEXT_T* qlibContext, U32* data16B)
{
    U32 ctag = QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_LOG_SAWR);

    /********************************************************************************************************
     * Send the command
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC__execute_signed_setter_L(qlibContext, ctag, data16B, QLIB_SEC_LOG_ENTRY_SIZE, TRUE, NULL));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__LOG_PWR(QLIB_CONTEXT_T* qlibContext, U32 section, U32* data16B)
{
    U32 ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_LOG_PWR, (U8)section, 0, 0);

    /********************************************************************************************************
     * Send the command
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext, ctag, data16B, QLIB_SEC_LOG_ENTRY_SIZE));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__MEM_CRC(QLIB_CONTEXT_T* qlibContext, U32 section, U32 addr, U32 len, U32* crc)
{
    /********************************************************************************************************
     * OP1, CTAG (32b), addr (32b), len (32b)
     * OP2, <CRC (32b)>
     * CTAG = CMD (8b), SID (8b), 16'b0
     * CMD =  MEM_CRC (54h)
    ********************************************************************************************************/
    U32 buff[(sizeof(U32) + sizeof(U32)) / sizeof(U32)]; // address + length
    U32 ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_MEM_CRC, (U8)section, 0, 0);

    /********************************************************************************************************
     * copy addr and len to buff[]
    ********************************************************************************************************/
    (void)memcpy(&buff[0], &addr, sizeof(U32));
    (void)memcpy(&buff[1], &len, sizeof(U32));

    /********************************************************************************************************
     * Send the command
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_write_read(qlibContext, ctag, buff, sizeof(buff), crc, sizeof(U32)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__SERASE(QLIB_CONTEXT_T* qlibContext, QLIB_ERASE_T type, U32 addr)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;
    U32           ctag   = 0;
    U32           addrl  = 0;
    U32*          addr_p = NULL;
    /*-----------------------------------------------------------------------------------------------------*/
    /* Select erase type and randomize LS bits to prevent zero bits encryption                             */
    /*-----------------------------------------------------------------------------------------------------*/
    switch (type)
    {
        case QLIB_ERASE_SECTOR_4K:

            addrl  = (U32)(addr ^ QLIB_CRYPTO_GetRandBits(&qlibContext->prng, LOG2(_4KB_)));
            ctag   = QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SERASE_4, addrl);
            addr_p = &addrl;
            break;

        case QLIB_ERASE_BLOCK_32K:
            addrl  = (U32)(addr ^ QLIB_CRYPTO_GetRandBits(&qlibContext->prng, LOG2(_32KB_)));
            ctag   = QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SERASE_32, addrl);
            addr_p = &addrl;
            break;

        case QLIB_ERASE_BLOCK_64K:
            addrl  = (U32)(addr ^ QLIB_CRYPTO_GetRandBits(&qlibContext->prng, LOG2(_64KB_)));
            ctag   = QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_SEC_SERASE_64, addrl);
            addr_p = &addrl;
            break;

        case QLIB_ERASE_SECTION:
            ctag = QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_SERASE_SEC);
            break;

        case QLIB_ERASE_CHIP:
            ctag = QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_SERASE_ALL);
            break;

        default:
            status = QLIB_STATUS__INVALID_PARAMETER;
            break;
    }

    QLIB_STATUS_RET_CHECK(status);
    /*-----------------------------------------------------------------------------------------------------*/
    /* Send the command                                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext, ctag, NULL, 0, FALSE, addr_p));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return status;
}

QLIB_STATUS_T QLIB_CMD_PROC__MEM_COPY(QLIB_CONTEXT_T* qlibContext, U32 dest, U32 src, U32 len)
{
    /********************************************************************************************************
     * OP1, CTAG (32b), src (32b), dest (32b), SIG (64b)
     * CTAG = CMD (8 bits), LEN (24b)
     * CMD = MEM_COPY (65h)
    ********************************************************************************************************/
    U32 buff[(sizeof(U32) + sizeof(U32)) / sizeof(U32)]; // src + dest
    U32 ctag = MAKE_32_BIT(QLIB_CMD_SEC_MEM_COPY, BYTE(len, 2), BYTE(len, 1), BYTE(len, 0));

    /********************************************************************************************************
     * copy src and dest to buff[]
    ********************************************************************************************************/
    (void)memcpy(&buff[0], &src, sizeof(U32));
    (void)memcpy(&buff[1], &dest, sizeof(U32));

    /********************************************************************************************************
     * Create signature
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__execute_signed_setter_L(qlibContext, ctag, buff, sizeof(buff), FALSE, NULL));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__ERASE_SECT_PLAIN(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex)
{
    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext,
                                           QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_ERASE_SECT_PLAIN, sectionIndex, 0, 0)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

#ifndef EXCLUDE_LMS
QLIB_STATUS_T QLIB_CMD_PROC__LMS_write(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, U32 offset, const U32* data, U32 dataSize)
{
    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_ASSERT_RET(ALIGNED_TO(dataSize, QLIB_SEC_LMS_PAGE_WORD_SIZE), QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(ALIGNED_TO(offset, QLIB_SEC_LMS_PAGE_WORD_SIZE), QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(dataSize <= QLIB_SEC_LMS_PAGE_MAX_SIZE, QLIB_STATUS__INVALID_PARAMETER);

    /********************************************************************************************************
     * Send the command
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext,
                                            QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_LMS_LOAD,
                                                                            kid,
                                                                            (U8)(dataSize / QLIB_SEC_LMS_PAGE_WORD_SIZE),
                                                                            (U8)(offset / QLIB_SEC_LMS_PAGE_WORD_SIZE)),
                                            data,
                                            dataSize));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__LMS_execute(QLIB_CONTEXT_T* qlibContext,
                                         QLIB_KID_T      kid,
                                         const _64BIT    digest,
                                         BOOL            reset,
                                         BOOL            grantPA)
{
    U32 ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_LMS_EXEC, kid, 0, 0);

    if (TRUE == reset)
    {
        ctag = ctag | QLIB_TM_CTAG_SCR_NEED_RESET_MASK;
    }

    if (TRUE == grantPA)
    {
        ctag = ctag | QLIB_TM_CTAG_SCR_NEED_GRANT_PA_MASK;
    }

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext, ctag, digest, sizeof(_64BIT)));

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}
#endif

QLIB_STATUS_T QLIB_CMD_PROC__LMS_set_KEY(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, const QLIB_LMS_KEY_T publicKey)
{
    /********************************************************************************************************
     * OP1, CTAG(32 bits), ID(128b), Public Key(192 bits), SIG(64 bits)
     * CTAG = CMD (8b), KID (8b), 16'b0
    ********************************************************************************************************/
    U32     dataOutBuf[(sizeof(QLIB_LMS_KEY_T) + sizeof(_64BIT)) / sizeof(U32)]; // key + signature
    U32     ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_SET_KEY_LMS, kid, 0, 0);
    _256BIT keyDigest;

    /********************************************************************************************************
     * This command requires an open session with Key Provisioning privileges
    ********************************************************************************************************/
    QLIB_ASSERT_RET(QLIB_KEY_MNGR__GET_KEY_TYPE(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid) ==
                        (QLIB_KID_T)QLIB_KID__SECTION_PROVISIONING,
                    QLIB_STATUS__COMMAND_IGNORED);

    /********************************************************************************************************
     * Prepare command data
    ********************************************************************************************************/
    (void)memcpy(dataOutBuf, publicKey, sizeof(QLIB_LMS_KEY_T));

    /********************************************************************************************************
     * Add signature of key digest
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_HASH(keyDigest, publicKey, sizeof(QLIB_LMS_KEY_T)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__prepare_signed_setter_cmd_L(qlibContext,
                                                                     &ctag,
                                                                     keyDigest,
                                                                     sizeof(_256BIT),
                                                                     &dataOutBuf[sizeof(QLIB_LMS_KEY_T) / sizeof(U32)],
                                                                     NULL,
                                                                     NULL));

    /********************************************************************************************************
     * Execute command
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext, ctag, dataOutBuf, sizeof(dataOutBuf)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

/*---------------------------------------------------------------------------------------------------------*/
/*                                           Auxiliary Commands                                            */
/*---------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_CMD_PROC__get_HW_VER_UNSIGNED(QLIB_CONTEXT_T* qlibContext, HW_VER_T* hwVer)
{
    QLIB_STATUS_T ret = QLIB_STATUS__SECURITY_ERR;

    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext,
                                             QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_VERSION),
                                             (U32*)hwVer,
                                             sizeof(U32));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_HW_VER_SIGNED(QLIB_CONTEXT_T* qlibContext, HW_VER_T* hwVer)
{
    QLIB_STATUS_T ret =
        QLIB_CMD_PROC__CALC_SIG(qlibContext, QLIB_SIGNED_DATA_TYPE_HW_VER, 0, NULL, 0, hwVer, sizeof(HW_VER_T), NULL);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return ret;
}

QLIB_STATUS_T QLIB_CMD_PROC__AWDT_expire(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_AWDT_EXPIRE)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__sleep(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_SLEEP)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__checkLastSsrErrors(QLIB_CONTEXT_T* qlibContext, U32 mask)
{
    U32           ssrReg       = QLIB_GET_LAST_SEC_STATUS_FIELD(qlibContext);
    U32           ssrRegMasked = ssrReg & mask;
    QLIB_STATUS_T ssrError     = QLIB_STATUS__OK;
    /*-----------------------------------------------------------------------------------------------------*/
    /* Check for errors                                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    if (ssrRegMasked != 0u)
    {
        if (READ_VAR_FIELD(ssrRegMasked, QLIB_REG_SSR__SES_ERR_S) != 0u)
        {
            ssrError = QLIB_STATUS__DEVICE_SESSION_ERR;
        }
        else if (READ_VAR_FIELD(ssrRegMasked, QLIB_REG_SSR__INTG_ERR_S) != 0u)
        {
            ssrError = QLIB_STATUS__DEVICE_INTEGRITY_ERR;
        }
        else if (READ_VAR_FIELD(ssrRegMasked, QLIB_REG_SSR__AUTH_ERR_S) != 0u)
        {
            ssrError = QLIB_STATUS__DEVICE_AUTHENTICATION_ERR;
        }
        else if (READ_VAR_FIELD(ssrRegMasked, QLIB_REG_SSR__PRIV_ERR_S) != 0u)
        {
            ssrError = QLIB_STATUS__DEVICE_PRIVILEGE_ERR;
        }
        else if (READ_VAR_FIELD(ssrRegMasked, QLIB_REG_SSR__IGNORE_ERR_S) != 0u)
        {
            ssrError = QLIB_STATUS__COMMAND_IGNORED;
        }
        else if (READ_VAR_FIELD(ssrRegMasked, QLIB_REG_SSR__SYS_ERR_S) != 0u)
        {
            ssrError = QLIB_STATUS__DEVICE_SYSTEM_ERR;
        }
        else if (READ_VAR_FIELD(ssrRegMasked, QLIB_REG_SSR__FLASH_ERR_S) != 0u)
        {
            ssrError = QLIB_STATUS__DEVICE_FLASH_ERR;
        }
        else if (READ_VAR_FIELD(ssrRegMasked, QLIB_REG_SSR__MC_ERR) != 0u)
        {
            ssrError = QLIB_STATUS__DEVICE_MC_ERR;
        }
        else if (READ_VAR_FIELD(ssrRegMasked, QLIB_REG_SSR__ERR) != 0u)
        {
            ssrError = QLIB_STATUS__DEVICE_ERR;
        }
        else if (READ_VAR_FIELD(ssrRegMasked, QLIB_REG_SSR__BUSY) != 0u)
        {
            ssrError = QLIB_STATUS__DEVICE_BUSY;
        }
        else
        {
            //This else is for the PC-Lint [MISRA 2012 Rule 15.7, required]
        }
    }
    /*-----------------------------------------------------------------------------------------------------*/
    /* Clear SSR if error occurred and synch MC                                                            */
    /*-----------------------------------------------------------------------------------------------------*/
    if ((ssrReg & SSR_MASK__ALL_ERRORS) != 0u)
    {
        QLIB_REG_SSR_T ssr;
        QLIB_STATUS_T  ret;
        if (qlibContext->multiTransactionCmd == 1u)
        {
            qlibContext->multiTransactionCmd = 0u;

#ifdef QLIB_SPI_OPTIMIZATION_ENABLED
            PLAT_SPI_MultiTransactionStop();
#endif //QLIB_SPI_OPTIMIZATION_ENABLED
        }
        ret = QLIB_CMD_PROC_execute_sec_cmd(qlibContext,
                                            QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_GET_MC),
                                            NULL,
                                            0,
                                            QLIB_ACTIVE_DIE_STATE(qlibContext).mc,
                                            sizeof(QLIB_MC_T),
                                            &ssr);
        if (QLIB_STATUS__OK != ret || READ_VAR_FIELD(ssr.asUint, QLIB_REG_SSR__ERR) != 0u)
        {
            QLIB_ACTIVE_DIE_STATE(qlibContext).mcInSync = FALSE;
        }
    }
    return ssrError;
}


QLIB_STATUS_T QLIB_CMD_PROC_execute_std_cmd(QLIB_CONTEXT_T* qlibContext,
                                            QLIB_BUS_MODE_T format,
                                            BOOL            dtr,
                                            BOOL            needWriteEnable,
                                            BOOL            waitWhileBusy,
                                            U8              cmd,
                                            const U32*      address,
                                            const U8*       writeData,
                                            U32             writeDataSize,
                                            U32             dummyCycles,
                                            U8*             readData,
                                            U32             readDataSize,
                                            QLIB_REG_SSR_T* ssr)
{
    U32 ssrTemp = 0;
    /********************************************************************************************************
     * No commands allowed in powered-down state, except of SPI_FLASH_CMD__RELEASE_POWER_DOWN
    ********************************************************************************************************/
    if ((QLIB_ACTIVE_DIE_STATE(qlibContext).isPoweredDown == 1u) && (cmd != SPI_FLASH_CMD__RELEASE_POWER_DOWN)
#if QLIB_NUM_OF_DIES > 1
        && (cmd != SPI_FLASH_CMD__DIE_SELECT)
#endif
    )
    {
        SET_VAR_FIELD_32(ssrTemp, QLIB_REG_SSR__IGNORE_ERR_S, 1u);
        SET_VAR_FIELD_32(ssrTemp, QLIB_REG_SSR__ERR, 1u);
        if (NULL != ssr)
        {
            ssr->asUint = ssrTemp;
        }
        return QLIB_STATUS__COMMAND_IGNORED;
    }

    QLIB_STATUS_RET_CHECK(QLIB_TM_Standard(qlibContext,
                                           QLIB_BUS_FORMAT(format, dtr),
                                           needWriteEnable,
                                           waitWhileBusy,
                                           cmd,
                                           address,
                                           writeData,
                                           writeDataSize,
                                           dummyCycles,
                                           readData,
                                           readDataSize,
                                           ssr));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_ECCR(QLIB_CONTEXT_T* qlibContext, STD_FLASH_ECC_STATUS_T* eccr)
{
    QLIB_BUS_MODE_T busMode = QLIB_STD_GET_BUS_MODE(qlibContext);
    U32             dataInSize;
    U8              dataIn[2];
    U32             dummyCycles =
        (QLIB_BUS_MODE_1_1_1 == busMode) ? SPI_FLASH_DUMMY_CYCLES__READ_ECCR_SPI : SPI_FLASH_DUMMY_CYCLES__READ_ECCR_QPI_OPI;

    dataInSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        busMode,
                                                        FALSE,
                                                        FALSE,
                                                        FALSE,
                                                        SPI_FLASH_CMD__READ_ECC_STATUS_REGISTER,
                                                        NULL,
                                                        NULL,
                                                        0,
                                                        dummyCycles,
                                                        dataIn,
                                                        dataInSize,
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
    eccr->asUint = dataIn[0];

    QLIB_ASSERT_RET((1u == dataInSize) || (QLIB_DATA_EXTENSION_VALID(qlibContext, dataIn)), QLIB_STATUS__COMMUNICATION_ERR);
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_ECCR(QLIB_CONTEXT_T* qlibContext, STD_FLASH_ECC_STATUS_T eccr)
{
    U32 dataOutSize;
    U8  dataOut[2];

    dataOutSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);
    QLIB_DATA_EXTENSION_SET_DATA_OUT(qlibContext, dataOut, eccr.asUint);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        QLIB_STD_GET_BUS_MODE(qlibContext),
                                                        FALSE,
                                                        TRUE,
                                                        TRUE,
                                                        SPI_FLASH_CMD__WRITE_ECC_STATUS_REGISTER,
                                                        NULL,
                                                        dataOut,
                                                        dataOutSize,
                                                        0,
                                                        NULL,
                                                        0,
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_AECCR(QLIB_CONTEXT_T* qlibContext, STD_FLASH_ADVANCED_ECC_T* aeccr)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        QLIB_STD_GET_BUS_MODE(qlibContext),
                                                        FALSE,
                                                        FALSE,
                                                        FALSE,
                                                        SPI_FLASH_CMD__READ_ADVANCED_ECC_STATUS_REGISTER,
                                                        NULL,
                                                        NULL,
                                                        0,
                                                        SPI_FLASH_DUMMY_CYCLES__READ_AECCR,
                                                        (U8*)(aeccr),
                                                        sizeof(STD_FLASH_ADVANCED_ECC_T),
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__clear_AECCR(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        QLIB_STD_GET_BUS_MODE(qlibContext),
                                                        FALSE,
                                                        FALSE,
                                                        FALSE,
                                                        SPI_FLASH_CMD__CLEAR_ADVANCED_ECC_STATUS_REGISTER,
                                                        NULL,
                                                        NULL,
                                                        0,
                                                        0,
                                                        NULL,
                                                        0,
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_EAR(QLIB_CONTEXT_T* qlibContext, U8* extAddr)
{
    QLIB_BUS_MODE_T busMode = QLIB_STD_GET_BUS_MODE(qlibContext);
    U32             dataInSize;
    U8              dataIn[2];
    U32             dummyCycles =
        (busMode == QLIB_BUS_MODE_1_1_1 ? SPI_FLASH_DUMMY_CYCLES__READ_EAR_SPI : SPI_FLASH_DUMMY_CYCLES__READ_EAR_QPI_OPI);

    dataInSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        busMode,
                                                        FALSE,
                                                        FALSE,
                                                        FALSE,
                                                        SPI_FLASH_CMD__READ_EAR,
                                                        NULL,
                                                        NULL,
                                                        0,
                                                        dummyCycles,
                                                        dataIn,
                                                        dataInSize,
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
    *extAddr = dataIn[0];

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_ASSERT_RET((1u == dataInSize) || QLIB_DATA_EXTENSION_VALID(qlibContext, dataIn), QLIB_STATUS__COMMUNICATION_ERR);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_EAR(QLIB_CONTEXT_T* qlibContext, U8 extAddr)
{
    U32 dataOutSize;
    U8  dataOut[2];

    dataOutSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);
    QLIB_DATA_EXTENSION_SET_DATA_OUT(qlibContext, dataOut, extAddr);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        QLIB_STD_GET_BUS_MODE(qlibContext),
                                                        FALSE,
                                                        TRUE,
                                                        FALSE,
                                                        SPI_FLASH_CMD__WRITE_EAR,
                                                        NULL,
                                                        dataOut,
                                                        dataOutSize,
                                                        0,
                                                        NULL,
                                                        0,
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_CR(QLIB_CONTEXT_T* qlibContext, U32 addr, U8* cr)
{
    U32 dataInSize;
    U8  dataIn[2];

    dataInSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);

    QLIB_ASSERT_RET(addr < SPI_FLASH__EXTENDED_CONFIGURATION_SIZE, QLIB_STATUS__INVALID_PARAMETER);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        QLIB_STD_GET_BUS_MODE(qlibContext),
                                                        FALSE,
                                                        FALSE,
                                                        FALSE,
                                                        SPI_FLASH_CMD__READ_CR,
                                                        &addr,
                                                        NULL,
                                                        0,
                                                        SPI_FLASH_DUMMY_CYCLES__READ_CR,
                                                        dataIn,
                                                        dataInSize,
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
    *cr = dataIn[0];

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_ASSERT_RET((1u == dataInSize) || QLIB_DATA_EXTENSION_VALID(qlibContext, dataIn), QLIB_STATUS__COMMUNICATION_ERR);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_default_CR(QLIB_CONTEXT_T* qlibContext, U32 addr, U8* cr)
{
    U32 dataInSize;
    U8  dataIn[2];

    dataInSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);

    QLIB_ASSERT_RET(addr < SPI_FLASH__EXTENDED_CONFIGURATION_SIZE, QLIB_STATUS__INVALID_PARAMETER);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        QLIB_STD_GET_BUS_MODE(qlibContext),
                                                        FALSE,
                                                        FALSE,
                                                        FALSE,
                                                        SPI_FLASH_CMD__READ_CR_DFLT,
                                                        &addr,
                                                        NULL,
                                                        0,
                                                        qlibContext->fastReadDummy,
                                                        dataIn,
                                                        dataInSize,
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
    *cr = dataIn[0];

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_ASSERT_RET((1u == dataInSize) || QLIB_DATA_EXTENSION_VALID(qlibContext, dataIn), QLIB_STATUS__COMMUNICATION_ERR);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_CR(QLIB_CONTEXT_T* qlibContext, U32 addr, U8 cr)
{
    U32 dataOutSize;
    U8  dataOut[2];
    U8  testCr;

    dataOutSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);
    QLIB_DATA_EXTENSION_SET_DATA_OUT(qlibContext, dataOut, cr);

    QLIB_ASSERT_RET(addr < SPI_FLASH__EXTENDED_CONFIGURATION_SIZE, QLIB_STATUS__INVALID_PARAMETER);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        QLIB_STD_GET_BUS_MODE(qlibContext),
                                                        FALSE,
                                                        TRUE,
                                                        TRUE,
                                                        SPI_FLASH_CMD__WRITE_CR,
                                                        &addr,
                                                        dataOut,
                                                        dataOutSize,
                                                        0,
                                                        NULL,
                                                        0,
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    /********************************************************************************************************
     * Verify success
    ********************************************************************************************************/
    if (addr != CONFIG_REG_ADDR__INTERRUPT_EVENTS)
    {
        // for interrupt events the register bits are of type write-1-clear
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__get_CR(qlibContext, addr, &testCr));
        QLIB_ASSERT_RET(testCr == cr, QLIB_STATUS__COMMAND_IGNORED);
    }
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__set_default_CR(QLIB_CONTEXT_T* qlibContext, U32 addr, U8 cr)
{
    U32 dataOutSize;
    U8  dataOut[2];

    dataOutSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);
    QLIB_DATA_EXTENSION_SET_DATA_OUT(qlibContext, dataOut, cr);

    QLIB_ASSERT_RET(addr < SPI_FLASH__EXTENDED_CONFIGURATION_SIZE, QLIB_STATUS__INVALID_PARAMETER);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        QLIB_STD_GET_BUS_MODE(qlibContext),
                                                        FALSE,
                                                        TRUE,
                                                        TRUE,
                                                        SPI_FLASH_CMD__WRITE_CR_DFLT,
                                                        &addr,
                                                        dataOut,
                                                        dataOutSize,
                                                        0,
                                                        NULL,
                                                        0,
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__get_FR(QLIB_CONTEXT_T* qlibContext, STD_FLASH_FLAGS_T* fr)
{
    QLIB_BUS_MODE_T busMode = QLIB_STD_GET_BUS_MODE(qlibContext);
    U32             dataInSize;
    U8              dataIn[2];
    U32             dummyCycles =
        (busMode == QLIB_BUS_MODE_1_1_1 ? SPI_FLASH_DUMMY_CYCLES__READ_FR_SPI : SPI_FLASH_DUMMY_CYCLES__READ_FR_QPI_OPI);

    dataInSize = QLIB_DATA_EXTENSION_SIZE(qlibContext);

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        busMode,
                                                        FALSE,
                                                        FALSE,
                                                        FALSE,
                                                        SPI_FLASH_CMD__READ_FR,
                                                        NULL,
                                                        NULL,
                                                        0,
                                                        dummyCycles,
                                                        dataIn,
                                                        dataInSize,
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));
    fr->asUint = dataIn[0];

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_ASSERT_RET((1u == dataInSize) || QLIB_DATA_EXTENSION_VALID(qlibContext, dataIn), QLIB_STATUS__COMMUNICATION_ERR);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__clear_FR(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_execute_std_cmd(qlibContext,
                                                        QLIB_STD_GET_BUS_MODE(qlibContext),
                                                        FALSE,
                                                        FALSE,
                                                        FALSE,
                                                        SPI_FLASH_CMD__CLEAR_FR,
                                                        NULL,
                                                        NULL,
                                                        0,
                                                        0,
                                                        NULL,
                                                        0,
                                                        &QLIB_ACTIVE_DIE_STATE(qlibContext).ssr));

    /********************************************************************************************************
     * Error checking
    ********************************************************************************************************/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    return QLIB_STATUS__OK;
}

#ifndef EXCLUDE_LMS_ATTESTATION
QLIB_STATUS_T QLIB_CMD_PROC__OTS_SET_KEY(QLIB_CONTEXT_T*                 qlibContext,
                                         const LMS_ATTEST_PRIVATE_SEED_T seed,
                                         const LMS_ATTEST_KEY_ID_T       keyId)
{
    QLIB_STATUS_T ret;
    U32           buf[(sizeof(LMS_ATTEST_PRIVATE_SEED_T) + sizeof(LMS_ATTEST_KEY_ID_T)) / sizeof(U32)];

    (void)memcpy((U8*)buf, (const U8*)seed, sizeof(LMS_ATTEST_PRIVATE_SEED_T));
    (void)memcpy((U8*)buf + sizeof(LMS_ATTEST_PRIVATE_SEED_T), keyId, sizeof(LMS_ATTEST_KEY_ID_T));

    /********************************************************************************************************
     * Send the command
    ********************************************************************************************************/
    ret = QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_OTS_SET_KEY), buf, sizeof(buf));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__OTS_INIT(QLIB_CONTEXT_T* qlibContext, U32* leafNum, LMS_ATTEST_KEY_ID_T keyId)
{
    QLIB_STATUS_T ret;
    U32           buf[(sizeof(U32) + sizeof(LMS_ATTEST_KEY_ID_T)) / sizeof(U32)];

    /********************************************************************************************************
     * Send the command
    ********************************************************************************************************/
    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_OTS_INIT), buf, sizeof(buf));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);

    /********************************************************************************************************
     * Set output
    ********************************************************************************************************/
    if (leafNum != NULL)
    {
        *leafNum = buf[0];
    }

    if (keyId != NULL)
    {
        (void)memcpy((U8*)keyId, (const U8*)(&buf[1]), sizeof(LMS_ATTEST_KEY_ID_T));
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__OTS_GET_ID(QLIB_CONTEXT_T* qlibContext, U32* leafNum, LMS_ATTEST_KEY_ID_T keyId)
{
    QLIB_STATUS_T ret;
    U32           buf[(sizeof(U32) + sizeof(LMS_ATTEST_KEY_ID_T)) / sizeof(U32)];

    /********************************************************************************************************
     * Send the command
    ********************************************************************************************************/
    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_OTS_GET_ID), buf, sizeof(buf));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);

    /********************************************************************************************************
     * Set output
    ********************************************************************************************************/
    if (leafNum != NULL)
    {
        *leafNum = buf[0];
    }

    if (keyId != NULL)
    {
        (void)memcpy((U8*)keyId, (const U8*)(&buf[1]), sizeof(LMS_ATTEST_KEY_ID_T));
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__OTS_SIGN_DIGIT(QLIB_CONTEXT_T* qlibContext, U8 digit, U32* chainIndex, LMS_ATTEST_CHUNK_T digitSig)
{
    /********************************************************************************************************
     * CTAG = CMD (8 bits), 16'b0, Di (8b)
    ********************************************************************************************************/
    U32           ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_OTS_SIGN_DIGIT, 0, 0, digit);
    U32           buf[(sizeof(U32) + sizeof(LMS_ATTEST_CHUNK_T)) / sizeof(U32)];
    QLIB_STATUS_T ret;

    /********************************************************************************************************
     * Send the command
    ********************************************************************************************************/
    ret = QLIB_CMD_PROC_execute_sec_cmd_read(qlibContext, ctag, buf, sizeof(buf));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);
    *chainIndex = buf[0];
    (void)memcpy((U8*)digitSig, (const U8*)(&buf[1]), sizeof(LMS_ATTEST_CHUNK_T));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CMD_PROC__OTS_PUB_CHAIN(QLIB_CONTEXT_T* qlibContext, U32 leafNum, U16 chainIndex, _192BIT pubChunk)
{
    QLIB_STATUS_T ret;
    U32           buf[2];

    buf[0] = leafNum;
    (void)memcpy((U8*)(&buf[1]), (U8*)(&chainIndex), sizeof(U16));

    /********************************************************************************************************
     * Send the command
    ********************************************************************************************************/
    ret = QLIB_CMD_PROC_execute_sec_cmd_write_read(qlibContext,
                                                   QLIB_CMD_PROC__MAKE_CTAG(QLIB_CMD_SEC_OTS_PUB_CHAIN),
                                                   buf,
                                                   sizeof(U32) + sizeof(U16),
                                                   pubChunk,
                                                   sizeof(_192BIT));

    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));
    QLIB_STATUS_RET_CHECK(ret);

    return QLIB_STATUS__OK;
}
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                             LOCAL FUNCTIONS                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This routine returns current sw MC counter which is synchronized to hw if needed
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      mc           Full MC
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_CMD_PROC_use_mc_L(QLIB_CONTEXT_T* qlibContext, QLIB_MC_T mc)
{
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__synch_MC(qlibContext));

    mc[TC]  = QLIB_TRANSACTION_CNTR_USE(qlibContext);
    mc[DMC] = QLIB_ACTIVE_DIE_STATE(qlibContext).mc[DMC];

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine refreshes SSK
 *
 * @param[in,out]   qlibContext   Context
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_CMD_PROC_refresh_ssk_L(QLIB_CONTEXT_T* qlibContext)
{
    _64BIT mc;
    {
        /*-------------------------------------------------------------------------------------------------*/
        /* Get MC                                                                                          */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_use_mc_L(qlibContext, mc));
        QLIB_CRYPTO_put_salt_on_session_key(mc[TC],
                                            QLIB_HASH_BUF_GET__KEY(QLIB_KEY_MNGR__CMD_CONTEXT_GET(qlibContext).hashBuf),
                                            QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.sessionKey);
    }

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine signs the given data
 *
 * @param[in]   qlibContext        Context
 * @param[in]   plain_ctag         CTAG
 * @param[in]   data_up_to256bit   Data to sign
 * @param[in]   data_size          Data size
 * @param[out]  signature          Data signature
 * @param[in]   refresh_ssk        if TRUE, SSK will be refreshed
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_CMD_PROC__sign_data_L(QLIB_CONTEXT_T* qlibContext,
                                                U32             plain_ctag,
                                                const U32*      data_up_to256bit,
                                                U32             data_size,
                                                U32*            signature,
                                                BOOL            refresh_ssk)
{
    QLIB_CRYPTO_CONTEXT_T* cryptContext = &QLIB_KEY_MNGR__CMD_CONTEXT_GET(qlibContext);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Refresh SSK key if required                                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    if (TRUE == refresh_ssk)
    {
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_refresh_ssk_L(qlibContext));
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Perform signing                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_HASH_BUF_GET__CTAG(cryptContext->hashBuf) = plain_ctag;
    (void)memcpy(QLIB_HASH_BUF_GET__DATA(cryptContext->hashBuf), data_up_to256bit, data_size);
    (void)memset((((U8*)QLIB_HASH_BUF_GET__DATA(cryptContext->hashBuf)) + data_size), 0, sizeof(_256BIT) - data_size);

    QLIB_CRYPTO_CalcAuthSignature(cryptContext->hashBuf, QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid, signature);

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine signs and encrypts the given data & address
 *
 * @param[in]       qlibContext                 Context
 * @param[out]      ctag                        CTAG pointer. If address is available, returns CTAG with encrypted address
 * @param[in]       data                        Data to sign/encrypt
 * @param[in]       data_size                   Data size
 * @param[out]      signature                   Data signature
 * @param[out]      encrypted_data              Returned encrypted data-in not NULL if data encryption is not required
 * @param[in,out]   addr                        Address to encrypt or NULL if address encryption is not required
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_CMD_PROC__prepare_signed_setter_cmd_L(QLIB_CONTEXT_T* qlibContext,
                                                                U32*            ctag,
                                                                const U32*      data,
                                                                U32             data_size,
                                                                U32*            signature,
                                                                U32*            encrypted_data,
                                                                U32*            addr)
{
    QLIB_CRYPTO_CONTEXT_T* cryptContext;
    BOOL                   refresh_ssk = TRUE;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Create signature of plain-text data                                                                 */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__sign_data_L(qlibContext, *ctag, data, data_size, signature, refresh_ssk));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Shall we encrypt?                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    if ((encrypted_data != NULL) || (addr != NULL))
    {
        cryptContext = &QLIB_KEY_MNGR__CMD_CONTEXT_GET(qlibContext);

        /*-------------------------------------------------------------------------------------------------*/
        /* Prepare encryption key                                                                          */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_CMD_PROC_build_encryption_key(qlibContext, cryptContext);

        /*-------------------------------------------------------------------------------------------------*/
        /* encrypt address and patch CTAG                                                                  */
        /*-------------------------------------------------------------------------------------------------*/
        if (addr != NULL)
        {
            QLIB_CMD_PROC_encrypt_address(*addr, *addr, cryptContext->cipherKey);
            *ctag = QLIB_CMD_PROC__MAKE_CTAG_ADDR(QLIB_CMD_PROC__CTAG_GET_CMD(*ctag), *addr);
        }

        /*-------------------------------------------------------------------------------------------------*/
        /* Encrypt data if needed                                                                          */
        /*-------------------------------------------------------------------------------------------------*/
        if (encrypted_data != NULL)
        {
            /*---------------------------------------------------------------------------------------------*/
            /* Encrypt data                                                                                */
            /*---------------------------------------------------------------------------------------------*/
            QLIB_CRYPTO_EncryptData(encrypted_data, data, cryptContext->cipherKey, data_size);
        }
    }

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine executes a given command with data and optionally encrypts it
 *
 * @param[in]       qlibContext    Context
 * @param[in]       ctag           CTAG
 * @param[in]       data           Data
 * @param[in]       data_size      Data size
 * @param[in]       encrypt_data   if TRUE, data will be sent encrypted
 * @param[in,out]   addr           if not NULL, address is encrypted into ctag
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_CMD_PROC__execute_signed_setter_L(QLIB_CONTEXT_T* qlibContext,
                                                            U32             ctag,
                                                            const U32*      data,
                                                            U32             data_size,
                                                            BOOL            encrypt_data,
                                                            U32*            addr)
{
    U32 dataOutBuf[(sizeof(_256BIT) + sizeof(_64BIT)) / sizeof(U32)]; // Maximum data + signature

    /*-----------------------------------------------------------------------------------------------------*/
    /* Encrypt and sign the command                                                                        */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__prepare_signed_setter_cmd_L(qlibContext,
                                                                     &ctag,
                                                                     data,
                                                                     data_size,
                                                                     &dataOutBuf[data_size / sizeof(U32)],
                                                                     encrypt_data == TRUE ? dataOutBuf : NULL,
                                                                     addr));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Copy the data manually to the output buffer is not encrypted                                        */
    /*-----------------------------------------------------------------------------------------------------*/
    if (FALSE == encrypt_data)
    {
        (void)memcpy(dataOutBuf, data, data_size);
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Execute command                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    return QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext, ctag, dataOutBuf, data_size + sizeof(_64BIT));
}

/************************************************************************************************************
 * @brief       This routine is used to close a Session
 *
 * @param[in,out]   qlibContext         Context
 * @param[in]       kid                 Key ID
 * @param[in]       revokePA            If TRUE, revoke plain-access privileges
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_CMD_PROC__Session_Close_L(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, BOOL revokePA)
{
    U8 mode = 0;
    /*-----------------------------------------------------------------------------------------------------*/
    /* CTAG = CMD (8b), KID (8b), MODE (8b), 8'b0                                                          */
    /*-----------------------------------------------------------------------------------------------------*/
    QLIB_SEC_CLOSE_CMD_MODE_SET(mode, revokePA);

    QLIB_STATUS_RET_CHECK(
        QLIB_CMD_PROC_execute_sec_cmd_only(qlibContext,
                                           QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_SESSION_CLOSE, kid, mode, 0)));
    QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, SSR_MASK__ALL_ERRORS));

    /*-----------------------------------------------------------------------------------------------------*/
    /* Clear context                                                                                       */
    /*-----------------------------------------------------------------------------------------------------*/
    if (revokePA == TRUE)
    {
        U8 sectionIndex = QLIB_KEY_MNGR__GET_KEY_SECTION(kid);
        if (sectionIndex < QLIB_NUM_OF_MAIN_SECTIONS)
        {
            QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[sectionIndex].plainEnabled = QLIB_SECTION_PLAIN_EN_NO;
        }
    }
    QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid = (QLIB_KID_T)QLIB_KID__INVALID;
    (void)memset(QLIB_HASH_BUF_GET__KEY(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.cmdContexArr[0].hashBuf), 0xFF, sizeof(KEY_T));
    (void)memset(QLIB_HASH_BUF_GET__KEY(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.cmdContexArr[1].hashBuf), 0xFF, sizeof(KEY_T));

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine is used to close a Session for chips where the SESSION_CLOSE command is not supported
 *
 * @param[in,out]   qlibContext         Context
 * @param[in]       kid                 Key ID
 * @param[in]       revokePA            If TRUE, revoke plain-access privileges
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_CMD_PROC__Session_Close_Bypass_L(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, BOOL revokePA)
{
    QLIB_STATUS_T ret = QLIB_STATUS__OK;

    if (revokePA == TRUE)
    {
        U8 sectionIndex = QLIB_KEY_MNGR__GET_KEY_SECTION(kid);
        QLIB_ASSERT_RET(sectionIndex < QLIB_NUM_OF_MAIN_SECTIONS, QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE);
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC__init_section_PA(qlibContext, (U32)sectionIndex));
        QLIB_ACTIVE_DIE_STATE(qlibContext).sectionsState[sectionIndex].plainEnabled = QLIB_SECTION_PLAIN_EN_NO;
        QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid                              = (QLIB_KID_T)QLIB_KID__INVALID;
    }
    else
    {
        QLIB_MC_T mc;
        U64       nonce = PLAT_GetNONCE();
        U8        mode  = 0;
        U32       ctag  = 0;
        KEY_T     key   = {0};
        _64BIT    seed;

        /*-------------------------------------------------------------------------------------------------*/
        /* OP1, CTAG (32b), NONCE (64b), SIG (64b)                                                         */
        /*-------------------------------------------------------------------------------------------------*/
        U32 buff[(sizeof(_64BIT) + sizeof(_64BIT)) / sizeof(U32)];

        /*-------------------------------------------------------------------------------------------------*/
        /* CTAG = CMD (8b), KID (8b), MODE (8b), 8'b0                                                      */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_SEC_OPEN_CMD_MODE_SET(mode, FALSE, FALSE);
        ctag = QLIB_CMD_PROC__MAKE_CTAG_PARAMS(QLIB_CMD_SEC_SESSION_OPEN, kid, mode, 0);

        /*-------------------------------------------------------------------------------------------------*/
        /* NONCE (64b)                                                                                     */
        /*-------------------------------------------------------------------------------------------------*/
        (void)memcpy((void*)buff, (void*)&nonce, sizeof(U64));

        /*-------------------------------------------------------------------------------------------------*/
        /* Get, sync and advance MC                                                                        */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK(QLIB_CMD_PROC_use_mc_L(qlibContext, mc));

        /*-------------------------------------------------------------------------------------------------*/
        /* now we are ready to create the signature and current session key                                */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_CRYPTO_SessionKeyAndSignature(key,
                                           ctag,
                                           mc,
                                           buff,
                                           NULL,
                                           QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.sessionKey,
                                           &buff[2],
                                           seed);
        (void)memcpy((void*)&qlibContext->prng.state, (void*)seed, sizeof(seed));

        /*-------------------------------------------------------------------------------------------------*/
        /* Perform bad open session command                                                                */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_CMD_PROC_execute_sec_cmd_write(qlibContext, ctag, buff, sizeof(_64BIT) + sizeof(_64BIT)),
                                   ret,
                                   error);

        QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.kid = (U8)QLIB_KID__INVALID;
        /*-------------------------------------------------------------------------------------------------*/
        /* Clear errors                                                                                    */
        /*-------------------------------------------------------------------------------------------------*/

        /*lint -save -e774 */
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_CMD_PROC__checkLastSsrErrors(qlibContext, 0), ret, error);
        /*lint -restore */

        /*-------------------------------------------------------------------------------------------------*/
        /* Verify session is close with no errors                                                          */
        /*-------------------------------------------------------------------------------------------------*/
        QLIB_STATUS_RET_CHECK_GOTO(QLIB_SEC__get_SSR(qlibContext, NULL, SSR_MASK__ALL_ERRORS), ret, error);

        QLIB_ASSERT_WITH_ERROR_GOTO(READ_VAR_FIELD(QLIB_GET_LAST_SEC_STATUS_FIELD(qlibContext), QLIB_REG_SSR__SES_READY) == 0u,
                                    QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE,
                                    ret,
                                    error);
    }
    /*-----------------------------------------------------------------------------------------------------*/
    /* Clear context                                                                                       */
    /*-----------------------------------------------------------------------------------------------------*/
    (void)memset(QLIB_HASH_BUF_GET__KEY(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.cmdContexArr[0].hashBuf), 0xFF, sizeof(KEY_T));
    (void)memset(QLIB_HASH_BUF_GET__KEY(QLIB_ACTIVE_DIE_STATE(qlibContext).keyMngr.cmdContexArr[1].hashBuf), 0xFF, sizeof(KEY_T));

error:
    return ret;
}
