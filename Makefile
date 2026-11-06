STMM ?= vendor
ifneq ($(strip $(STMM)),vendor)
ifeq ($(strip $(STMM)),unofficial)
$(error STMM=unofficial is not available: the reconstructed payload is not yet qualified for any EDK2 base, including pre-2025 releases)
else
$(error STMM supports vendor or unofficial; unofficial is not yet available)
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
