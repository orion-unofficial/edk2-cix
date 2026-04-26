// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2019, Linaro Limited
 */

#include <assert.h>
#include <compiler.h>
#include <crypto/crypto.h>
#include <crypto/crypto_impl.h>
#include <mbedtls/sm4.h>
#include <stdlib.h>
#include <string.h>
#include <tee_api_types.h>
#include <utee_defines.h>
#include <util.h>

struct mbed_sm4_cbc_ctx {
	struct crypto_cipher_ctx ctx;
	int mbed_mode;
	mbedtls_sm4_context sm4_ctx;
	unsigned char iv[TEE_SM4_BLOCK_SIZE];
};

static const struct crypto_cipher_ops mbed_sm4_cbc_ops;

static struct mbed_sm4_cbc_ctx *to_sm4_cbc_ctx(struct crypto_cipher_ctx *ctx)
{
	assert(ctx && ctx->ops == &mbed_sm4_cbc_ops);

	return container_of(ctx, struct mbed_sm4_cbc_ctx, ctx);
}

static TEE_Result mbed_sm4_cbc_init(struct crypto_cipher_ctx *ctx,
				    TEE_OperationMode mode, const uint8_t *key1,
				    size_t key1_len,
				    const uint8_t *key2 __unused,
				    size_t key2_len __unused,
				    const uint8_t *iv, size_t iv_len)
{
	struct mbed_sm4_cbc_ctx *c = to_sm4_cbc_ctx(ctx);
	int mbed_res = 0;

	if (iv_len != sizeof(c->iv))
		return TEE_ERROR_BAD_PARAMETERS;
	memcpy(c->iv, iv, sizeof(c->iv));

	if (mode == TEE_MODE_ENCRYPT) {
		c->mbed_mode = MBEDTLS_SM4_ENCRYPT;
		mbed_res = mbedtls_sm4_setkey_enc(&c->sm4_ctx, key1,
						  key1_len * 8);
	} else {
		c->mbed_mode = MBEDTLS_SM4_DECRYPT;
		mbed_res = mbedtls_sm4_setkey_dec(&c->sm4_ctx, key1,
						  key1_len * 8);
	}

	if (mbed_res)
		return TEE_ERROR_BAD_STATE;

	return TEE_SUCCESS;
}

#if defined(CFG_MBEDTLS_TE)
#include <mbed_impl.h>

DEFINE_TO_MBED_SEC_KEY(sm4, SM4)

static TEE_Result mbed_sm4_cbc_init2(struct crypto_cipher_ctx *ctx,
				     TEE_OperationMode mode,
				     const crypto_sec_key_t *key1,
				     const crypto_sec_key_t *key2 __unused,
				     const uint8_t *iv, size_t iv_len)
{
	struct mbed_sm4_cbc_ctx *c = to_sm4_cbc_ctx(ctx);
	int mbed_res = 0;
	mbedtls_sm4_sec_key_t skey = {0};

	if (iv_len != sizeof(c->iv))
		return TEE_ERROR_BAD_PARAMETERS;
	memcpy(c->iv, iv, sizeof(c->iv));

	TO_MBED_SEC_KEY(sm4, key1, &skey);

	if (mode == TEE_MODE_ENCRYPT) {
		c->mbed_mode = MBEDTLS_SM4_ENCRYPT;
		mbed_res = mbedtls_sm4_setseckey_enc(&c->sm4_ctx, &skey);
	} else {
		c->mbed_mode = MBEDTLS_SM4_DECRYPT;
		mbed_res = mbedtls_sm4_setseckey_dec(&c->sm4_ctx, &skey);
	}

	if (mbed_res)
		return TEE_ERROR_BAD_STATE;

	return TEE_SUCCESS;
}
#endif /* CFG_MBEDTLS_TE */

static TEE_Result mbed_sm4_cbc_update(struct crypto_cipher_ctx *ctx,
				      bool last_block __unused,
				      const uint8_t *data, size_t len,
				      uint8_t *dst)
{
	struct mbed_sm4_cbc_ctx *c = to_sm4_cbc_ctx(ctx);

	if (mbedtls_sm4_crypt_cbc(&c->sm4_ctx, c->mbed_mode, len, c->iv,
				  data, dst))
		return TEE_ERROR_BAD_STATE;

	return TEE_SUCCESS;
}

static void mbed_sm4_cbc_final(struct crypto_cipher_ctx *ctx)
{
	(void)ctx;
}

static void mbed_sm4_cbc_free_ctx(struct crypto_cipher_ctx *ctx)
{
	struct mbed_sm4_cbc_ctx *c = to_sm4_cbc_ctx(ctx);
	mbedtls_sm4_free(&c->sm4_ctx);
	free(c);
}

static void mbed_sm4_cbc_copy_state(struct crypto_cipher_ctx *dst_ctx,
				    struct crypto_cipher_ctx *src_ctx)
{
	struct mbed_sm4_cbc_ctx *src = to_sm4_cbc_ctx(src_ctx);
	struct mbed_sm4_cbc_ctx *dst = to_sm4_cbc_ctx(dst_ctx);

	memcpy(dst->iv, src->iv, sizeof(dst->iv));
	dst->mbed_mode = src->mbed_mode;
#if defined(CFG_MBEDTLS_TE)
	assert(mbedtls_sm4_clone(&dst->sm4_ctx, &src->sm4_ctx) == 0);
#else
	dst->sm4_ctx = src->sm4_ctx;
#endif
}

static const struct crypto_cipher_ops mbed_sm4_cbc_ops = {
	.init = mbed_sm4_cbc_init,
	.update = mbed_sm4_cbc_update,
	.final = mbed_sm4_cbc_final,
	.free_ctx = mbed_sm4_cbc_free_ctx,
	.copy_state = mbed_sm4_cbc_copy_state,
#if defined(CFG_MBEDTLS_TE)
	.init2 = mbed_sm4_cbc_init2,
#endif
};

TEE_Result crypto_sm4_cbc_alloc_ctx(struct crypto_cipher_ctx **ctx_ret)
{
	struct mbed_sm4_cbc_ctx *c = NULL;

	c = calloc(1, sizeof(*c));
	if (!c)
		return TEE_ERROR_OUT_OF_MEMORY;

	mbedtls_sm4_init(&c->sm4_ctx);
	c->ctx.ops = &mbed_sm4_cbc_ops;
	*ctx_ret = &c->ctx;

	return TEE_SUCCESS;
}
