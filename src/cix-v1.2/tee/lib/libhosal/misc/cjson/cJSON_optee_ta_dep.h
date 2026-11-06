/*
 * Copyright (c) 2020, Arm Technology (China) Co., Ltd.
 * All rights reserved.
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#ifndef __CJSON_DEP_H__
#define __CJSON_DEP_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <trace.h>
#include <assert.h>
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"

#define sprintf(__buf__, __fmt__, ...)                                         \
    snprintf(__buf__, INT_MAX, __fmt__, ##__VA_ARGS__)

//#warning "cJSON in OP-TEE ONLY suport integer type number!"
//#warning "cJSON in OP-TEE doesn't support sscanf function!"

static inline double strtod(const char *str, char **endptr)
{
    int sign = 0;
    uint32_t result = 0;
    char *p         = NULL;

    /* reverse to get the port number */
    result = 0;
    p      = (char *)str;
    if ('-' == *p) {
        sign = -1;
        p++;
    }
    while (true) {
        if (((*p) >= '0') && ((*p) <= '9')) {
            result = result * 10 + ((*p) - '0');
        } else {
            if (*p != '\0') {
                EMSG("Error! cJSON in OP-TEE only support integer "
                     "type number!");
                assert(0);
            }
            *endptr = p;
            break;
        }
        p++;
    }

    if (sign) {
        result = 0 - result;
    }
    return (double)result;
}

static inline int sscanf(const char *str, const char *format, ...)
{
    EMSG("Error! cJSON in OP-TEE doesn't support sscanf!!!");
    assert(0);

    (void)(str);
    (void)(format);

    return -1;
}

static inline void *cjson_dep_malloc(size_t size)
{
    return malloc(size);
}

static inline void *cjson_dep_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static inline void cjson_dep_free(void *ptr)
{
    return free(ptr);
}

#endif /* __CJSON_OPTEE_TA_DEP_H__ */
