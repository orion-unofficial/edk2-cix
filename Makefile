STMM ?= vendor
ifeq ($(filter $(strip $(STMM)),vendor unofficial),)
$(error STMM supports vendor or unofficial)
endif
ifeq ($(strip $(STMM)),unofficial)
ifneq ($(strip $(CIX_RELEASE)),unofficial)
$(error STMM=unofficial requires CIX_RELEASE=unofficial)
endif
endif

ifeq ($(strip $(STMM)),unofficial)
STMM_FIRMWARE_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
ifeq ($(wildcard $(STMM_FIRMWARE_ROOT)/src/edk2/ArmPkg/Include/IndustryStandard/ArmFfaBootInfo.h),)
$(error STMM=unofficial requires an EDK2 202502-or-newer base; pre-2025 bases are not qualified)
endif
ifneq ($(shell cat "$(STMM_FIRMWARE_ROOT)/VERSION"),1.3.1)
$(error STMM=unofficial currently supports Radxa 1.3.1 only)
endif
ifeq ($(wildcard $(STMM_FIRMWARE_ROOT)/src/cix-v1.2/stmm-unofficial/CixStandaloneMmPkg/CixStandaloneMm.dsc),)
$(error STMM=unofficial requires the rendered stmm-unofficial component)
endif
endif

ifneq ($(strip $(CIX_RELEASE)),)
ifeq ($(filter 1.2 v1.2 V1.2 unofficial,$(strip $(CIX_RELEASE))),)
$(error CIX_RELEASE supports only 1.2 (or v1.2) or unofficial)
endif
ifneq ($(strip $(ARTEFACT_MODE)),)
ifneq ($(ARTEFACT_MODE),custom)
$(error CIX_RELEASE requires ARTEFACT_MODE=custom)
endif
endif
endif

ROOT_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
REPO_ROOT := $(patsubst %/,%,$(dir $(ROOT_MAKEFILE)))
export REPO_ROOT

include $(REPO_ROOT)/.github/common/Makefile
