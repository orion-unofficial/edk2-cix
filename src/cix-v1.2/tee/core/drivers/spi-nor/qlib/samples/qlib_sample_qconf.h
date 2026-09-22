/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_qconf.h
* @brief      This file contains QLIB QCONF sample related headers
*
* ### project qlib_samples
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_QCONF__H_
#define _QLIB_SAMPLE_QCONF__H_

#define Q2_API 1
#ifdef Q2_API
#define QCONF_TARGET w77q128jw_revA
#define QLIB_TARGET all_targets
#define QLIB_NUM_OF_DIES 1
#endif


#ifdef __cplusplus
extern "C" {
#endif

#include "../qconf/qconf.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                            DEFINITIONS                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

// Section address and size
#define BOOT_SECTION_INDEX W77Q_BOOT_SECTION
#define BOOT_SECTION_BASE  (0u)
#define BOOT_SECTION_SIZE  (_1MB_)

#define PA_WRITE_PROTECT_SECTION_INDEX 1
#define PA_WRITE_PROTECT_SECTION_BASE  (BOOT_SECTION_BASE + BOOT_SECTION_SIZE)
#define PA_WRITE_PROTECT_SECTION_SIZE  (_512KB_)

#define DIGEST_WRITE_PROTECTED_SECTION_INDEX 2
#define DIGEST_WRITE_PROTECTED_SECTION_BASE  (PA_WRITE_PROTECT_SECTION_BASE + PA_WRITE_PROTECT_SECTION_SIZE)
#define DIGEST_WRITE_PROTECTED_SECTION_SIZE  (_256KB_)

#define FW_UPDATE_SECTION_INDEX 3
#define FW_UPDATE_SECTION_BASE  (DIGEST_WRITE_PROTECTED_SECTION_BASE + DIGEST_WRITE_PROTECTED_SECTION_SIZE)
#define FW_UPDATE_SECTION_SIZE  (_512KB_)

#define SECURE_DATA_SECTION_INDEX 4
#define SECURE_DATA_SECTION_BASE  (FW_UPDATE_SECTION_BASE + FW_UPDATE_SECTION_SIZE)
#define SECURE_DATA_SECTION_SIZE  (_256KB_)

#define EMPTY_SECTION_INDEX 5
#define EMPTY_SECTION_BASE  (0)
#define EMPTY_SECTION_SIZE  (0)

#define FW_STORAGE_SECTION_INDEX 6
#define FW_STORAGE_SECTION_BASE  (SECURE_DATA_SECTION_BASE + SECURE_DATA_SECTION_SIZE)
#define FW_STORAGE_SECTION_SIZE  (_1MB_)

#define RECOVERY_SECTION_INDEX W77Q_BOOT_SECTION_FALLBACK
#define RECOVERY_SECTION_BASE  (FW_STORAGE_SECTION_BASE + FW_STORAGE_SECTION_SIZE)
#define RECOVERY_SECTION_SIZE  (_512KB_)

// Device Master key
#define QCONF_KD                                       \
    {                                                  \
        0x11111111, 0x12121212, 0x13131313, 0x14141414 \
    }

// Device Secret Key
#define QCONF_KDS                                      \
    {                                                  \
        0x15151515, 0x16161616, 0x17171717, 0x18181818 \
    }

// Device Pre-Provisioned Master Key
// Should be updated according to the actual key in the flash
#define QCONF_KD1                                      \
    {                                                  \
        0x01010101, 0x02020202, 0x03030303, 0x04040404 \
    }

// Secure User ID
#define QCONF_SUID                                         \
    {                                                      \
        0x19191919u, 0x10101010u, 0x21212121u, 0x22222222u \
    }

// Section 0 Full access key
#define QCONF_FULL_ACCESS_K_0                              \
    {                                                      \
        0x23232323u, 0x24242424u, 0x25252525u, 0x26262626u \
    }

// Section 1 Full access key
#define QCONF_FULL_ACCESS_K_1                              \
    {                                                      \
        0x27272727u, 0x28282828u, 0x29292929u, 0x20202020u \
    }

// Section 2 Full access key
#define QCONF_FULL_ACCESS_K_2                              \
    {                                                      \
        0x31313131u, 0x32323232u, 0x33333333u, 0x34343434u \
    }

// Section 3 Full access key
#define QCONF_FULL_ACCESS_K_3                              \
    {                                                      \
        0x35353535u, 0x36363636u, 0x37373737u, 0x38383838u \
    }

// Section 4 Full access key
#define QCONF_FULL_ACCESS_K_4                              \
    {                                                      \
        0x39393939u, 0x30303030u, 0x41414141u, 0x42424242u \
    }

// Section 5 Full access key
#define QCONF_FULL_ACCESS_K_5                              \
    {                                                      \
        0x43434343u, 0x44444444u, 0x45454545u, 0x46464646u \
    }

// Section 6 Full access key
#define QCONF_FULL_ACCESS_K_6                              \
    {                                                      \
        0x47474747u, 0x48484848u, 0x49494949u, 0x40404040u \
    }

// Section 7 Full access key
#define QCONF_FULL_ACCESS_K_7                              \
    {                                                      \
        0x51515151u, 0x52525252u, 0x53535353u, 0x54545454u \
    }

// Section 0 restricted access key
#define QCONF_RESTRICTED_K_0                               \
    {                                                      \
        0x55555555u, 0x56565656u, 0x57575757u, 0x58585858u \
    }

// Section 1 restricted access key
#define QCONF_RESTRICTED_K_1                               \
    {                                                      \
        0x59595959u, 0x50505050u, 0x61616161u, 0x62626262u \
    }

// Section 2 restricted access key
#define QCONF_RESTRICTED_K_2                               \
    {                                                      \
        0x63636363u, 0x64646464u, 0x65656565u, 0x66666666u \
    }

// Section 3 restricted access key
#define QCONF_RESTRICTED_K_3                               \
    {                                                      \
        0x67676767u, 0x68686868u, 0x69696969u, 0x60606060u \
    }

// Section 4 restricted access key
#define QCONF_RESTRICTED_K_4                               \
    {                                                      \
        0x71717171u, 0x72727272u, 0x73737373u, 0x74747474u \
    }

// Section 5 restricted access key
#define QCONF_RESTRICTED_K_5                               \
    {                                                      \
        0x75757575u, 0x76767676u, 0x77777777u, 0x78787878u \
    }

// Section 6 restricted access key
#define QCONF_RESTRICTED_K_6                               \
    {                                                      \
        0x79797979u, 0x70707070u, 0x81818181u, 0x82828282u \
    }

// Section 7 restricted access key
#define QCONF_RESTRICTED_K_7                               \
    {                                                      \
        0x83838383u, 0x84848484u, 0x85858585u, 0x86868686u \
    }

// Vault Full access key
#define QCONF_VAULT_FULL_ACCESS                            \
    {                                                      \
        0x87878787u, 0x88888888u, 0x89898989u, 0x90909090u \
    }

// Vault restricted access key
#define QCONF_VAULT_RESTRICTED                             \
    {                                                      \
        0x91919191u, 0x92929292u, 0x93939393u, 0x94949494u \
    }

// LMS keys TODO LMS set proper values
#define QCONF_LMS_K_0                          \
    {                                          \
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u \
    }
#define QCONF_LMS_K_1                                                                                                        \
    {                                                                                                                        \
        0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x312baa29u, 0x1ed3164fu, 0x365ddb4cu, 0x63e05063u, 0x2a11e72cu, \
            0x64b28a30u                                                                                                      \
    }
#define QCONF_LMS_K_2                          \
    {                                          \
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u \
    }
#define QCONF_LMS_K_3                          \
    {                                          \
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u \
    }
#define QCONF_LMS_K_4                          \
    {                                          \
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u \
    }
#define QCONF_LMS_K_5                          \
    {                                          \
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u \
    }
#define QCONF_LMS_K_6                          \
    {                                          \
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u \
    }
#define QCONF_LMS_K_7                          \
    {                                          \
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u \
    }
#define QCONF_LMS_K_8                          \
    {                                          \
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u \
    }

/************************************************************************************************************
 * @brief       This routine shows QCONF configuration flow with initialization sequence
 *
 * @param[in]   userData       Pointer to data associated with this qlib instance (NULL if no data required).

 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_QconfConfigRun(void* userData);

/************************************************************************************************************
 * @brief       This routine shows QCONF recovery flow with initialization sequence
 *
 * @param[in]   userData       Pointer to data associated with this qlib instance (NULL if no data required).
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_QconfRecoveryRun(void* userData);

/************************************************************************************************************
* @brief       This routine shows QCONF configuration flow, assuming initialization already executed
*
* @param[out]  qlibContext     [QLIB internal state](md_definitions.html#DEF_CONTEXT)
*
* @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_QconfConfig(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
* @brief       This routine shows QCONF recovery flow, assuming initialization already executed
*
* @param[out]  qlibContext     [QLIB internal state](md_definitions.html#DEF_CONTEXT)
*
* @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_QconfRecovery(QLIB_CONTEXT_T* qlibContext);

/************************************************************************************************************
* @brief       This routine get QCONF for nor_init
*
* @param[out]  qlibTable     [QLIB internal state]
*
* @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QCONF_Fetch_Config(QCONF_T* qlibTable);

#ifdef __cplusplus
}
#endif

#endif // _QLIB_SAMPLE_QCONF__H_
