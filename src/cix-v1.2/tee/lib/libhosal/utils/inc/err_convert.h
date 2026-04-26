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
#ifndef __ERR_CONVERT_H__
#define __ERR_CONVERT_H__

#include "osal_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef OSAL_ENV_OPTEE_TA
#include <tee_api_types.h>

/* TEE Connection extended API error code.
 * MUST be aligned with connection.h. there is build assert in connection.c to
 * guarantee this */
#define _TEE_EXT_CONN_ERROR_TIMEOUT 0xF1007020
#define _TEE_EXT_CONN_RECEIVE_WANT_MORE 0xF1007021
#define _TEE_EXT_HTTP_DOWNLOAD_DONE 0xF1007030

static inline TEE_Result convert_osal_err_to_tee_result(osal_err_t ret)
{
    if (OSAL_SUCCESS == ret) {
        return TEE_SUCCESS;
    }

    if ((((uint32_t)(ret)) < 0x80000000) || (((uint32_t)(ret)) > 0x8FFFFFFF)) {
        /* the err_t has the same error code with TEE_Result */
        return (TEE_Result)(ret);
    }

    /* convert connection adaption layer error code */
    if (OSAL_ERROR_CONN_RECV_WANT_MORE == ret) {
        return _TEE_EXT_CONN_RECEIVE_WANT_MORE;
    }

    /* convert all timeout to busy */
    if ((OSAL_ERROR_CONN_SEND_TIMEOUT == ret) ||
        (OSAL_ERROR_CONN_RECV_TIMEOUT == ret) ||
        (OSAL_ERROR_CONN_CONNECT_TIMEOUT == ret) ||
        (OSAL_ERROR_SOCKET_TIMEOUT == ret)) {
        return _TEE_EXT_CONN_ERROR_TIMEOUT;
    }

    /* when http download done, return no data */
    if (OSAL_ERROR_HTTP_DOWNLOAD_DONE == ret) {
        return _TEE_EXT_HTTP_DOWNLOAD_DONE;
    }

    /* otherwise, all error code is converted to generic error */
    return TEE_ERROR_GENERIC;
}
#endif /* OSAL_ENV_OPTEE_TA */

#ifdef OSAL_ENV_LINUX_USER
#include <errno.h>

static inline int32_t convert_osal_err_to_errno(osal_err_t ret)
{
    switch (ret)
    {
    case OSAL_SUCCESS:
        return 0;
    case OSAL_ERROR_OUT_OF_MEMORY:
        return ENOMEM;
    default:
        return -1;
    }
}
#endif /* OSAL_ENV_LINUX_USER */

#ifdef __cplusplus
}
#endif

#endif /* __ERR_CONVERT_H__*/
