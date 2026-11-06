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

#include <assert.h>
#include <crypto/crypto.h>
#include <crypto/crypto_impl.h>
#if defined(CFG_CRYPTO_AES)
#include <mbedtls/aes.h>
#endif
#if defined(CFG_CRYPTO_SM4)
#include <mbedtls/sm4.h>
#endif
#include <mbedtls/cipher.h>
#include <stdlib.h>
#include <string.h>
#include <string_ext.h>
#include <tee_api_types.h>
#include <utee_defines.h>
#include <util.h>

struct mbed_xts_ctx {
	struct crypto_cipher_ctx ctx;
	mbedtls_cipher_id_t cipher;	/* AES | SM4 */
	union {
#if defined(CFG_CRYPTO_AES)
		mbedtls_aes_xts_context aes;
#endif /* CFG_CRYPTO_AES */
#if defined(CFG_CRYPTO_SM4)
		mbedtls_sm4_xts_context sm4;
#endif /* CFG_CRYPTO_SM4 */
	} u;
	TEE_Result (*to_tee_res)(int err);
	TEE_OperationMode mode;
	uint8_t tweak[16];
};

static const struct crypto_cipher_ops mbed_xts_ops;

static struct mbed_xts_ctx *to_xts_ctx(struct crypto_cipher_ctx *ctx)
{
	assert(ctx && ctx->ops == &mbed_xts_ops);

	return container_of(ctx, struct mbed_xts_ctx, ctx);
}

#if defined(CFG_CRYPTO_AES)
static TEE_Result aes_to_tee_res(int err)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	switch(err) {
	case 0:
		res = TEE_SUCCESS;
		break;
	case MBEDTLS_ERR_AES_ALLOC_FAILED:
		res = TEE_ERROR_OUT_OF_MEMORY;
		break;
	case MBEDTLS_ERR_AES_BAD_INPUT_DATA:
	case MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH:
	case MBEDTLS_ERR_AES_INVALID_KEY_LENGTH:
		res = TEE_ERROR_BAD_PARAMETERS;
		break;
	default:
		break;
	}

	return res;
}
#endif /* CFG_CRYPTO_AES */

#if defined(CFG_CRYPTO_SM4)
static TEE_Result sm4_to_tee_res(int err)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	switch(err) {
	case 0:
		res = TEE_SUCCESS;
		break;
	case MBEDTLS_ERR_SM4_ALLOC_FAILED:
		res = TEE_ERROR_OUT_OF_MEMORY;
		break;
	case MBEDTLS_ERR_SM4_BAD_INPUT_DATA:
	case MBEDTLS_ERR_SM4_INVALID_INPUT_LENGTH:
	case MBEDTLS_ERR_SM4_INVALID_KEY_LENGTH:
		res = TEE_ERROR_BAD_PARAMETERS;
		break;
	default:
		break;
	}

	return res;
}
#endif /* CFG_CRYPTO_SM4 */

static TEE_Result mbed_xts_init(struct crypto_cipher_ctx *ctx,
			       TEE_OperationMode mode,
			       const uint8_t *key1, size_t key1_len,
			       const uint8_t *key2, size_t key2_len,
			       const uint8_t *iv, size_t iv_len)
{
	int err = 0;
	TEE_Result res = TEE_SUCCESS;
	uint8_t *key = NULL;
	struct mbed_xts_ctx *c = to_xts_ctx(ctx);

	if (mode != TEE_MODE_ENCRYPT && mode != TEE_MODE_DECRYPT) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (key1_len != key2_len)
		return TEE_ERROR_BAD_PARAMETERS;
	if (iv) {
		if (iv_len != sizeof(c->tweak))
			return TEE_ERROR_BAD_PARAMETERS;
		memcpy(c->tweak, iv, sizeof(c->tweak));
	} else {
		memset(c->tweak, 0, sizeof(c->tweak));
	}

	if ((int)iv_len != TEE_AES_BLOCK_SIZE) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	key = calloc(1, 2 * key1_len);
	if (NULL == key) {
		return TEE_ERROR_OUT_OF_MEMORY;
	}
	memcpy(key, key1, key1_len);
	memcpy(key + key1_len, key2, key2_len);

#if defined(CFG_CRYPTO_AES)
	if (MBEDTLS_CIPHER_ID_AES == c->cipher) {
		if (TEE_MODE_ENCRYPT == mode) {
			err = mbedtls_aes_xts_setkey_enc(&c->u.aes, key,
							 key1_len << 4);
		} else {
			err = mbedtls_aes_xts_setkey_dec(&c->u.aes, key,
							 key1_len << 4);
		}
	} else
#endif
#if defined(CFG_CRYPTO_SM4)
	if (MBEDTLS_CIPHER_ID_SM4 == c->cipher) {
		if (TEE_MODE_ENCRYPT == mode) {
			err = mbedtls_sm4_xts_setkey_enc(&c->u.sm4, key,
							 key1_len << 4);
		} else {
			err = mbedtls_sm4_xts_setkey_dec(&c->u.sm4, key,
							 key1_len << 4);
		}
	} else
#endif
	{
		res = TEE_ERROR_NOT_SUPPORTED;
		goto out;
	}

	res = c->to_tee_res(err);
	if (err != 0) {
		goto out;
	}

	c->mode = mode;

out:
	memzero_explicit(key, key1_len << 1);
	free(key);
	return res;
}

#if defined(CFG_MBEDTLS_TE)
#include <mbed_impl.h>

#if defined(CFG_CRYPTO_AES)
DEFINE_TO_MBED_SEC_KEY(aes, AES)
#endif
#if defined(CFG_CRYPTO_SM4)
DEFINE_TO_MBED_SEC_KEY(sm4, SM4)
#endif
static TEE_Result mbed_xts_init2(struct crypto_cipher_ctx *ctx,
			         TEE_OperationMode mode,
			         const crypto_sec_key_t *key1,
			         const crypto_sec_key_t *key2,
			         const uint8_t *iv, size_t iv_len)
{
	int err = 0;
	TEE_Result res = TEE_SUCCESS;
	struct mbed_xts_ctx *c = to_xts_ctx(ctx);

	if (mode != TEE_MODE_ENCRYPT && mode != TEE_MODE_DECRYPT) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (key1->ek3len != key2->ek3len)
		return TEE_ERROR_BAD_PARAMETERS;
	if (iv) {
		if (iv_len != sizeof(c->tweak))
			return TEE_ERROR_BAD_PARAMETERS;
		memcpy(c->tweak, iv, sizeof(c->tweak));
	} else {
		memset(c->tweak, 0, sizeof(c->tweak));
	}

	if ((int)iv_len != TEE_AES_BLOCK_SIZE) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

#if defined(CFG_CRYPTO_AES)
	if (MBEDTLS_CIPHER_ID_AES == c->cipher) {
		mbedtls_aes_sec_key_t skey1 = {0};
		mbedtls_aes_sec_key_t skey2 = {0};
		TO_MBED_SEC_KEY(aes, key1, &skey1);
		TO_MBED_SEC_KEY(aes, key2, &skey2);
		if (TEE_MODE_ENCRYPT == mode) {
			err = mbedtls_aes_xts_setseckey_enc(&c->u.aes, &skey1,
							    &skey2);
		} else {
			err = mbedtls_aes_xts_setseckey_dec(&c->u.aes, &skey1,
							    &skey2);
		}
	} else
#endif
#if defined(CFG_CRYPTO_SM4)
	if (MBEDTLS_CIPHER_ID_SM4 == c->cipher) {
		mbedtls_sm4_sec_key_t skey1 = {0};
		mbedtls_sm4_sec_key_t skey2 = {0};
		TO_MBED_SEC_KEY(sm4, key1, &skey1);
		TO_MBED_SEC_KEY(sm4, key2, &skey2);
		if (TEE_MODE_ENCRYPT == mode) {
			err = mbedtls_sm4_xts_setseckey_enc(&c->u.sm4, &skey1,
							    &skey2);
		} else {
			err = mbedtls_sm4_xts_setseckey_dec(&c->u.sm4, &skey1,
							    &skey2);
		}
	} else
#endif
	{
		res = TEE_ERROR_NOT_SUPPORTED;
		goto out;
	}

	res = c->to_tee_res(err);
	if (err != 0) {
		goto out;
	}

	c->mode = mode;

out:
	return res;
}
#endif /* CFG_MBEDTLS_TE */

static TEE_Result mbed_xts_update(struct crypto_cipher_ctx *ctx,
				 bool last_block __unused,
				 const uint8_t *data, size_t len, uint8_t *dst)
{
	int err = 0;
	struct mbed_xts_ctx *c = to_xts_ctx(ctx);

#if defined(CFG_CRYPTO_AES)
	if (MBEDTLS_CIPHER_ID_AES == c->cipher) {
		err = mbedtls_aes_crypt_xts(&c->u.aes,
					    (TEE_MODE_ENCRYPT == c->mode) ?
					    MBEDTLS_AES_ENCRYPT :
					    MBEDTLS_AES_DECRYPT,
					    len, c->tweak, data, dst);
	} else
#endif
#if defined(CFG_CRYPTO_SM4)
	if (MBEDTLS_CIPHER_ID_SM4 == c->cipher) {
		err = mbedtls_sm4_crypt_xts(&c->u.sm4,
					    (TEE_MODE_ENCRYPT == c->mode) ?
					    MBEDTLS_SM4_ENCRYPT :
					    MBEDTLS_SM4_DECRYPT,
					    len, c->tweak, data, dst);
	} else
#endif
	{
		assert(0);
	}

	return c->to_tee_res(err);
}

static void mbed_xts_final(struct crypto_cipher_ctx *ctx)
{
	(void)ctx;
}

static void mbed_xts_free_ctx(struct crypto_cipher_ctx *ctx)
{
	struct mbed_xts_ctx *c = to_xts_ctx(ctx);
#if defined (CFG_CRYPTO_AES)
	if (MBEDTLS_CIPHER_ID_AES == c->cipher) {
		mbedtls_aes_xts_free(&c->u.aes);
	} else
#endif
#if defined (CFG_CRYPTO_SM4)
	if (MBEDTLS_CIPHER_ID_SM4 == c->cipher) {
		mbedtls_sm4_xts_free(&c->u.sm4);
	} else
#endif
	{
		assert(0);
	}

	free(c);
}

static void mbed_xts_copy_state(struct crypto_cipher_ctx *dst_ctx,
			       struct crypto_cipher_ctx *src_ctx)
{
	struct mbed_xts_ctx *src = to_xts_ctx(src_ctx);
	struct mbed_xts_ctx *dst = to_xts_ctx(dst_ctx);

	assert(dst->cipher == src->cipher);
#if defined(CFG_CRYPTO_AES)
	if (MBEDTLS_CIPHER_ID_AES == src->cipher) {
		assert(mbedtls_aes_xts_clone(&dst->u.aes, &src->u.aes) == 0);
	} else
#endif
#if defined(CFG_CRYPTO_SM4)
	if (MBEDTLS_CIPHER_ID_SM4 == src->cipher) {
		assert(mbedtls_sm4_xts_clone(&dst->u.sm4, &src->u.sm4) == 0);
	} else
#endif
	{
		dst->u = src->u;
	}
	dst->mode = src->mode;
	memcpy(dst->tweak, src->tweak, sizeof(src->tweak));
}

static const struct crypto_cipher_ops mbed_xts_ops = {
	.init = mbed_xts_init,
	.update = mbed_xts_update,
	.final = mbed_xts_final,
	.free_ctx = mbed_xts_free_ctx,
	.copy_state = mbed_xts_copy_state,
#if defined(CFG_MBEDTLS_TE)
	.init2 = mbed_xts_init2,
#endif
};

static TEE_Result crypto_mbed_xts_alloc_ctx(struct crypto_cipher_ctx **ctx_ret,
					    mbedtls_cipher_id_t cipher)
{
	struct mbed_xts_ctx *c = NULL;
	TEE_Result (*to_tee_res)(int err) = NULL;

	c = calloc(1, sizeof(*c));
	if (!c)
		return TEE_ERROR_OUT_OF_MEMORY;

#if defined(CFG_CRYPTO_AES)
	if (MBEDTLS_CIPHER_ID_AES == cipher) {
		mbedtls_aes_xts_init(&c->u.aes);
		to_tee_res = aes_to_tee_res;
	} else
#endif
#if defined(CFG_CRYPTO_SM4)
	if (MBEDTLS_CIPHER_ID_SM4 == cipher) {
		mbedtls_sm4_xts_init(&c->u.sm4);
		to_tee_res = sm4_to_tee_res;
	} else
#endif
	{
		free(c);
		c = NULL;
		return TEE_ERROR_NOT_SUPPORTED;
	}

	c->ctx.ops = &mbed_xts_ops;
	c->cipher = cipher;
	c->to_tee_res = to_tee_res;
	*ctx_ret = &c->ctx;

	return TEE_SUCCESS;
}

#if defined(CFG_CRYPTO_AES)
TEE_Result crypto_aes_xts_alloc_ctx(struct crypto_cipher_ctx **ctx_ret)
{
	return crypto_mbed_xts_alloc_ctx(ctx_ret, MBEDTLS_CIPHER_ID_AES);
}
#endif

#if defined(CFG_CRYPTO_SM4)
TEE_Result crypto_sm4_xts_alloc_ctx(struct crypto_cipher_ctx **ctx_ret)
{
	return crypto_mbed_xts_alloc_ctx(ctx_ret, MBEDTLS_CIPHER_ID_SM4);
}
#endif
