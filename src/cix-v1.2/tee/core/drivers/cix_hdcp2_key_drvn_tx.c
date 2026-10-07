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

#include <string.h>
#include <drivers/cix_hdcp2_hw_tx.h>
#include <drivers/cix_hdcp2_key_drvn_tx.h>
#include <trace.h>

#define HDCP2_DKEY_SZ           16
#define HDCP2_KD_SZ             32
#define HDCP2_H_SZ              32
#define HDCP2_H_DATA_SZ         14    //  H-data is (rtx || RxCaps || TxCaps) this is 14-bytes
#define HDCP2_RTX_SZ            8
#define HDCP2_RN_SZ             8
#define HDCP2_TX_CAPS_SZ        3
#define HDCP2_RX_CAPS_SZ        3

//------------------------------------------------------------------------------
//  Function:   hdcp2_key_drvn_tx_init
//              Initialize key derivation module
//
//  Parameters:
//      kdata - hdcp2_kd_t ptr
//
//  Returns:
//      true=OK / false=fail
//------------------------------------------------------------------------------
bool hdcp2_key_drvn_tx_init(void *kdata, uint32_t dp_port_index __unused)
{
	struct hdcp2_kd_t *tx_data = kdata;
	memset(tx_data->dkey0,  0, HDCP2_DKEY_SZ);
	memset(tx_data->dkey1,  0, HDCP2_DKEY_SZ);
	memset(tx_data->kd,  0, HDCP2_KD_SZ);
	return true;
}
//------------------------------------------------------------------------------
//  Function:   hdcp2_key_drvn_tx_reset
//              Reset key derivation hardware
//
//  Parameters:
//
//  Returns:
//      true=OK / false=fail
//------------------------------------------------------------------------------
bool hdcp2_key_drvn_tx_reset(uint32_t dp_port_index)
{
	hdcp2_hw_tx_stream_cipher_write(dp_port_index, false);
	hdcp2_hw_tx_hardware_keys(dp_port_index, false);
	hdcp2_hw_tx_aes_ctr_reset(dp_port_index);
	return true;
}
//------------------------------------------------------------------------------
//  Function:   hdcp2_key_drvn_tx_src_select
//              Select source for key derivation hardware
//
//  Parameters:
//      sel - clock input source
//
//  Returns:
//      true=OK / false=fail
//------------------------------------------------------------------------------
bool hdcp2_key_drvn_tx_src_select(aes_tx_select_t sel, uint32_t dp_port_index)
{
	switch (sel) {
	case aes_tx_sel_disabled:
		hdcp2_hw_tx_aes_input_write(dp_port_index, 0);
		break;
	case aes_tx_sel_normal:
		hdcp2_hw_tx_aes_input_write(dp_port_index, 1);
		break;
	case aes_tx_sel_key_derivation:
		hdcp2_hw_tx_aes_input_write(dp_port_index, 2);
		break;
	case aes_tx_sel_ekh:
		hdcp2_hw_tx_aes_input_write(dp_port_index, 4);
		break;
	default:
		break;
	}
	return true;
}
//------------------------------------------------------------------------------
//  Function:   hdcp2_key_drvn_tx_load
//              load rrx, rtx, and rn
//
//  Parameters:
//      kdata - hdcp2_kd_t ptr
//
//  Returns:
//      true=OK / false=fail
//------------------------------------------------------------------------------
bool hdcp2_key_drvn_tx_load(void *kdata, uint32_t dp_port_index)
{
	struct hdcp2_kd_t *tx_data = kdata;
#if 0
    for (int i = 0; i < 8; i++) {
        printk(KERN_ERR"%s, rtx[%d]=%x\n", __func__, i, tx_data->rtx[i]);
    }
    for (int i = 0; i < 8; i++) {
        printk(KERN_ERR"%s, rrx[%d]=%x\n", __func__, i, tx_data->rrx[i]);
    }
    for (int i = 0; i < 16; i++) {
        printk(KERN_ERR"%s, km[%d]=%x\n", __func__, i, tx_data->km[i]);
    }
#endif
	hdcp2_hw_tx_rrx_write(dp_port_index, tx_data->rrx);
	hdcp2_hw_tx_rtx_write(dp_port_index, tx_data->rtx);
	hdcp2_hw_tx_rn_clear(dp_port_index);
	return true;
}
//------------------------------------------------------------------------------
//  Function:   hdcp2_key_drvn_tx_ctr_reset
//              Reset derivation counter
//
//  Parameters:
//
//  Returns:
//      true=OK / false=fail
//------------------------------------------------------------------------------
bool hdcp2_key_drvn_tx_ctr_reset(uint32_t dp_port_index)
{
	hdcp2_hw_tx_aes_ctr_reset(dp_port_index);
	return true;
}
//------------------------------------------------------------------------------
//  Function:   hdcp2_key_drvn_tx_ctr_inc
//              Increment key derivation counter
//
//  Parameters:
//
//  Returns:
//      true=OK / false=fail
//------------------------------------------------------------------------------
bool hdcp2_key_drvn_tx_ctr_inc(uint32_t dp_port_index)
{
	hdcp2_hw_tx_aes_ctr_inc(dp_port_index);
	return true;
}
//------------------------------------------------------------------------------
//  Function:   hdcp2_key_drvn_tx_kd_calc
//              generate dkey0
//              generate dkey1
//              populate kd with dkey0 || dkey1
//
//  Parameters:
//      kdata - hdcp2_kd_t ptr
//
//  Returns:
//      true=OK / false=fail
//------------------------------------------------------------------------------
bool hdcp2_key_drvn_tx_kd_calc(void *kdata, uint32_t dp_port_index)
{
	struct hdcp2_kd_t *tx_data = kdata;

	//  generate dkey0
	hdcp2_hw_tx_km_write(dp_port_index, tx_data->km);    //  write km  (triggers AES execution)
	hdcp2_hw_tx_dkey_read(dp_port_index, tx_data->dkey0);
	//  generate dkey1
	hdcp2_key_drvn_tx_ctr_inc(dp_port_index);    //  increment aes counter
	hdcp2_hw_tx_km_write(dp_port_index, tx_data->km);    //  write km  (triggers AES execution)
	hdcp2_hw_tx_dkey_read(dp_port_index, tx_data->dkey1);
	//  assemble kd
//	memset(tx_data->kd, 0, HDCP2_KD_SZ);    //  clear kd
	memcpy(tx_data->kd,  tx_data->dkey0, HDCP2_DKEY_SZ);
	memcpy(tx_data->kd + 16, tx_data->dkey1, HDCP2_DKEY_SZ);
	return true;
}
bool hdcp2_key_drvn_tx_kd_generate(struct hdcp2_kd_t* kd_data, uint32_t dp_port_index)
{
	//------------------------------------------------------------------------------
	//  Perform key derivation to generate 256bit kd.
	//------------------------------------------------------------------------------
//	hdcp2_key_drvn_tx_init(kd_data);    //  initialize key derivation unit
	hdcp2_key_drvn_tx_reset(dp_port_index);    //  reset the the engine
	hdcp2_key_drvn_tx_src_select(aes_tx_sel_key_derivation, dp_port_index);    //  select source for key derivation
	hdcp2_key_drvn_tx_load(kd_data, dp_port_index);    //  load up values required for kd calculation
	hdcp2_key_drvn_tx_kd_calc(kd_data, dp_port_index);    //  generate kd
	return true;
}
//------------------------------------------------------------------------------
//  Function:   hdcp2_key_drvn_tx_dkey2_calc
//
//  This function generates dkey2.
//  When this function is called, rn should be a random number.
//  When dkey0 and dkey1 are calculated, rn is initialized to zero.
//
//  Parameters:
//      dkey2_data - hdcp2_dkey2_t ptr
//
//  Returns:
//      true=OK / false=fail
//------------------------------------------------------------------------------
bool hdcp2_key_drvn_tx_dkey2_calc(struct hdcp2_dkey2_t* dkey2_data, uint32_t dp_port_index)
{
	struct hdcp2_dkey2_t *tx_data = dkey2_data;
	//  update rn, increment aes counter
	hdcp2_hw_tx_rn_write(dp_port_index, tx_data->rn);    //  write rn (currently random)
	hdcp2_key_drvn_tx_ctr_inc(dp_port_index);    //  bump the counter by one
	//  generate dkey2
	hdcp2_hw_tx_km_write(dp_port_index, tx_data->km);    //  write km  (triggers AES execution)
	hdcp2_hw_tx_dkey_read(dp_port_index, tx_data->dkey2); //  copy hardware registers into HDCP 2.x transmitter data structure
	return true;
}

