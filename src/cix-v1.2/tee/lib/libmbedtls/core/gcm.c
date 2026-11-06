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
#include <compiler.h>
#include <crypto/crypto.h>
#include <crypto/crypto_impl.h>
#include <mbedtls/gcm.h>
#include <stdlib.h>
#include <string.h>
#include <tee_api_types.h>
#include <utee_defines.h>
#include <util.h>
#include <string_ext.h>

#define TEE_GCM_TAG_MAX_LENGTH		16

struct internal_mbed_gcm_ctx {
	mbedtls_cipher_id_t cipher;
	mbedtls_gcm_context gctx;
	TEE_OperationMode mode;
};

struct mbed_gcm_ctx {
	struct crypto_authenc_ctx aec;
	struct internal_mbed_gcm_ctx ctx;
};

static const struct crypto_authenc_ops mbed_gcm_ops;

static struct mbed_gcm_ctx *
to_mbed_gcm_ctx(struct crypto_authenc_ctx *aec)
{
	assert(aec->ops == &mbed_gcm_ops);

	return container_of(aec, struct mbed_gcm_ctx, aec);
}

static TEE_Result to_tee_result(int err)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	switch(err) {
	case 0:
		res = TEE_SUCCESS;
		break;
	case MBEDTLS_ERR_GCM_ALLOC_FAILED:
		res = TEE_ERROR_OUT_OF_MEMORY;
		break;
	case MBEDTLS_ERR_GCM_BAD_INPUT:
		res = TEE_ERROR_BAD_PARAMETERS;
		break;
	case MBEDTLS_ERR_GCM_AUTH_FAILED:
		res = TEE_ERROR_MAC_INVALID;
		break;
	default:
		break;
	}

	return res;
}

static void mbed_gcm_free_ctx(struct crypto_authenc_ctx *aec)
{
	struct mbed_gcm_ctx *gcm = to_mbed_gcm_ctx(aec);
	mbedtls_gcm_free(&gcm->ctx.gctx);
	free(gcm);
}

static void mbed_gcm_copy_state(struct crypto_authenc_ctx *dst_ctx,
			       struct crypto_authenc_ctx *src_ctx)
{
	struct mbed_gcm_ctx *dst = to_mbed_gcm_ctx(dst_ctx);
	struct mbed_gcm_ctx *src = to_mbed_gcm_ctx(src_ctx);

	assert(dst->ctx.cipher == src->ctx.cipher);
	assert(mbedtls_gcm_clone(&dst->ctx.gctx, &src->ctx.gctx) == 0);
	dst->ctx.mode = src->ctx.mode;
}

static TEE_Result mbed_gcm_init(struct crypto_authenc_ctx *aec,
				TEE_OperationMode mode,
				const uint8_t *key, size_t key_len,
				const uint8_t *nonce, size_t nonce_len,
				size_t tag_len __unused,
				size_t aad_len __unused,
			        size_t payload_len __unused)
{
	int rc = 0;
	TEE_Result res = TEE_SUCCESS;
	struct mbed_gcm_ctx *gcm = to_mbed_gcm_ctx(aec);

	if (mode != TEE_MODE_ENCRYPT && mode != TEE_MODE_DECRYPT) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if ((rc = mbedtls_gcm_setkey(&gcm->ctx.gctx, gcm->ctx.cipher,
				     key, key_len << 3))) {
		goto err;
	}

	if ((rc = mbedtls_gcm_starts(&gcm->ctx.gctx,
				     ((TEE_MODE_ENCRYPT == mode) ?
				     MBEDTLS_GCM_ENCRYPT :
				     MBEDTLS_GCM_DECRYPT),
				     nonce,
				     nonce_len,
				     NULL,
				     0)) != 0) {
		goto err;
	}

	gcm->ctx.mode = mode;
	return TEE_SUCCESS;

err:
	res = to_tee_result(rc);
	return res;
}

#if defined(CFG_MBEDTLS_TE)
#include <mbed_impl.h>

DEFINE_TO_MBED_SEC_KEY(gcm, GCM)

static TEE_Result mbed_gcm_init2(struct crypto_authenc_ctx *aec,
				 TEE_OperationMode mode,
				 const crypto_sec_key_t *key,
				 const uint8_t *nonce, size_t nonce_len,
				 size_t tag_len __unused,
				 size_t aad_len __unused,
			         size_t payload_len __unused)
{
	int rc = 0;
	TEE_Result res = TEE_SUCCESS;
	struct mbed_gcm_ctx *gcm = to_mbed_gcm_ctx(aec);
	mbedtls_gcm_sec_key_t skey = {0};

	if (mode != TEE_MODE_ENCRYPT && mode != TEE_MODE_DECRYPT) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	TO_MBED_SEC_KEY(gcm, key, &skey);
	if ((rc = mbedtls_gcm_setseckey(&gcm->ctx.gctx, gcm->ctx.cipher,
				        &skey))) {
		goto err;
	}

	if ((rc = mbedtls_gcm_starts(&gcm->ctx.gctx,
				     ((TEE_MODE_ENCRYPT == mode) ?
				     MBEDTLS_GCM_ENCRYPT :
				     MBEDTLS_GCM_DECRYPT),
				     nonce,
				     nonce_len,
				     NULL,
				     0)) != 0) {
		goto err;
	}

	gcm->ctx.mode = mode;
	return TEE_SUCCESS;

err:
	res = to_tee_result(rc);
	return res;
}
#endif /*CFG_MBEDTLS_TE*/

/* Add the AAD (note: data can be NULL if len == 0) */
static TEE_Result mbed_gcm_update_aad(struct crypto_authenc_ctx *aec,
				      const uint8_t *data, size_t len)
{
	int rc = 0;
	struct mbed_gcm_ctx *gcm = to_mbed_gcm_ctx(aec);

	if ((rc = mbedtls_gcm_update_aad(&gcm->ctx.gctx, len, data)) != 0) {
		return to_tee_result(rc);
	}

	return TEE_SUCCESS;
}

static TEE_Result mbed_gcm_update_payload(struct crypto_authenc_ctx *aec,
					  TEE_OperationMode m,
					  const uint8_t *src,
					  size_t len,
					  uint8_t *dst)
{
	int rc = 0;
	struct mbed_gcm_ctx *gcm = to_mbed_gcm_ctx(aec);

	if (m != gcm->ctx.mode) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	rc = mbedtls_gcm_update(&gcm->ctx.gctx, len, src, dst);
	return to_tee_result(rc);
}

static TEE_Result mbed_gcm_enc_final(struct crypto_authenc_ctx *aec,
				     const uint8_t *src,
				     size_t len,
				     uint8_t *dst,
				     uint8_t *tag,
				     size_t *tag_len)
{
	int rc = 0;
	TEE_Result res = TEE_SUCCESS;
	struct mbed_gcm_ctx *gcm = to_mbed_gcm_ctx(aec);

	res = mbed_gcm_update_payload(aec, TEE_MODE_ENCRYPT, src, len, dst);
	if (res != TEE_SUCCESS) {
		return res;
	}

	rc = mbedtls_gcm_finish(&gcm->ctx.gctx, tag, *tag_len);
	return to_tee_result(rc);
}

static TEE_Result mbed_gcm_dec_final(struct crypto_authenc_ctx *aec,
				     const uint8_t *src,
				     size_t len,
				     uint8_t *dst,
				     const uint8_t *tag,
				     size_t tag_len)
{
	int rc = 0;
	TEE_Result res = TEE_SUCCESS;
	struct mbed_gcm_ctx *gcm = to_mbed_gcm_ctx(aec);
	uint8_t dst_tag[TEE_GCM_TAG_MAX_LENGTH] = {0};

	if (tag_len == 0)
		return TEE_ERROR_SHORT_BUFFER;
	if ( (tag == NULL) || (tag_len > TEE_GCM_TAG_MAX_LENGTH) )
		return TEE_ERROR_BAD_STATE;
	res = mbed_gcm_update_payload(aec, TEE_MODE_DECRYPT, src, len, dst);
	if (res != TEE_SUCCESS) {
		return res;
	}

	rc = mbedtls_gcm_finish(&gcm->ctx.gctx, dst_tag, tag_len);
	if ((rc == 0) && (consttime_memcmp(dst_tag, tag, tag_len) != 0)) {
		rc = MBEDTLS_ERR_GCM_AUTH_FAILED;
	}
	return to_tee_result(rc);
}

static void mbed_gcm_final(struct crypto_authenc_ctx *aec)
{
	(void)aec;
}

static const struct crypto_authenc_ops mbed_gcm_ops = {
	.init = mbed_gcm_init,
	.update_aad = mbed_gcm_update_aad,
	.update_payload = mbed_gcm_update_payload,
	.enc_final = mbed_gcm_enc_final,
	.dec_final = mbed_gcm_dec_final,
	.final = mbed_gcm_final,
	.free_ctx = mbed_gcm_free_ctx,
	.copy_state = mbed_gcm_copy_state,
#if defined(CFG_MBEDTLS_TE)
	.init2 = mbed_gcm_init2,
#endif
};

static TEE_Result crypto_mbed_gcm_alloc_ctx(struct crypto_authenc_ctx **ctx_ret,
					    mbedtls_cipher_id_t cipher)
{
	struct mbed_gcm_ctx *ctx = NULL;

	ctx = calloc(1, sizeof(*ctx));

	if (!ctx)
		return TEE_ERROR_OUT_OF_MEMORY;

	mbedtls_gcm_init(&ctx->ctx.gctx);
	ctx->aec.ops = &mbed_gcm_ops;
	ctx->ctx.cipher = cipher;

	*ctx_ret = &ctx->aec;

	return TEE_SUCCESS;
}

#ifdef CFG_CRYPTO_AES
TEE_Result crypto_aes_gcm_alloc_ctx(struct crypto_authenc_ctx **ctx_ret)
{
	return crypto_mbed_gcm_alloc_ctx(ctx_ret, MBEDTLS_CIPHER_ID_AES);
}
#endif

#ifdef CFG_CRYPTO_SM4
TEE_Result crypto_sm4_gcm_alloc_ctx(struct crypto_authenc_ctx **ctx_ret)
{
	return crypto_mbed_gcm_alloc_ctx(ctx_ret, MBEDTLS_CIPHER_ID_SM4);
}
#endif
