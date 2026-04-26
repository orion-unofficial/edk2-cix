/**
 * Copyright (C), 2020, Arm Technology (China) Co., Ltd.
 * All rights reserved
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#ifndef __PROPERTY_H__
#define __PROPERTY_H__

#ifndef __ASSEMBLY__

#if defined __cplusplus || defined __cplusplus__
extern "C" {
#endif

enum {
    PROP_SUCCESS           = 0,
    PROP_ERR_OVERFLOW      = 0xFFFF300F,
    PROP_ERR_ACCESS_DENIED = 0xFFFF0001,
    PROP_ERR_EXCESS_DATA   = 0xFFFF0004,
    PROP_ERR_GENERIC       = 0xFFFF0000,
};

extern int dump_property(char **propdata);
extern char *get_property(const char *key);
extern int set_property(const char *key, const char *value);

#if defined __cplusplus || defined __cplusplus__
}
#endif

#endif  //__ASSEMBLY__
#endif
