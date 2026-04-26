/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2024 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_sample_spi_interface.c
* @brief      This file contains QLIB sample code for setting a new SPI interface.
*
* @example    qlib_sample_spi_interface.c
*
* @page       spi_interface SPI interface sample code
* This sample code shows how to set the SPI interface.\n
*
* @include    samples/qlib_sample_spi_interface.c
*
************************************************************************************************************/

/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                                  INCLUDES
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
#include <stdio.h>

#include "qlib.h"
#include "qlib_sample_spi_interface.h"
#include "qlib_sample.h"
#include "qlib_sample_qconf.h"
#include "qlib_sample_secure_storage.h"

#ifndef QLIB_NO_DIRECT_FLASH_ACCESS
#ifndef NO_COMMON_PLATFORM_SPI
#include "common_platform_spi.h"
#endif
#endif

static QLIB_STATUS_T QLIB_SAMPLE_SetSPI_L(QLIB_CONTEXT_T*      qlibContext,
                                          QLIB_BUS_FORMAT_T    busFormat,
                                          U32                  dummyCycles,
                                          QLIB_STD_ADDR_MODE_T addrMode) __RAM_SECTION;

#ifndef QLIB_NO_DIRECT_FLASH_ACCESS
#ifndef NO_COMMON_PLATFORM_SPI
static QLIB_STATUS_T QLIB_SAMPLE_QlibSpiToPlatSpi_L(QLIB_BUS_FORMAT_T    busFormat,
                                                    U32                  dummyCycles,
                                                    QLIB_STD_ADDR_MODE_T addrMode,
                                                    PLAT_SPI_T*          platSpi);
#endif
#endif
/*-----------------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------------------
                                             INTERFACE FUNCTIONS
-------------------------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------------------*/
QLIB_STATUS_T QLIB_SAMPLE_SetSpiInterface(QLIB_CONTEXT_T* qlibContext)
{
    QLIB_STATUS_T status = QLIB_STATUS__OK;
#ifdef QLIB_SUPPORT_QPI
    QLIB_BUS_FORMAT_T busFormat = QLIB_BUS_FORMAT(QLIB_BUS_MODE_4_4_4, FALSE);
    U32               dummyCycles =
        16; // max needed dummy cycles for QPI SDR in W77Q/T 256Mb to 1Gb. This is not relevant for W77Q 16Mb to 128Mb flash. (please refer to W77Q/T spec)
#else
    QLIB_BUS_FORMAT_T busFormat = QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE);
    U32               dummyCycles =
        8; // max needed dummy cycles for SPI SDR in W77Q/T 256Mb to 1Gb. This is not relevant for W77Q 16Mb to 128Mb flash. (please refer to W77Q/T spec)
#endif

    QLIB_STD_ADDR_MODE_T addrMode;

    QLIB_BUS_FORMAT_T    origBusFormat   = QLIB_BUS_FORMAT(qlibContext->busInterface.busMode, qlibContext->busInterface.dtr);
    U32                  origDummyCycles = qlibContext->fastReadDummy;
    QLIB_STD_ADDR_MODE_T origAddrMode    = qlibContext->addrMode;

    if (Q2_4_BYTES_ADDRESS_MODE(qlibContext) == 1u)
    {
        addrMode = QLIB_STD_ADDR_MODE__4_BYTE;
    }
    else
    {
        addrMode = QLIB_STD_ADDR_MODE__3_BYTE;
    }

    /*-------------------------------------------------------------------------------------------------------
     Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Set SPI interface
     In case XIP is used, the SPI interface parameters should be set in QLIB, flash and in core simultaneously,
     to allow both proper QLIB operation and command fetch operations from core.
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_SetSPI_L(qlibContext, busFormat, dummyCycles, addrMode), status, disconnect);

    /*-------------------------------------------------------------------------------------------------------
     Perform some commands to test new SPI mode
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_SecureSectionWithPlainAccessRead(qlibContext), status, recover_spi);

    /*-------------------------------------------------------------------------------------------------------
     return to original SPI parameters
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_SAMPLE_SetSPI_L(qlibContext, origBusFormat, origDummyCycles, origAddrMode),
                               status,
                               disconnect);

    /*-------------------------------------------------------------------------------------------------------
     Release the ownership of flash communication channel
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Disconnect(qlibContext));

    return QLIB_STATUS__OK;

recover_spi:
    (void)QLIB_SAMPLE_SetSPI_L(qlibContext, origBusFormat, origDummyCycles, origAddrMode);
disconnect:
    (void)QLIB_Disconnect(qlibContext);

    return status;
}

QLIB_STATUS_T QLIB_SAMPLE_SetSpiInterfaceRun(void* userData)
{
    QLIB_CONTEXT_T qlibContext;
    QLIB_STATUS_T  status = QLIB_STATUS__OK;

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
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_InitDevice(&qlibContext, QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE)), status, disconnect);

    /*-------------------------------------------------------------------------------------------------------
     Release the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Disconnect(&qlibContext));

    /*-------------------------------------------------------------------------------------------------------
     Sample for setting new SPI interface parameters
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_SetSpiInterface(&qlibContext));

    return QLIB_STATUS__OK;

disconnect:
    (void)QLIB_Disconnect(&qlibContext);
    return status;
}

#ifndef QLIB_NO_DIRECT_FLASH_ACCESS
#ifndef NO_COMMON_PLATFORM_SPI
/************************************************************************************************************
 * @brief       This routine translates qlib SPI parameters to platform types
 *              User should implement such function for its own platform.
 *
 * @param[in]   busFormat    qlib bus format
 * @param[in]   dummyCycles  dummy cycles for fast read command
 * @param[in]   addrMode     address mode (3/4 bytes)
 * @param[out]  platSpi      platform SPI parameters
 *
 * @return      QLIB_STATUS__OK if no error occurred, QLIB_STATUS__(ERROR) otherwise
************************************************************************************************************/
static QLIB_STATUS_T QLIB_SAMPLE_QlibSpiToPlatSpi_L(QLIB_BUS_FORMAT_T    busFormat,
                                                    U32                  dummyCycles,
                                                    QLIB_STD_ADDR_MODE_T addrMode,
                                                    PLAT_SPI_T*          platSpi)
{
    QLIB_STATUS_T ret = QLIB_STATUS__OK;
    BOOL          dtr = QLIB_BUS_FORMAT_GET_DTR(busFormat);
    platSpi->dtr      = ((dtr == TRUE) ? PLAT_SPI_DTR__ADDR_DATA : PLAT_SPI_DTR__NO_DTR);
    switch (QLIB_BUS_FORMAT_GET_MODE(busFormat))
    {
        case QLIB_BUS_MODE_1_1_1:
            platSpi->mode = PLAT_SPI_FORMAT_1_1_1;
            break;
        case QLIB_BUS_MODE_1_1_2:
        case QLIB_BUS_MODE_1_2_2:
            platSpi->mode = PLAT_SPI_FORMAT_1_2_2;
            break;
        case QLIB_BUS_MODE_1_1_4:
        case QLIB_BUS_MODE_1_4_4:
            platSpi->mode = PLAT_SPI_FORMAT_1_4_4;
            break;
        case QLIB_BUS_MODE_4_4_4:
            platSpi->mode = PLAT_SPI_FORMAT_4_4_4;
            break;
        case QLIB_BUS_MODE_1_8_8:
            platSpi->mode = PLAT_SPI_FORMAT_1_8_8;
            break;
        case QLIB_BUS_MODE_8_8_8:
            platSpi->mode = PLAT_SPI_FORMAT_8_8_8;
            platSpi->dtr  = ((dtr == TRUE) ? PLAT_SPI_DTR__ALL : PLAT_SPI_DTR__NO_DTR);
            break;
        default:
            ret = QLIB_STATUS__INVALID_PARAMETER;
            break;
    }
    platSpi->dummyCycles    = dummyCycles;
    platSpi->addrMode3Bytes = (addrMode == QLIB_STD_ADDR_MODE__4_BYTE ? false : true);

    return ret;
}
#endif
#endif

static QLIB_STATUS_T QLIB_SAMPLE_SetSPI_L(QLIB_CONTEXT_T*      qlibContext,
                                          QLIB_BUS_FORMAT_T    busFormat,
                                          U32                  dummyCycles,
                                          QLIB_STD_ADDR_MODE_T addrMode)
{
#ifndef QLIB_NO_DIRECT_FLASH_ACCESS
#ifndef NO_COMMON_PLATFORM_SPI
    PLAT_SPI_T platSpi;
#endif
#endif
    QLIB_ASSERT_RET((addrMode == QLIB_STD_ADDR_MODE__3_BYTE) || (Q2_4_BYTES_ADDRESS_MODE(qlibContext) != 0u),
                    QLIB_STATUS__INVALID_PARAMETER);

#ifndef QLIB_NO_DIRECT_FLASH_ACCESS
#ifndef NO_COMMON_PLATFORM_SPI
    // If direct flash access is needed or QLIB runs from flash, We need to set the SPI fetch command in core.
    // User should implement these functions according to the platform it uses.
    QLIB_STATUS_RET_CHECK(QLIB_SAMPLE_QlibSpiToPlatSpi_L(busFormat, dummyCycles, addrMode, &platSpi));
    QLIB_ASSERT_RET(PLAT_SPI_SetFetchCmd(&platSpi) == 0u, QLIB_STATUS__HARDWARE_FAILURE);
#endif
#endif

#ifndef EXCLUDE_Q2_4_BYTES_ADDRESS_MODE
    if (Q2_4_BYTES_ADDRESS_MODE(qlibContext) != 0u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_SetAddressMode(qlibContext, addrMode));
    }
#endif
#ifndef EXCLUDE_FAST_READ_DUMMY_CONFIG
    if (W77Q_FAST_READ_DUMMY_CONFIG(qlibContext) != 0u)
    {
        QLIB_STATUS_RET_CHECK(QLIB_SetFastReadDummyCycles(qlibContext, dummyCycles));
    }
#else
    (void)dummyCycles;
#endif
    QLIB_STATUS_RET_CHECK(QLIB_SetInterface(qlibContext, busFormat));
    return QLIB_STATUS__OK;
}
