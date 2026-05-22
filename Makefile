ROOT_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
REPO_ROOT := $(patsubst %/,%,$(dir $(ROOT_MAKEFILE)))
export REPO_ROOT

include $(REPO_ROOT)/.github/common/Makefile
