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

#ifndef HDCP2_HW_TX__
#define HDCP2_HW_TX__

#include <stdint.h>
#include <stdbool.h>
#include <mm/core_mmu.h>

#define DP0_HDCP_CTL_BASE                                   0x14064000
#define DP1_HDCP_CTL_BASE                                   0x140d4000
#define DP2_HDCP_CTL_BASE                                   0x14144000
#define DP3_HDCP_CTL_BASE                                   0x141b4000
#define DP4_HDCP_CTL_BASE                                   0x14224000
#define HDCP_CTL_SIZE                                       0x500

//------------------------------------------------------------------------------
//   HDCP control
//------------------------------------------------------------------------------
#define TR_DPTX_HDCP_ENABLE                                 0x400ul
#define TR_DPTX_HDCP_MODE                                   0x404ul
#define TR_DPTX_HDCP_KS_31_0                                0x408ul
#define TR_DPTX_HDCP_KS_63_32                               0x40cul
#define TR_DPTX_HDCP_KM_31_0                                0x410ul
#define TR_DPTX_HDCP_KM_63_32                               0x414ul
#define TR_DPTX_HDCP_AN_31_0                                0x418ul
#define TR_DPTX_HDCP_RTX_31_0                               0x418ul
#define TR_DPTX_HDCP_AN_63_32                               0x41cul
#define TR_DPTX_HDCP_RTX_63_32                              0x41cul
#define TR_DPTX_HDCP_RESERVED_420                           0x420ul
#define TR_DPTX_HDCP_AUTH_IN_PROGRESS                       0x424ul
#define TR_DPTX_HDCP_R0_STATUS                              0x428ul
#define TR_DPTX_HDCP_CIPHER_CONTROL                         0x42cul
#define TR_DPTX_HDCP_BKSV_31_0                              0x430ul
#define TR_DPTX_HDCP_RRX_31_0                               0x430ul
#define TR_DPTX_HDCP_BKSV_63_32                             0x434ul
#define TR_DPTX_HDCP_RRX_63_32                              0x434ul
#define TR_DPTX_HDCP_AKSV_31_0                              0x438ul
#define TR_DPTX_HDCP_AKSV_63_32                             0x43cul
#define TR_DPTX_HDCP_LC128_31_0                             0x440ul
#define TR_DPTX_HDCP_LC128_63_32                            0x444ul
#define TR_DPTX_HDCP_LC128_95_64                            0x448ul
#define TR_DPTX_HDCP_LC128_127_96                           0x44cul
#define TR_DPTX_HDCP_REPEATER                               0x450ul
#define TR_DPTX_HDCP_STREAM_CIPHER_ENABLE                   0x454ul
#define TR_DPTX_HDCP_M0_31_0                                0x458ul
#define TR_DPTX_HDCP_M0_63_32                               0x45cul
#define TR_DPTX_HDCP_AES_INPUT_SELECT                       0x460ul
#define TR_DPTX_HDCP_AES_COUNTER_DISABLE                    0x464ul
#define TR_DPTX_HDCP_AES_COUNTER_ADVANCE                    0x468ul
#define TR_DPTX_HDCP_ECF_31_0                               0x46cul
#define TR_DPTX_HDCP_ECF_63_32                              0x470ul
#define TR_DPTX_HDCP_AES_COUNTER_RESET                      0x474ul
#define TR_DPTX_HDCP_RN_31_0                                0x478ul
#define TR_DPTX_HDCP_RN_63_32                               0x47cul
#define TR_DPTX_HDCP_RNG_CIPHER_STORE_AN                    0x480ul
#define TR_DPTX_HDCP_RNG_CIPHER_AN_31_0                     0x484ul
#define TR_DPTX_HDCP_RNG_CIPHER_AN_63_32                    0x488ul
#define TR_DPTX_HDCP_HOST_TIMER                             0x48cul
#define TR_DPTX_HDCP_ENCRYPTION_STATUS                      0x490ul
#define TR_DPTX_HDCP_RESERVED_494                           0x494ul
#define TR_DPTX_HDCP_CONTENT_TYPE_SELECT_31_0               0x498ul
#define TR_DPTX_HDCP_CONTENT_TYPE_SELECT_63_32              0x49cul

//------------------------------------------------------------------------------
//  This section is Trilinear display port I/O
//------------------------------------------------------------------------------
vaddr_t     dp_ctl_base_sel(uint32_t dp_port_index);
void        hdcp2_hw_tx_enable_write(uint32_t dp_port_index, bool enable);
uint32_t    hdcp2_hw_tx_enable_read(uint32_t dp_port_index);
void        hdcp2_hw_tx_mode_write(uint32_t dp_port_index, uint32_t val);
uint32_t    hdcp2_hw_tx_mode_read(uint32_t dp_port_index);
void        hdcp2_hw_tx_hardware_keys(uint32_t dp_port_index, bool enable);
bool        hdcp2_hw_tx_km_write(uint32_t dp_port_index, uint8_t* km);
bool        hdcp2_hw_tx_km_read(uint32_t dp_port_index, uint8_t* km);
bool        hdcp2_hw_tx_ks_write(uint32_t dp_port_index, uint8_t* ks);
bool        hdcp2_hw_tx_ks_read(uint32_t dp_port_index, uint8_t* ks);
bool        hdcp2_hw_tx_rtx_write(uint32_t dp_port_index, uint8_t* rtx);
bool        hdcp2_hw_tx_rtx_read(uint32_t dp_port_index, uint8_t* rtx);
bool        hdcp2_hw_tx_riv_write(uint32_t dp_port_index, uint8_t* rtx);
bool        hdcp2_hw_tx_riv_read(uint32_t dp_port_index, uint8_t* rtx);
bool        hdcp2_hw_tx_rrx_write(uint32_t dp_port_index, uint8_t* rrx);
bool        hdcp2_hw_tx_rrx_read(uint32_t dp_port_index, uint8_t* rrx);
bool        hdcp2_hw_tx_dkey_read(uint32_t dp_port_index, uint8_t* dkey);
bool        hdcp2_hw_tx_lc128_write(uint32_t dp_port_index, const uint8_t* lc128);
void        hdcp2_hw_tx_repeater_write(uint32_t dp_port_index, bool enable);
uint32_t    hdcp2_hw_tx_repeater_read(uint32_t dp_port_index, bool enable);
void        hdcp2_hw_tx_stream_cipher_write(uint32_t dp_port_index, bool enable);
uint32_t    hdcp2_hw_tx_stream_cipher_read(uint32_t dp_port_index);
void        hdcp2_hw_tx_aes_input_write(uint32_t dp_port_index, uint32_t input_id);
uint32_t    hdcp2_hw_tx_aes_input_read(uint32_t dp_port_index);
void        hdcp2_hw_tx_aes_ctr_disable_write(uint32_t dp_port_index, bool enable);
uint32_t    hdcp2_hw_tx_aes_ctr_disable_read(uint32_t dp_port_index);
void        hdcp2_hw_tx_aes_ctr_inc(uint32_t dp_port_index);
bool        hdcp2_hw_tx_encryption_ctl_write(uint32_t dp_port_index, uint8_t* ctl);
bool        hdcp2_hw_tx_encryption_ctl_read(uint32_t dp_port_index, uint8_t* ctl);
void        hdcp2_hw_tx_aes_ctr_reset(uint32_t dp_port_index);
bool        hdcp2_hw_tx_rn_clear(uint32_t dp_port_index);
bool        hdcp2_hw_tx_rn_write(uint32_t dp_port_index, uint8_t* ctl);
bool        hdcp2_hw_tx_rn_read(uint32_t dp_port_index, uint8_t* ctl);
void        hdcp2_hw_tx_scrambler_reset(uint32_t dp_port_index);
void        hdcp2_hw_tx_source0_enable(uint32_t dp_port_index);

#endif

