ifneq ($(strip $(CIX_RELEASE)),)
ifeq ($(filter 1.2 v1.2 V1.2,$(strip $(CIX_RELEASE))),)
$(error CIX_RELEASE supports only 1.2 (or v1.2))
endif
ifneq ($(strip $(ARTEFACT_MODE)),)
ifneq ($(ARTEFACT_MODE),custom)
$(error CIX_RELEASE=1.2 requires ARTEFACT_MODE=custom)
endif
endif
endif

ROOT_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
REPO_ROOT := $(patsubst %/,%,$(dir $(ROOT_MAKEFILE)))
export REPO_ROOT

include $(REPO_ROOT)/.github/common/Makefile
