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

#include <drivers/cix_hdcp2_hw_tx.h>
#include <drivers/cix_hdcp2_key_drvn_tx.h>
#include <drivers/cix_hdcp2_tx_state.h>

//static const uint8_t test_lc128[] = { 0x93U, 0xceU, 0x5aU, 0x56U, 0xa0U, 0xa1U, 0xf4U, 0xf7U, 0x3cU, 0x65U, 0x8aU, 0x1bU, 0xd2U, 0xaeU, 0xf0U, 0xf7U };
static const uint8_t product_lc128[] = { 0xb5U, 0xd8U, 0xe9U, 0xabU, 0x5fU, 0x8aU, 0xfeU, 0xcaU, 0x38U, 0x55U, 0xb1U, 0xa5U, 0x1eU, 0xc9U, 0xbcU, 0x0fU };

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Local functions
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//  Function:   hdcp2_cipher_enable
//              Enable HDCP cipher once authentication is complete
//
//  Parameters:
//      cipher_data - hdcp2_cipher_data_t pointer
//
//  Returns:
//      None
//------------------------------------------------------------------------------
void hdcp2_cipher_enable(struct hdcp2_cipher_data_t* cipher_data, uint32_t dp_port_index)
{
	hdcp2_hw_tx_enable_write(dp_port_index, false);	//  disable HDCP transmit
	hdcp2_key_drvn_tx_ctr_reset(dp_port_index);	//  reset counter
	hdcp2_key_drvn_tx_src_select(aes_tx_sel_normal, dp_port_index);	//  set AES to stream cipher
	hdcp2_hw_tx_enable_write(dp_port_index, true);	//  enable HDCP transmit
	hdcp2_hw_tx_lc128_write(dp_port_index, product_lc128);	//  re-load the lc128 value
	hdcp2_hw_tx_riv_write(dp_port_index, cipher_data->riv);	//  write riv
	hdcp2_hw_tx_ks_write(dp_port_index, cipher_data->ks);	//  write ks
	hdcp2_hw_tx_stream_cipher_write(dp_port_index, true);	//  enable stream cipher
//	hdcp2_hw_tx_scrambler_reset();
//	hdcp2_hw_tx_source0_enable();
}
//------------------------------------------------------------------------------
//  Function:   hdcp2_cipher_disable
//              Disable HDCP cipher once authentication is complete
//
//  Parameters:
//
//  Returns:
//      None
//------------------------------------------------------------------------------
void hdcp2_cipher_disable(uint32_t dp_port_index)
{
	hdcp2_hw_tx_enable_write(dp_port_index, false);	//Disable HDCP to reset the internal state machines to the default state.
	hdcp2_hw_tx_mode_write(dp_port_index, 2);	//  Select HDCP 2.x mode
	hdcp2_hw_tx_repeater_write(dp_port_index, false); //  Enable (0x01) or disable (0x00) repeater mode as required
	hdcp2_hw_tx_stream_cipher_write(dp_port_index, false);	//  Disable the stream cipher
	hdcp2_hw_tx_aes_input_write(dp_port_index, 1);	//  AES input select set to cipher mode
	hdcp2_hw_tx_aes_ctr_reset(dp_port_index);	//  Issue a reset to the AES counter
	hdcp2_hw_tx_enable_write(dp_port_index, true);	//  Enable HDCP
}
//------------------------------------------------------------------------------
//  Function:   hdcp2_tx_state_init
//              Initialize HDCP transmit authentication state machine.
//
//  Parameters:
//
//  Returns:
//      none
//------------------------------------------------------------------------------
void hdcp2_tx_state_init(uint32_t dp_port_index)
{
	//  hardware init
	hdcp2_hw_tx_enable_write(dp_port_index, false);	//Disable HDCP to reset the internal state machines to the default state.
	hdcp2_hw_tx_mode_write(dp_port_index, 2);	//  Select HDCP 2.x mode
	hdcp2_hw_tx_repeater_write(dp_port_index, false);	//  Enable (0x01) or disable (0x00) repeater mode as required
	hdcp2_hw_tx_stream_cipher_write(dp_port_index, false);	//  Disable the stream cipher
	hdcp2_hw_tx_aes_input_write(dp_port_index, 1);	//  AES input select set to cipher mode
	hdcp2_hw_tx_aes_ctr_reset(dp_port_index);	//  Issue a reset to the AES counter
	hdcp2_hw_tx_enable_write(dp_port_index, true);	//  Enable HDCP
	hdcp2_hw_tx_lc128_write(dp_port_index, product_lc128);	//  Set the lc128 value
}
