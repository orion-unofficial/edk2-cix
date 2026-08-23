/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_qconf.c
* @brief      This file contains QLIB QCONF related functions usage
*
* @example    qlib_sample_qconf.c
*
* @page       qconf sample code
* This sample code shows QCONF API usage
*
* @include    samples/qlib_sample_qconf.c
*
************************************************************************************************************/
/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  INCLUDES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#include "qlib.h"
#include "qlib_sample_qconf.h"

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  DEFINITIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#ifndef QCONF_STRUCT_ON_RAM
extern const QCONF_T qconf_g;
#else
extern QCONF_T qconf_g;
#endif

/*-----------------------------------------------------------------------------------------------------------
This variables point to the qconf fields. The variables value is the address needed to automatically calculate
CRC and digest. They are used in a python script to update the qconf structure in the binary file after compilation.
-----------------------------------------------------------------------------------------------------------*/
QCONF_T* qconf_offset = (QCONF_T*)&qconf_g;

QLIB_STATUS_T QCONF_Fetch_Config(QCONF_T* qlibTable) {
    memcpy(qlibTable, &qconf_g, sizeof(QCONF_T));
    return QLIB_STATUS__OK;
}

#ifdef DEPRECATED_CONFIGURATION_API

QLIB_SECTION_CONFIG_TABLE_T* qconf_SectionTable_0_offset = (QLIB_SECTION_CONFIG_TABLE_T*)&qconf_g.sectionTable[0];

#ifndef Q2_API

uint32_t*                    qconf_SectionTable_baseAddr_offset = (uint32_t*)(&qconf_g.sectionTable[0][0].baseAddr);
uint32_t                     qconf_SectionTable_baseAddr_length = sizeof(&qconf_g.sectionTable[0][0].baseAddr);
uint32_t*                    qconf_SectionTable_size_offset     = (uint32_t*)&qconf_g.sectionTable[0][0].size;
uint32_t                     qconf_SectionTable_size_length     = sizeof(qconf_g.sectionTable[0][0].size);
QLIB_POLICY_T*               qconf_SectionTable_policy_offset   = (QLIB_POLICY_T*)&qconf_g.sectionTable[0][0].policy;
uint32_t                     qconf_SectionTable_policy_length   = sizeof(qconf_g.sectionTable[0][0].policy);
uint32_t*                    qconf_SectionTable_crc_offset      = (uint32_t*)&qconf_g.sectionTable[0][0].crc;
uint32_t                     qconf_SectionTable_crc_length      = sizeof(qconf_g.sectionTable[0][0].crc);
uint64_t*                    qconf_SectionTable_digest_offset   = (uint64_t*)&qconf_g.sectionTable[0][0].digest;
uint32_t                     qconf_SectionTable_digest_length   = sizeof(qconf_g.sectionTable[0][0].digest);
QLIB_SECTION_CONFIG_TABLE_T* qconf_SectionTable_1_offset        = (QLIB_SECTION_CONFIG_TABLE_T*)&qconf_g.sectionTable[0][1];

#else // Q2_API

uint32_t*                    qconf_SectionTable_baseAddr_offset = (uint32_t*)(&qconf_g.sectionTable[0].baseAddr);
uint32_t                     qconf_SectionTable_baseAddr_length = sizeof(qconf_g.sectionTable[0].baseAddr);
uint32_t*                    qconf_SectionTable_size_offset     = (uint32_t*)&qconf_g.sectionTable[0].size;
uint32_t                     qconf_SectionTable_size_length     = sizeof(qconf_g.sectionTable[0].size);
QLIB_POLICY_T*               qconf_SectionTable_policy_offset   = (QLIB_POLICY_T*)&qconf_g.sectionTable[0].policy;
uint32_t                     qconf_SectionTable_policy_length   = sizeof(qconf_g.sectionTable[0].policy);
uint32_t*                    qconf_SectionTable_crc_offset      = (uint32_t*)&qconf_g.sectionTable[0].crc;
uint32_t                     qconf_SectionTable_crc_length      = sizeof(qconf_g.sectionTable[0].crc);
uint64_t*                    qconf_SectionTable_digest_offset   = (uint64_t*)&qconf_g.sectionTable[0].digest;
uint32_t                     qconf_SectionTable_digest_length   = sizeof(qconf_g.sectionTable[0].digest);
QLIB_SECTION_CONFIG_TABLE_T* qconf_SectionTable_1_offset        = (QLIB_SECTION_CONFIG_TABLE_T*)&qconf_g.sectionTable[1];

#endif // Q2_API

#else // DEPRECATED_CONFIGURATION_API

QLIB_EXTENDED_SECTION_CONF_T* qconf_SectionTable_0_offset =
    (QLIB_EXTENDED_SECTION_CONF_T*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0];
uint32_t*      qconf_SectionTable_baseAddr_offset = (uint32_t*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0].baseAddr;
uint32_t       qconf_SectionTable_baseAddr_length = sizeof(qconf_g.cfgSectionArray[0].sectionConfigTable[0].baseAddr);
uint32_t*      qconf_SectionTable_size_offset     = (uint32_t*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0].size;
uint32_t       qconf_SectionTable_size_length     = sizeof(qconf_g.cfgSectionArray[0].sectionConfigTable[0].size);
QLIB_POLICY_T* qconf_SectionTable_policy_offset =
    (QLIB_POLICY_T*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.policy;
uint32_t  qconf_SectionTable_policy_length = sizeof(qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.policy);
uint32_t* qconf_SectionTable_crc_offset    = (uint32_t*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.crc;
uint32_t  qconf_SectionTable_crc_length    = sizeof(qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.crc);
uint64_t* qconf_SectionTable_digest_offset = (uint64_t*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.digest;
uint32_t  qconf_SectionTable_digest_length = sizeof(qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.digest);
QLIB_EXTENDED_SECTION_CONF_T* qconf_SectionTable_1_offset =
    (QLIB_EXTENDED_SECTION_CONF_T*)&qconf_g.cfgSectionArray[0].sectionConfigTable[1];

#endif // DEPRECATED_CONFIGURATION_API

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_SAMPLE_QconfConfigRun(void* userData)
{
    QLIB_CONTEXT_T    qlibContext;
    QLIB_BUS_FORMAT_T busFormat = QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE);

    qconf_offset = (QCONF_T*)&qconf_g;

#ifdef DEPRECATED_CONFIGURATION_API
    qconf_SectionTable_0_offset = (QLIB_SECTION_CONFIG_TABLE_T*)&qconf_g.sectionTable[0];
#ifndef Q2_API
    qconf_SectionTable_baseAddr_offset = (uint32_t*)(&qconf_g.sectionTable[0][0].baseAddr);
    qconf_SectionTable_baseAddr_length = sizeof(&qconf_g.sectionTable[0][0].baseAddr);
    qconf_SectionTable_size_offset     = (uint32_t*)&qconf_g.sectionTable[0][0].size;
    qconf_SectionTable_size_length     = sizeof(qconf_g.sectionTable[0][0].size);
    qconf_SectionTable_policy_offset   = (QLIB_POLICY_T*)&qconf_g.sectionTable[0][0].policy;
    qconf_SectionTable_policy_length   = sizeof(qconf_g.sectionTable[0][0].policy);
    qconf_SectionTable_crc_offset      = (uint32_t*)&qconf_g.sectionTable[0][0].crc;
    qconf_SectionTable_crc_length      = sizeof(qconf_g.sectionTable[0][0].crc);
    qconf_SectionTable_digest_offset   = (uint64_t*)&qconf_g.sectionTable[0][0].digest;
    qconf_SectionTable_digest_length   = sizeof(qconf_g.sectionTable[0][0].digest);
    qconf_SectionTable_1_offset        = (QLIB_SECTION_CONFIG_TABLE_T*)&qconf_g.sectionTable[0][1];
#else  // Q2_API
    qconf_SectionTable_baseAddr_offset = (uint32_t*)(&qconf_g.sectionTable[0].baseAddr);
    qconf_SectionTable_baseAddr_length = sizeof(qconf_g.sectionTable[0].baseAddr);
    qconf_SectionTable_size_offset     = (uint32_t*)&qconf_g.sectionTable[0].size;
    qconf_SectionTable_size_length     = sizeof(qconf_g.sectionTable[0].size);
    qconf_SectionTable_policy_offset   = (QLIB_POLICY_T*)&qconf_g.sectionTable[0].policy;
    qconf_SectionTable_policy_length   = sizeof(qconf_g.sectionTable[0].policy);
    qconf_SectionTable_crc_offset      = (uint32_t*)&qconf_g.sectionTable[0].crc;
    qconf_SectionTable_crc_length      = sizeof(qconf_g.sectionTable[0].crc);
    qconf_SectionTable_digest_offset   = (uint64_t*)&qconf_g.sectionTable[0].digest;
    qconf_SectionTable_digest_length   = sizeof(qconf_g.sectionTable[0].digest);
    qconf_SectionTable_1_offset        = (QLIB_SECTION_CONFIG_TABLE_T*)&qconf_g.sectionTable[1];
#endif // Q2_API
#else  // DEPRECATED_CONFIGURATION_API
    qconf_SectionTable_0_offset        = (QLIB_EXTENDED_SECTION_CONF_T*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0];
    qconf_SectionTable_baseAddr_offset = (uint32_t*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0].baseAddr;
    qconf_SectionTable_baseAddr_length = sizeof(qconf_g.cfgSectionArray[0].sectionConfigTable[0].baseAddr);
    qconf_SectionTable_size_offset     = (uint32_t*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0].size;
    qconf_SectionTable_size_length     = sizeof(qconf_g.cfgSectionArray[0].sectionConfigTable[0].size);
    qconf_SectionTable_policy_offset   = (QLIB_POLICY_T*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.policy;
    qconf_SectionTable_policy_length   = sizeof(qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.policy);
    qconf_SectionTable_crc_offset      = (uint32_t*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.crc;
    qconf_SectionTable_crc_length      = sizeof(qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.crc);
    qconf_SectionTable_digest_offset   = (uint64_t*)&qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.digest;
    qconf_SectionTable_digest_length   = sizeof(qconf_g.cfgSectionArray[0].sectionConfigTable[0].sectionConfig.digest);
    qconf_SectionTable_1_offset        = (QLIB_EXTENDED_SECTION_CONF_T*)&qconf_g.cfgSectionArray[0].sectionConfigTable[1];
#endif // DEPRECATED_CONFIGURATION_API

    /*-------------------------------------------------------------------------------------------------------
     Init QLIB  - needed when running QLIB either on a device or on a remote server.
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_InitLib(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Set user data - store a pointer to data the user might need for its platform
    -------------------------------------------------------------------------------------------------------*/
    QLIB_SetUserData(&qlibContext, userData);
    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Init Flash Device - not needed when using QLIB on a remote server
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_InitDevice(&qlibContext, busFormat));

    /*-------------------------------------------------------------------------------------------------------
    Release the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Disconnect(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Flash will obtain secure configuration according to qconf structure
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_QconfConfig(&qlibContext));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_SAMPLE_QconfRecoveryRun(void* userData)
{
#ifdef QLIB_SUPPORT_XIP
    TOUCH(userData);
    /*-------------------------------------------------------------------------------------------------------
      recovery functionality is designed to run from RAM only since it formats the flash
    -------------------------------------------------------------------------------------------------------*/
    return QLIB_STATUS__NOT_SUPPORTED;
#else
    QLIB_CONTEXT_T    qlibContext;
    QLIB_BUS_FORMAT_T busFormat = QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE);
    QLIB_STATUS_T     status    = QLIB_STATUS__OK;

    /*-------------------------------------------------------------------------------------------------------
     Init QLIB  - needed when running QLIB either on a device or on a remote server.
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_InitLib(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Set the user data into Qlib's context
    -------------------------------------------------------------------------------------------------------*/
    QLIB_SetUserData(&qlibContext, userData);

    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Init Flash Device - not needed when using QLIB on a remote server
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_InitDevice(&qlibContext, busFormat), status, disconnect);

    /*-------------------------------------------------------------------------------------------------------
     Release the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Disconnect(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
    Flash will obtain legacy configuration, otc contains keys needed for reconfiguration
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_QconfRecovery(&qlibContext));

    return QLIB_STATUS__OK;

disconnect:
    (void)QLIB_Disconnect(&qlibContext);
    return status;
#endif
}

QLIB_STATUS_T QLIB_SAMPLE_QconfConfig(QLIB_CONTEXT_T* qlibContext)
{
    /*-------------------------------------------------------------------------------------------------------
    Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));

    /*-------------------------------------------------------------------------------------------------------
    Flash will obtain secure configuration according to qconf structure
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QCONF_Config(qlibContext, &qconf_g));

    /*-------------------------------------------------------------------------------------------------------
    User FW should start here...
    -------------------------------------------------------------------------------------------------------*/

    /*-------------------------------------------------------------------------------------------------------
    Release the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Disconnect(qlibContext));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_SAMPLE_QconfRecovery(QLIB_CONTEXT_T* qlibContext)
{
    /*-------------------------------------------------------------------------------------------------------
    Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));

    /*-------------------------------------------------------------------------------------------------------
    Flash will obtain legacy configuration, otc contains keys needed for reconfiguration
    -------------------------------------------------------------------------------------------------------*/
#ifdef DEPRECATED_CONFIGURATION_API
    QLIB_STATUS_RET_CHECK(QCONF_Recovery(qlibContext, &qconf_g.otc));
#else  // DEPRECATED_CONFIGURATION_API
    {
        QCONF_OTC_T otc;
        U32         die;
        U32         section;

        (void)memcpy((void*)otc.deviceMasterKey, (void*)qconf_g.dieConfig.deviceMasterKey, sizeof(KEY_T));
        (void)memcpy((void*)otc.deviceSecretKey, (void*)qconf_g.dieConfig.deviceSecretKey, sizeof(KEY_T));
        (void)memcpy((void*)otc.preProvisionedMasterKey, (void*)qconf_g.dieConfig.preProvisionedMasterKey, sizeof(KEY_T));
        (void)memcpy((void*)otc.suid, (void*)qconf_g.dieConfig.suid, sizeof(otc.suid));

        for (die = 0; die < QLIB_NUM_OF_DIES; ++die)
        {
            for (section = 0; section < QLIB_NUM_OF_SECTIONS; ++section)
            {
                (void)memcpy(otc.restrictedKeys[die][section],
                             qconf_g.cfgSectionArray[die].sectionConfigTable[section].restrictedKey,
                             sizeof(KEY_T));
                (void)memcpy(otc.fullAccessKeys[die][section],
                             qconf_g.cfgSectionArray[die].sectionConfigTable[section].fullAccessKey,
                             sizeof(KEY_T));
                (void)memcpy(otc.lmsKeys[die][section],
                             qconf_g.cfgSectionArray[die].sectionConfigTable[section].lmsKey,
                             sizeof(QLIB_LMS_KEY_T));
            }
        }

        QLIB_STATUS_RET_CHECK(QCONF_Recovery(qlibContext, &otc));
    }
#endif // DEPRECATED_CONFIGURATION_API

    /*-------------------------------------------------------------------------------------------------------
    Release the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Disconnect(qlibContext));

    return QLIB_STATUS__OK;
}
