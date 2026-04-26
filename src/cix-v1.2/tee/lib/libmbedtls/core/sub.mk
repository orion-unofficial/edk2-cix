srcs-y += mbed_helpers.c
srcs-y += tomcrypt.c
srcs-$(call cfg-one-enabled, CFG_CRYPTO_MD5 CFG_CRYPTO_SHA1 CFG_CRYPTO_SHA224 \
			     CFG_CRYPTO_SHA256 CFG_CRYPTO_SHA384 \
			     CFG_CRYPTO_SHA512) += hash.c

ifeq ($(CFG_CRYPTO_AES),y)
srcs-y += aes.c
srcs-$(CFG_CRYPTO_ECB) += aes_ecb.c
srcs-$(CFG_CRYPTO_CBC) += aes_cbc.c
srcs-$(CFG_CRYPTO_CTR) += aes_ctr.c
endif
ifeq ($(CFG_CRYPTO_DES),y)
srcs-$(CFG_CRYPTO_ECB) += des_ecb.c
srcs-$(CFG_CRYPTO_ECB) += des3_ecb.c
srcs-$(CFG_CRYPTO_CBC) += des_cbc.c
srcs-$(CFG_CRYPTO_CBC) += des3_cbc.c
endif

srcs-$(CFG_CRYPTO_HMAC) += hmac.c
ifeq ($(CFG_CRYPTO_CMAC),y)
srcs-y := $(filter-out aes_cmac.c,$(srcs-y))
srcs-$(call cfg-one-enabled, CFG_CRYPTO_AES \
                 CFG_CRYPTO_DES \
                 CFG_CRYPTO_SM4) += cmac.c
endif

ifneq ($(CFG_CRYPTO_DSA),y)
srcs-$(call cfg-one-enabled, CFG_CRYPTO_RSA  CFG_CRYPTO_DH \
			     CFG_CRYPTO_ECC) += bignum.c
endif
srcs-$(CFG_CRYPTO_RSA) += rsa.c
srcs-$(CFG_CRYPTO_DH) += dh.c
srcs-$(CFG_CRYPTO_ECC) += ecc.c

srcs-$(CFG_CRYPTO_SM2_DSA) += sm2-dsa.c
srcs-$(CFG_CRYPTO_SM2_KEP) += sm2-kep.c
srcs-$(CFG_CRYPTO_SM2_PKE) += sm2-pke.c

# Trust Engine specific configs (TEE core only)
ifeq ($(CFG_MBEDTLS_TE),y)

ifeq ($(CFG_CRYPTO_SM4),y)
srcs-$(call cfg-all-enabled, CFG_CRYPTO_SM4_ECB_FROM_CRYPTOLIB \
                 CFG_CRYPTO_ECB) += sm4_ecb.c
srcs-$(call cfg-all-enabled, CFG_CRYPTO_SM4_CBC_FROM_CRYPTOLIB \
                 CFG_CRYPTO_CBC) += sm4_cbc.c
srcs-$(call cfg-all-enabled, CFG_CRYPTO_SM4_CTR_FROM_CRYPTOLIB \
                 CFG_CRYPTO_CTR) += sm4_ctr.c
endif

ifeq ($(CFG_CRYPTOLIB_NAME_mbedtls),y)

ifeq ($(CFG_CRYPTO_AES_GCM_FROM_CRYPTOLIB),y)
srcs-$(call cfg-one-enabled, CFG_CRYPTO_AES \
                 CFG_CRYPTO_SM4) += gcm.c
endif

ifeq ($(CFG_CRYPTO_XTS),y)
srcs-$(call cfg-one-enabled, CFG_CRYPTO_AES \
                 CFG_CRYPTO_SM4) += xts.c
endif

ifeq ($(CFG_CRYPTO_CCM),y)
srcs-$(call cfg-one-enabled, CFG_CRYPTO_AES \
                 CFG_CRYPTO_SM4) += ccm.c
endif

ifeq ($(filter bignum.c,$(srcs-y)),)
srcs-$(call cfg-one-enabled, CFG_CRYPTO_RSA     \
                 CFG_CRYPTO_DH      \
                 CFG_CRYPTO_DSA     \
                 CFG_CRYPTO_ECC     \
                 CFG_CRYPTO_SM2_PKE \
                 CFG_CRYPTO_SM2_DSA \
                 CFG_CRYPTO_SM2_KEP) += bignum.c
endif

#srcs-$(call cfg-one-enabled, CFG_CRYPTO_SM2_PKE \
#                 CFG_CRYPTO_SM2_DSA \
#                 CFG_CRYPTO_SM2_KEP) += sm2.c

srcs-$(CFG_CRYPTO_DSA) += dsa.c

endif # CFG_CRYPTOLIB_NAME_mbedtls
endif # CFG_MBEDTLS_TE

ifneq ($(CFG_MBEDTLS_TE)_$(CFG_TE_TRNG),n_n)
ifneq ($(CFG_WITH_SOFTWARE_PRNG),y)
srcs-y += rng.c
endif

MBED_CORE_TOP := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

srcs-y += $(patsubst $(MBED_CORE_TOP)/%,%,  \
          $(wildcard $(MBED_CORE_TOP)/te/*.c))

global-incdirs-y += include/te
incdirs-y += include
incdirs-y += te_driver

endif # (CFG_MBEDTLS_TE || CFG_TE_TRNG)

srcs-y += platform.c

# mbedtls self test
#subdirs-$(CFG_MBEDTLS_SELFTEST) += test

subdirs-$(CFG_TE_TRNGD) += te_trngd

subdirs-$(CFG_TE_DRIVER) += te_driver
subdirs-$(CFG_MBEDTLS_TEST) += mbedtls_test

cflags-lib-$(call cfg-one-enabled, CFG_MBEDTLS_TE               \
                                   CFG_MBEDTLS_TEST             \
                                   CFG_TE_DRIVER) +=            \
                -DOSAL_ENV_OPTEE_OS                             \
                -DOSAL_LOG_PREFIX_NAME=\"MBEDTLS_CORE\ \"       \
                -DOSAL_MAX_LOG_LEVEL=$(CFG_TEE_CORE_LOG_LEVEL)  \
                -include assert.h                               \
                -DOSAL_COMPILE_ASSERT=COMPILE_TIME_ASSERT       \
                -DOSAL_ASSERT_BREAK=assert\(0\)                 \
                -Ilib/libhosal/misc/sqlist						\
				-DMBEDTLS_PLATFORM_STD_PRINTF=NULL
