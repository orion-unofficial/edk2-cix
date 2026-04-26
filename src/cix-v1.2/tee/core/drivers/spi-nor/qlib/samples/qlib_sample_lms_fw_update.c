/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2023 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_lms_fw_update.c
* @brief      This file contains QLIB LMS FW update sample code
*
* @example    qlib_sample_lms_fw_update.c
*
* @page       LMS FW update sample code
* This sample code shows how to update the data in a section with LMS command.\n
*
* @include    samples/qlib_sample_lms_fw_update.h
*
************************************************************************************************************/

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  INCLUDES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#include <stdio.h>

#include "qlib.h"
#include "qlib_sample.h"
#include "qlib_sample_lms_fw_update.h"
#include "qlib_sample_qconf.h"
#include "qlib_utils_digest.h"
#include "qlib_utils_crc.h"

#if !defined(EXCLUDE_LMS) && !defined(Q2_API)
#include "lms_api.h"
#include "lms.h"
#include "qlib_utils_lms.h"

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                          FUNCTION DECLARATION
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_LMS_SetKey_L(QLIB_CONTEXT_T*         qlibContext,
                                U32                     dieId,
                                U32                     sectionID,
                                const QLIB_LMS_KEY_ID_T keyId,
                                const U8*               publicKey);

#ifndef LMS_SAMPLE_PREPARE_IN_ADVANCE
QLIB_STATUS_T QLIB_LMS_GenerateAndSetKey_L(QLIB_CONTEXT_T* qlibContext,
                                           U32             dieId,
                                           U32             sectionID,
                                           ALG_LMS_T*      lmsAlg,
                                           const U8*       keyId);

QLIB_STATUS_T QLIB_SAMPLE_LmsCalcSectionIntegrity_L(U32* fwBuffer,
                                                    U32  fwSize,
                                                    U32  sectionSize,
                                                    BOOL isRollback,
                                                    U64* digest,
                                                    U32* crc);
#endif
/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_SAMPLE_LmsFwUpdateRun(void* userData)
{
    QLIB_CONTEXT_T    qlibContext;
    QLIB_STATUS_T     status    = QLIB_STATUS__OK;
    QLIB_BUS_FORMAT_T busFormat = QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE);

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
     Run sample code
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_LmsFwUpdate(&qlibContext));

    goto exit;

disconnect:
    (void)QLIB_Disconnect(&qlibContext);

exit:
    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_LmsFwUpdate(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T           status      = QLIB_STATUS__OK;
    U32                     sectionID   = FW_UPDATE_SECTION_INDEX;
    U32                     dieId       = 0;
    const char              fw[]        = {'i', ' ', 'a', 'm', ' ', 'a', ' ', 'n', 'e', 'w', ' ', 'f', 'w'};
    U32                     version     = 0;
    U32                     testVersion = 0;
    KEY_T                   key         = QCONF_FULL_ACCESS_K_3;
    const QLIB_LMS_KEY_ID_T keyId =
        {0xfa, 0x16, 0xb4, 0x90, 0xbd, 0xee, 0x56, 0xbd, 0xAc, 0x5b, 0xB6, 0x6F, 0xDE, 0xBF, 0x7D, 0xFA};
#ifndef LMS_SAMPLE_PREPARE_IN_ADVANCE
    U32                        fw32[1u + ((sizeof(fw) - 1u) / 4u)];
    U32                        newVersion;
    ALG_LMS_T                  lmsAlg = {0};
    QLIB_LMS_MSG_DATA_STRUCT_T lmsMsg;
    QLIB_LMS_SIG_BUFFER_T      signature;
    LMS_SIGN_MESSAGE_OP_T      signParams = {keyId, lmsMsg, sizeof(QLIB_LMS_MSG_DATA_STRUCT_T), signature, sizeof(signature), 0};
    U64                        digest     = 0;
    U32                        crc        = 0;
    QLIB_POLICY_T              policy     = {0};
#else
    U32                        newVersion = 100u;
    QLIB_LMS_MSG_DATA_STRUCT_T lmsMsg     = {0x00, 0x00, 0x03, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x9b, 0x00, 0x00, 0x00, 0x98, 0x55, 0xfc, 0x30, 0x9b, 0x23,
                                             0xdd, 0x98, 0xe2, 0x3e, 0x5c, 0x0a, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    U32                        signature[sizeof(QLIB_LMS_SIG_BUFFER_T) / sizeof(U32)] =
        {0x00000001, 0x00000007, 0x1c0d5e24, 0xde47b706, 0xc84d12b3, 0xa68bbb43, 0x7d5a031f, 0x1f253809, 0x4c352faa, 0x6225fb78,
         0xaf76e6fe, 0x3c61d436, 0xb3f20574, 0xa33ad07a, 0x3cd48a6e, 0x5b3bca23, 0xfbbc1fcb, 0x5cbdbe6d, 0x196d7f4e, 0x36155cf7,
         0xfe2f0dfb, 0x64a636ec, 0x3fb50eaf, 0xb492d231, 0xbc045d53, 0x47558821, 0x987a4517, 0xfa3af3ff, 0x3e14d209, 0x2a9bd12d,
         0x612d6df6, 0x018b54b3, 0x6138556d, 0x5a484153, 0x4b437b1f, 0x08c6a86f, 0x8a1522a3, 0x0d957450, 0x22d5fc17, 0x9d80661f,
         0x4107bd14, 0x1665dbbd, 0x78104132, 0x9de92156, 0xf5e4a59a, 0xc8bb1e7e, 0xf4962498, 0x46645917, 0x9e1f1ac4, 0xaee0b596,
         0xa6963727, 0xc3fc7f46, 0xead2ffa0, 0x7d326c6f, 0xc0ae4db2, 0xce63333c, 0x984cbb36, 0x82ea10b6, 0x565251a6, 0x59b1b8ab,
         0x4225dfd6, 0x60dbef6e, 0x0a34b8d6, 0x4ac6b44d, 0xcc4ee193, 0xb685a04a, 0x56bda48d, 0xfb7d0f84, 0xf9c90066, 0x5d51c49e,
         0xb0ac7b8e, 0x6df7184c, 0xa197bccf, 0xf8f3b8c6, 0xd8689217, 0x7fd7282e, 0x0b801141, 0x251cd65a, 0x3a5552ba, 0x34e9e8d1,
         0xf549c7c3, 0xe2a219ea, 0x2de1614d, 0x409dcc26, 0xb5fb3597, 0x748e8171, 0xe9e08ff5, 0x9d506a79, 0x7d579a51, 0xa9823763,
         0x8fe3ef10, 0xe0c8be54, 0xd60cca6f, 0x72dee352, 0x46e462fd, 0xc5cfaf06, 0x360a8065, 0x6e314af0, 0xe6fc5eaf, 0x1989db64,
         0x621350b5, 0x1b396026, 0x950be1c1, 0x25bb5ed8, 0xef1f33b3, 0x1884f3b9, 0xdf8bc697, 0xe5db33ac, 0x9d5e1405, 0x80c0b6f4,
         0x196681f8, 0x16f65f2f, 0x85b261c9, 0xf3fae0cc, 0x16af91a5, 0x5337e54f, 0xd172c61d, 0x4ca62ae3, 0x438474dc, 0x1539b718,
         0x3703f411, 0x8a1a6c12, 0x77a26b66, 0x607ce38e, 0x48abc516, 0x78a31bb7, 0xd0ab80cb, 0x00f3a7fc, 0x3d4d57f7, 0x984bfedf,
         0x19986cad, 0x9387f05e, 0xef417e23, 0x4627ab02, 0x43888ae6, 0x6e20760e, 0x217f3fe8, 0x3fae81d9, 0xdfb2bc15, 0x2e3639e7,
         0x3054878d, 0x7b1cbb4b, 0xded8c670, 0xf7a5a94b, 0x90caa693, 0x0c778d6d, 0xe7510d74, 0x3cb668aa, 0x22313e90, 0xad657af6,
         0x727e387b, 0xacf56316, 0x3c42c167, 0x8ac4760e, 0xa2ced6d0, 0xaabd83ad, 0xee5b66f8, 0x28dd01aa, 0x3cb821dc, 0xce156820,
         0xbc3ef635, 0x3fa45746, 0x8773f9ef, 0x5641e9f0, 0x567a9401, 0xdb9187c7, 0xf05f3b61, 0xe18fdbce, 0x8e19ef4c, 0xba0d0d89,
         0x8d1a0efa, 0x6c6f16f1, 0x3a0c1519, 0x57f44f66, 0x521a6c4c, 0x000a4101, 0xc89db4b3, 0xd8e587a5, 0xeddbb5dc, 0x62307c56,
         0x14ac61ee, 0xc15a0a22, 0x42ceb077, 0x29933a5a, 0x968f6a70, 0xd32ae4ee, 0x9d596dfd, 0x1e271a76, 0x5a6d1305, 0xf13f550e,
         0x0bff7bfd, 0x88c858db, 0xcae0223c, 0x1829f4cc, 0x0bc8c693, 0x11139326, 0xbd51be76, 0xd897a266, 0x5930c391, 0x1bc6fbb9,
         0xaab7f6f2, 0x989d7e9b, 0x2b3c0c8f, 0x22855dfd, 0xa0515f3b, 0xb3dcf4c8, 0xd762dde5, 0x2604f9b8, 0x8b341be2, 0x2aee7e34,
         0x075f12dc, 0x900a51ed, 0xf7d361b3, 0x768ae295, 0xbf2b4756, 0x4ee746a9, 0xbd397f06, 0x69e3205b, 0x638ce03a, 0x7d0ca859,
         0xd6c198c4, 0x7e36b270, 0xb5b433f5, 0x2f4f9426, 0x7b91a3ba, 0x60555b14, 0x98016e76, 0x58db012e, 0xee289e99, 0xa180bd6b,
         0xb409a7d8, 0x483a4bda, 0x6f2ad710, 0xd5a227b9, 0xf7c6c92b, 0x6d4b4bd5, 0x0d377958, 0x03767fa2, 0x38d867fe, 0x0a1d2b67,
         0xa8d7b04b, 0x8a261e63, 0x3e41b9c2, 0x0b632100, 0x49f08ae5, 0x395509a1, 0x620256e5, 0xb15c0c08, 0xadf1941d, 0xeaa96111,
         0xd553ff7a, 0xc6ee0bc9, 0xf3a27c47, 0xb3d513a3, 0x3a4f14e8, 0x86cf66ab, 0xe849f437, 0x27420054, 0xa3dc8ee2, 0x8ad941ae,
         0xce2e4f3d, 0x6e2c97e2, 0xfda434d4, 0x7f68d8e4, 0x8dba7c89, 0x3f11e95e, 0xb5f83010, 0xdb6bd7fc, 0x13ac62f1, 0xc9d4fb8d,
         0x6b7984d3, 0xe9f394aa, 0x6237e463, 0x1aeb695c, 0x953f6b81, 0x980b824c, 0x14f67eab, 0xb3c59d4c, 0xbad60ce8, 0x88bf84ea,
         0xddc34506, 0x7924dc32, 0xd66c7f3f, 0x5f02f0b7, 0x864fa5a5, 0x63704ad7, 0xb495bb96, 0xe4de7723, 0x4b74bf49, 0x01fd0264,
         0x67ddbf06, 0xfdcc9d99, 0xcf34f66c, 0x7c085698, 0xcff4137f, 0x51d9c2fb, 0x64119a7e, 0xdac48fed, 0xee4c294a, 0x4ad64c9a,
         0x6afa5e3b, 0x72614203, 0x805e8747, 0xe7885490, 0xb0f05b2d, 0xd4c1828c, 0xddd78e6a, 0x37e5423a, 0xf45bbbe4, 0xd82a22ed,
         0x5e94e59d, 0x74636c67, 0x56f47384, 0x68f50e98, 0x0000000d, 0xfb8492a4, 0x362d39bf, 0x2da52f40, 0x7c0edc87, 0x89df527c,
         0x76f96225, 0x0a79deec, 0xfed8f306, 0xca54967a, 0xabbbc4b8, 0x8cdf7f46, 0x98f1adf8, 0xcdc3f263, 0xbb489794, 0x6e20fe28,
         0x278a782b, 0x33edbc6b, 0xb1742266, 0x3b6d6548, 0x4eef42f3, 0x57683c51, 0x93ba0a88, 0xbd088998, 0xbb9241a7, 0x829a8bd3,
         0x8a65c80e, 0x0bffb484, 0xec7adcff, 0xee01fe0e, 0x34241185, 0xd0269f60, 0x5ecf562c, 0xd6b5d1b7, 0xc44ad1df, 0x841e2f15,
         0x3d8d6399, 0x1a229448, 0x7323203e, 0xe6008d7c, 0x957e2b3d, 0xf5c7bad1, 0x7cf36209, 0x6be1b3e9, 0xeedaf322, 0x9b02d5fb,
         0xab7bf8d1, 0x65ddc498, 0x927eeab0, 0xa6d65a7a, 0xfb1d86ef, 0xec7d63fe, 0x00f575b1, 0xf318cfd4, 0x120c9686, 0x087cdca8,
         0xcdcc9bb0, 0x25513f45, 0x9445536f, 0x915206ac, 0x132e4183, 0xb6957c07, 0x35a6edbf, 0x6ed9b081, 0xd927272b, 0xaddedefb,
         0x0a759c29, 0xb2a652c3, 0xb6251fe7, 0xc92f0926, 0x36488d00, 0x2aa0b544, 0x63698fb3, 0xb5b4081a, 0xa023e523, 0xd2a26b7e,
         0x5bc399ae, 0xc8ca4662, 0xa80441a6, 0xc2a1aa0b, 0x8374dcd8, 0xa5ff008e, 0x0c283859, 0xe07b3d10, 0x84b59d87, 0x523b1c7c,
         0x74a4643d, 0x6ee1b490, 0x59e48138, 0x539cfccf, 0xf6669876, 0x615e9181, 0x5a82d44d, 0xfee9f22f, 0x596aaae5, 0x6905a63b,
         0x0248324e, 0x6e242d65, 0x4675125e, 0x0afefdf8, 0xb356fcfb, 0x29a59c18, 0xb70f7ef5, 0xa7457e0c, 0xc5d91fbc, 0x9bcf202d,
         0x07996b40, 0x958d6d29, 0x854bc89f, 0x692c6992, 0xc15f0345, 0xc4e11bc6, 0xbb38c3ec, 0xff7d7fea, 0x8201f04e, 0x1a6be944,
         0x1f933374, 0x0d5f73c3, 0x33215423, 0x2a54dbe6, 0x94e15532};

    // The large buffer come to replace the signed LMS message.
    // the parameters used to generate this buffer match the flash configuration from qlib_sample_qconf,
    // the section digest and CRC to match the section data as `fw` buffer in this sample, section size of 0x100000,
    // section version 100 and configuration for section policy configuration as rollback, plain read with digest and crc protection

#endif

#ifndef LMS_SAMPLE_PREPARE_IN_ADVANCE
    /*-------------------------------------------------------------------------------------------------------
     QLIB_LMS_GenerateAndSetKey_L function should be called only once to generate key pair and set the
     section LMS key on chip.
     On next FW update user uses the same key pair to sign new messages.
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_LMS_GenerateAndSetKey_L(qlibContext, dieId, sectionID, &lmsAlg, keyId));
#else
    // pre-prepared key pair for seed 2923be84e16cd6ae529049f1f1bbe9ebb3a6db3c870c3e99
    uint8_t publicKey[] = {0x6b, 0x66, 0x51, 0x03, 0xc2, 0xaa, 0xe4, 0xbc, 0x49, 0x14, 0x0c, 0xdb,
                           0x19, 0xe1, 0x7c, 0x69, 0x30, 0x15, 0xa3, 0xfb, 0x22, 0xcd, 0xf9, 0x16};
    QLIB_STATUS_RET_CHECK(QLIB_LMS_SetKey_L(qlibContext, dieId, sectionID, keyId, publicKey));
#endif
    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));

    /*---------------------------------------------------------------------------------------------------
     Erase upper half of the sector & copy the FW buffer
    ---------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_LoadKey(qlibContext, sectionID, key, TRUE), status, remove_key);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(qlibContext, sectionID, QLIB_SESSION_ACCESS_FULL), status, remove_key);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Erase(qlibContext, sectionID, FW_UPDATE_SECTION_SIZE / 2u, FW_UPDATE_SECTION_SIZE / 2u, TRUE),
                               status,
                               remove_key);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Write(qlibContext, (const U8*)fw, sectionID, (FW_UPDATE_SECTION_SIZE / 2u), sizeof(fw), TRUE),
                               status,
                               remove_key);

    QLIB_STATUS_RET_CHECK_GOTO(QLIB_CloseSession(qlibContext, sectionID), status, remove_key);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_RemoveKey(qlibContext, sectionID, TRUE), status, disconnect);

    /*-------------------------------------------------------------------------------------------------------
     Get current section version
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_GetSectionConfiguration(qlibContext, sectionID, NULL, NULL, NULL, NULL, NULL, &version),
                               status,
                               disconnect);
#ifndef LMS_SAMPLE_PREPARE_IN_ADVANCE
    newVersion = version + 1;
#else
    /*---------------------------------------------------------------------------------------------------
        Verify that the version in the LMS command is greater than the version in the designated section
    ---------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO(version < newVersion, QLIB_STATUS__TEST_FAIL, status, disconnect);

#endif

#ifndef LMS_SAMPLE_PREPARE_IN_ADVANCE
    /*-------------------------------------------------------------------------------------------------------
     Moving FW to aligned location to compute expected section integrity
    -------------------------------------------------------------------------------------------------------*/
    (void)memset(fw32, 0xff, sizeof(fw32));
    (void)memcpy((void*)fw32, (void*)fw, sizeof(fw));

    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_LmsCalcSectionIntegrity_L(fw32, sizeof(fw32), FW_UPDATE_SECTION_SIZE, TRUE, &digest, &crc));

    /*-------------------------------------------------------------------------------------------------------
     Create the set_scr command
    -------------------------------------------------------------------------------------------------------*/
    policy.digestIntegrity         = 1;
    policy.digestIntegrityOnAccess = 1;
    policy.checksumIntegrity       = 1;
    policy.rollbackProt            = 1;
    policy.plainAccessReadEnable   = 1;
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_UTILS_LMS_CreateSetScrMsg(sectionID,
                                                              &policy,
                                                              &digest,
                                                              &crc,
                                                              newVersion,
                                                              TRUE,
                                                              QLIB_SECTION_CONF_ACTION__NO,
                                                              lmsMsg),
                               status,
                               disconnect);

    /*-------------------------------------------------------------------------------------------------------
     sign the set_scr command
    -------------------------------------------------------------------------------------------------------*/
    QLIB_ASSERT_WITH_ERROR_GOTO(lmsSignMessage(&lmsAlg, &signParams) == ALG_SUCCESS,
                                QLIB_STATUS__COMMAND_FAIL,
                                status,
                                disconnect);

#endif
    /*---------------------------------------------------------------------------------------------------
        Send the LMS command to the flash
    ---------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SendLMSCommand(qlibContext, lmsMsg, keyId, (U8*)signature, sectionID), status, disconnect);

    /*---------------------------------------------------------------------------------------------------
        Verify that after the execution of the LMS command, the version of the designated section was updated
    ---------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_GetSectionConfiguration(qlibContext, sectionID, NULL, NULL, NULL, NULL, NULL, &testVersion),
                               status,
                               disconnect);
    QLIB_ASSERT_WITH_ERROR_GOTO(newVersion == testVersion, QLIB_STATUS__TEST_FAIL, status, disconnect);

    QLIB_STATUS_RET_CHECK(QLIB_Disconnect(qlibContext));
    goto exit;

remove_key:
    (void)QLIB_RemoveKey(qlibContext, sectionID, TRUE);
disconnect:
    (void)QLIB_Disconnect(qlibContext);
exit:
    return status;
}

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                             LOCAL FUNCTIONS                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief       Set the section LMS public key on chip
 *
 * @param[in]  qlibContext
 * @param[in]  dieId
 * @param[in]  sectionID
 * @param[in]  keyId
 * @param[in]  publicKey
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_LMS_SetKey_L(QLIB_CONTEXT_T*         qlibContext,
                                U32                     dieId,
                                U32                     sectionID,
                                const QLIB_LMS_KEY_ID_T keyId,
                                const U8*               publicKey)
{
    KEY_T                      deviceMasterKey                   = QCONF_KD;
    QLIB_FLASH_CONFIG_T        flashCfg                          = {0};
    QLIB_DIE_CONFIG_T          dieCfg                            = {0};
    QLIB_SECTIONS_CONF_TABLE_T cfgSectionArray[QLIB_NUM_OF_DIES] = {0};
    QLIB_STATUS_T              status                            = QLIB_STATUS__OK;

    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Prepare the configuration with the keys
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_GetDeviceConfigMultiDie(qlibContext,
                                                            &flashCfg,
                                                            &dieCfg,
                                                            cfgSectionArray,
                                                            QLIB_GET_NUM_OF_DIES(qlibContext)),
                               status,
                               disconnect);
    (void)memcpy(dieCfg.deviceMasterKey, deviceMasterKey, sizeof(KEY_T));
    (void)memcpy(cfgSectionArray[dieId].sectionConfigTable[sectionID].lmsKey, keyId, sizeof(QLIB_LMS_KEY_ID_T));
    (void)memcpy((U8*)cfgSectionArray[dieId].sectionConfigTable[sectionID].lmsKey + sizeof(QLIB_LMS_KEY_ID_T),
                 publicKey,
                 QLIB_LMS_PARAM_N);

    /*-------------------------------------------------------------------------------------------------------
     Set LMS public key for FW update section
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_ConfigDeviceMultiDie(qlibContext,
                                                         NULL,
                                                         &dieCfg,
                                                         cfgSectionArray,
                                                         QLIB_GET_NUM_OF_DIES(qlibContext)),
                               status,
                               disconnect);

    QLIB_STATUS_RET_CHECK(QLIB_Disconnect(qlibContext));
    return QLIB_STATUS__OK;

disconnect:
    (void)QLIB_Disconnect(qlibContext);
    return status;
}

#ifndef LMS_SAMPLE_PREPARE_IN_ADVANCE
/************************************************************************************************************
 * @brief       generate key pair and set the section LMS key on chip
 *
 * @param[in]  qlibContext
 * @param[in]  dieId
 * @param[in]  sectionID
 * @param[in]  lmsAlg
 * @param[in]  keyId
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_LMS_GenerateAndSetKey_L(QLIB_CONTEXT_T* qlibContext,
                                           U32             dieId,
                                           U32             sectionID,
                                           ALG_LMS_T*      lmsAlg,
                                           const U8*       keyId)
{
    uint8_t               publicKey[24];
    LMS_GEN_KEY_PAIR_OP_T genKeyParams    = {keyId, LMOTS_SHA256_N24_W4_PARAM_SET_ID, LMS_SHA256_M24_H20_PARAM_SET_ID};
    LMS_GET_PUB_KEY_OP_T  getPubKeyParams = {keyId, publicKey, sizeof(publicKey), 0};

    QLIB_ASSERT_RET(lmsInit(lmsAlg) == ALG_SUCCESS, QLIB_STATUS__COMMAND_FAIL);
    QLIB_ASSERT_RET(lmsGenerateKeyPair(lmsAlg, &genKeyParams) == ALG_SUCCESS, QLIB_STATUS__COMMAND_FAIL);
    QLIB_ASSERT_RET(lmsGetPublicKey(lmsAlg, &getPubKeyParams) == ALG_SUCCESS, QLIB_STATUS__COMMAND_FAIL);

    QLIB_STATUS_RET_CHECK(QLIB_LMS_SetKey_L(qlibContext, dieId, sectionID, getPubKeyParams.keyId, getPubKeyParams.publicKey));

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief       This routine creates a set_scr command
 *
 * @param[in]  fwBuffer
 * @param[in]  fwSize
 * @param[in]  sectionSize
 * @param[in]  isRollback
 * @param[out] digest
 * @param[out] crc
 *
 * @return      QLIB_STATUS__OK on success or QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_LmsCalcSectionIntegrity_L(U32* fwBuffer,
                                                    U32  fwSize,
                                                    U32  sectionSize,
                                                    BOOL isRollback,
                                                    U64* digest,
                                                    U32* crc)
{
    U32 padSize;

    QLIB_ASSERT_RET(fwBuffer != NULL, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(fwSize > 0, QLIB_STATUS__INVALID_PARAMETER);
    QLIB_ASSERT_RET(sectionSize >= fwSize, QLIB_STATUS__INVALID_PARAMETER);

    padSize = (isRollback == FALSE ? sectionSize : (sectionSize / 2)) - fwSize;
    QLIB_STATUS_RET_CHECK(QLIB_UTILS_CalcCRCWithPadding(fwBuffer, fwSize, 0xFFFFFFFF, padSize, crc));
    QLIB_STATUS_RET_CHECK(QLIB_UTILS_CalcDigestWithPadding(fwBuffer, fwSize, 0xFF, padSize, digest));
    return QLIB_STATUS__OK;
}

#endif // LMS_SAMPLE_PREPARE_IN_ADVANCE
#endif
