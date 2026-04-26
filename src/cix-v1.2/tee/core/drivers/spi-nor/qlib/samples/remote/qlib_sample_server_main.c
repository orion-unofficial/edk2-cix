/************************************************************************************************************
 * @internal
 * @remark     Winbond Electronics Corporation - Confidential
 * @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
 * @endinternal
 *
 * @file       qlib_sample_server_main.c
 * @brief      This file includes sample QLIB server implementation
 *
 * ### project qlib
 *
 ***********************************************************************************************************/
#ifdef _WIN32

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <ws2tcpip.h>
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")

#include "qlib.h"
#include "qlib_server.h"
#include "qlib_sample_server_client_common.h"
#include "qlib_sample_qconf.h"
#include "qlib_sample_secure_storage.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                  TYPES                                                  */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
typedef enum
{
    SERVER_STATE_INIT,
    SERVER_STATE_REGISTRATION_COMPLETED,
    SERVER_STATE_TRYING_TO_CONNECT,
    SERVER_STATE_READY,
} SERVER_STATE_T;

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 GLOBALS                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
static SERVER_STATE_T state;

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                 DEFINES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#define TEXT_COLOR_RED    FOREGROUND_RED
#define TEXT_COLOR_GREEN  FOREGROUND_GREEN
#define TEXT_COLOR_BLUE   FOREGROUND_BLUE
#define TEXT_COLOR_YELLOW (FOREGROUND_RED | FOREGROUND_GREEN)
#define TEXT_COLOR_WHITE  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)

#define COLOR_PRINTF(color, ...)                             \
    {                                                        \
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);   \
                                                             \
        SetConsoleTextAttribute(hConsole, color);            \
        printf(__VA_ARGS__);                                 \
        SetConsoleTextAttribute(hConsole, TEXT_COLOR_WHITE); \
    }

#define STATUS_RET_CHECK_RETURN_1(func, message)   \
    {                                              \
        QLIB_STATUS_T ___ret;                      \
        if (QLIB_STATUS__OK != (___ret = (func)))  \
        {                                          \
            COLOR_PRINTF(TEXT_COLOR_RED, message); \
            return 1;                              \
        }                                          \
    }

#ifndef PRINT_BUF
#define PRINT_BUF(buf, size)                                             \
    {                                                                    \
        unsigned int ____i;                                              \
        for (____i = 0; ____i < (size); ++____i)                         \
        {                                                                \
            COLOR_PRINTF(TEXT_COLOR_WHITE, "%02x", ((U8*)(buf))[____i]); \
        }                                                                \
        COLOR_PRINTF(TEXT_COLOR_WHITE, "\n\r");                          \
    }
#endif // PRINT_BUF

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                             LOCAL FUNCTIONS                                             */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief   Packet processing thread
 *
 * @param   data    Qlib client object
 *
 * @return O if no error occurred
************************************************************************************************************/
DWORD WINAPI PacketProcessThread(void* data)
{
    QLIB_STATUS_T         ret;
    QLIB_SERVER_CLIENT_T* client = (QLIB_SERVER_CLIENT_T*)data;

    do
    {
        ret = QLIB_SERVER_HandlePacket(client);
    } while (QLIB_STATUS__OK == ret);
    COLOR_PRINTF(TEXT_COLOR_WHITE, "Connection to client lost.\r\n");

    return 0;
}

/************************************************************************************************************
 * @brief   Custom packet callback
 *
 * @param[in]   client  pointer to client object
 * @param[in]   buf     Custom packet data buffer
 * @param[in]   size    Custom packet size
************************************************************************************************************/
QLIB_STATUS_T OnCustomPacket(struct _QLIB_SERVER_CLIENT_T* client, void* buf, U32 size)
{
    TOUCH(client);
    TOUCH(buf);
    TOUCH(size);

    COLOR_PRINTF(TEXT_COLOR_YELLOW, "Custom packet received (%lu bytes):", (unsigned long)size);
    PRINT_BUF(buf, size);

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief   Registration callback
************************************************************************************************************/
QLIB_STATUS_T OnRegistration(struct _QLIB_SERVER_CLIENT_T* client)
{
    // Client registered and we are synchronized. Now we can use QLIB API with client's qlibContext.
    // Now we shall wait for QLIB_PACKET_CLIENT2SERVER__DATA_IS_READY notification.
    state = SERVER_STATE_REGISTRATION_COMPLETED;

    COLOR_PRINTF(TEXT_COLOR_YELLOW,
                 "Client %08lx%08lx completed registration\n\r",
                 (unsigned long)client->qlibContext.dieState[0].wid[1],
                 (unsigned long)client->qlibContext.dieState[0].wid[0]);

    return QLIB_STATUS__OK;
}

/************************************************************************************************************
 * @brief   Custom packet callback
 *
 * @param[in]   port    Port number
 * @param[out]  sock    Pointer to the new socket handler
 *
 * @return  0 on successful termination
************************************************************************************************************/
int OpenListenerSocket(char* port, SOCKET* sock)
{
    int              iResult;
    SOCKET           listenSocket = INVALID_SOCKET;
    SOCKET           clientSocket = INVALID_SOCKET;
    struct addrinfo* result       = NULL;
    struct addrinfo  hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags    = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, port, &hints, &result);
    if (iResult != 0)
    {
        COLOR_PRINTF(TEXT_COLOR_RED, "getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    COLOR_PRINTF(TEXT_COLOR_WHITE, "Waiting for client\r\n");
    listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listenSocket == INVALID_SOCKET)
    {
        COLOR_PRINTF(TEXT_COLOR_RED, "socket failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        COLOR_PRINTF(TEXT_COLOR_RED, "bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR)
    {
        COLOR_PRINTF(TEXT_COLOR_RED, "listen failed with error: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Accept a client socket
    clientSocket = accept(listenSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET)
    {
        COLOR_PRINTF(TEXT_COLOR_RED, "accept failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    *sock = clientSocket;
    return 0;
}

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                               ENTRY POINT                                               */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

/************************************************************************************************************
 * @brief   MAIN ENTRY POINT
 *
 * @param   argc
 * @param   argv
 *
 * @return  0 on successful termination
************************************************************************************************************/
int main(int argc, char** argv)
{
    U32                            buf[1024 / sizeof(U32)] = {0};
    QLIB_SERVER_CLIENT_T           client;
    QLIB_SERVER_CLIENT_CALLBACKS_T callbacks = {buf, sizeof(buf), OnCustomPacket, OnRegistration, NULL};
    QLIB_STATUS_T                  ret;
    char*                          port = QLIB_REMOTE_SAMPLE_PORT;
    SOCKET                         sock;
    WSADATA                        wsaData;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Unused parameters                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    TOUCH(argc);
    TOUCH(argv);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Setup globals                                                                                       */
    /*-----------------------------------------------------------------------------------------------------*/
    state = SERVER_STATE_INIT;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Initialize Winsock                                                                                  */
    /*-----------------------------------------------------------------------------------------------------*/
    if (WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        COLOR_PRINTF(TEXT_COLOR_RED, "WSAStartup failed with error\n");
        return QLIB_STATUS__TEST_FAIL;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Open listener socket                                                                                */
    /*-----------------------------------------------------------------------------------------------------*/
    if (OpenListenerSocket(port, &sock))
    {
        return 1;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Initialize client                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    STATUS_RET_CHECK_RETURN_1(QLIB_SERVER_InitClient(&client, (void*)sock, &callbacks), "Init client FAILED.\r\n");

    /*-----------------------------------------------------------------------------------------------------*/
    /* Initialize Qlib                                                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    STATUS_RET_CHECK_RETURN_1(QLIB_InitLib(&client.qlibContext), "Qlib init FAILED.\r\n");

    /*-----------------------------------------------------------------------------------------------------*/
    /* Create packet processing thread for this client                                                     */
    /*-----------------------------------------------------------------------------------------------------*/
    (void)CreateThread(NULL, 0, PacketProcessThread, &client, 0, NULL);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Start working with this client                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    while (1)
    {
        switch (state)
        {
            case SERVER_STATE_INIT:
            {
                break;
            }

            case SERVER_STATE_REGISTRATION_COMPLETED:
            {
                state = SERVER_STATE_TRYING_TO_CONNECT;
                break;
            }

            case SERVER_STATE_TRYING_TO_CONNECT:
            {
                ret = QLIB_Connect(&client.qlibContext);
                if (ret == QLIB_STATUS__OK)
                {
                    COLOR_PRINTF(TEXT_COLOR_YELLOW, "Connection with client succeeded\n\r");
                    state = SERVER_STATE_READY;
                }
                else
                {
                    COLOR_PRINTF(TEXT_COLOR_RED, "Connection with client FAILED. Will retry\n\r");
                    (void)QLIB_Disconnect(&client.qlibContext);
                }

                break;
            }

            case SERVER_STATE_READY:
            {
                /*-----------------------------------------------------------------------------------------*/
                /* Run the samples                                                                         */
                /*-----------------------------------------------------------------------------------------*/
                STATUS_RET_CHECK_RETURN_1(QLIB_Disconnect(&client.qlibContext), "Qlib disconnect FAILED.\r\n");
                STATUS_RET_CHECK_RETURN_1(QLIB_SAMPLE_QconfRecovery(&client.qlibContext), "Qconf recovery FAILED.\r\n");
                STATUS_RET_CHECK_RETURN_1(QLIB_SAMPLE_QconfConfig(&client.qlibContext), "Qconf config sample FAILED.\r\n");
                STATUS_RET_CHECK_RETURN_1(QLIB_Connect(&client.qlibContext), "Re-connection with client FAILED.\r\n");
                STATUS_RET_CHECK_RETURN_1(QLIB_SAMPLE_SecureSectionWithPlainAccessRead(&client.qlibContext),
                                          "Secure section with plain access read sample FAILED.\r\n");
                STATUS_RET_CHECK_RETURN_1(QLIB_SAMPLE_SecureSectionFullKey(&client.qlibContext),
                                          "Secure section full key sample FAILED.\r\n");
                STATUS_RET_CHECK_RETURN_1(QLIB_SAMPLE_SecureSectionRestrictedKey(&client.qlibContext),
                                          "Secure section restricted key sample FAILED.\r\n");

                COLOR_PRINTF(TEXT_COLOR_YELLOW, "Sample server sample succeeded\r\n");
                return 0;
            }
        }
    }

    return 0;
}

#else
/*---------------------------------------------------------------------------------------------------------*/
/*                                                  STUB                                                   */
/*---------------------------------------------------------------------------------------------------------*/
int main(void)
{
    return 0;
}
#endif
