/**
 * Copyright (C), 2018-2020, Arm Technology (China) Co., Ltd.
 * All rights reserved
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#ifndef __OSAL_SOCKET_H__
#define __OSAL_SOCKET_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSAL_SOCKET_TIMEOUT_NOWAIT (0)
#define OSAL_SOCKET_TIMEOUT_FOREVER (-1)

/* protocol family, ipv4 or ipv6 */
typedef enum _osal_socket_domain_t {
    OSAL_SOCKET_AF_INET   = 2,
    OSAL_SOCKET_AF_INET6  = 10,
    OSAL_SOCKET_AF_UNSPEC = 0,
} osal_socket_domain_t;

/* Socket protocol types (TCP/UDP) */
typedef enum _osal_socket_type_t {
    OSAL_SOCKET_STREAM = 1,
    OSAL_SOCKET_DGRAM  = 2,
} osal_socket_type_t;

typedef void *osal_socket_t;

/* the socket client APIs */
OSAL_API osal_err_t osal_socket_create(osal_socket_t *handler,
                                       osal_socket_type_t type,
                                       osal_socket_domain_t domain);
OSAL_API osal_err_t osal_socket_connect(osal_socket_t handler,
                                        const char *server_addr,
                                        uint16_t server_port,
                                        int32_t timeout_ms);
OSAL_API osal_err_t osal_socket_send(osal_socket_t handler,
                                     const uint8_t *buf,
                                     size_t *size,
                                     int32_t timeout_ms);
OSAL_API osal_err_t osal_socket_receive(osal_socket_t handler,
                                        uint8_t *buf,
                                        size_t *size,
                                        int32_t timeout_ms);
OSAL_API void osal_socket_disconnect(osal_socket_t handler);
OSAL_API void osal_socket_destroy(osal_socket_t handler);


#ifdef __cplusplus
}
#endif

#endif /* __OSAL_SOCKET_H__ */
