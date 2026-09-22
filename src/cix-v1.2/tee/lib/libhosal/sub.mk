# Make helper of hosal for optee_os and ta
# Build options:
# HOSAL_ENV_OPTEE_OS := y
# or
# HOSAL_ENV_OPTEE_TA := y
#

HOSAL_TOP := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

include $(HOSAL_TOP)/hosal_common.mk

global-incdirs-y += apis/hal apis/osal

HAL_OPTEE_SRCS  :=

HAL_TA_SRCS     := $(patsubst $(HOSAL_TOP)/%,%,  \
                   $(wildcard $(HOSAL_TOP)/src/hal/envs/optee_ta/*.c))

OSAL_OPTEE_SRCS := $(patsubst $(HOSAL_TOP)/%,%,  \
                   $(wildcard $(HOSAL_TOP)/src/osal/envs/optee_os/*.c))

OSAL_TA_SRCS    := $(patsubst $(HOSAL_TOP)/%,%,         \
                   $(wildcard $(HOSAL_TOP)/src/osal/envs/optee_ta/*.c))

srcs-y += $(HAL_COMM_SRCS) $(OSAL_COMM_SRCS)

srcs-$(HOSAL_ENV_OPTEE_OS) += $(HAL_OPTEE_SRCS) $(OSAL_OPTEE_SRCS)

srcs-$(HOSAL_ENV_OPTEE_TA) += $(HAL_TA_SRCS) $(OSAL_TA_SRCS)

incdirs-y += utils/inc misc/sqlist

cflags-lib-$(HOSAL_ENV_OPTEE_OS) += -DOPTEE_TEEX_PTA                            \
                                    -DOSAL_ENV_OPTEE_OS                         \
                                    -DOSAL_MAX_LOG_LEVEL=$(CFG_TEE_CORE_LOG_LEVEL) \
                                    -DOSAL_ASSERT_BREAK=assert\(0\)             \
                                    -include assert.h

cflags-lib-$(HOSAL_ENV_OPTEE_TA) += -DOSAL_ENV_OPTEE_TA                         \
                                    -DOSAL_MAX_LOG_LEVEL=$(CFG_TEE_TA_LOG_LEVEL)\
                                    -DOSAL_COMPILE_ASSERT=COMPILE_TIME_ASSERT   \
                                    -DOSAL_ASSERT_BREAK=assert\(0\)             \
                                    -include assert.h
