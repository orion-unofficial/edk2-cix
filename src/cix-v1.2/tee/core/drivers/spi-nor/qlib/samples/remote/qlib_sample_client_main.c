/************************************************************************************************************
 * @internal
 * @remark     Winbond Electronics Corporation - Confidential
 * @copyright  Copyright (c) 2022 by Winbond Electronics Corporation . All rights reserved
 * @endinternal
 *
 * @file       qlib_sample_client_main.c
 * @brief      This file includes sample QLIB client implementation
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

#ifdef _WIN32
#include <conio.h>
#include <ws2tcpip.h>
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

#include "qlib.h"

#include "qlib_client.h"
#include "qlib_server_client_common.h"
#include "qlib_sample_server_client_common.h"
#include "..\qlib_sample.h"

#include "ft4222_bridge.h"
#include "ft4222_gpio.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 DEFINES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define SPI_CLOCK_RATE_IN_Hz FT4222_SPI_FREQ_10M

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                             LOCAL FUNCTIONS                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T onCustomPacket(struct _QLIB_CLIENT_T* client, void* buf, U32 size);

QLIB_STATUS_T onRegistration(struct _QLIB_CLIENT_T* client);

QLIB_STATUS_T onStandardCommand(struct _QLIB_CLIENT_T* client,
                                QLIB_STATUS_T          status,
                                QLIB_BUS_FORMAT_T      busFormat,
                                BOOL                   needWriteEnable,
                                BOOL                   waitWhileBusy,
                                U8                     cmd,
                                const U32*             address,
                                const U8*              writeData,
                                U32                    writeDataSize,
                                U32                    dummyCycles,
                                U8*                    readData,
                                U32                    readDataSize,
                                QLIB_REG_SSR_T*        ssr);

QLIB_STATUS_T onSecureCommand(struct _QLIB_CLIENT_T* client,
                              QLIB_STATUS_T          status,
                              U32                    ctag,
                              const U32*             writeData,
                              U32                    writeDataSize,
                              U32*                   readData,
                              U32                    readDataSize,
                              QLIB_REG_SSR_T*        ssr);

QLIB_STATUS_T onInvalidCommand(struct _QLIB_CLIENT_T* client);

QLIB_STATUS_T qlibSampleClientConnectToServer(const char* port, SOCKET* sock);

/************************************************************************************************************
 * @brief This function handles custom packets from Server
 *
 * @param client
 * @param buf
 * @param size
 *
 * @return QLIB_STATUS__OK if no error occurred, QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T onCustomPacket(struct _QLIB_CLIENT_T* client, void* buf, U32 size)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Unused parameters                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    TOUCH(client);
    TOUCH(buf);
    TOUCH(size);

    QLIB_SAMPLE_PRINTF("Custom command received, not supported in this sample.\r\n");
    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief This functions handles registration event
 *
 * @param client
 *
 * @return QLIB_STATUS__OK if no error occurred, QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T onRegistration(struct _QLIB_CLIENT_T* client)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Unused parameters                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    TOUCH(client);

    QLIB_SAMPLE_PRINTF("Client registered on the server.\r\n");
    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief Standard command handler.
 *
 * @param client
 * @param status
 * @param busFormat
 * @param needWriteEnable
 * @param waitWhileBusy
 * @param cmd
 * @param address
 * @param writeData
 * @param writeDataSize
 * @param dummyCycles
 * @param readData
 * @param readDataSize
 * @param ssr
 *
 * @return QLIB_STATUS__OK if no error occurred, QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T onStandardCommand(struct _QLIB_CLIENT_T* client,
                                QLIB_STATUS_T          status,
                                QLIB_BUS_FORMAT_T      busFormat,
                                BOOL                   needWriteEnable,
                                BOOL                   waitWhileBusy,
                                U8                     cmd,
                                const U32*             address,
                                const U8*              writeData,
                                U32                    writeDataSize,
                                U32                    dummyCycles,
                                U8*                    readData,
                                U32                    readDataSize,
                                QLIB_REG_SSR_T*        ssr)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Unused parameters                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    TOUCH(client);
    TOUCH(status);
    TOUCH(busFormat);
    TOUCH(needWriteEnable);
    TOUCH(waitWhileBusy);
    TOUCH(cmd);
    TOUCH(address);
    TOUCH(writeData);
    TOUCH(writeDataSize);
    TOUCH(dummyCycles);
    TOUCH(readData);
    TOUCH(readDataSize);
    TOUCH(ssr);

    QLIB_SAMPLE_PRINTF("Standard command received and is handled.\r\n");
    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief Secure command handler.
 *
 * @param client
 * @param status
 * @param ctag
 * @param writeData
 * @param writeDataSize
 * @param readData
 * @param readDataSize
 * @param ssr
 *
 * @return QLIB_STATUS__OK if no error occurred, QLIB_STATUS__[ERROR] otherwise
************************************************************************************************************/
QLIB_STATUS_T onSecureCommand(struct _QLIB_CLIENT_T* client,
                              QLIB_STATUS_T          status,
                              U32                    ctag,
                              const U32*             writeData,
                              U32                    writeDataSize,
                              U32*                   readData,
                              U32                    readDataSize,
                              QLIB_REG_SSR_T*        ssr)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Unused parameters                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    TOUCH(client);
    TOUCH(status);
    TOUCH(ctag);
    TOUCH(writeData);
    TOUCH(writeDataSize);
    TOUCH(readData);
    TOUCH(readDataSize);
    TOUCH(ssr);

    QLIB_SAMPLE_PRINTF("Secure command received and is handled, status %d.\r\n", status);
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T onInvalidCommand(struct _QLIB_CLIENT_T* client)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Unused parameters                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    TOUCH(client);

    QLIB_SAMPLE_PRINTF("Invalid command received, nothing to do.\r\n");
    return QLIB_STATUS__OK;
}

QLIB_STATUS_T qlibSampleClientConnectToServer(const char* port, SOCKET* sock)
{
    int              iResult;
    struct addrinfo* result = NULL;
    struct addrinfo* ptr    = NULL;
    struct addrinfo  hints;

    *sock = INVALID_SOCKET;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = (int)IPPROTO_TCP;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Resolve the server address and port                                                                 */
    /*-----------------------------------------------------------------------------------------------------*/
    iResult = getaddrinfo("localhost", port, &hints, &result);
    if (iResult != 0)
    {
        QLIB_SAMPLE_PRINTF("getaddrinfo failed with error: %d\n", iResult);
        (void)WSACleanup();
        return QLIB_STATUS__COMMUNICATION_ERR;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Attempt to connect to an address, until one succeeds                                                */
    /*-----------------------------------------------------------------------------------------------------*/
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {
        /*-------------------------------------------------------------------------------------------------*/
        /* Create a socket for connecting to the server                                                    */
        /*-------------------------------------------------------------------------------------------------*/
        *sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (INVALID_SOCKET == *sock)
        {
            QLIB_SAMPLE_PRINTF("socket failed with error: %d\n", WSAGetLastError());
            (void)WSACleanup();
            return QLIB_STATUS__COMMUNICATION_ERR;
        }

        /*-------------------------------------------------------------------------------------------------*/
        /* Connect to the server                                                                           */
        /*-------------------------------------------------------------------------------------------------*/
        iResult = connect(*sock, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (SOCKET_ERROR == iResult)
        {
            (void)closesocket(*sock);
            *sock = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (*sock == INVALID_SOCKET)
    {
        QLIB_SAMPLE_PRINTF("Unable to connect to server!\n");
        (void)WSACleanup();
        return QLIB_STATUS__COMMUNICATION_ERR;
    }

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief main entry point
************************************************************************************************************/
int main(void)
{
    const char*    port = QLIB_REMOTE_SAMPLE_PORT;
    SOCKET         sock = INVALID_SOCKET;
    QLIB_CONTEXT_T qlibContext;
    QLIB_CLIENT_T  client;
    WSADATA        wsaData;
    FT4222_DESC_T  userData;
    FT_STATUS      ft_status;
    QLIB_STATUS_T  ret;

    const QLIB_CLIENT_CALLBACKS_T callbacks = {
        .customBuf         = NULL,
        .customSize        = 0,
        .onCustomPacket    = onCustomPacket,
        .onConnect         = NULL,
        .onDisconnect      = NULL,
        .onInvalidPacket   = onInvalidCommand,
        .onRegistrationAck = onRegistration,
        .onStandardCmd     = onStandardCommand,
        .onSecureCmd       = onSecureCommand,
    };

    /*-----------------------------------------------------------------------------------------------------*/
    /* Initialize HW                                                                                       */
    /*-----------------------------------------------------------------------------------------------------*/

    ft_status = FT4222_Init(&userData, SPI_CLOCK_RATE_IN_Hz, NULL);
    if (ft_status != FT_OK)
    {
        return (int)ft_status;
    }
    (void)FT4222_GpioInit(&userData);
    (void)FT4222_HwReset(&userData);
    QLIB_SetUserData(&qlibContext, &userData);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Initialize Winsock                                                                                  */
    /*-----------------------------------------------------------------------------------------------------*/
    if (WSAStartup(MAKEWORD(2u, 2u), &wsaData) != 0)
    {
        QLIB_SAMPLE_PRINTF("WSAStartup failed with error\n");
        return (int)QLIB_STATUS__TEST_FAIL;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Initialize QLIB                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
again:
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_InitLib(&qlibContext), ret, again);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Connect(&qlibContext), ret, again);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_InitDevice(&qlibContext, QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_1_1, FALSE)), ret, again);
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_Disconnect(&qlibContext), ret, again);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Initialize CLIENT                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    ret = QLIB_CLIENT_Init(&client, (void*)sock, &qlibContext, &callbacks, NULL);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Connect to server's socket                                                                          */
    /*-----------------------------------------------------------------------------------------------------*/
    (void)qlibSampleClientConnectToServer(port, (SOCKET*)&client.socket);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Turn LED on after successful initialization                                                         */
    /*-----------------------------------------------------------------------------------------------------*/
    //gpio_put(LED_GPIO, 1);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Register the client into the server                                                                 */
    /*-----------------------------------------------------------------------------------------------------*/
    ret = QLIB_CLIENT_Register(&client);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Enter main loop                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    while (1)
    {
        /*-------------------------------------------------------------------------------------------------*/
        /* Process packets from server                                                                     */
        /*-------------------------------------------------------------------------------------------------*/
        ret = QLIB_CLIENT_HandlePacket(&client);
        QLIB_ASSERT_RET(QLIB_STATUS__OK == ret, 1);
    }
}
#else
/*---------------------------------------------------------------------------------------------------------*/
/*                                                  STUB                                                   */
/*---------------------------------------------------------------------------------------------------------*/
int main(void)
{
    printf("Hello, World!!!\n");
    return 0;
}
#endif
