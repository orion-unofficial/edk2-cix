STMM ?= vendor
ifeq ($(filter $(strip $(STMM)),vendor unofficial),)
$(error STMM supports vendor or unofficial)
endif
ifeq ($(strip $(STMM)),unofficial)
ifneq ($(strip $(CIX_RELEASE)),unofficial)
$(error STMM=unofficial requires CIX_RELEASE=unofficial)
endif
ifneq ($(or $(FIRMWARE_BOARD),O6):$(or $(FIRMWARE_TARGET),RELEASE),O6:RELEASE)
$(error STMM=unofficial supports O6 RELEASE only)
endif
endif
ifneq ($(strip $(CIX_RELEASE)),)
ifeq ($(filter 1.2 v1.2 V1.2 unofficial,$(strip $(CIX_RELEASE))),)
$(error CIX_RELEASE supports 1.2 or unofficial)
endif
ifneq ($(or $(ARTEFACT_MODE),custom),custom)
$(error CIX_RELEASE requires ARTEFACT_MODE=custom)
endif
endif

STMM_FEATURE_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/.)
ifeq ($(strip $(STMM)),unofficial)
ifeq ($(wildcard $(STMM_FEATURE_ROOT)/src/cix-v1.2/stmm-unofficial/ArmPkg/Include/IndustryStandard/ArmFfaBootInfo.h)$(wildcard $(STMM_FEATURE_ROOT)/src/cix-v1.2/stmm-unofficial/MdePkg/Include/IndustryStandard/ArmFfaBootInfo.h),)
$(error STMM=unofficial requires the rendered modern secure-world component with FF-A boot information)
endif
endif

ROOT_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
REPO_ROOT := $(patsubst %/,%,$(dir $(ROOT_MAKEFILE)))
export REPO_ROOT

include $(REPO_ROOT)/.github/common/Makefile
