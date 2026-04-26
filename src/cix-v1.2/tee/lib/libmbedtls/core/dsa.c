/*
 * Copyright (c) 2018-2020, Arm Technology (China) Co., Ltd.
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
#include <mbedtls/dsa.h>
#include <stdlib.h>
#include <string.h>
#include <tee_api_types.h>
#include <tee/tee_cryp_utl.h>
#include <utee_defines.h>
#include <util.h>

#include "mbd_rand.h"

#define MBEDTLS_MPI_COPY(X, Y) do {                                         \
	lmd_res = mbedtls_mpi_copy((X), (Y));                               \
	if (lmd_res != 0) {                                                 \
		FMSG("mbedtls_mpi_copy failed, returned 0x%x\n", -lmd_res); \
		res = get_tee_result(lmd_res);                              \
		goto out;                                                   \
	}                                                                   \
} while(0)

#if defined(CFG_CRYPTO_DSA)
static TEE_Result get_tee_result(int err)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	switch (err) {
	case 0:
		res = TEE_SUCCESS;
		break;
	case MBEDTLS_ERR_MPI_BAD_INPUT_DATA:
	case MBEDTLS_ERR_DSA_BAD_INPUT_DATA:
	case MBEDTLS_ERR_DSA_SIG_LEN_MISMATCH:
		res = TEE_ERROR_BAD_PARAMETERS;
		break;
	case MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL:
	case MBEDTLS_ERR_DSA_BUFFER_TOO_SMALL:
		res = TEE_ERROR_SHORT_BUFFER;
		break;
	case MBEDTLS_ERR_MPI_ALLOC_FAILED:
	case MBEDTLS_ERR_DSA_ALLOC_FAILED:
		res = TEE_ERROR_OUT_OF_MEMORY;
		break;
	case MBEDTLS_ERR_DSA_VERIFY_FAILED:
		res = TEE_ERROR_SECURITY;
		break;
	default:
		break;
	}

	return res;
}

static void* bn_alloc_max(struct bignum **bn)
{
	*bn = crypto_bignum_allocate(CFG_CORE_BIGNUM_MAX_BITS);
	return *bn;
}

TEE_Result crypto_acipher_alloc_dsa_keypair(struct dsa_keypair *s,
					    size_t key_size_bits __unused)
{
	memset(s, 0, sizeof(*s));
	if (!bn_alloc_max(&s->g))
		return TEE_ERROR_OUT_OF_MEMORY;

	if (!bn_alloc_max(&s->p))
		goto err;
	if (!bn_alloc_max(&s->q))
		goto err;
	if (!bn_alloc_max(&s->y))
		goto err;
	if (!bn_alloc_max(&s->x))
		goto err;

	return TEE_SUCCESS;
err:
	crypto_bignum_free(s->g);
	crypto_bignum_free(s->p);
	crypto_bignum_free(s->q);
	crypto_bignum_free(s->y);
	return TEE_ERROR_OUT_OF_MEMORY;
}

TEE_Result
crypto_acipher_alloc_dsa_public_key(struct dsa_public_key *s,
				    size_t key_size_bits __unused)
{
	memset(s, 0, sizeof(*s));
	if (!bn_alloc_max(&s->g))
		return TEE_ERROR_OUT_OF_MEMORY;

	if (!bn_alloc_max(&s->p))
		goto err;
	if (!bn_alloc_max(&s->q))
		goto err;
	if (!bn_alloc_max(&s->y))
		goto err;

	return TEE_SUCCESS;

err:
	crypto_bignum_free(s->g);
	crypto_bignum_free(s->p);
	crypto_bignum_free(s->q);
	return TEE_ERROR_OUT_OF_MEMORY;
}

TEE_Result crypto_acipher_gen_dsa_key(struct dsa_keypair *key,
				      size_t key_size)
{
	TEE_Result res = TEE_SUCCESS;
	int lmd_res = 0;
	size_t pbits = 0;
	mbedtls_dsa_context dsa;

	memset(&dsa, 0, sizeof(dsa));
	pbits = mbedtls_mpi_bitlen((mbedtls_mpi*)key->p);
	if (key_size != pbits) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	mbedtls_dsa_init(&dsa);
	MBEDTLS_MPI_COPY(&dsa.p, (mbedtls_mpi*)key->p);
	MBEDTLS_MPI_COPY(&dsa.q, (mbedtls_mpi*)key->q);
	MBEDTLS_MPI_COPY(&dsa.g, (mbedtls_mpi*)key->g);

	if ((lmd_res = mbedtls_dsa_genkey(&dsa, mbd_rand, NULL)) != 0) {
		FMSG("mbedtls_dsa_genkey failed, ret 0x%x\n", -lmd_res);
		res = get_tee_result(lmd_res);
		goto out;
	}

	MBEDTLS_MPI_COPY((mbedtls_mpi*)key->x, &dsa.x);
	MBEDTLS_MPI_COPY((mbedtls_mpi*)key->y, &dsa.y);

out:
	mbedtls_dsa_free(&dsa);
	return res;
}

TEE_Result crypto_acipher_dsa_sign(uint32_t algo,
				   struct dsa_keypair *key,
				   const uint8_t *msg,
				   size_t msg_len,
				   uint8_t *sig,
				   size_t *sig_len)
{
	int lmd_res = 0;
	TEE_Result res = TEE_SUCCESS;
	size_t hash_size = 0;
	size_t qlen = 0;
	mbedtls_dsa_context dsa;
	mbedtls_mpi r, s;

	memset(&dsa, 0, sizeof(dsa));
	memset(&r, 0, sizeof(r));
	memset(&s, 0, sizeof(s));

	if (algo != TEE_ALG_DSA_SHA1 &&
	    algo != TEE_ALG_DSA_SHA224 &&
	    algo != TEE_ALG_DSA_SHA256) {
		return TEE_ERROR_NOT_IMPLEMENTED;
	}

	res = tee_alg_get_digest_size(TEE_DIGEST_HASH_TO_ALGO(algo),
				       &hash_size);
	if (res != TEE_SUCCESS)
		return res;

	qlen = mbedtls_mpi_size((mbedtls_mpi*)key->q);
	if (qlen < hash_size)
		hash_size = qlen;

	if (msg_len != hash_size) {
		return TEE_ERROR_SECURITY;
	}

	if (*sig_len < 2 * qlen) {
		*sig_len = 2 * qlen;
		return TEE_ERROR_SHORT_BUFFER;
	}

	mbedtls_dsa_init(&dsa);
	mbedtls_mpi_init(&r);
	mbedtls_mpi_init(&s);

	MBEDTLS_MPI_COPY(&dsa.p, (mbedtls_mpi*)key->p);
	MBEDTLS_MPI_COPY(&dsa.q, (mbedtls_mpi*)key->q);
	MBEDTLS_MPI_COPY(&dsa.g, (mbedtls_mpi*)key->g);
	MBEDTLS_MPI_COPY(&dsa.x, (mbedtls_mpi*)key->x);

	if ((lmd_res = mbedtls_dsa_sign(&dsa.p,
					&dsa.q,
					&dsa.g,
					&r,
					&s,
					&dsa.x,
					msg,
					msg_len,
					mbd_rand,
					NULL)) != 0) {
		FMSG("mbedtls_dsa_sign failed, ret 0x%x\n", -lmd_res);
		res = get_tee_result(lmd_res);
		goto out;
	}

	memset(sig, 0, 2 * qlen);
	lmd_res = mbedtls_mpi_write_binary(&r, sig, qlen);
	if (lmd_res != 0) {
		FMSG("mbedtls_mpi_write_binary failed, ret 0x%x\n", -lmd_res);
		res = get_tee_result(lmd_res);
		goto out;
	}
	lmd_res = mbedtls_mpi_write_binary(&s,
					   sig + qlen,
					   qlen);
	if (lmd_res != 0) {
		FMSG("mbedtls_mpi_write_binary failed, ret 0x%x\n", -lmd_res);
		res = get_tee_result(lmd_res);
		goto out;
	}

	*sig_len = 2 * qlen;
	res = TEE_SUCCESS;

out:
	mbedtls_dsa_free(&dsa);
	mbedtls_mpi_free(&r);
	mbedtls_mpi_free(&s);

	return res;
}

TEE_Result crypto_acipher_dsa_verify(uint32_t algo,
				     struct dsa_public_key *key,
				     const uint8_t *msg,
				     size_t msg_len,
				     const uint8_t *sig,
				     size_t sig_len)
{
	TEE_Result res = TEE_SUCCESS;
	int lmd_res = 0;
	mbedtls_dsa_context dsa;
	mbedtls_mpi r, s;

	memset(&dsa, 0, sizeof(dsa));
	memset(&r, 0, sizeof(r));
	memset(&s, 0, sizeof(s));

	if (algo != TEE_ALG_DSA_SHA1 &&
	    algo != TEE_ALG_DSA_SHA224 &&
	    algo != TEE_ALG_DSA_SHA256) {
		return TEE_ERROR_NOT_IMPLEMENTED;
	}

	mbedtls_dsa_init(&dsa);
	mbedtls_mpi_init(&r);
	mbedtls_mpi_init(&s);

	MBEDTLS_MPI_COPY(&dsa.p, (mbedtls_mpi*)key->p);
	MBEDTLS_MPI_COPY(&dsa.q, (mbedtls_mpi*)key->q);
	MBEDTLS_MPI_COPY(&dsa.g, (mbedtls_mpi*)key->g);
	MBEDTLS_MPI_COPY(&dsa.y, (mbedtls_mpi*)key->y);
	lmd_res = mbedtls_mpi_read_binary(&r, (uint8_t *)sig, sig_len / 2);
	if (lmd_res != 0) {
		FMSG("mbedtls_mpi_read_binary failed, ret 0x%x\n", -lmd_res);
		res = get_tee_result(lmd_res);
		goto out;
	}

	lmd_res = mbedtls_mpi_read_binary(&s,
					  (uint8_t *)sig + sig_len / 2,
					  sig_len / 2);
	if (lmd_res != 0) {
		FMSG("mbedtls_mpi_read_binary failed, ret 0x%x\n", -lmd_res);
		res = get_tee_result(lmd_res);
		goto out;
	}

	if ((lmd_res = mbedtls_dsa_verify(&dsa.p,
					  &dsa.q,
					  &dsa.g,
					  msg,
					  msg_len,
					  &dsa.y,
					  &r,
					  &s)) != 0) {
		FMSG("mbedtls_dsa_verify failed, ret 0x%x\n", -lmd_res);
		res = get_tee_result(lmd_res);
		goto out;
	}

	res = TEE_SUCCESS; /* succeed */

out:
	mbedtls_dsa_free(&dsa);
	mbedtls_mpi_free(&r);
	mbedtls_mpi_free(&s);

	return res;
}
#endif /*CFG_CRYPTO_DSA*/

