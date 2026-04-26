/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2019 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       qlib_errors.h
* @brief      This file contains error types definitions and checks
*
* ### project qlib
*
************************************************************************************************************/

#ifndef __QLIB_ERRORS_H__
#define __QLIB_ERRORS_H__

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib.h"

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                         ERROR CODES DEFINITIONS                                         */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
// clang-format off
#define _QLIB_STATUS(ELEMENT)                                                    \
    {                                                                            \
        ELEMENT(QLIB_STATUS__OK),                                                \
        ELEMENT(QLIB_STATUS__TEST_FAIL),                                         \
                                                                                 \
        ELEMENT(QLIB_STATUS__INVALID_PARAMETER),                                 \
        ELEMENT(QLIB_STATUS__COMMAND_FAIL),                                      \
        ELEMENT(QLIB_STATUS__SYSTEM_IN_INCORRECT_STATE),                         \
        ELEMENT(QLIB_STATUS__NOT_CONNECTED),                                     \
        ELEMENT(QLIB_STATUS__PARAMETER_OUT_OF_RANGE),                            \
        ELEMENT(QLIB_STATUS__INVALID_DATA_SIZE),                                 \
        ELEMENT(QLIB_STATUS__INVALID_DATA_ALIGNMENT),                            \
        ELEMENT(QLIB_STATUS__COMMUNICATION_ERR),                                 \
        ELEMENT(QLIB_STATUS__SECURITY_ERR),                                      \
                                                                                 \
        /* Implementation State */                                               \
        ELEMENT(QLIB_STATUS__NOT_IMPLEMENTED),                                   \
        ELEMENT(QLIB_STATUS__NOT_SUPPORTED),                                     \
        ELEMENT(QLIB_STATUS__OBSOLETE_FUNCTION),                                 \
                                                                                 \
        /* HW errors */                                                          \
        ELEMENT(QLIB_STATUS__COMMAND_IGNORED),                                   \
        ELEMENT(QLIB_STATUS__DEVICE_BUSY),                                       \
        ELEMENT(QLIB_STATUS__DEVICE_ERR),                                        \
        ELEMENT(QLIB_STATUS__DEVICE_ERR_MULTI),                                  \
        ELEMENT(QLIB_STATUS__DEVICE_SESSION_ERR),                                \
        ELEMENT(QLIB_STATUS__DEVICE_INTEGRITY_ERR),                              \
        ELEMENT(QLIB_STATUS__DEVICE_AUTHENTICATION_ERR),                         \
        ELEMENT(QLIB_STATUS__DEVICE_PRIVILEGE_ERR),                              \
        ELEMENT(QLIB_STATUS__DEVICE_SYSTEM_ERR),                                 \
        ELEMENT(QLIB_STATUS__DEVICE_FLASH_ERR),                                  \
        ELEMENT(QLIB_STATUS__DEVICE_MC_ERR),                                     \
        ELEMENT(QLIB_STATUS__HARDWARE_FAILURE),                                  \
        ELEMENT(QLIB_STATUS__TIME_OUT),                                          \
        ELEMENT(QLIB_STATUS__CONNECTIVITY_ERR),                                  \
        ELEMENT(QLIB_STATUS__OUT_OF_MEMORY),                                     \
                                                                                 \
        ELEMENT(QLIB_STATUS__WARNING_SINGLE_ERROR_CORRECTED),                    \
        ELEMENT(QLIB_STATUS__WARNING_MULTIPLE_PROGRAMMING),                      \
                                                                                 \
        ELEMENT(QLIB_STATUS_LAST)                                                \
    }
// clang-format on

typedef enum _QLIB_STATUS(GENERATE_ENUM) QLIB_STATUS_T;

#define QLIB_STATUS_STR_TAB _QLIB_STATUS(GENERATE_STRING)

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                         ERROR UTILS DEFINITIONS                                         */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

#define QLIB_STATUS_RET_CHECK(func)               \
    {                                             \
        QLIB_STATUS_T ___ret;                     \
        if (QLIB_STATUS__OK != (___ret = (func))) \
        {                                         \
            return ___ret;                        \
        }                                         \
    }

#define QLIB_STATUS_RET_CHECK_GOTO(func, ret, exit) \
    {                                               \
        if (QLIB_STATUS__OK != ((ret) = (func)))    \
        {                                           \
            goto exit;                              \
        }                                           \
    }

#define QLIB_ASSERT_RET(cond, err) \
    {                              \
        if (!(cond))               \
        {                          \
            return err;            \
        }                          \
    }

#define QLIB_ASSERT_GOTO(cond, exit) \
    {                                \
        if (!(cond))                 \
        {                            \
            goto exit;               \
        }                            \
    }

#define QLIB_ASSERT_WITH_ERROR_GOTO(cond, err, var, exit) \
    {                                                     \
        if (!(cond))                                      \
        {                                                 \
            (var) = (err);                                \
            goto exit;                                    \
        }                                                 \
    }


#define QLIB_ATOMIC_SEQUENCE_RET(context, func)                             \
    {                                                                       \
        QLIB_STATUS_T __ret;                                                \
        while ((__ret = QLIB_Connect(context)) == QLIB_STATUS__DEVICE_BUSY) \
            ;                                                               \
        QLIB_STATUS_RET_CHECK(__ret);                                       \
        __ret = func;                                                       \
        QLIB_Disconnect(context);                                           \
        QLIB_STATUS_RET_CHECK(__ret);                                       \
    }

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                          ERROR TYPES TO STRING                                          */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/


#ifdef __cplusplus
}
#endif //__cplusplus

#endif // __QLIB_ERRORS_H__
