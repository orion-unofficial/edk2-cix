PLATFORM_FLAVOR ?= sky1

include core/arch/arm/cpu/cortex-armv8-0.mk

$(call force,CFG_GENERIC_BOOT,y)
$(call force,CFG_PM_STUBS,y)
$(call force,CFG_SECURE_TIME_SOURCE_CNTPCT,y)
$(call force,CFG_WITH_ARM_TRUSTED_FW,y)
$(call force,CFG_WITH_USER_TA,y)
$(call force,CFG_GIC,y)
$(call force,CFG_ARM_GICV3,y)
$(call force,CFG_CIX_UART,y)
$(call force,CFG_IPC_ENABLE,y)
$(call force,CFG_CIX_HUK_ENABLE,y)
$(call force,CFG_WITH_SOFTWARE_PRNG,n)
$(call force,CFG_HWRNG_PTA,y)
$(call force,CFG_HWRNG_QUALITY,1024)
$(call force,CFG_CIX_KEY_META_ENABLE,y)
#$(call force,CFG_ENCRYPT_TA,y)

ifeq ($(CIX_DRM_ENABLE),y)
CFG_TEE_SDP_MEM_BASE ?= 0xBDE00000
CFG_TEE_SDP_MEM_SIZE ?= 0x00800000
CFG_SECURE_DATA_PATH ?= y
CFG_SDP_PTA ?= y
endif

ta-targets = ta_arm32
ifeq ($(CFG_ARM64_core),y)
$(call force,CFG_WITH_LPAE,y)
ta-targets = ta_arm64
else
$(call force,CFG_ARM32_core,y)
endif

CFG_WITH_STACK_CANARIES ?= y

#
# Config for sky1
#
ifeq ($(PLATFORM_FLAVOR),sky1)

CFG_CRYPTOLIB_NAME := mbedtls
CFG_CRYPTOLIB_DIR := lib/libmbedtls

$(call force,CFG_TEE_RAM_VA_SIZE,0x00400000)

#
# support Tx HDCP
#
CFG_CIX_DP_HDCP_TX ?= y

CFG_MMAP_REGIONS ?= 15

#
# Merge from JIAYU driver
#
CFG_WITH_STATS ?= n
CFG_CRYPTO_WITH_CE ?= n
# support user TA concurrent
CFG_USER_TA_CONCURRENT ?= n
# support ree TA encryption
CFG_HAVE_TAENC ?= n
# support RPMB FS
$(call force,CFG_RPMB_FS,n)
# support ree TA anti-rollback
CFG_TA_ANTI_ROLLBACK ?= n

CFG_SCTLR_ALIGNMENT_CHECK := n

# Trust engine options
# Built TE into mbedtls library
# HW TRNG
# HOSAL library
CFG_MBEDTLS_TE ?= n
CFG_TE_TRNG ?= y

ifneq ($(CFG_MBEDTLS_TE)_$(CFG_TE_TRNG),n_n)
CFG_HOSAL := y
CFG_MBEDTLS_SELFTEST ?= n
endif

ifeq ($(CFG_MBEDTLS_TE),y)
CFG_CORE_MBEDTLS_MPI := n

CFG_CRYPTO_AES_GCM_FROM_CRYPTOLIB  ?= n
CFG_CRYPTO_SM4_ECB_FROM_CRYPTOLIB  ?= y
CFG_CRYPTO_SM4_CBC_FROM_CRYPTOLIB  ?= y
CFG_CRYPTO_SM4_CTR_FROM_CRYPTOLIB  ?= y
CFG_CRYPTO_SM3_FROM_CRYPTOLIB      ?= y
CFG_CRYPTO_SM2_PKE_FROM_CRYPTOLIB  ?= y
CFG_CRYPTO_HMAC_SM3_FROM_CRYPTOLIB ?= y

# distinguish with ARM CE mbedtls ALT algo
CFG_CRYPTO_AES_CIX_ENG		?= y
CFG_CRYPTO_SHA1_CIX_ENG	?= y
CFG_CRYPTO_SHA256_CIX_ENG	?= y

# TE driver selection
CFG_TE_DRIVER := $(call cfg-one-enabled,CFG_MBEDTLS_TE   \
                                        CFG_TE_UNIT_TEST \
                                        CFG_TE_TRNG \
                                        CFG_TE_TRNGD)
CFG_CRYPTO_SM4_CCM ?= n
CFG_CRYPTO_SM4_GCM ?= n
else # CFG_MBEDTLS_TE
# TE driver selection
CFG_TE_DRIVER := $(call cfg-one-enabled,CFG_MBEDTLS_TE   \
                                        CFG_TE_UNIT_TEST \
                                        CFG_TE_TRNG \
                                        CFG_TE_TRNGD)
CFG_CORE_MBEDTLS_MPI ?= y
endif # CFG_MBEDTLS_TE
#
# end of Merge from JIAYU driver
#

CFG_POST_CODE_MEM ?= y
CFG_TZDRAM_START ?= 0x80500000
CFG_TZDRAM_SIZE ?= 0x1FFE000

ifeq ($(CFG_CIX_KEY_META_ENABLE),y)
CFG_TEE_KEY_META_SHM ?= 0x824FE000
CFG_TEE_KEY_META_SIZE ?= 0x00001000
endif

CFG_TZ_TF_A_SHM ?= 0x824FF000
CFG_TZ_TF_A_SIZE ?= 0x00001000

CFG_SHMEM_START ?= 0x82500000
CFG_SHMEM_SIZE ?= 0x00B00000
CFG_NUM_THREADS = 32
CFG_FW_STMM_SHMEM_START ?= 0x85F0A000
CFG_FW_STMM_SHMEM_SIZE ?= 0x00001000
CFG_OS_STMM_SHMEM_START ?= 0x85F20000
CFG_OS_STMM_SHMEM_SIZE ?= 0x00008000
$(call force,CFG_TEE_CORE_NB_CORE,12)

ifeq (${BUILD_MODE}, debug)
CONFIG_CIX_DEBUG ?= y
endif

endif

CFG_IN_TREE_EARLY_TAS += avb/023f8f1a-292a-432b-8fc4-de8471358067
