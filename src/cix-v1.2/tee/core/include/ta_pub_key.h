/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2015, Linaro Limited
 */
#ifndef KERNEL_TA_PUB_KEY_H
#define KERNEL_TA_PUB_KEY_H

#include <types_ext.h>

typedef struct {
	uint32_t key_exponent;
	size_t module_size;
	uint8_t key_module[512];
}ta_pub_key_info;

extern uint32_t ta_pub_key_exponent;
extern uint8_t ta_pub_key_modulus[];
extern size_t ta_pub_key_modulus_size;

#endif /*KERNEL_TA_PUB_KEY_H*/

