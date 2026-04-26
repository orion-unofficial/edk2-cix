/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_cmd_proc.h
* @brief      Command processor: This file handles processing of flash API commands
*
* ### project qlib
*
************************************************************************************************************/
#ifndef __QLIB_CMD_PROC_H__
#define __QLIB_CMD_PROC_H__

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


#define SSR_MASK__ALL_ERRORS                                                                                                     \
    ((U32)(MASK_FIELD(QLIB_REG_SSR__SES_ERR_S) | MASK_FIELD(QLIB_REG_SSR__INTG_ERR_S) | MASK_FIELD(QLIB_REG_SSR__AUTH_ERR_S) |   \
           MASK_FIELD(QLIB_REG_SSR__PRIV_ERR_S) | MASK_FIELD(QLIB_REG_SSR__IGNORE_ERR_S) | MASK_FIELD(QLIB_REG_SSR__SYS_ERR_S) | \
           MASK_FIELD(QLIB_REG_SSR__FLASH_ERR_S) | MASK_FIELD(QLIB_REG_SSR__MC_ERR) | MASK_FIELD(QLIB_REG_SSR__ERR) |            \
           MASK_FIELD(QLIB_REG_SSR__BUSY)))

#define SSR_MASK__IGNORE_INTEG_ERR                                                                                          \
    (MASK_FIELD(QLIB_REG_SSR__AUTH_ERR_S) | MASK_FIELD(QLIB_REG_SSR__PRIV_ERR_S) | MASK_FIELD(QLIB_REG_SSR__IGNORE_ERR_S) | \
     MASK_FIELD(QLIB_REG_SSR__SYS_ERR_S) | MASK_FIELD(QLIB_REG_SSR__FLASH_ERR_S) | MASK_FIELD(QLIB_REG_SSR__MC_ERR) |       \
     MASK_FIELD(QLIB_REG_SSR__BUSY))

#define SSR_MASK__IGNORE_IGNORE_ERR                                                                                       \
    (MASK_FIELD(QLIB_REG_SSR__INTG_ERR_S) | MASK_FIELD(QLIB_REG_SSR__AUTH_ERR_S) | MASK_FIELD(QLIB_REG_SSR__PRIV_ERR_S) | \
     MASK_FIELD(QLIB_REG_SSR__SYS_ERR_S) | MASK_FIELD(QLIB_REG_SSR__FLASH_ERR_S) | MASK_FIELD(QLIB_REG_SSR__MC_ERR) |     \
     MASK_FIELD(QLIB_REG_SSR__BUSY))

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 MACROS                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_GET_LAST_SEC_STATUS_FIELD(qlibContext)      (QLIB_ACTIVE_DIE_STATE(qlibContext).ssr.asUint)
#define QLIB_SET_LAST_SEC_STATUS_FIELD(qlibContext, val) (QLIB_ACTIVE_DIE_STATE(qlibContext).ssr.asUint = (val))

#define QLIB_DATA_EXTENSION_SET_DATA_OUT(qlibContext, dataOut, value)                    \
    {                                                                                    \
        (dataOut)[0] = (value);                                                          \
        (dataOut)[1] = (W77Q_SUPPORT_SSE(qlibContext) != 0u) ? (U8)(~(value)) : (value); \
    }

#define QLIB_DATA_EXTENSION_VALID(qlibContext, dataIn) \
    ((W77Q_SUPPORT_SSE(qlibContext) == 0u) || ((dataIn)[0] == ((~((dataIn)[1])) & 0xFFu)))

#define QLIB_DATA_EXTENSION_ACTIVE(qlibContext, mode, dtr)                         \
    ((QLIB_BUS_FORMAT(QLIB_BUS_MODE_8_8_8, TRUE) == QLIB_BUS_FORMAT(mode, dtr)) || \
     ((W77Q_SUPPORT_SSE(qlibContext) != 0u) &&                                     \
      ((QLIB_BUS_FORMAT(QLIB_BUS_MODE_8_8_8, FALSE) == QLIB_BUS_FORMAT(mode, dtr)) || (QLIB_BUS_MODE_4_4_4 == (mode)))))

#define QLIB_DATA_EXTENSION_SIZE(qlibContext) \
    (QLIB_DATA_EXTENSION_ACTIVE(qlibContext, QLIB_STD_GET_BUS_MODE(qlibContext), (qlibContext)->busInterface.dtr) ? 2u : 1u)

#ifdef QLIB_SUPPORT_OPI
#define QLIB_CMD_EXTENSION_SIZE(qlibContext) \
    (((QLIB_BUS_MODE_8_8_8 == QLIB_STD_GET_BUS_MODE(qlibContext)) && (TRUE == (qlibContext)->busInterface.dtr)) ? 2u : 1u)
#else
#define QLIB_CMD_EXTENSION_SIZE(qlibContext) (1u)
#endif

#define QLIB_DATA_EXTENSION_SET_CMD_BUFFER(qlibContext, buffer, cmd) \
    {                                                                \
        (buffer)[0] = cmd;                                           \
        if (QLIB_CMD_EXTENSION_SIZE(qlibContext) > 1u)               \
        {                                                            \
            (buffer)[1] = cmd;                                       \
        }                                                            \
    }
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           INTERFACE FUNCTIONS                                           */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This routine returns the Secure status register
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      ssr           Secure status register
 * @param[in]       mask          SSR mask
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_SSR_UNSIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_REG_SSR_T* ssr, U32 mask) __RAM_SECTION;

/************************************************************************************************************
 * @brief       This routine returns assured with signature the Secure status register
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      ssr           Secure status register
 * @param[in]       mask          SSR mask
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_SSR_SIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_REG_SSR_T* ssr, U32 mask);

/************************************************************************************************************
 * @brief       This routine returns the Extended Secure Status Register
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      essr          Extended secure status register
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_ESSR_UNSIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_REG_ESSR_T* essr) __RAM_SECTION;

/************************************************************************************************************
 * @brief       This routine returns assured with signature the Extended Secure Status Register
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      essr          Extended secure status register
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_ESSR_SIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_REG_ESSR_T* essr);

/*---------------------------------------------------------------------------------------------------------*/
/*                                             STATUS COMMANDS                                             */
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This routine returns Winbond ID (WID)
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      WID           WID
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_WID_UNSIGNED(QLIB_CONTEXT_T* qlibContext, _64BIT WID);

/************************************************************************************************************
 * @brief       This routine returns signature assured Winbond ID (WID)
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      WID           WID
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_WID_SIGNED(QLIB_CONTEXT_T* qlibContext, _64BIT WID);


/************************************************************************************************************
 * @brief       This routine returns Secret User ID (SUID)
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      SUID          SUID
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_SUID_UNSIGNED(QLIB_CONTEXT_T* qlibContext, _128BIT SUID);

/************************************************************************************************************
 * @brief       This routine returns Secret User ID via CALC_SIG command i.e. with a signature
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      SUID          SUID
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_SUID_SIGNED(QLIB_CONTEXT_T* qlibContext, _128BIT SUID);

/************************************************************************************************************
 * @brief       This routine returns AWDTSR value
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      AWDTSR        AWDTSR
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_AWDTSR(QLIB_CONTEXT_T* qlibContext, AWDTSR_T* AWDTSR);


/*---------------------------------------------------------------------------------------------------------*/
/*                                         CONFIGURATION COMMANDS                                          */
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This routine performs Secure Format (SFORMAT)
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       modeReset     If set, after command execution completes W77H is soft-reset and reloads its configurations
 * @param[in]       modeDefault   If set, after the format, the device is returned to its Factory-default configurations
 * @param[in]       modeInit      If set, the format erases also the pre-provisioned Master Key and initializes the Monotonic Counter
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__SFORMAT(QLIB_CONTEXT_T* qlibContext, BOOL modeReset, BOOL modeDefault, BOOL modeInit);

/************************************************************************************************************
 * @brief       This routine performs format
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       modeReset     If set, after command execution completes W77Q is soft-reset and reloads its configurations
 * @param[in]       modeDefault   If set, after the format, the device is returned to its Factory-default configurations
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__FORMAT(QLIB_CONTEXT_T* qlibContext, BOOL modeReset, BOOL modeDefault);


/************************************************************************************************************
 * @brief       This routine sets new key
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       kid           Key ID
 * @param[in]       key_buff      New key data
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_KEY(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, const KEY_T key_buff);

/************************************************************************************************************
 * @brief       This routine sets the SUID
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       SUID          SUID
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_SUID(QLIB_CONTEXT_T* qlibContext, const _128BIT SUID);

/************************************************************************************************************
 * @brief       This routine sets new GMC
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       GMC           GMC
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_GMC(QLIB_CONTEXT_T* qlibContext, const GMC_T GMC);

/************************************************************************************************************
 * @brief       This routine returns GMC
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      GMC           GMC
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_GMC_UNSIGNED(QLIB_CONTEXT_T* qlibContext, GMC_T GMC);

/************************************************************************************************************
 * @brief       This routine returns assured with signature GMC
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      GMC           GMC
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_GMC_SIGNED(QLIB_CONTEXT_T* qlibContext, GMC_T GMC);

/************************************************************************************************************
 * @brief       This routine sets new GMT
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       pGMT          pointer to GMT
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_GMT(QLIB_CONTEXT_T* qlibContext, const GMT_T* pGMT);

/************************************************************************************************************
 * @brief       This routine gets GMT
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      pGMT          pointer to GMT
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_GMT_UNSIGNED(QLIB_CONTEXT_T* qlibContext, GMT_T* pGMT);

/************************************************************************************************************
 * @brief       This routine gets GMT via CALC_SIG command. Session to any section should be opened
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      pGMT          pointer to GMT
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_GMT_SIGNED(QLIB_CONTEXT_T* qlibContext, GMT_T* pGMT);

/************************************************************************************************************
 * @brief       This routine sets watchdog configuration register
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       AWDTCFG       Watchdog configuration
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_AWDT(QLIB_CONTEXT_T* qlibContext, AWDTCFG_T AWDTCFG);

/************************************************************************************************************
 * @brief       This routine gets watchdog configuration register
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      AWDTCFG       Watchdog configuration
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_AWDT_UNSIGNED(QLIB_CONTEXT_T* qlibContext, AWDTCFG_T* AWDTCFG);

/************************************************************************************************************
 * @brief       This routine gets signature assured watchdog configuration register
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      AWDTCFG       Watchdog configuration
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_AWDT_SIGNED(QLIB_CONTEXT_T* qlibContext, AWDTCFG_T* AWDTCFG);

/************************************************************************************************************
 * @brief       This routine performs watchdog touch
 *
 * @param[in,out]   qlibContext   Context
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__AWDT_TOUCH(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This routine configures watchdog with plain-access (if allowed)
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       AWDTCFG       Watchdog configuration
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_AWDT_PLAIN(QLIB_CONTEXT_T* qlibContext, AWDTCFG_T AWDTCFG);

/************************************************************************************************************
 * @brief       This routine performs watchdog touch in plain-access
 *
 * @param[in,out]   qlibContext   Context
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__AWDT_TOUCH_PLAIN(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This routine sets section configuration
 *
 * @param[in,out]   qlibContext    Context
 * @param[in]       sectionIndex   Section index
 * @param[in]       SCRn           SCR
 * @param[in]       reset          Reset the device after section configuration
 * @param[in]       reload         If TRUE,  updated configuration is automatically loaded and takes effect
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_SCRn(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex, const SCRn_T SCRn, BOOL reset, BOOL reload);

/************************************************************************************************************
 * @brief       This routine sets section configuration with swap
 *
 * @param[in,out]   qlibContext    Context
 * @param[in]       sectionIndex   Section index
 * @param[in]       SCRn           SCR
 * @param[in]       reset          Reset the device after swap
 * @param[in]       reload         If TRUE,  updated configuration is automatically loaded and takes effect
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_SCRn_swap(QLIB_CONTEXT_T* qlibContext,
                                           U32             sectionIndex,
                                           const SCRn_T    SCRn,
                                           BOOL            reset,
                                           BOOL            reload);

/************************************************************************************************************
 * @brief       This routine gets SCR
 *
 * @param[in,out]   qlibContext    Context
 * @param[in]       sectionIndex   Section index
 * @param[out]      SCRn           SCR
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_SCRn_UNSIGNED(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex, SCRn_T SCRn);

/************************************************************************************************************
 * @brief       This routine gets SCR via secure CALC_SIG command. Session to any section should be opened
 *
 * @param[in,out]   qlibContext    Context
 * @param[in]       sectionIndex   Section index
 * @param[out]      SCRn           SCR
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_SCRn_SIGNED(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex, SCRn_T SCRn);

/************************************************************************************************************
 * @brief       This routine sets new reset response
 *
 * @param[in,out]   qlibContext     Context
 * @param[in]       is_RST_RESP1    if TRUE, its RESP1 otherwise RESP2
 * @param           RST_RESP_half
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_RST_RESP(QLIB_CONTEXT_T* qlibContext, BOOL is_RST_RESP1, const U32* RST_RESP_half);

/************************************************************************************************************
 * @brief       This routine reads 128B reset response
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      RST_RESP      Reset Response
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_RST_RESP(QLIB_CONTEXT_T* qlibContext, U32* RST_RESP);

/************************************************************************************************************
 * @brief       This routine sets ACLR register
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       ACLR          ACLR
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_ACLR(QLIB_CONTEXT_T* qlibContext, ACLR_T ACLR);

/************************************************************************************************************
 * @brief       This routine gets ACLR register
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      ACLR          ACLR
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_ACLR(QLIB_CONTEXT_T* qlibContext, ACLR_T* ACLR);


/************************************************************************************************************
 * @brief       This routine gets the keys provisioning status
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      status        keys status
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_KEYS_STATUS_UNSIGNED(QLIB_CONTEXT_T* qlibContext, U64* status);

/************************************************************************************************************
 * @brief       This routine returns Flash monotonic counters (TC and DMC)
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      mc            Monotonic Counter
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_MC_UNSIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_MC_T mc);

/************************************************************************************************************
 * @brief       This routine returns signature assured Flash monotonic counters (TC and DMC)
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      mc            Monotonic Counter
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_MC_SIGNED(QLIB_CONTEXT_T* qlibContext, QLIB_MC_T mc);

/************************************************************************************************************
 * @brief       This routine synchronizes MC counters (TC and DMC) from HW to RAM
 *
 * @param[in,out]   qlibContext   Context
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__synch_MC(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This routine performs MC maintenance
 *
 * @param[in,out]   qlibContext   Context
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__MC_MAINT(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This routine is used to open a Session and establish a Secure Channel
 *
 * @param[in,out]   qlibContext         Context
 * @param[in]       kid                 Key ID
 * @param[in]       key                 Key data
 * @param[in]       includeWID          if TRUE, WID is used in session opening
 * @param[in]       ignoreScrValidity   if TRUE, SCR validity is ignored
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__Session_Open(QLIB_CONTEXT_T* qlibContext,
                                          QLIB_KID_T      kid,
                                          const KEY_T     key,
                                          BOOL            includeWID,
                                          BOOL            ignoreScrValidity);

/************************************************************************************************************
 * @brief       This routine is used to close a Session
 *
 * @param[in,out]   qlibContext         Context
 * @param[in]       kid                 Key ID
 * @param[in]       revokePA            If TRUE, revoke plain-access privileges
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__Session_Close(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, BOOL revokePA);

/************************************************************************************************************
 * @brief           This function performs INIT_SECTION_PA command
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       sectionIndex   [Section index](md_definitions.html#DEF_SECTION)
 *
 * @return          QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__init_section_PA(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex);

/************************************************************************************************************
 * @brief           This function performs PA_GRANT_PLAIN command
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       sectionIndex   [Section index](md_definitions.html#DEF_SECTION)
 *
 * @return          QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__PA_grant_plain(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex);

/************************************************************************************************************
 * @brief           This function performs PA_GRANT command
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       kid           Key ID used to authenticate the command (FKi or RKi)
 *
 * @return          QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__PA_grant(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid);

/************************************************************************************************************
 * @brief           This function performs PA_REVOKE command
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       sectionIndex  [Section index](md_definitions.html#DEF_SECTION)
 * @param[in]       revokeType    whether to revoke only write plain access or read access as well
 *
 * @return          QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__PA_revoke(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex, QLIB_PA_REVOKE_TYPE_T revokeType);

/************************************************************************************************************
 * @brief       This routine returns CDI value
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       mode          CDI mode
 * @param[out]      cdi           CDI value
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__CALC_CDI(QLIB_CONTEXT_T* qlibContext, U32 mode, _256BIT cdi);

/************************************************************************************************************
 * @brief       This routine forces integrity check on given section
 *
 * @param[in,out]   qlibContext    Context
 * @param[in]       sectionIndex   Section index
 * @param[in]       verifyDigest   TRUE to verify digest, FALSE for CRC
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__Check_Integrity(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex, BOOL verifyDigest);

/*---------------------------------------------------------------------------------------------------------*/
/*                                        SECURE TRANSPORT COMMANDS                                        */
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This routine returns TC counter
 *
 * @param[in,out]   qlibContext     Context
 * @param[out]      tc              Transaction Counter
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_TC(QLIB_CONTEXT_T* qlibContext, U32* tc);

/************************************************************************************************************
 * @brief       This routine returns signed data of registers or sector hash
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       dataType      Data type
 * @param[in]       section       Section number
 * @param[in]       params        params - [Start Address Offset, length] - Optional parameters for some data types
 * @param[in]       paramsSize    size of params
 * @param[out]      data          Data output or NULL
 * @param[in]       size          Output data size
 * @param[out]      signature     Output signature or NULL
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__CALC_SIG(QLIB_CONTEXT_T*         qlibContext,
                                      QLIB_SIGNED_DATA_TYPE_T dataType,
                                      U32                     section,
                                      _64BIT                  params,
                                      U32                     paramsSize,
                                      void*                   data,
                                      U32                     size,
                                      _64BIT                  signature);

/************************************************************************************************************
 * @brief       This routine securely reads the 128b RNGR register, that includes a 32b random number
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      rngr       Read data buffer
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_RNGR_SEC(QLIB_CONTEXT_T* qlibContext, RNGR_T rngr);

/************************************************************************************************************
 * @brief       This routine reads the 128b RNGR register, that includes a 32b random number
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      rngr       Read data buffer
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_RNGR_PLAIN(QLIB_CONTEXT_T* qlibContext, RNGR_T rngr);

/************************************************************************************************************
 * @brief       This routine reads the 96 msb of the RNGR register, that include the entropy counters
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      rngr_cnt      RNGR counters
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_RNGR_COUNTER(QLIB_CONTEXT_T* qlibContext, RNGR_CNT_T rngr_cnt);

/************************************************************************************************************
 * @brief       This routine performs secure read
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       addr          Address
 * @param[out]      data32B       Read data buffer
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__SRD(QLIB_CONTEXT_T* qlibContext, U32 addr, U32* data32B);

/************************************************************************************************************
 * @brief       This routine performs secure authenticated read
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       addr          Address
 * @param[out]      data32B       Read data buffer
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__SARD(QLIB_CONTEXT_T* qlibContext, U32 addr, U32* data32B);

#ifdef QLIB_SIGN_DATA_BY_FLASH
/************************************************************************************************************
 * @brief   This function sends a raw encrypted address and collects the signature
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       enc_addr      Encrypted address
 * @param[out]      signature     Output signature
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__SARD_Sign(QLIB_CONTEXT_T* qlibContext, U32 enc_addr, _64BIT signature);

/************************************************************************************************************
 * @brief   This function returns expected signature for the given encrypted address assuming 0xFF data
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       enc_addr      Encrypted address
 * @param[out]      signature     Output signature
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__SARD_Verify(QLIB_CONTEXT_T* qlibContext, U32 enc_addr, _64BIT signature);
#endif

#ifndef QLIB_SUPPORT_XIP
/************************************************************************************************************
 * @brief       This routine performs multi-block secure read
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       addr          Address
 * @param[out]      data          Read data buffer
 * @param[in]       size          Read data size in bytes
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__SRD_Multi(QLIB_CONTEXT_T* qlibContext, U32 addr, U32* data, U32 size);

/************************************************************************************************************
 * @brief       This routine performs multi-block secure authenticated read
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       addr          Address
 * @param[out]      data          Read data buffer
 * @param[in]       size          Read data size in bytes
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__SARD_Multi(QLIB_CONTEXT_T* qlibContext, U32 addr, U32* data, U32 size);
#endif // QLIB_SUPPORT_XIP

/************************************************************************************************************
 * @brief       This routine performs secure authenticated write
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       addr          Address
 * @param[in]       data          Data to write (32Bytes)
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__SAWR(QLIB_CONTEXT_T* qlibContext, U32 addr, const U32* data);

/************************************************************************************************************
 * @brief       This routine performs secure read of 16 Bytes from the secure log
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      addr          Address
 * @param[out]      data16B       Read data buffer
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__LOG_SRD(QLIB_CONTEXT_T* qlibContext, U32* addr, U32* data16B);

/************************************************************************************************************
 * @brief       This routine performs plain text read of 16 Bytes from the secure log
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       section       Section number
 * @param[out]      addr          Address
 * @param[out]      data16B       Read data buffer
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__LOG_PRD(QLIB_CONTEXT_T* qlibContext, U32 section, U32* addr, U32* data16B);

/************************************************************************************************************
 * @brief       This routine performs secure authenticated write of 16 Bytes from the secure log
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       data16B       Write data buffer
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__LOG_SAWR(QLIB_CONTEXT_T* qlibContext, U32* data16B);

/************************************************************************************************************
 * @brief       This routine performs plain text write of 16 Bytes from the secure log
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       section       Section number
 * @param[in]       data16B       Write data buffer
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__LOG_PWR(QLIB_CONTEXT_T* qlibContext, U32 section, U32* data16B);

/************************************************************************************************************
 * @brief       This routine calculates the CRC-32 checksum of a memory section part
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       section       Section number
 * @param[in]       addr          Address
 * @param[in]       len           Length of the memory range
 * @param[out]      crc           Calculated CRC
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__MEM_CRC(QLIB_CONTEXT_T* qlibContext, U32 section, U32 addr, U32 len, U32* crc);

/************************************************************************************************************
 * @brief       This routine performs all kind of secure erase commands
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       type          Secure erase type
 * @param[in]       addr          Secure erase address
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__SERASE(QLIB_CONTEXT_T* qlibContext, QLIB_ERASE_T type, U32 addr);

/************************************************************************************************************
 * @brief       This function copy a range of memory within a single Section
 *
 * @param       qlibContext-   QLIB state object
 * @param       dest           Starting address offset of destination memory range to copy to
 * @param       src            Starting address offset of source memory range to copy from
 * @param       len            Length of memory range to copy
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__MEM_COPY(QLIB_CONTEXT_T* qlibContext, U32 dest, U32 src, U32 len);

/************************************************************************************************************
 * @brief       This routine performs non-secure (plain) sector erase
 *
 * @param[in,out]   qlibContext     Context
 * @param[in]       sectionIndex    sector index
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__ERASE_SECT_PLAIN(QLIB_CONTEXT_T* qlibContext, U32 sectionIndex);

#ifndef EXCLUDE_LMS
/************************************************************************************************************
 * @brief       This routine writes one page of the LMS command data
 *
 * @param[in,out]   qlibContext Context
 * @param[in]       kid         ID of the LMS key used
 * @param[in]       offset      byte offset of the page in the LMS data structure
 * @param[in]       data        page data
 * @param[in]       dataSize    page data size (up to 256B)
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__LMS_write(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, U32 offset, const U32* data, U32 dataSize);

/************************************************************************************************************
 * @brief       This routine initiate execution of an LMS Command, after the entire LMS Command Data Structure was received by the W77H
 *
 * @param[in,out]   qlibContext Context
 * @param[in]       kid         ID of the LMS key used
 * @param[in]       digest      digest of the LMS Command Data Structure
 * @param[in]       reset       Reset the device after section configuration
 * @param[in]       grantPA     Initialize PA to the section after section configuration
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__LMS_execute(QLIB_CONTEXT_T* qlibContext,
                                         QLIB_KID_T      kid,
                                         const _64BIT    digest,
                                         BOOL            reset,
                                         BOOL            grantPA);
#endif

/************************************************************************************************************
 * @brief       This routine securely programs one of the long-term LMS Root Keys to the device.
 *
 * @param[in,out]   qlibContext  Context
 * @param[in]       kid          ID of the LMS key used
 * @param[in]       publicKey    the public key which includes the Key ID Tag and the long-term LMS Root Key
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__LMS_set_KEY(QLIB_CONTEXT_T* qlibContext, QLIB_KID_T kid, const QLIB_LMS_KEY_T publicKey);

/*---------------------------------------------------------------------------------------------------------*/
/*                                              AUX COMMANDS                                               */
/*---------------------------------------------------------------------------------------------------------*/
/************************************************************************************************************
 * @brief       This routine returns HW version
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      hwVer        HW version
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_HW_VER_UNSIGNED(QLIB_CONTEXT_T* qlibContext, HW_VER_T* hwVer);

/************************************************************************************************************
 * @brief       This routine returns HW version assured by signature
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      hwVer         HW version
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_HW_VER_SIGNED(QLIB_CONTEXT_T* qlibContext, HW_VER_T* hwVer);

/************************************************************************************************************
 * @brief       This routine force-triggers the watchdog timer
 *
 * @param[in,out]   qlibContext         Context
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__AWDT_expire(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This routine puts the Flash into sleep mode
 *
 * @param[in,out]       qlibContext   Context
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__sleep(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This routine check if there is some error in the last SSR reed from flash and return it
 *              This routine do not reads SSR rather use a local version from last command
 *              Note: if an error found, this routine automatically execute OP1 to clear the error bits
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       mask          SSR mask
 *
 * @return      QLIB_STATUS__OK if no error occurred (according to ssr error bits)
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__checkLastSsrErrors(QLIB_CONTEXT_T* qlibContext, U32 mask) __RAM_SECTION;


/************************************************************************************************************
 * @brief       This routine executes standard command using TM interface
 *
 * @param[in]   qlibContext       pointer to qlib context
 * @param[in]   format            SPI transaction format
 * @param[in]   dtr               If TRUE, SPI transaction is using DTR mode
 * @param[in]   needWriteEnable   If TRUE, WriteEnable command is sent prior to the given command
 * @param[in]   waitWhileBusy     If TRUE, busy-wait polling is performed after exec. of command
 * @param[in]   cmd               Command value
 * @param[in]   address           Pointer to address value or NULL if no address available
 * @param[in]   writeData         Pointer to output data or NULL if no output data available
 * @param[in]   writeDataSize     Size of the output data
 * @param[in]   dummyCycles       Delay Cycles between output and input command phases
 * @param[out]  readData          Pointer to input data or NULL if no input data required
 * @param[in]   readDataSize      Size of the input data
 * @param[out]  ssr              Pointer to status register following the transaction
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
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
                                            QLIB_REG_SSR_T* ssr) __RAM_SECTION;

/************************************************************************************************************
 * @brief       This routine reads the ECC status register
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      eccr          ECC status register
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_ECCR(QLIB_CONTEXT_T* qlibContext, STD_FLASH_ECC_STATUS_T* eccr);

/************************************************************************************************************
 * @brief       This routine sets the ECC status register
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       eccr          ECC status register
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_ECCR(QLIB_CONTEXT_T* qlibContext, STD_FLASH_ECC_STATUS_T eccr);

/************************************************************************************************************
 * @brief       This routine reads the Advanced ECC Register
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      aeccr         Advanced ECC register
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_AECCR(QLIB_CONTEXT_T* qlibContext, STD_FLASH_ADVANCED_ECC_T* aeccr);

/************************************************************************************************************
 * @brief       This routine clears the Extended Address Register (EAR)
 *
 * @param[in,out]   qlibContext   Context
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__clear_AECCR(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
 * @brief       This routine reads the Extended Address Register (EAR)
 *
 * @param[in,out]   qlibContext   Context
 * @param[out]      extAddr       Extended Address Register
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_EAR(QLIB_CONTEXT_T* qlibContext, U8* extAddr);

/************************************************************************************************************
 * @brief       This routine sets the Extended Address Register (EAR)
 *
 * @param[in,out]   qlibContext   Context
 * @param[in]       extAddr       Extended Address Register
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_EAR(QLIB_CONTEXT_T* qlibContext, U8 extAddr);

/************************************************************************************************************
 * @brief       This routine returns the value of the volatile Extended Configuration Registers
 *
 * @param       qlibContext   qlib context object
 * @param[in]   addr          Configuration register address
 * @param[out]  cr            The value of the configuration register
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_CR(QLIB_CONTEXT_T* qlibContext, U32 addr, U8* cr) __RAM_SECTION;

/************************************************************************************************************
 * @brief       This routine returns the value of the non-volatile Extended Configuration Register Default
 *
 * @param       qlibContext   qlib context object
 * @param[in]   addr          Configuration register address
 * @param[out]  cr            The value of the configuration register default
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_default_CR(QLIB_CONTEXT_T* qlibContext, U32 addr, U8* cr);

/************************************************************************************************************
 * @brief       This routine sets the value of the volatile Extended Configuration Register
 *
 * @param       qlibContext   qlib context object
 * @param[in]   addr          Configuration register address
 * @param[in]   cr            The value of the configuration register
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_CR(QLIB_CONTEXT_T* qlibContext, U32 addr, U8 cr) __RAM_SECTION;

/************************************************************************************************************
 * @brief       This routine sets the value of the non-volatile Extended Configuration Register Default
 *
 * @param       qlibContext   qlib context object
 * @param[in]   addr          Configuration register address
 * @param[in]   cr            The value of the configuration register
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__set_default_CR(QLIB_CONTEXT_T* qlibContext, U32 addr, U8 cr);

/************************************************************************************************************
 * @brief       This routine returns the value of the Flag Register
 *
 * @param       qlibContext   qlib context object
 * @param[out]  fr            The value of the flag register
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__get_FR(QLIB_CONTEXT_T* qlibContext, STD_FLASH_FLAGS_T* fr);

/************************************************************************************************************
 * @brief       This routine clears the value of the Flag Register
 *
 * @param       qlibContext   qlib context object
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__clear_FR(QLIB_CONTEXT_T* qlibContext);

#ifndef EXCLUDE_LMS_ATTESTATION
/************************************************************************************************************
 * @brief       This routine provisions the OTS Non-Volatile Root Key and the Key Identifier Tag
 *
 * @param       qlibContext   qlib context object
 * @param[in]   seed          private key seed
 * @param[in]   keyId         key ID
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__OTS_SET_KEY(QLIB_CONTEXT_T*                 qlibContext,
                                         const LMS_ATTEST_PRIVATE_SEED_T seed,
                                         const LMS_ATTEST_KEY_ID_T       keyId);

/************************************************************************************************************
 * @brief       This routine initializes the OTS function before starting a new OTS calculation
 *
 * @param       qlibContext   qlib context object
 * @param[out]  leafNum       Leaf number
 * @param[out]  keyId         key ID
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__OTS_INIT(QLIB_CONTEXT_T* qlibContext, U32* leafNum, LMS_ATTEST_KEY_ID_T keyId);

/************************************************************************************************************
 * @brief       This routine reads the Key-Identifier-Tag and the leaf-index, without affecting any of the internal states
 *
 * @param       qlibContext   qlib context object
 * @param[out]  leafNum       Leaf number
 * @param[out]  keyId         key ID
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__OTS_GET_ID(QLIB_CONTEXT_T* qlibContext, U32* leafNum, LMS_ATTEST_KEY_ID_T keyId);

/************************************************************************************************************
 * @brief       This routine calculates the OTS Digit Signature
 *
 * @param       qlibContext   qlib context object
 * @param[in]   digit         message digit
 * @param[out]  chainIndex    digit index
 * @param[out]  digitSig      OTS signature chunk
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__OTS_SIGN_DIGIT(QLIB_CONTEXT_T* qlibContext, U8 digit, U32* chainIndex, LMS_ATTEST_CHUNK_T digitSig);

/************************************************************************************************************
 * @brief       This function gets the OTS public key chunk for a given leaf index and digit index
 *
 * @param       qlibContext   qlib context object
 * @param[in]   leafNum       Leaf number
 * @param[in]   chainIndex    Digit index
 * @param[out]  pubChunk      chunk of the public key corresponding to the leafId and chainId
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_CMD_PROC__OTS_PUB_CHAIN(QLIB_CONTEXT_T* qlibContext, U32 leafNum, U16 chainIndex, _192BIT pubChunk);
#endif

#ifdef __cplusplus
}
#endif

#endif // __QLIB_CMD_PROC_H__
