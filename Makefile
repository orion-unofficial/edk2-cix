# Source-built trusted firmware lacks a vendor-authorized signing key.
ifneq ($(strip $(CIX_RELEASE)),)
$(error CIX_RELEASE must be empty: source-built TF-A/OP-TEE cannot be signed with a vendor-trusted key. Leave it unset or use CIX_RELEASE=)
endif

ROOT_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
REPO_ROOT := $(patsubst %/,%,$(dir $(ROOT_MAKEFILE)))
export REPO_ROOT

include $(REPO_ROOT)/.github/common/Makefile
