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
#ifndef __STRING_EXT_H__
#define __STRING_EXT_H__

#include "osal_common.h"
#include "osal_err.h"

#ifdef __cplusplus
extern "C" {
#endif

extern osal_err_t
utils_bin_to_str(uint8_t *data, size_t size, char *str, size_t str_buf_size);
extern osal_err_t utils_str_to_bin(const char *str,
                            bool is_with_prefix,
                            uint8_t *data,
                            size_t size);
extern void utils_invert_data(uint8_t *src, uint8_t *dst, size_t size);
extern bool utils_is_valid_hex_str(const char *str, bool is_with_prefix);
extern osal_err_t utils_str_to_dec_num(const char *str, size_t str_len, uint32_t *num);
extern uint8_t *utils_alloc_printf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif /* __STRING_EXT_H__*/
