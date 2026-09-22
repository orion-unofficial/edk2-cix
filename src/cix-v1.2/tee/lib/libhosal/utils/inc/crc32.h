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

#ifndef __CRC32_H__
#define __CRC32_H__

#ifdef __cplusplus
extern "C" {
#endif

static inline unsigned int ___update_crc32(const unsigned char *data,
                                           unsigned long size,
                                           unsigned int crc_init_value)
{
    unsigned long i, j;
    unsigned int byte, tmp_crc, mask;

    tmp_crc = ~crc_init_value;
    for (i = 0; i < size; i++) {
        byte    = (data[i] & 0x000000FF);
        tmp_crc = tmp_crc ^ byte;
        for (j = 0; j < 8; j++) {
            mask    = (tmp_crc & 1) ? 0xFFFFFFFF : 0;
            tmp_crc = (tmp_crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~tmp_crc;
}

/* not week implementation */
static inline unsigned int calc_crc32(const unsigned char *data,
                                      unsigned long size)
{
    return ___update_crc32(data, size, 0);
}

static inline unsigned int update_crc32(const unsigned char *data,
                                        unsigned long size,
                                        unsigned int tmp_crc32)
{
    return ___update_crc32(data, size, tmp_crc32);
}

#ifdef __cplusplus
}
#endif

#endif /* __CRC32_H__ */
