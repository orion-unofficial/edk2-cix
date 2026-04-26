/*
 * Copyright (c) 2018-2019, CIX Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SKY1_DP_GOP_H
#define SKY1_DP_GOP_H

#define DP_GOP_RESERVED  0x16000514
#define SKY1_SIP_DP_GOP_GET 0x1
#define SKY1_SIP_DP_GOP_SET 0x2
#define DP_GOP_MASK        (0xF << 17)

int sky1_dp_gop_handler(u_register_t x1, u_register_t x2);

#endif /*SKY1_DP_GOP_H */
