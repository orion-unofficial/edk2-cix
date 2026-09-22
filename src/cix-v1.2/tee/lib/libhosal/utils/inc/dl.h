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

#ifndef __DL_H__
#define __DL_H__

#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *dlhandler_t;

OSAL_API int dl_open(dlhandler_t *handler, const char *dl_file);
OSAL_API void dl_close(dlhandler_t handler);
OSAL_API int dl_symbol(dlhandler_t handler,
                                  const char *symbol_str,
                                  void **symbol);

#ifdef __cplusplus
}
#endif

#endif /* __DL_H__ */
