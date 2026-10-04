/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2024 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_spi_interface.h
* @brief      This file contains QLIB sample code definitions for setting SPI interface
*
* ### project qlib_sample
*
************************************************************************************************************/

#ifndef _QLIB_SAMPLE_SET_SPI_IF__H_
#define _QLIB_SAMPLE_SET_SPI_IF__H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************************************************
 * @brief       This routine shows how to Set the SPI interface including initialization sequence
 *
 * @param[in]   userData      Pointer to data associated with this qlib instance.
 *
 * @return      0 if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SetSpiInterfaceRun(void* userData);

/************************************************************************************************************
 * @brief       This function demonstrates how to Set the SPI interface.
 *
 * @param[out]  qlibContext      [QLIB internal state](md_definitions.html#DEF_CONTEXT)
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
QLIB_STATUS_T QLIB_SAMPLE_SetSpiInterface(QLIB_CONTEXT_T* qlibContext);

#ifdef __cplusplus
}
#endif

#endif // _QLIB_SAMPLE_SET_SPI_IF__H_
