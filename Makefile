ROOT_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
STMM_FEATURE_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/.)
include $(STMM_FEATURE_ROOT)/src/stmm-integration-v1/selection.mk

REPO_ROOT := $(patsubst %/,%,$(dir $(ROOT_MAKEFILE)))
export REPO_ROOT

include $(REPO_ROOT)/.github/common/Makefile
