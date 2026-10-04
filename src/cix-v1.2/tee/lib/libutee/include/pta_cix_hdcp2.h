// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024, CIX Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __PTA_CIX_HDCP2_H
#define __PTA_CIX_HDCP2_H

#define PTA_CIX_HDCP2_UUID { 0x3f3eb880, 0x3639, 0x11ec, \
			{ 0x9b, 0x9d, 0x0f, 0x3f, 0xc9, 0x46, 0x8f, 0x49 } }

#define PTA_DP_HDCP2_CMD_HW_INIT 0
#define PTA_DP_HDCP2_CMD_CIPHER_EN 1
#define PTA_DP_HDCP2_CMD_CIPHER_DIS 2
#define PTA_DP_HDCP2_CMD_GET_KD 3
#define PTA_DP_HDCP2_CMD_GET_DKEY2 4

#endif /* __PTA_CIX_HDCP2_H */
