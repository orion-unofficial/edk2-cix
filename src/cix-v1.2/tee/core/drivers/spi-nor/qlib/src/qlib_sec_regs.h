/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sec_regs.h
* @brief      This file contains secure register definitions
*
* ### project qlib
*
************************************************************************************************************/
#ifndef _QLIB_SEC_REGS_H_
#define _QLIB_SEC_REGS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib.h"

// clang-format off
#ifdef SWIG
#ifdef _WIN32
#pragma warning(disable : 305)
#pragma warning(disable : 462)
#else
#pragma SWIG nowarn=305,462
#endif // _WIN32
#endif // SWIG
// clang-format on

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 DEFINES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_NUM_OF_MAIN_SECTIONS     8UL // main array sections 0..7
#define QLIB_NUM_OF_SECTIONS          9UL // all sections (including the vault)
#define QLIB_SEC_READ_PAGE_SIZE_BYTE  _32B_
#define QLIB_SEC_WRITE_PAGE_SIZE_BYTE _32B_
#define QLIB_MIN_SECTION_SIZE         _512KB_
#define QLIB_MAX_SECTION_SIZE         _16MB_
#define QLIB_SEC_LMS_PAGE_WORD_SIZE   16UL
#define QLIB_SEC_LMS_PAGE_MAX_SIZE    _256B_
#define QLIB_SEC_LOG_ENTRY_SIZE       _16B_

#define QLIB_SECTION_ID_0     0UL
#define QLIB_SECTION_ID_1     1UL
#define QLIB_SECTION_ID_2     2UL
#define QLIB_SECTION_ID_3     3UL
#define QLIB_SECTION_ID_4     4UL
#define QLIB_SECTION_ID_5     5UL
#define QLIB_SECTION_ID_6     6UL
#define QLIB_SECTION_ID_7     7UL
#define QLIB_SECTION_ID_VAULT 8UL
/*---------------------------------------------------------------------------------------------------------*/
/************************************************************************************************************
 * Section Mapping Register (SMRn) conversions
************************************************************************************************************/
#define QLIB_REG_SMRn__BASE_IN_BYTES_TO_TAG(baseBytes) ((baseBytes) >> 16u)
#define QLIB_REG_SMRn__BASE_IN_TAG_TO_BYTES(baseTag)   (((U32)(baseTag)) << 16u)
#define QLIB_REG_SMRn__LEN_IN_BYTES_TO_SCALE(lenBytes) (((0u == (lenBytes)) || ((lenBytes) > _1MB_)) ? 0u : 1u)
#define QLIB_REG_SMRn__LEN_IN_BYTES_TO_TAG(qlibContext, lenBytes)                                                 \
    ((W77Q_SEC_SIZE_SCALE(qlibContext) != 0u)                                                                     \
         ? ((0u == (lenBytes)) ? 0u : (((lenBytes) >> (((lenBytes) > _1MB_) ? LOG2(_1MB_) : LOG2(_64KB_))) - 1u)) \
         : (LOG2((lenBytes) >> 16u)))
#define QLIB_REG_SMRn__LEN_IN_TAG_TO_BYTES(qlibContext, lenTag, scale)                                           \
    ((W77Q_SEC_SIZE_SCALE((qlibContext)) != 0u)                                                                  \
         ? (((0u == (lenTag)) && (0u == (scale))) ? 0u : (((0u == (scale)) ? _1MB_ : _64KB_) * (1u + (lenTag)))) \
         : (_64KB_ << MIN((U8)(lenTag), (U8)QLIB_REG_SMRn__MAX_LEN((qlibContext)))))

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                  TYPES                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

#define ERASE_T U32 // follow 11.5.6

/*---------------------------------------------------------------------------------------------------------*/
/* Registers Types                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/
typedef U32 GMC_T[5]; ///< 160b

typedef union GMT_T
{
    U32 asArray[(QLIB_NUM_OF_MAIN_SECTIONS * sizeof(U16)) / sizeof(U32) + 1u];
    PACKED_START
    struct
    {
        U16 sectionMapReg[QLIB_NUM_OF_MAIN_SECTIONS];
        U32 version;
    } PACKED asStruct;
    PACKED_END
} GMT_T;

typedef U32 DEVCFG_T;  ///< 32b
typedef U32 AWDTCFG_T; ///< 32b
typedef U32 AWDTSR_T;  ///< 32b
typedef U32 SCRn_T[5]; ///< 160b
typedef U32 ACLR_T;    ///< 32b
typedef U32 SSPRn_T;  ///< 32b
typedef U32 HW_VER_T; ///< 32b
typedef U32 ACL_STATUS_T[QLIB_NUM_OF_SECTIONS];

// RNGR registers (in case W77Q_RNG_FEATURE supported)
typedef U32 RNGR_T[4];     ///< 128b
typedef U32 RNGR_CNT_T[3]; ///< 96b
/*---------------------------------------------------------------------------------------------------------*/
/* Section Mapping Register (SMRn) fields                                                                  */
/*---------------------------------------------------------------------------------------------------------*/
// SMRn fields (base [0:11], len [12:14], enable [15:15]) in W77Q   (16Mb to 32Mb)  MCD device
// SMRn fields (base [0:10], len [11:14], enable [15:15]) in W77Q   (64Mb to 128Mb) HCD device
// SMRn fields (base [0:10], len [11:14], scale  [15:15]) in W77Q/T (256Mb to 1Gb)  device
#define QLIB_REG_SMRn__BASE_START_BIT             (0u)
#define QLIB_REG_SMRn__BASE_NUM_BITS(qlibContext) (12u - Q2_EXTEND_SEC_LEN_BITS(qlibContext))

#define QLIB_REG_SMRn__BASE(qlibContext)    QLIB_REG_SMRn__BASE_START_BIT, QLIB_REG_SMRn__BASE_NUM_BITS((qlibContext))
#define QLIB_REG_SMRn__LEN(qlibContext)     QLIB_REG_SMRn__BASE_NUM_BITS((qlibContext)), (3u + Q2_EXTEND_SEC_LEN_BITS((qlibContext)))
#define QLIB_REG_SMRn__MAX_LEN(qlibContext) ((1u << (3u + Q2_EXTEND_SEC_LEN_BITS((qlibContext)))) - 1u)
#define QLIB_REG_SMRn__ENABLE               15u, 1u
#define QLIB_REG_SMRn__SCALE                15u, 1u

/*---------------------------------------------------------------------------------------------------------*/
/* Global Mapping Table (GMT) fields and access                                                            */
/*---------------------------------------------------------------------------------------------------------*/
// GMT fields (SMR(i) [(16*i):(16*i+15)] , version [128:159])
#define QLIB_REG_GMT_IS_CONFIGURED(gmt) ((gmt).asStruct.sectionMapReg[0] != 0xffffu)

#define QLIB_REG_GMT_GET_VER(gmt)      ((gmt).asStruct.version)
#define QLIB_REG_GMT_SET_VER(gmt, val) ((gmt).asStruct.version = (val))

#define QLIB_REG_GMT_GET_SMRn(gmt, section)      ((gmt).asStruct.sectionMapReg[(section)])
#define QLIB_REG_GMT_SET_SMRn(gmt, section, val) ((gmt).asStruct.sectionMapReg[(section)] = (val))

#define QLIB_REG_GMT_GET_BASE(qlibContext, gmt, section) \
    READ_VAR_FIELD(QLIB_REG_GMT_GET_SMRn(gmt, section), QLIB_REG_SMRn__BASE(qlibContext))
#define QLIB_REG_GMT_GET_LEN(qlibContext, gmt, section) \
    READ_VAR_FIELD(QLIB_REG_GMT_GET_SMRn(gmt, section), QLIB_REG_SMRn__LEN(qlibContext))
#define QLIB_REG_GMT_GET_ENABLE(gmt, section) READ_VAR_FIELD(QLIB_REG_GMT_GET_SMRn(gmt, section), QLIB_REG_SMRn__ENABLE)
#define QLIB_REG_GMT_GET_SCALE(gmt, section)  READ_VAR_FIELD(QLIB_REG_GMT_GET_SMRn(gmt, section), QLIB_REG_SMRn__SCALE)

#define QLIB_REG_GMT_SET_BASE(qlibContext, gmt, section, val)                                                  \
    {                                                                                                          \
        U8 QLIB_REG_GMT_SET_BASE_temp           = QLIB_REG_SMRn__BASE_NUM_BITS(qlibContext);                   \
        (gmt).asStruct.sectionMapReg[(section)] = (U16)_SET_FIELD_VAL((gmt).asStruct.sectionMapReg[(section)], \
                                                                      QLIB_REG_GMT_SET_BASE_temp,              \
                                                                      QLIB_REG_SMRn__BASE_START_BIT,           \
                                                                      (val));                                  \
    }

#define QLIB_REG_GMT_SET_LEN(qlibContext, gmt, section, val) \
    SET_VAR_FIELD_16(QLIB_REG_GMT_GET_SMRn(gmt, section), QLIB_REG_SMRn__LEN(qlibContext), val)
#define QLIB_REG_GMT_SET_ENABLE(gmt, section, val) \
    SET_VAR_FIELD_16(QLIB_REG_GMT_GET_SMRn(gmt, section), QLIB_REG_SMRn__ENABLE, val)
#define QLIB_REG_GMT_SET_SCALE(gmt, section, val) SET_VAR_FIELD_16(QLIB_REG_GMT_GET_SMRn(gmt, section), QLIB_REG_SMRn__SCALE, val)

/*---------------------------------------------------------------------------------------------------------*/
/* Section Security Policy Register (SSPRn) fields                                                         */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_SSPRn__AUTH_CFG     0u, 1u
#define QLIB_REG_SSPRn__INTEGRITY_AC 1u, 1u
#define QLIB_REG_SSPRn__WP_EN        2u, 1u
#define QLIB_REG_SSPRn__ROLLBACK_EN  3u, 1u
#define QLIB_REG_SSPRn__PA_RD_EN     4u, 1u
#define QLIB_REG_SSPRn__PA_WR_EN     5u, 1u
#define QLIB_REG_SSPRn__AUTH_PA      6u, 1u
#define QLIB_REG_SSPRn__AUTH_AC      7u, 1u
#define QLIB_REG_SSPRn__SLOG         8u, 1u

/*---------------------------------------------------------------------------------------------------------*/
/* Section Configuration Registers (SCRn) fields                                                           */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_SCRn_GET_SSPRn(SCRn)         (((U32*)(SCRn))[0])
#define QLIB_REG_SCRn_SET_SSPRn(SCRn, val)    ((((U32*)(SCRn))[0]) = (val))
#define QLIB_REG_SCRn_GET_CHECKSUM(SCRn)      (((U32*)(SCRn))[1])
#define QLIB_REG_SCRn_GET_CHECKSUM_PTR(SCRn)  &(SCRn)[1]
#define QLIB_REG_SCRn_SET_CHECKSUM(SCRn, val) ((((U32*)(SCRn))[1]) = (val))
#define QLIB_REG_SCRn_GET_DIGEST(SCRn)        (((U64)((SCRn)[2])) | (((U64)((SCRn)[3])) << 32u))
#define QLIB_REG_SCRn_GET_DIGEST_PTR(SCRn)    &(SCRn)[2]
#define QLIB_REG_SCRn_SET_DIGEST(SCRn, val)                     \
    {                                                           \
        (SCRn)[2] = (U32)(((U64)(val)) & 0xFFFFFFFFU);          \
        (SCRn)[3] = (U32)((((U64)(val)) >> 32U) & 0xFFFFFFFFU); \
    }
#define QLIB_REG_SCRn_GET_VER(SCRn)      (((U32*)(SCRn))[4])
#define QLIB_REG_SCRn_SET_VER(SCRn, val) ((((U32*)(SCRn))[4]) = (val))

/*---------------------------------------------------------------------------------------------------------*/
/* Global Memory Configuration Register (GMC)                                                              */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_GMC_GET_AWDT_DFLT(gmc)      (((U32*)(gmc))[0])
#define QLIB_REG_GMC_SET_AWDT_DFLT(gmc, val) ((((U32*)(gmc))[0]) = (val))
#define QLIB_REG_GMC_GET_DEVCFG(gmc)         (((U32*)(gmc))[1])
#define QLIB_REG_GMC_SET_DEVCFG(gmc, val)    ((((U32*)(gmc))[1]) = (val))
#define QLIB_REG_GMC_GET_VER(gmc)            (((U32*)(gmc))[4])
#define QLIB_REG_GMC_SET_VER(gmc, val)       ((((U32*)(gmc))[4]) = (val))

/*---------------------------------------------------------------------------------------------------------*/
/* Authenticated Watchdog Timer Configuration (AWDTCFG) fields                                             */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_AWDTCFG__AWDT_EN       0u, 1u
#define QLIB_REG_AWDTCFG__LFOSC_EN      1u, 1u
#define QLIB_REG_AWDTCFG__SRST_EN       2u, 1u
#define QLIB_REG_AWDTCFG__AUTH_WDT      3u, 1u
#define QLIB_REG_AWDTCFG__RSTO_EN       4u, 1u
#define QLIB_REG_AWDTCFG__RSTI_OVRD     5u, 1u
#define QLIB_REG_AWDTCFG__RSTI_EN       6u, 1u
#define QLIB_REG_AWDTCFG__RST_IN_EN     7u, 1u
#define QLIB_REG_AWDTCFG__KID           8u, 4u
#define QLIB_REG_AWDTCFG__TH            12u, 5u
#define QLIB_REG_AWDTCFG__FB_EN         17u, 1u
#define QLIB_REG_AWDTCFG__RESERVED1     18u, 2u
#define QLIB_REG_AWDTCFG__OSC_RATE_FRAC 20u, 4u
#define QLIB_REG_AWDTCFG__OSC_RATE_KHZ  24u, 7u
#define QLIB_REG_AWDTCFG__LOCK          31u, 1u

#define QLIB_REG_AWDTCFG_RESERVED_MASK (MASK_FIELD(QLIB_REG_AWDTCFG__RESERVED1))

#define QLIB_AWDTCFG__OSC_RATE_KHZ_DEFAULT 54u

/*---------------------------------------------------------------------------------------------------------*/
/* Authenticated Watchdog Timer Status Register (AWDTSR) fields                                            */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_AWDTSR__AWDT_VAL(qlibContext) 0u, (20u + (2u * W77Q_AWDT_VAL_SEC_FRACT(qlibContext)))
#define QLIB_REG_AWDTSR__AWDT_RES(qlibContext) \
    (20u + (2u * W77Q_AWDT_VAL_SEC_FRACT(qlibContext))), (11u - (2u * W77Q_AWDT_VAL_SEC_FRACT(qlibContext)))
#define QLIB_REG_AWDTSR__AWDT_EXP_S 31u, 1u

/*---------------------------------------------------------------------------------------------------------*/
/* Device Configurations (DEVCFG) fields                                                                   */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_DEVCFG__SECT_SEL      0u, 3u
#define QLIB_REG_DEVCFG__RESERVED_1    3u, 1u
#define QLIB_REG_DEVCFG__RST_RESP_EN   4u, 1u
#define QLIB_REG_DEVCFG__FB_EN         5u, 1u
#define QLIB_REG_DEVCFG__CK_SPECUL     6u, 1u
#define QLIB_REG_DEVCFG__RESERVED_2    7u, 1u ///< Reserved. Write as 0.
#define QLIB_REG_DEVCFG__FORMAT_EN     8u, 1u
#define QLIB_REG_DEVCFG__STM_EN        9u, 1u
#define QLIB_REG_DEVCFG__BOOT_FAIL_RST 10u, 1u
#define QLIB_REG_DEVCFG__RESERVED_3    11u, 2u
#define QLIB_REG_DEVCFG__CTAG_MODE     13u, 1u
#define QLIB_REG_DEVCFG__VAULT_MEM     14u, 2u
#define QLIB_REG_DEVCFG__RNG_PA_EN     16u, 1u
#define QLIB_REG_DEVCFG__RNG_TH        17u, 1u ///< Reserved. Write as 1.
#define QLIB_REG_DEVCFG__RESERVED_4    18u, 2u ///< Reserved. Write as 1.
#define QLIB_REG_DEVCFG__RST_PA        20u, 8u
#define QLIB_REG_DEVCFG__RESERVED_5    28u, 3u ///< Reserved.
#define QLIB_REG_DEVCFG__CFG_LOCK      31u, 1u

#define QLIB_REG_DEVCFG_RESERVED_ZERO_MASK                                               \
    (MASK_FIELD(QLIB_REG_DEVCFG__RESERVED_1) | MASK_FIELD(QLIB_REG_DEVCFG__RESERVED_2) | \
     MASK_FIELD(QLIB_REG_DEVCFG__RESERVED_3) | MASK_FIELD(QLIB_REG_DEVCFG__RESERVED_5))

#define QLIB_REG_DEVCFG_RESERVED_ONE_MASK (MASK_FIELD(QLIB_REG_DEVCFG__RESERVED_4) | MASK_FIELD(QLIB_REG_DEVCFG__RNG_TH))
/*---------------------------------------------------------------------------------------------------------*/
/* HW Version Register (HW_VER) fields                                                                     */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_HW_VER__REVISION   0u, 4u
#define QLIB_REG_HW_VER__HASH_VER   4u, 4u
#define QLIB_REG_HW_VER__SEC_VER    8u, 8u
#define QLIB_REG_HW_VER__FLASH_SIZE 16u, 4u
#define QLIB_REG_HW_VER__FLASH_VER  20u, 4u
#define QLIB_REG_HW_VER__RESERVED_1 24u, 7u
#define QLIB_REG_HW_VER__DB         31u, 1u

/*---------------------------------------------------------------------------------------------------------*/
/* ACLR fields                                                                                             */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_ACLR__WR_LOCK       0u, 8u
#define QLIB_REG_ACLR__WR_LOCK_VAULT 8u, 1u
#define QLIB_REG_ACLR__RESERVED_1    9u, 7u
#define QLIB_REG_ACLR__RD_LOCK       16u, 8u
#define QLIB_REG_ACLR__RD_LOCK_VAULT 24u, 1u
#define QLIB_REG_ACLR__RESERVED_2    25u, 7u

#define QLIB_REG_ACLR_RESERVED_MASK (MASK_FIELD(QLIB_REG_ACLR__RESERVED_1) | MASK_FIELD(QLIB_REG_ACLR__RESERVED_2))


/*---------------------------------------------------------------------------------------------------------*/
/* Random Number Generator Register(RNGR) - for devices that support W77Q_RNG_FEATURE                      */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_RNGR_GET_RND(rngr)       (((U32*)(rngr))[0])
#define QLIB_REG_RNGR_GET_CLOCK_CNT(rngr) (((U32*)(rngr))[1])
#define QLIB_REG_RNGR_GET_EVENT_CNT(rngr) (((U32*)(rngr))[2])

/*---------------------------------------------------------------------------------------------------------*/
/* SSR ('_S' is sticky field)                                                                              */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_SSR__BUSY         0u, 1u
#define QLIB_REG_SSR__ERR          2u, 1u
#define QLIB_REG_SSR__SES_READY    4u, 1u
#define QLIB_REG_SSR__RESP_READY   5u, 1u
#define QLIB_REG_SSR__POR          6u, 1u
#define QLIB_REG_SSR__FB_REMAP     7u, 1u
#define QLIB_REG_SSR__AWDT_EXP     8u, 1u
#define QLIB_REG_SSR__SES_ERR_S    10u, 1u
#define QLIB_REG_SSR__INTG_ERR_S   12u, 1u
#define QLIB_REG_SSR__AUTH_ERR_S   13u, 1u
#define QLIB_REG_SSR__PRIV_ERR_S   14u, 1u
#define QLIB_REG_SSR__IGNORE_ERR_S 15u, 1u
#define QLIB_REG_SSR__SYS_ERR_S    16u, 1u
#define QLIB_REG_SSR__FLASH_ERR_S  17u, 1u
#define QLIB_REG_SSR__MC_ERR       19u, 1u
#define QLIB_REG_SSR__MC_MAINT     20u, 2u
#define QLIB_REG_SSR__SUSPEND_E    22u, 1u
#define QLIB_REG_SSR__SUSPEND_W    23u, 1u
#define QLIB_REG_SSR__STATE        24u, 3u
#define QLIB_REG_SSR__FULL_PRIV    27u, 1u
#define QLIB_REG_SSR__KID          28u, 4u

#define SSR__RESP_READY_BIT MASK_FIELD(QLIB_REG_SSR__RESP_READY)
#define SSR__BUSY_BIT       MASK_FIELD(QLIB_REG_SSR__BUSY)
#define SSR__IGNORE_BIT     MASK_FIELD(QLIB_REG_SSR__IGNORE_ERR_S)
#define SSR__ERR_BIT        MASK_FIELD(QLIB_REG_SSR__ERR)

/*---------------------------------------------------------------------------------------------------------*/
/* QLIB_REG_SSR__STATE values                                                                              */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_SSR__STATE_IN_RESET     0u
#define QLIB_REG_SSR__STATE_WORKING      2u
#define QLIB_REG_SSR__STATE_LOCKED       4u
#define QLIB_REG_SSR__STATE_WORKING_MASK 6u

/*---------------------------------------------------------------------------------------------------------*/
/* SSR bit-field structure                                                                                 */
/*---------------------------------------------------------------------------------------------------------*/
typedef struct
{
    U32 BUSY : 1U;
    U32 RESERVED_0 : 1U;
    U32 ERR : 1U;
    U32 RESERVED_1 : 1U;

    U32 SES_READY : 1U;
    U32 RESP_READY : 1U;
    U32 POR : 1U;
    U32 FB_REMAP : 1U;

    U32 AWDT_EXP : 1U;
    U32 RESERVED_2 : 1U;
    U32 SES_ERR_S : 1U;
    U32 RESERVED_3 : 1U;

    U32 INTG_ERR_S : 1U;
    U32 AUTH_ERR_S : 1U;
    U32 PRIV_ERR_S : 1U;
    U32 IGNORE_ERR_S : 1U;

    U32 SYS_ERR_S : 1U;
    U32 FLASH_ERR_S : 1U;
    U32 RESERVED_4 : 1U;
    U32 MC_ERR : 1U;

    U32 MC_MAINT : 2U;
    U32 SUSPEND_E : 1U;
    U32 SUSPEND_W : 1U;

    U32 STATE : 3U;
    U32 FULL_PRIV : 1U;

    U32 KID : 4U;
} QLIB_REG_SSR_STRUCT_T;

/*---------------------------------------------------------------------------------------------------------*/
/* SSR bitfield-U32 union                                                                                  */
/*---------------------------------------------------------------------------------------------------------*/
typedef union
{
    U32                   asUint;
    QLIB_REG_SSR_STRUCT_T asStruct;
} QLIB_REG_SSR_T;

#define QLIB_REG_ESSR__SSR        0u, 32u
#define QLIB_REG_ESSR__DIE_ID     32u, 2u
#define QLIB_REG_ESSR__RESERVED_1 34u, 2u
#define QLIB_REG_ESSR__RNG_RDY    36u, 1u
#define QLIB_REG_ESSR__AWDT_75    37u, 1u
#define QLIB_REG_ESSR__CMVP_DONE  38u, 1u
#define QLIB_REG_ESSR__CMVP_PASS  39u, 1u
#define QLIB_REG_ESSR__ECC_SEC    40u, 1u
#define QLIB_REG_ESSR__ECC_DED    41u, 1u
#define QLIB_REG_ESSR__ECC_DIS    42u, 1u
#define QLIB_REG_ESSR__RESERVED_2 43u, 17u
#define QLIB_REG_ESSR__KID_MSB    60u, 4u

/*---------------------------------------------------------------------------------------------------------*/
/* ESSR bit-field structure                                                                                */
/*---------------------------------------------------------------------------------------------------------*/
typedef struct
{
    QLIB_REG_SSR_STRUCT_T SSR;
    U32                   DIE_ID : 2;
    U32                   RESERVED_1 : 2;
    U32                   RNG_RDY : 1;
    U32                   AWDT_75 : 1;
    U32                   CMVP_DONE : 1;
    U32                   CMVP_PASS : 1;
    U32                   ECC_SEC : 1;
    U32                   ECC_DED : 1;
    U32                   ECC_DIS : 1;
    U32                   RESERVED_2 : 17;
    U32                   KID_MSB : 4;
} QLIB_REG_ESSR_STRUCT_T;

/*---------------------------------------------------------------------------------------------------------*/
/* ESSR bitfield-U32 union                                                                                 */
/*---------------------------------------------------------------------------------------------------------*/
typedef union
{
    U32                    asUint32;
    U64                    asUint64;
    QLIB_REG_ESSR_STRUCT_T asStruct;
} QLIB_REG_ESSR_T;

/*---------------------------------------------------------------------------------------------------------*/
/* Transaction Counter fields (HCD device)                                                                 */
/*---------------------------------------------------------------------------------------------------------*/
#define QLIB_REG_TC__CNTR 0u, 30u
#define QLIB_REG_TC__DIE  30u, 2u

/************************************************************************************************************
 * KEYS STATUS fields
************************************************************************************************************/
#define QLIB_REG_KEYS_STAT__RESTRICTED      0, 9
#define QLIB_REG_KEYS_STAT__FULL            16, 9
#define QLIB_REG_KEYS_STAT__LMS_PUBLIC      32, 9
#define QLIB_REG_KEYS_STAT__SECRET          48, 1
#define QLIB_REG_KEYS_STAT__MASTER          49, 1
#define QLIB_REG_KEYS_STAT__PRE_PROV_MASTER 50, 1

#ifdef __cplusplus
}
#endif

#endif //_QLIB_SEC_REGS_H_
