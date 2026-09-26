/**
 * Copyright (C), 2018-2020, Arm Technology (China) Co., Ltd.
 * All rights reserved
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and
 * any derivative work shall retain this copyright notice.
 *
 */
#include "osal_log.h"
#include "osal_err.h"
#include "osal_assert.h"
#include "osal_mem.h"
#include "osal_socket.h"
#include "utils_ext.h"
#include <tee_internal_api.h>
#include <tee_isocket.h>
#include <tee_tcpsocket.h>
#include <tee_udpsocket.h>

#define _GET_TEE_IP_VERSION(__domin__)                                         \
    (((__domin__) == OSAL_SOCKET_AF_INET)                                      \
         ? (TEE_IP_VERSION_4)                                                  \
         : (((__domin__) == OSAL_SOCKET_AF_INET6) ? (TEE_IP_VERSION_6)         \
                                                  : (TEE_IP_VERSION_DC)))

typedef struct _socket_context_t {
    TEE_iSocketHandle ctx;
    TEE_iSocket *socket;
    bool is_server;
    osal_socket_type_t type;
    osal_socket_domain_t domain;
} socket_context_t;

osal_err_t osal_socket_create(osal_socket_t *handler,
                              osal_socket_type_t type,
                              osal_socket_domain_t domain)
{
    osal_err_t ret        = OSAL_SUCCESS;
    socket_context_t *ctx = NULL;

    OSAL_ASSERT_MSG(handler, "Parameter handler is NULL!\n");

    ctx = osal_malloc(sizeof(socket_context_t));
    UTILS_CHECK_CONDITION(ctx, OSAL_ERROR_OUT_OF_MEMORY, "Malloc %zd failed!\n",
                         sizeof(socket_context_t));

    memset(ctx, 0, sizeof(socket_context_t));

    /* default socket is client */
    ctx->is_server = false;
    ctx->socket    = NULL;
    ctx->type      = type;
    ctx->domain    = domain;

    *handler = (osal_socket_t)(ctx);

finish:
    return ret;
}

osal_err_t osal_socket_connect(osal_socket_t handler,
                               const char *server_addr,
                               uint16_t server_port,
                               int32_t timeout_ms)
{
    osal_err_t ret                = OSAL_SUCCESS;
    TEE_Result tee_res            = TEE_ERROR_GENERIC;
    socket_context_t *ctx         = NULL;
    TEE_tcpSocket_Setup tcp_setup = {0};
    TEE_udpSocket_Setup udp_setup = {0};
    void *setup                   = NULL;
    uint32_t protocol_error       = 0;

    OSAL_ASSERT_MSG(handler, "Parameter handler is NULL!\n");
    OSAL_ASSERT_MSG(server_addr, "Parameter server_addr is NULL!\n");

    if (timeout_ms != OSAL_SOCKET_TIMEOUT_FOREVER) {
        OSAL_LOG_DEBUG(
            "Warnning! iSocket in TEE doesn't support connect timeout!!!\n");
        OSAL_LOG_DEBUG("Always connect forever for timeout ms: %d\n",
                      timeout_ms);
    }

    ctx = (socket_context_t *)(handler);

    ctx->is_server = false;
    if (ctx->type == OSAL_SOCKET_STREAM) {
        ctx->socket           = TEE_tcpSocket;
        tcp_setup.ipVersion   = _GET_TEE_IP_VERSION(ctx->domain);
        tcp_setup.server_addr = (char *)server_addr;
        tcp_setup.server_port = server_port;
        setup                 = &tcp_setup;
    } else {
        ctx->socket           = TEE_udpSocket;
        udp_setup.ipVersion   = _GET_TEE_IP_VERSION(ctx->domain);
        udp_setup.server_addr = (char *)server_addr;
        udp_setup.server_port = server_port;
        setup                 = &udp_setup;
    }
    tee_res = ctx->socket->open(&(ctx->ctx), setup, &protocol_error);
    UTILS_CHECK_CONDITION(
        (tee_res == TEE_SUCCESS) && (protocol_error == TEE_SUCCESS),
        OSAL_ERROR_SOCKET_ERR,
        "TEE iSocket Open failed: tee_res: 0x%x, protocol error: 0x%x\n",
        tee_res, protocol_error);

    ret = OSAL_SUCCESS;
finish:
    return ret;
}

osal_err_t osal_socket_send(osal_socket_t handler,
                            const uint8_t *buf,
                            size_t *size,
                            int32_t timeout_ms)
{
    osal_err_t ret               = OSAL_SUCCESS;
    TEE_Result tee_res           = TEE_ERROR_GENERIC;
    socket_context_t *ctx        = NULL;
    uint32_t tmp_size            = 0;
    uint32_t tee_isocket_timeout = 0;

    OSAL_ASSERT_MSG(handler, "Parameter handler is NULL!\n");
    OSAL_ASSERT_MSG(buf, "Parameter buf is NULL!\n");
    OSAL_ASSERT_MSG(size, "Parameter size is NULL!\n");

    ctx = (socket_context_t *)(handler);

    if (timeout_ms != OSAL_SOCKET_TIMEOUT_FOREVER) {
        OSAL_LOG_DEBUG(
            "Warnning! iSocket in TEE doesn't support send with timeout!!!\n");
        OSAL_LOG_DEBUG("Always send with timeout forever for timeout ms: %d\n",
                      timeout_ms);
    }

    tee_isocket_timeout = (timeout_ms == OSAL_SOCKET_TIMEOUT_FOREVER)
                              ? (TEE_TIMEOUT_INFINITE)
                              : ((uint32_t)timeout_ms);
    tmp_size = (uint32_t)(*size);
    tee_res  = ctx->socket->send(ctx->ctx, (const void *)buf, &tmp_size,
                                tee_isocket_timeout);
    if (tee_res == TEE_ISOCKET_ERROR_TIMEOUT) {
        ret = OSAL_ERROR_SOCKET_TIMEOUT;
        goto finish;
    }
    if (tee_res == TEE_ERROR_COMMUNICATION) {
        ret = OSAL_ERROR_SOCKET_RESET;
        goto finish;
    }
    UTILS_CHECK_CONDITION(TEE_SUCCESS == tee_res, OSAL_ERROR_SOCKET_ERR,
                         "TEE iSocket send failed: res: 0x%x, error: 0x%x\n",
                         tee_res, ctx->socket->error(ctx->ctx));

    /* update write size */
    *size = tmp_size;

finish:
    return ret;
}

osal_err_t osal_socket_receive(osal_socket_t handler,
                               uint8_t *buf,
                               size_t *size,
                               int32_t timeout_ms)
{
    osal_err_t ret               = OSAL_SUCCESS;
    TEE_Result tee_res           = TEE_ERROR_GENERIC;
    socket_context_t *ctx        = NULL;
    uint32_t tmp_size            = 0;
    uint32_t tee_isocket_timeout = 0;

    OSAL_ASSERT_MSG(handler, "Parameter handler is NULL!\n");
    OSAL_ASSERT_MSG(buf, "Parameter buf is NULL!\n");
    OSAL_ASSERT_MSG(size, "Parameter size is NULL!\n");

    ctx = (socket_context_t *)(handler);

    /* socket in tee-supplicant support recv with no wait */

    tee_isocket_timeout = (timeout_ms == OSAL_SOCKET_TIMEOUT_FOREVER)
                              ? (TEE_TIMEOUT_INFINITE)
                              : ((uint32_t)timeout_ms);
    tmp_size = (uint32_t)(*size);
    tee_res  = ctx->socket->recv(ctx->ctx, (void *)buf, &tmp_size,
                                tee_isocket_timeout);
    if (tee_res == TEE_ISOCKET_ERROR_TIMEOUT) {
        ret = OSAL_ERROR_SOCKET_TIMEOUT;
        goto finish;
    }
    if (tee_res == TEE_ERROR_COMMUNICATION) {
        ret = OSAL_ERROR_SOCKET_RESET;
        goto finish;
    }

    UTILS_CHECK_CONDITION(TEE_SUCCESS == tee_res, OSAL_ERROR_SOCKET_ERR,
                         "TEE iSocket receive failed: res: 0x%x, error: 0x%x\n",
                         tee_res, ctx->socket->error(ctx->ctx));

    /* update write size */
    *size = tmp_size;

finish:
    return ret;
}

void osal_socket_disconnect(osal_socket_t handler)
{
    osal_err_t ret        = OSAL_SUCCESS;
    TEE_Result tee_res    = TEE_ERROR_GENERIC;
    socket_context_t *ctx = NULL;

    OSAL_ASSERT_MSG(handler, "Parameter handler is NULL!\n");

    ctx = (socket_context_t *)(handler);

    tee_res = ctx->socket->close(ctx->ctx);
    UTILS_CHECK_CONDITION((TEE_SUCCESS == tee_res) ||
                             (TEE_ERROR_COMMUNICATION == tee_res),
                         OSAL_ERROR_SOCKET_ERR,
                         "TEE iSocket close failed: res: 0x%x, error: 0x%x\n",
                         tee_res, ctx->socket->error(ctx->ctx));

    ctx->socket    = NULL;
    ctx->is_server = false;
finish:
    if (OSAL_SUCCESS != ret) {
        TEE_Panic(tee_res);
    }
    return;
}

void osal_socket_destroy(osal_socket_t handler)
{
    socket_context_t *ctx = NULL;

    OSAL_ASSERT_MSG(handler, "Parameter handler is NULL!\n");

    ctx = (socket_context_t *)(handler);

    osal_free(ctx);

    return;
}
