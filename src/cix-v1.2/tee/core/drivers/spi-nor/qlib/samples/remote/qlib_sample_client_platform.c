/************************************************************************************************************
 * @internal
 * @remark     Winbond Electronics Corporation - Confidential
 * @copyright  Copyright (c) 2022 by Winbond Electronics Corporation . All rights reserved
 * @endinternal
 *
 * @file       qlib_sample_client_platform.c
 * @brief      This file includes platform specific implementation for QLIB client
 *
 * ### project qlib
 *
 ***********************************************************************************************************/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include <stdio.h>

#include "qlib.h"
#include "qlib_client.h"
#include "qlib_sample.h"
#include "qlib_client_platform.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                              API FUNCTIONS                                              */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

#ifdef _WIN32
QLIB_STATUS_T QLIB_CLIENT_Send(void* socket, void* dataOut, U32 dataOutSize)
{
    int iResult;

    iResult = send((SOCKET)socket, dataOut, dataOutSize, 0);
    if (iResult == SOCKET_ERROR)
    {
        QLIB_SAMPLE_PRINTF("send failed with error: %d closing socket\r\n", WSAGetLastError());
        (void)closesocket((SOCKET)socket);
        (void)WSACleanup();
        return QLIB_STATUS__COMMUNICATION_ERR;
    }
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_CLIENT_Receive(void* socket, void* dataIn, U32 dataInSize, BOOL blocking)
{
    int rxLen;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Unused parameters                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    TOUCH(blocking);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Read input packet                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    rxLen = recv((SOCKET)socket, dataIn, dataInSize, 0);
    if (rxLen <= 0) // no data received
    {
        QLIB_SAMPLE_PRINTF("Socket %s\r\n", rxLen == 0 ? "was closed by peer" : "crushed");
        (void)closesocket((SOCKET)socket);
        return QLIB_STATUS__COMMUNICATION_ERR;
    }

    return QLIB_STATUS__OK;
}
#else

/*---------------------------------------------------------------------------------------------------------*/
/*                                                  STUB                                                   */
/*---------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_CLIENT_Send(void* socket, void* dataOut, U32 dataOutSize)
{
    TOUCH(socket);
    TOUCH(dataOut);
    TOUCH(dataOutSize);
    return QLIB_STATUS__NOT_IMPLEMENTED;
}

QLIB_STATUS_T QLIB_CLIENT_Receive(void* socket, void* dataIn, U32 dataInSize, BOOL blocking)
{
    TOUCH(socket);
    TOUCH(dataIn);
    TOUCH(dataInSize);
    TOUCH(blocking);
    return QLIB_STATUS__NOT_IMPLEMENTED;
}
#endif
