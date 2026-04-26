/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_fw_update.h
* @brief      This file contains QLIB fw update related headers
*
* ### project qlib_sample
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_FW_UPDATE__H_
#define _QLIB_SAMPLE_FW_UPDATE__H_

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------------------------------------
 should be aligned to the demo configuration in sample
-----------------------------------------------------------------------------------------------------------*/
#define QLIB_SAMPLE_SECTION_SIZE MIN(_1MB_, UINTPTR_MAX) // section to be updated is assumed to be _1M_

#define QLIB_SAMPLE_MAX_FW_SIZE (QLIB_SAMPLE_SECTION_SIZE / 2u) // two halves ( active and not active )

#define FW_UPDATE_DEMO_DATA_SECTION_NUM 3
#define FW_UPDATE_DEMO_CODE_SECTION_NUM 0

/************************************************************************************************************
 * @brief       This routine shows secure update section flow with initialization sequence
 *
 * @param[in]   userData      Pointer to data associated with this qlib instance.
 * 
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_FwUpdateRun(void* userData);

/************************************************************************************************************
 * @brief       This routine shows secure update section flow.  Assumes that device is Connected and proper
 *              proper key to corresponding section is loaded
 *
 * @param[out]      qlibContext         [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]       buff                data buffer for to be set in section
 * @param[in]       buffSize            data buffer size in bytes
 * @param[in]       section             [Section index](md_definitions.html#DEF_SECTION)
 * @param[in,out]   digestIntegrity     If not NULL, section will be updated with digest, and copied here
 * @param[in,out]   checksumIntegrity   If not NULL, section will be updated with crc, and copied here
 * @param[in]       newVersion          Pointer to new version, if NULL no change will be set
 * @param[in]       swap                Defines if need to swap partitions or swap and reset
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SectionUpdate(QLIB_CONTEXT_T*            qlibContext,
                                        const U8*                  buff,
                                        U32                        buffSize,
                                        U32                        section,
                                        U64*                       digestIntegrity,
                                        U32*                       checksumIntegrity,
                                        U32*                       newVersion,
                                        BOOL                       swap,
                                        QLIB_SECTION_CONF_ACTION_T action);

/************************************************************************************************************
 * @brief       This routine shows secure update firmware flow. Assumes that device is Connected and proper
 *              proper key to corresponding section is loaded
 *
 * @param[out]  qlibContext             [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]   fw                      FW buffer
 * @param[in]   fw_size                 FW buffer size in bytes
 * @param[in]   section                 [Section index](md_definitions.html#DEF_SECTION)
 * @param[in]   newVersion              Pointer to new version, if NULL consequent number will be set
 * @param[in]   checkDigest             If not NULL, fw will be updated with this digest, and copied here
 * @param[in]   checkCrc                If not NULL, fw will be updated with this crc, and copied here
 * @param[in]   shouldCalcCrcDigest     If FALSE, CRC and Digest are given in the input buffers, o/w will be calculated locally
 * @param[in]   swap                    Defines if need to swap partitions or swap and reset
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_FirmwareUpdate(QLIB_CONTEXT_T* qlibContext,
                                         const U32*      fw,
                                         U32             fw_size,
                                         U32             section,
                                         U32*            newVersion,
                                         U64*            checkDigest,
                                         U32*            checkCrc,
                                         BOOL            shouldCalcCrcDigest,
                                         BOOL            swap,
                                         BOOL            reset);

#ifndef QLIB_SUPPORT_XIP
/************************************************************************************************************
 * @brief       This routine gets secure update firmware flow parameters. It will calculate digest/crc if needed.
 *              Assumed that proper key to corresponding section is loaded. The user that knows the crc/digest
 *              for his fw will not use it
 *
 * @param[out]  qlibContext         [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 * @param[in]   section             [Section index](md_definitions.html#DEF_SECTION)
 * @param[in]   buff                FW buffer
 * @param[in]   buffSize            FW buffer size in bytes
 * @param[out]  digestIntegrity     If not NULL, fw will be updated with digest, and copied here
 * @param[out]  checksumIntegrity   If not NULL, fw will be updated with crc, and copied here
 * @param[out]  oldVersion          If not NULL, current section version will be set here
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_GetUpdateSectionParams(QLIB_CONTEXT_T* qlibContext,
                                                 U32             section,
                                                 const U32*      buff,
                                                 U32             buffSize,
                                                 U64*            digestIntegrity,
                                                 U32*            checksumIntegrity,
                                                 U32*            oldVersion);

#endif // QLIB_SUPPORT_XIP

#ifdef __cplusplus
}
#endif

#endif // _QLIB_SAMPLE_FW_UPDATE__H_
