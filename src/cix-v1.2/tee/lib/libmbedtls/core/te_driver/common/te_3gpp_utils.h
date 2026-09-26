/*
 * Copyright (c) 2021, Arm Technology (China) Co., Ltd.
 * All rights reserved.
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#ifndef __TRUSTENGINE_3GPP_UTILS_H__
#define __TRUSTENGINE_3GPP_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * \brief               Identifier 1 byte = 8 bits
 */
#define BYTE_BITS (8U)

/**
 * \brief               Convert bits to bytes and round down.
 */
#define FLOOR_BYTES(l) ((l) / BYTE_BITS)

/**
 * \brief               Convert bits to bytes and round up.
 */
#define CEIL_BYTES(l) (((l) / BYTE_BITS) + ((((l) % BYTE_BITS) > 0) ? 1 : 0))

/**
 * \brief               Clear the end of \p _a_ n-bits data to 0 from \p _s_ .
 */
#define WIPE_TAIL_BITS(_a_, _s_) ((_a_) & (0xFF << ((BYTE_BITS - (_s_)) % BYTE_BITS)))

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_3GPP_UTILS_H__ */