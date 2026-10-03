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

#include "osal_log.h"
#include <stdarg.h>
#include <printk.h>
#include <trace.h>

__attribute__((format(printf, 1, 2))) int32_t osal_log_printf(const char *fmt,
                                                              ...)
{
    va_list ap;
    char buf[MAX_PRINT_SIZE] = {0};
    int res                  = 0;

    va_start(ap, fmt);
    res = vsnprintk(buf, MAX_PRINT_SIZE - 1, fmt, ap);
    va_end(ap);

    if (res > 0) {
        trace_ext_puts(buf);
    }

    return res;
}
