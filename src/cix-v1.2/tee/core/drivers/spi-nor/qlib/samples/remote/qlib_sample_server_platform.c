/************************************************************************************************************
 * @internal
 * @remark     Winbond Electronics Corporation - Confidential
 * @copyright  Copyright (c) 2022 by Winbond Electronics Corporation . All rights reserved
 * @endinternal
 *
 * @file       qlib_sample_server_platform.c
 * @brief      This file includes platform specific implementation for QLIB server
 *
 * ### project qlib
 *
 ***********************************************************************************************************/

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#ifdef _WIN32
#include "windows.h"
#endif
#include "stdio.h"

#include "qlib.h"
#include "qlib_server_platform.h"
#include "qlib_server_client_common.h"
#include "qlib_sample_server_client_common.h"
#include "qlib_sample_qconf.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                   API                                                   */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

#ifdef _WIN32
QLIB_STATUS_T QLIB_SERVER_Send(void* socket, void* dataOut, U32 dataOutSize)
{
    int iResult;

    iResult = send((SOCKET)socket, (char*)dataOut, dataOutSize, 0);
    if (iResult == SOCKET_ERROR)
    {
        printf("send failed with error: %d closing socket\n", WSAGetLastError());
        closesocket((SOCKET)socket);
        WSACleanup();
        return QLIB_STATUS__COMMUNICATION_ERR;
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_SERVER_Receive(void* socket, void* dataIn, U32 dataInSize, BOOL blocking)
{
    int rxLen;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Unused parameters                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    TOUCH(blocking);

    rxLen = recv((SOCKET)socket, dataIn, dataInSize, 0);
    if (rxLen <= 0)
    {
        printf("Connection lost\r\n");
        closesocket((SOCKET)socket);
        return QLIB_STATUS__COMMUNICATION_ERR;
    }

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_SERVER_GetKeys(const QLIB_WID_T id, KEY_ARRAY_T fk, KEY_ARRAY_T rk)
{
    /*-----------------------------------------------------------------------------------------------------*/
    /* Unused parameters                                                                                   */
    /*-----------------------------------------------------------------------------------------------------*/
    TOUCH(id);

    static const KEY_T g_tRestrictedKey0 = QCONF_RESTRICTED_K_0;
    static const KEY_T g_tRestrictedKey1 = QCONF_RESTRICTED_K_1;
    static const KEY_T g_tRestrictedKey2 = QCONF_RESTRICTED_K_2;
    static const KEY_T g_tRestrictedKey3 = QCONF_RESTRICTED_K_3;
    static const KEY_T g_tRestrictedKey4 = QCONF_RESTRICTED_K_4;
    static const KEY_T g_tRestrictedKey5 = QCONF_RESTRICTED_K_5;
    static const KEY_T g_tRestrictedKey6 = QCONF_RESTRICTED_K_6;
    static const KEY_T g_tRestrictedKey7 = QCONF_RESTRICTED_K_7;
    static const KEY_T g_tFullAccessKey0 = QCONF_FULL_ACCESS_K_0;
    static const KEY_T g_tFullAccessKey1 = QCONF_FULL_ACCESS_K_1;
    static const KEY_T g_tFullAccessKey2 = QCONF_FULL_ACCESS_K_2;
    static const KEY_T g_tFullAccessKey3 = QCONF_FULL_ACCESS_K_3;
    static const KEY_T g_tFullAccessKey4 = QCONF_FULL_ACCESS_K_4;
    static const KEY_T g_tFullAccessKey5 = QCONF_FULL_ACCESS_K_5;
    static const KEY_T g_tFullAccessKey6 = QCONF_FULL_ACCESS_K_6;
    static const KEY_T g_tFullAccessKey7 = QCONF_FULL_ACCESS_K_7;

    memcpy(&rk[0], g_tRestrictedKey0, sizeof(KEY_T));
    memcpy(&rk[1], g_tRestrictedKey1, sizeof(KEY_T));
    memcpy(&rk[2], g_tRestrictedKey2, sizeof(KEY_T));
    memcpy(&rk[3], g_tRestrictedKey3, sizeof(KEY_T));
    memcpy(&rk[4], g_tRestrictedKey4, sizeof(KEY_T));
    memcpy(&rk[5], g_tRestrictedKey5, sizeof(KEY_T));
    memcpy(&rk[6], g_tRestrictedKey6, sizeof(KEY_T));
    memcpy(&rk[7], g_tRestrictedKey7, sizeof(KEY_T));

    memcpy(&fk[0], g_tFullAccessKey0, sizeof(KEY_T));
    memcpy(&fk[1], g_tFullAccessKey1, sizeof(KEY_T));
    memcpy(&fk[2], g_tFullAccessKey2, sizeof(KEY_T));
    memcpy(&fk[3], g_tFullAccessKey3, sizeof(KEY_T));
    memcpy(&fk[4], g_tFullAccessKey4, sizeof(KEY_T));
    memcpy(&fk[5], g_tFullAccessKey5, sizeof(KEY_T));
    memcpy(&fk[6], g_tFullAccessKey6, sizeof(KEY_T));
    memcpy(&fk[7], g_tFullAccessKey7, sizeof(KEY_T));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_SERVER_TimerStart(void** timer)
{
    U64 t;
    *timer = malloc(sizeof(U64));
    if (*timer == NULL)
    {
        return QLIB_STATUS__OUT_OF_MEMORY;
    }

    t = GetTickCount64();
    memcpy(*timer, &t, sizeof(t));

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_SERVER_TimerGetMS(void* timer, U64* ms)
{
    U64 t;

    memcpy(&t, timer, sizeof(t));

    *ms = GetTickCount64() - t;

    return QLIB_STATUS__OK;
}

QLIB_STATUS_T QLIB_SERVER_TimerStop(void* timer)
{
    free(timer);

    return QLIB_STATUS__OK;
}
#else
/*---------------------------------------------------------------------------------------------------------*/
/*                                                  STUB                                                   */
/*---------------------------------------------------------------------------------------------------------*/

QLIB_STATUS_T QLIB_SERVER_Send(void* socket, void* dataOut, U32 dataOutSize)
{
    TOUCH(socket);
    TOUCH(dataOut);
    TOUCH(dataOutSize);
    return QLIB_STATUS__NOT_IMPLEMENTED;
}

QLIB_STATUS_T QLIB_SERVER_Receive(void* socket, void* dataIn, U32 dataInSize, BOOL blocking)
{
    TOUCH(socket);
    TOUCH(dataIn);
    TOUCH(dataInSize);
    TOUCH(blocking);
    return QLIB_STATUS__NOT_IMPLEMENTED;
}

QLIB_STATUS_T QLIB_SERVER_GetKeys(const QLIB_WID_T id, KEY_ARRAY_T fk, KEY_ARRAY_T rk)
{
    TOUCH(id);
    TOUCH(fk);
    TOUCH(rk);
    return QLIB_STATUS__NOT_IMPLEMENTED;
}

QLIB_STATUS_T QLIB_SERVER_TimerStart(void** timer)
{
    TOUCH(timer);
    return QLIB_STATUS__NOT_IMPLEMENTED;
}

QLIB_STATUS_T QLIB_SERVER_TimerGetMS(void* timer, U64* ms)
{
    TOUCH(timer);
    TOUCH(ms);
    return QLIB_STATUS__NOT_IMPLEMENTED;
}

QLIB_STATUS_T QLIB_SERVER_TimerStop(void* timer)
{
    TOUCH(timer);
    return QLIB_STATUS__NOT_IMPLEMENTED;
}
#endif
