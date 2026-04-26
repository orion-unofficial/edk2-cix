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

#ifndef __MBED_IMPL_H__
#define __MBED_IMPL_H__

#include <crypto/crypto.h>
#include <crypto/crypto_impl.h>

#ifndef ASM

#define DEFINE_TO_MBED_SEC_KEY(nm, NM)                         \
static void to_##nm##_sec_key(const crypto_sec_key_t *key,     \
			      mbedtls_##nm##_sec_key_t *mbkey) \
{                                                              \
	memset(mbkey, 0, sizeof(*mbkey));                      \
	mbkey->sel = (CRYPTO_KLAD_MODEL_KEY == key->sel) ?     \
		     MBEDTLS_##NM##_KL_KEY_MODEL :             \
		     MBEDTLS_##NM##_KL_KEY_ROOT;               \
	mbkey->ek3bits = key->ek3len << 3;                     \
	memcpy(mbkey->ek1, key->ek1, sizeof(mbkey->ek1));      \
	memcpy(mbkey->ek2, key->ek2, sizeof(mbkey->ek2));      \
	memcpy(mbkey->ek3, key->ek3, sizeof(mbkey->ek3));      \
}

/* convert crypto_sec_key_t to mbedtls_xxx_sec_key_t */
#define TO_MBED_SEC_KEY(nm, csk, msk) to_##nm##_sec_key(csk, msk)

#endif /* !ASM */
#endif /* __MBED_IMPL_H__ */
