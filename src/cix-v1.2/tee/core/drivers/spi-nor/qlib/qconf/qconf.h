/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qconf.h
* @brief      This file contains (QCONF)[qconf.html] common types and definitions (exported to the user)
*
* ### project qlib
*
************************************************************************************************************/

#ifndef __QCONF_H__
#define __QCONF_H__

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
/*                                                 DEFINE                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

// QCONF magic word - The value of magicWord in configuration struct (QCONF_T). For further information [QCONF](qconf.html).
#define QCONF_MAGIC_WORD (0xC07F1506u)

// This is the default value set in SFDP table
#define DEFAULT_FAST_READ_DUMMY (8u)

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                  TYPES                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
typedef struct QCONF_OTC_T
{
    KEY_T deviceMasterKey;
    KEY_T deviceSecretKey;
#ifndef Q2_API
    KEY_T          preProvisionedMasterKey;
    KEY_T          restrictedKeys[QLIB_NUM_OF_DIES][QLIB_NUM_OF_SECTIONS];
    KEY_T          fullAccessKeys[QLIB_NUM_OF_DIES][QLIB_NUM_OF_SECTIONS];
    QLIB_LMS_KEY_T lmsKeys[QLIB_NUM_OF_DIES][QLIB_NUM_OF_SECTIONS];
#else
    KEY_T                      restrictedKeys[QLIB_NUM_OF_SECTIONS];
    KEY_T                      fullAccessKeys[QLIB_NUM_OF_SECTIONS];
#endif
    _128BIT suid;
} QCONF_OTC_T;

/************************************************************************************************************
 * QLIB configuration structure\n
************************************************************************************************************/
typedef struct QCONF_T
{
    U32 magicWord; ///< Constant value of 0xC07F1506 @ref QCONF_MAGIC_WORD

#ifndef QCONF_STRUCT_ON_RAM
    U64 flashBaseAddr; ///< Flash base address
#endif

#ifdef DEPRECATED_CONFIGURATION_API
    QCONF_OTC_T otc; ///< One-time configuration
#ifndef Q2_API
    QLIB_SECTION_CONFIG_TABLE_T sectionTable[QLIB_NUM_OF_DIES]; ///< Section configuration table
#else
    QLIB_SECTION_CONFIG_TABLE_T sectionTable; ///< Section configuration table
#endif
    QLIB_WATCHDOG_CONF_T watchdogDefault; ///< Watchdog default configuration structure
    QLIB_DEVICE_CONF_T   deviceConf;      ///< Device configuration structure
#else
    U32                        numOfDies;
    QLIB_FLASH_CONFIG_T        flashConfig;
    QLIB_DIE_CONFIG_T          dieConfig;
    QLIB_SECTIONS_CONF_TABLE_T cfgSectionArray[QLIB_NUM_OF_DIES];
#endif //DEPRECATED_CONFIGURATION_API
} QCONF_T;

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INTERFACE                                                */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       This routine configures W77Q.
 *              This routine should be called from the FW as soon as possible right after @ref QLIB_InitDevice
 *
 * @param[out]  qlibContext   [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]   configHeader  pointer to the flash configuration header
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QCONF_Config(QLIB_CONTEXT_T* qlibContext, const QCONF_T* configHeader);

/************************************************************************************************************
 * @brief       This routine formats the entire flash data and configurations and re-configure it with
 *              legacy access to all the flash size and provision the keys provided.
 *
 * @param[out]  qlibContext   [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]   otc           pointer to a struct holding all the OTC values to be programmed
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QCONF_Recovery(QLIB_CONTEXT_T* qlibContext, const QCONF_OTC_T* otc);


#ifdef __cplusplus
}
#endif

#endif // __QCONF_H__
