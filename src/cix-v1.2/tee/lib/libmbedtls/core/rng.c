/*
 * Copyright (c) 2020, Arm Technology (China) Co., Ltd.
 * All rights reserved.
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#include <compiler.h>
#include <assert.h>
#include <crypto/crypto.h>
#include <rng_support.h>
#include <tee/tee_cryp_utl.h>
#include <types_ext.h>
#include <mbedtls/entropy_poll.h>

TEE_Result crypto_rng_init(const void *data __unused,
			   size_t dlen __unused)
{
	return TEE_SUCCESS;
}

void crypto_rng_add_event(enum crypto_rng_src sid __unused,
			  unsigned int *pnum __unused,
			  const void *data __unused,
			  size_t dlen __unused)
{
}

TEE_Result crypto_rng_read(void *buf, size_t blen)
{
	size_t n __unused;

	if (!buf)
		return TEE_ERROR_BAD_PARAMETERS;

	if (mbedtls_hardware_poll(NULL, (unsigned char*)buf, blen, &n))
		return TEE_ERROR_GENERIC;
	else
		return TEE_SUCCESS;
}

uint8_t hw_get_random_byte(void)
{
	uint8_t byte = 0;
	size_t n __unused;
	assert(!mbedtls_hardware_poll(NULL, &byte, 1, &n));
	return byte;
}
