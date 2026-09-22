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

struct mbed_sm4_ctr_ctx {
	struct crypto_cipher_ctx ctx;
	mbedtls_sm4_context sm4_ctx;
	size_t nc_off;
	unsigned char counter[TEE_SM4_BLOCK_SIZE];
	unsigned char block[TEE_SM4_BLOCK_SIZE];
};

static const struct crypto_cipher_ops mbed_sm4_ctr_ops;

static struct mbed_sm4_ctr_ctx *to_sm4_ctr_ctx(struct crypto_cipher_ctx *ctx)
{
	assert(ctx && ctx->ops == &mbed_sm4_ctr_ops);

	return container_of(ctx, struct mbed_sm4_ctr_ctx, ctx);
}

static TEE_Result mbed_sm4_ctr_init(struct crypto_cipher_ctx *ctx,
				    TEE_OperationMode mode __unused,
				    const uint8_t *key1, size_t key1_len,
				    const uint8_t *key2 __unused,
				    size_t key2_len __unused,
				    const uint8_t *iv, size_t iv_len)
{
	struct mbed_sm4_ctr_ctx *c = to_sm4_ctr_ctx(ctx);

	if (iv_len != sizeof(c->counter))
		return TEE_ERROR_BAD_PARAMETERS;
	memcpy(c->counter, iv, sizeof(c->counter));

	c->nc_off = 0;

	if (mbedtls_sm4_setkey_enc(&c->sm4_ctx, key1, key1_len * 8))
		return TEE_ERROR_BAD_STATE;

	return TEE_SUCCESS;
}

#if defined(CFG_MBEDTLS_TE)
#include <mbed_impl.h>

DEFINE_TO_MBED_SEC_KEY(sm4, SM4)

static TEE_Result mbed_sm4_ctr_init2(struct crypto_cipher_ctx *ctx,
				     TEE_OperationMode mode __unused,
				     const crypto_sec_key_t *key1,
				     const crypto_sec_key_t *key2 __unused,
				     const uint8_t *iv, size_t iv_len)
{
	struct mbed_sm4_ctr_ctx *c = to_sm4_ctr_ctx(ctx);
	mbedtls_sm4_sec_key_t skey = {0};

	if (iv_len != sizeof(c->counter))
		return TEE_ERROR_BAD_PARAMETERS;
	memcpy(c->counter, iv, sizeof(c->counter));

	TO_MBED_SEC_KEY(sm4, key1, &skey);
	c->nc_off = 0;

	if (mbedtls_sm4_setseckey_enc(&c->sm4_ctx, &skey))
		return TEE_ERROR_BAD_STATE;

	return TEE_SUCCESS;
}
#endif /* CFG_MBEDTLS_TE */

static TEE_Result mbed_sm4_ctr_update(struct crypto_cipher_ctx *ctx,
				      bool last_block __unused,
				      const uint8_t *data, size_t len,
				      uint8_t *dst)
{
	struct mbed_sm4_ctr_ctx *c = to_sm4_ctr_ctx(ctx);

	if (mbedtls_sm4_crypt_ctr(&c->sm4_ctx, len, &c->nc_off, c->counter,
				   c->block, data, dst))
		return TEE_ERROR_BAD_STATE;

	return TEE_SUCCESS;
}

static void mbed_sm4_ctr_final(struct crypto_cipher_ctx *ctx)
{
	struct mbed_sm4_ctr_ctx *c = to_sm4_ctr_ctx(ctx);

	memset(c->block, 0, sizeof(c->block));
}

static void mbed_sm4_ctr_free_ctx(struct crypto_cipher_ctx *ctx)
{
	struct mbed_sm4_ctr_ctx *c = to_sm4_ctr_ctx(ctx);
	mbedtls_sm4_free(&c->sm4_ctx);
	free(c);
}

static void mbed_sm4_ctr_copy_state(struct crypto_cipher_ctx *dst_ctx,
				    struct crypto_cipher_ctx *src_ctx)
{
	struct mbed_sm4_ctr_ctx *src = to_sm4_ctr_ctx(src_ctx);
	struct mbed_sm4_ctr_ctx *dst = to_sm4_ctr_ctx(dst_ctx);

	memcpy(dst->counter, src->counter, sizeof(dst->counter));
	memcpy(dst->block, src->block, sizeof(dst->block));
	dst->nc_off = src->nc_off;
#if defined(CFG_MBEDTLS_TE)
	assert(mbedtls_sm4_clone(&dst->sm4_ctx, &src->sm4_ctx) == 0);
#else
	dst->sm4_ctx = src->sm4_ctx;
#endif
}

static const struct crypto_cipher_ops mbed_sm4_ctr_ops = {
	.init = mbed_sm4_ctr_init,
	.update = mbed_sm4_ctr_update,
	.final = mbed_sm4_ctr_final,
	.free_ctx = mbed_sm4_ctr_free_ctx,
	.copy_state = mbed_sm4_ctr_copy_state,
#if defined(CFG_MBEDTLS_TE)
	.init2 = mbed_sm4_ctr_init2,
#endif
};

TEE_Result crypto_sm4_ctr_alloc_ctx(struct crypto_cipher_ctx **ctx_ret)
{
	struct mbed_sm4_ctr_ctx *c = NULL;

	c = calloc(1, sizeof(*c));
	if (!c)
		return TEE_ERROR_OUT_OF_MEMORY;

	mbedtls_sm4_init(&c->sm4_ctx);
	c->ctx.ops = &mbed_sm4_ctr_ops;
	*ctx_ret = &c->ctx;

	return TEE_SUCCESS;
}
