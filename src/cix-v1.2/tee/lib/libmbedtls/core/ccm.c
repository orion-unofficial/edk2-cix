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
#include <mbedtls/ccm.h>
#include <stdlib.h>
#include <string.h>
#include <tee_api_types.h>
#include <utee_defines.h>
#include <util.h>
#include <string_ext.h>

#define TEE_CCM_TAG_MAX_LENGTH		16
/*
 * Corresponding to CCM_ENCRYPT/CCM_DECRYPT in
 * mbedtls/library/ccm.c
 */
#define MBED_CCM_ENCRYPT 0
#define MBED_CCM_DECRYPT 1

struct internal_mbed_ccm_ctx {
	mbedtls_cipher_id_t cipher;
	mbedtls_ccm_context cctx;
	TEE_OperationMode mode;
};

struct mbed_ccm_ctx {
	struct crypto_authenc_ctx aec;
	struct internal_mbed_ccm_ctx ctx;
};

static const struct crypto_authenc_ops mbed_ccm_ops;

static struct mbed_ccm_ctx *
to_mbed_ccm_ctx(struct crypto_authenc_ctx *aec)
{
	assert(aec->ops == &mbed_ccm_ops);

	return container_of(aec, struct mbed_ccm_ctx, aec);
}

static TEE_Result to_tee_result(int err)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	switch(err) {
	case 0:
		res = TEE_SUCCESS;
		break;
#if defined(CFG_MBEDTLS_TE)
	case MBEDTLS_ERR_CCM_ALLOC_FAILED:
		res = TEE_ERROR_OUT_OF_MEMORY;
		break;
#endif
	case MBEDTLS_ERR_CCM_BAD_INPUT:
		res = TEE_ERROR_BAD_PARAMETERS;
		break;
	case MBEDTLS_ERR_CCM_AUTH_FAILED:
		res = TEE_ERROR_MAC_INVALID;
		break;
	default:
		break;
	}

	return res;
}

static void mbed_ccm_free_ctx(struct crypto_authenc_ctx *aec)
{
	struct mbed_ccm_ctx *ccm = to_mbed_ccm_ctx(aec);
	mbedtls_ccm_free(&ccm->ctx.cctx);
	free(ccm);
}

static void mbed_ccm_copy_state(struct crypto_authenc_ctx *dst_ctx,
			       struct crypto_authenc_ctx *src_ctx)
{
	struct mbed_ccm_ctx *dst = to_mbed_ccm_ctx(dst_ctx);
	struct mbed_ccm_ctx *src = to_mbed_ccm_ctx(src_ctx);

	assert(mbedtls_ccm_clone(&dst->ctx.cctx, &src->ctx.cctx) == 0);
	dst->ctx.mode = src->ctx.mode;
}

static TEE_Result mbed_ccm_init(struct crypto_authenc_ctx *aec,
			        TEE_OperationMode mode,
			        const uint8_t *key, size_t key_len,
			        const uint8_t *nonce, size_t nonce_len,
			        size_t tag_len, size_t aadlen,
			        size_t payload_len)
{
	int rc = 0;
	TEE_Result res = TEE_SUCCESS;
	struct mbed_ccm_ctx *ccm = to_mbed_ccm_ctx(aec);

	if (mode != TEE_MODE_ENCRYPT && mode != TEE_MODE_DECRYPT) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if ((rc = mbedtls_ccm_setkey(&ccm->ctx.cctx, ccm->ctx.cipher,
				     key, key_len << 3))) {
		goto err;
	}

	if ((rc = mbedtls_ccm_starts(&ccm->ctx.cctx,
				     (TEE_MODE_ENCRYPT == mode ?
				      MBED_CCM_ENCRYPT : MBED_CCM_DECRYPT),
				     nonce,
				     nonce_len,
				     tag_len,
				     aadlen,
				     payload_len))) {
		goto err;
	}

	ccm->ctx.mode = mode;
	return TEE_SUCCESS;

err:
	res = to_tee_result(rc);
	return res;
}

#if defined(CFG_MBEDTLS_TE)
#include <mbed_impl.h>

DEFINE_TO_MBED_SEC_KEY(ccm, CCM)

static TEE_Result mbed_ccm_init2(struct crypto_authenc_ctx *aec,
			         TEE_OperationMode mode,
			         const crypto_sec_key_t *key,
			         const uint8_t *nonce, size_t nonce_len,
			         size_t tag_len, size_t aadlen,
			         size_t payload_len)
{
	int rc = 0;
	TEE_Result res = TEE_SUCCESS;
	struct mbed_ccm_ctx *ccm = to_mbed_ccm_ctx(aec);
	mbedtls_ccm_sec_key_t skey = {0};

	if (mode != TEE_MODE_ENCRYPT && mode != TEE_MODE_DECRYPT) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	TO_MBED_SEC_KEY(ccm, key, &skey);
	if ((rc = mbedtls_ccm_setseckey(&ccm->ctx.cctx, ccm->ctx.cipher,
				        &skey))) {
		goto err;
	}

	if ((rc = mbedtls_ccm_starts(&ccm->ctx.cctx,
				     (TEE_MODE_ENCRYPT == mode ?
				      MBED_CCM_ENCRYPT : MBED_CCM_DECRYPT),
				     nonce,
				     nonce_len,
				     tag_len,
				     aadlen,
				     payload_len))) {
		goto err;
	}

	ccm->ctx.mode = mode;
	return TEE_SUCCESS;

err:
	res = to_tee_result(rc);
	return res;
}

#endif /* CFG_MBEDTLS_TE */

/* Add the AAD (note: data can be NULL if len == 0) */
static TEE_Result mbed_ccm_update_aad(struct crypto_authenc_ctx *aec,
				     const uint8_t *data, size_t len)
{
	int rc = 0;
	struct mbed_ccm_ctx *ccm = to_mbed_ccm_ctx(aec);

	rc = mbedtls_ccm_update_aad(&ccm->ctx.cctx, len, data);
	return to_tee_result(rc);
}

static TEE_Result mbed_ccm_update_payload(struct crypto_authenc_ctx *aec,
					 TEE_OperationMode m,
					 const uint8_t *src, size_t len,
					 uint8_t *dst)
{
	int rc = 0;
	struct mbed_ccm_ctx *ccm = to_mbed_ccm_ctx(aec);

	if (m != ccm->ctx.mode) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	rc = mbedtls_ccm_update(&ccm->ctx.cctx, len, src, dst);
	return to_tee_result(rc);
}

static TEE_Result mbed_ccm_enc_final(struct crypto_authenc_ctx *aec,
				    const uint8_t *src, size_t len,
				    uint8_t *dst, uint8_t *tag, size_t *tag_len)
{
	int rc = 0;
	TEE_Result res = TEE_SUCCESS;
	struct mbed_ccm_ctx *ccm = to_mbed_ccm_ctx(aec);

	res = mbed_ccm_update_payload(aec, TEE_MODE_ENCRYPT, src, len, dst);
	if (res != TEE_SUCCESS) {
		return res;
	}

	rc = mbedtls_ccm_finish(&ccm->ctx.cctx, tag, *tag_len);
	return to_tee_result(rc);
}

static TEE_Result mbed_ccm_dec_final(struct crypto_authenc_ctx *aec,
				    const uint8_t *src, size_t len,
				    uint8_t *dst, const uint8_t *tag,
				    size_t tag_len)
{
	int rc = 0;
	TEE_Result res = TEE_SUCCESS;
	struct mbed_ccm_ctx *ccm = to_mbed_ccm_ctx(aec);
	uint8_t dst_tag[TEE_CCM_TAG_MAX_LENGTH] = {0};

	if (tag_len == 0)
		return TEE_ERROR_SHORT_BUFFER;
	if ( (tag == NULL) || (tag_len > TEE_CCM_TAG_MAX_LENGTH) )
		return TEE_ERROR_BAD_STATE;

	res = mbed_ccm_update_payload(aec, TEE_MODE_DECRYPT, src, len, dst);
	if (res != TEE_SUCCESS) {
		return res;
	}

	rc = mbedtls_ccm_finish(&ccm->ctx.cctx, dst_tag, tag_len);
	if ((rc == 0) && (consttime_memcmp(dst_tag, tag, tag_len) != 0)) {
		rc = MBEDTLS_ERR_CCM_AUTH_FAILED;
	}
	return to_tee_result(rc);
}

static void mbed_ccm_final(struct crypto_authenc_ctx *aec)
{
	(void)aec;
}

static const struct crypto_authenc_ops mbed_ccm_ops = {
	.init = mbed_ccm_init,
	.update_aad = mbed_ccm_update_aad,
	.update_payload = mbed_ccm_update_payload,
	.enc_final = mbed_ccm_enc_final,
	.dec_final = mbed_ccm_dec_final,
	.final = mbed_ccm_final,
	.free_ctx = mbed_ccm_free_ctx,
	.copy_state = mbed_ccm_copy_state,
#if defined(CFG_MBEDTLS_TE)
	.init2 = mbed_ccm_init2,
#endif
};

static TEE_Result crypto_mbed_ccm_alloc_ctx(struct crypto_authenc_ctx **ctx_ret,
					    mbedtls_cipher_id_t cipher)
{
	struct mbed_ccm_ctx *ctx = NULL;

	ctx = calloc(1, sizeof(*ctx));

	if (!ctx)
		return TEE_ERROR_OUT_OF_MEMORY;

	mbedtls_ccm_init(&ctx->ctx.cctx);
	ctx->aec.ops = &mbed_ccm_ops;
	ctx->ctx.cipher = cipher;

	*ctx_ret = &ctx->aec;

	return TEE_SUCCESS;
}

#ifdef CFG_CRYPTO_AES
TEE_Result crypto_aes_ccm_alloc_ctx(struct crypto_authenc_ctx **ctx_ret)
{
	return crypto_mbed_ccm_alloc_ctx(ctx_ret, MBEDTLS_CIPHER_ID_AES);
}
#endif

#ifdef CFG_CRYPTO_SM4
TEE_Result crypto_sm4_ccm_alloc_ctx(struct crypto_authenc_ctx **ctx_ret)
{
	return crypto_mbed_ccm_alloc_ctx(ctx_ret, MBEDTLS_CIPHER_ID_SM4);
}
#endif
