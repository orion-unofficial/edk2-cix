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

#ifndef HDCP2_KEY_DRVN_TX__
#define HDCP2_KEY_DRVN_TX__

#define RIV_SIZE    8
#define KS_SIZE     16
#define RTX_SIZE    8
#define RRX_SIZE    8
#define KM_SIZE     16
#define RN_SIZE     8
#define RIV_SIZE    8

struct hdcp2_kd_t {
	unsigned char rtx[8]; // random TX value
	unsigned char rrx[8]; //  random RX value
	unsigned char km[16]; //  device master key km (128-bits)
	unsigned char dkey0[16]; // dkey0 is used to build kd
	unsigned char dkey1[16]; // dkey1 is used to build kd
	unsigned char kd[32];			//  kd is used to calculate H
};

struct hdcp2_dkey2_t {
	unsigned char rn[8];
	unsigned char km[16]; //  device master key km (128-bits)
	unsigned char dkey2[16];
};

struct hdcp2_cipher_data_t {
	unsigned char riv[8];
	unsigned char ks[16];
};

//------------------------------------------------------------------------------
//  Module Typedefs
//------------------------------------------------------------------------------
typedef enum {
	aes_tx_sel_disabled,
	aes_tx_sel_normal,
	aes_tx_sel_key_derivation,
	aes_tx_sel_ekh,
} aes_tx_select_t;

//------------------------------------------------------------------------------
//  Functional Interface
//------------------------------------------------------------------------------
bool hdcp2_key_drvn_tx_init(void *kdata, uint32_t dp_port_index);
bool hdcp2_key_drvn_tx_reset(uint32_t dp_port_index);
bool hdcp2_key_drvn_tx_src_select(aes_tx_select_t sel, uint32_t dp_port_index);
bool hdcp2_key_drvn_tx_load(void *kdata, uint32_t dp_port_index);
bool hdcp2_key_drvn_tx_ctr_reset(uint32_t dp_port_index);
bool hdcp2_key_drvn_tx_ctr_inc(uint32_t dp_port_index);
bool hdcp2_key_drvn_tx_kd_calc(void *kdata, uint32_t dp_port_index);
bool hdcp2_key_drvn_tx_kd_generate(struct hdcp2_kd_t* kd_data, uint32_t dp_port_index);
bool hdcp2_key_drvn_tx_dkey2_calc(struct hdcp2_dkey2_t* dkey2_data, uint32_t dp_port_index);

#endif  // HDCP2_KEY_DRVN_TX__
