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
#include <kernel/pseudo_ta.h>
#include <pta_cix_hdcp2.h>
#include <drivers/cix_hdcp2_tx_state.h>
#include <drivers/cix_hdcp2_key_drvn_tx.h>

#define TA_NAME		"tx_hdcp2.pta"

uint32_t dp_port_index;

static TEE_Result cix_hdcp2_hw_init(uint32_t type, TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
					  TEE_PARAM_TYPE_NONE,
					  TEE_PARAM_TYPE_NONE,
					  TEE_PARAM_TYPE_NONE);

	if (exp_pt != type) {
		DMSG("bad parameter types");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	hdcp2_tx_state_init(dp_port_index);

        DMSG("hdcp2 hw init ok");

	return TEE_SUCCESS;
}

static TEE_Result cix_hdcp2_cipher_enable(uint32_t type, TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	struct hdcp2_cipher_data_t cipher_data;
	uint32_t riv_length;
	uint8_t* riv;
	uint32_t ks_length;
        uint8_t* ks;
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
                                          TEE_PARAM_TYPE_MEMREF_INPUT,
                                          TEE_PARAM_TYPE_NONE,
                                          TEE_PARAM_TYPE_NONE);

        if (exp_pt != type) {
                DMSG("bad parameter types");
                return TEE_ERROR_BAD_PARAMETERS;
        }

	riv = params[0].memref.buffer;
	riv_length = params[0].memref.size;
	ks = params[1].memref.buffer;
	ks_length = params[1].memref.size;

	if (!riv || riv_length != RIV_SIZE) {
		DMSG("bad riv parameter");
                return TEE_ERROR_BAD_PARAMETERS;
	}

	if (!ks || ks_length != KS_SIZE) {
                DMSG("bad ks parameter");
                return TEE_ERROR_BAD_PARAMETERS;
        }

	memset(&cipher_data, 0, sizeof(cipher_data));
	memcpy(cipher_data.riv, riv, riv_length);
	memcpy(cipher_data.ks, ks, ks_length);

	hdcp2_cipher_enable(&cipher_data, dp_port_index);

	return TEE_SUCCESS;
}

static TEE_Result cix_hdcp2_cipher_disable(uint32_t type, TEE_Param params[TEE_NUM_PARAMS] __unused)
{
        uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
                                          TEE_PARAM_TYPE_NONE,
                                          TEE_PARAM_TYPE_NONE,
                                          TEE_PARAM_TYPE_NONE);

        if (exp_pt != type) {
                DMSG("bad parameter types");
                return TEE_ERROR_BAD_PARAMETERS;
        }

	hdcp2_cipher_disable(dp_port_index);

        return TEE_SUCCESS;
}

static TEE_Result cix_hdcp2_get_kd(uint32_t type, TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	struct hdcp2_kd_t kd_data;
	uint32_t rtx_length;
        uint8_t* rtx;
        uint32_t rrx_length;
        uint8_t* rrx;
	uint32_t km_length;
        uint8_t* km;
        uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
                                          TEE_PARAM_TYPE_MEMREF_INPUT,
                                          TEE_PARAM_TYPE_MEMREF_INPUT,
                                          TEE_PARAM_TYPE_MEMREF_OUTPUT);

        if (exp_pt != type) {
                DMSG("bad parameter types");
                return TEE_ERROR_BAD_PARAMETERS;
        }

        rtx = params[0].memref.buffer;
        rtx_length = params[0].memref.size;
        rrx = params[1].memref.buffer;
        rrx_length = params[1].memref.size;
	km = params[2].memref.buffer;
        km_length = params[2].memref.size;

        DMSG("rtx_length = %d\n", rtx_length);
        DMSG("rtx:\n");
        DHEXDUMP(rtx, rtx_length);

        DMSG("rrx_length = %d\n", rrx_length);
        DMSG("rrx:\n");
        DHEXDUMP(rrx, rrx_length);

        DMSG("km_length = %d\n", km_length);
        DMSG("km:\n");
        DHEXDUMP(km, km_length);

	if (!rtx || rtx_length != RTX_SIZE) {
                MSG("bad rtx parameter");
                return TEE_ERROR_BAD_PARAMETERS;
        }

	if (!rrx || rrx_length != RRX_SIZE) {
                MSG("bad rrx parameter");
                return TEE_ERROR_BAD_PARAMETERS;
        }

	if (!km || km_length != KM_SIZE) {
                MSG("bad km parameter");
                return TEE_ERROR_BAD_PARAMETERS;
        }

	memset(&kd_data, 0, sizeof(kd_data));
        memcpy(&kd_data.rtx, rtx, rtx_length);
        memcpy(&kd_data.rrx, rrx, rrx_length);
	memcpy(&kd_data.km, km, km_length);
        DMSG("kd_data.rtx:\n");
        DHEXDUMP(&kd_data.rtx, rtx_length);

        DMSG("kd_data.rrx:\n");
        DHEXDUMP(&kd_data.rrx, rrx_length);

        DMSG("kd_data.km:\n");
        DHEXDUMP(&kd_data.km, km_length);

	hdcp2_key_drvn_tx_kd_generate(&kd_data, dp_port_index);

        DMSG("kd_data.kd:\n");
        DHEXDUMP(&kd_data.kd, 32);
	memcpy(params[3].memref.buffer, &kd_data.kd, 32);
	params[3].memref.size = 32;

        return TEE_SUCCESS;
}

static TEE_Result cix_hdcp2_get_dkey2(uint32_t type, TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	struct hdcp2_dkey2_t dkey2_data;
        uint32_t rn_length;
        uint8_t* rn;
        uint32_t km_length;
        uint8_t* km;
        uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
                                          TEE_PARAM_TYPE_MEMREF_INPUT,
                                          TEE_PARAM_TYPE_MEMREF_OUTPUT,
                                          TEE_PARAM_TYPE_NONE);

        if (exp_pt != type) {
                DMSG("bad parameter types");
                return TEE_ERROR_BAD_PARAMETERS;
        }

        rn = params[0].memref.buffer;
        rn_length = params[0].memref.size;
        km = params[1].memref.buffer;
        km_length = params[1].memref.size;

        if (!rn || rn_length != RN_SIZE) {
                MSG("bad rn parameter");
                return TEE_ERROR_BAD_PARAMETERS;
        }

        if (!km || km_length != KM_SIZE) {
                MSG("bad km parameter");
                return TEE_ERROR_BAD_PARAMETERS;
        }

        memset(&dkey2_data, 0, sizeof(dkey2_data));
        memcpy(dkey2_data.rn, rn, rn_length);
        memcpy(dkey2_data.km, km, km_length);

	hdcp2_key_drvn_tx_dkey2_calc(&dkey2_data, dp_port_index);

	memcpy(params[2].memref.buffer, dkey2_data.dkey2, 16);
        params[3].memref.size = 16;

        return TEE_SUCCESS;
}

/*
 * Trusted Application Entry Points
 */

static TEE_Result create_ta(void)
{
	DMSG("create entry point for pseudo TA \"%s\"", TA_NAME);
	return TEE_SUCCESS;
}

static void destroy_ta(void)
{
	DMSG("destroy entry point for pseudo ta \"%s\"", TA_NAME);
}

static TEE_Result open_session(uint32_t nParamTypes __unused,
		TEE_Param pParams[TEE_NUM_PARAMS] __unused,
		void **ppSessionContext __unused)
{
	DMSG("open entry point for pseudo ta \"%s\"", TA_NAME);

	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
                                          TEE_PARAM_TYPE_NONE,
                                          TEE_PARAM_TYPE_NONE,
                                          TEE_PARAM_TYPE_NONE);

        if (exp_pt != nParamTypes) {
                DMSG("bad parameter types");
                return TEE_ERROR_BAD_PARAMETERS;
        }

        dp_port_index = pParams[0].value.a;
	DMSG("open session for dp_port_index = %d", dp_port_index);

	return TEE_SUCCESS;
}

static void close_session(void *pSessionContext __unused)
{
	DMSG("close entry point for pseudo ta \"%s\"", TA_NAME);
}

static TEE_Result invoke_command(void *pSessionContext __unused,
		uint32_t nCommandID, uint32_t nParamTypes,
		TEE_Param pParams[TEE_NUM_PARAMS])
{
	DMSG("command entry point for pseudo ta \"%s\"", TA_NAME);

	switch (nCommandID) {
	case PTA_DP_HDCP2_CMD_HW_INIT:
		return cix_hdcp2_hw_init(nParamTypes, pParams);
	case PTA_DP_HDCP2_CMD_CIPHER_EN:
                return cix_hdcp2_cipher_enable(nParamTypes, pParams);
	case PTA_DP_HDCP2_CMD_CIPHER_DIS:
                return cix_hdcp2_cipher_disable(nParamTypes, pParams);
	case PTA_DP_HDCP2_CMD_GET_KD:
                return cix_hdcp2_get_kd(nParamTypes, pParams);
	case PTA_DP_HDCP2_CMD_GET_DKEY2:
                return cix_hdcp2_get_dkey2(nParamTypes, pParams);
	default:
		break;
	}
	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = PTA_CIX_HDCP2_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .create_entry_point = create_ta,
		   .destroy_entry_point = destroy_ta,
		   .open_session_entry_point = open_session,
		   .close_session_entry_point = close_session,
		   .invoke_command_entry_point = invoke_command);
