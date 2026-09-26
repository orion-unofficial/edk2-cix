# make helper for optee_os

PATH_TAG  := lib/libmbedtls/core/te_driver
TE_SW_TOP := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
TOP_DIR   := $(shell echo $(TE_SW_TOP) | sed "s@/$(PATH_TAG)/.*@@g")

TE_UNIT_TEST := $(CFG_TE_UNIT_TEST)
TE_HWA_DEBUG := $(CFG_TE_HWA_DEBUG)
TE_TEST_ENV  := optee

include $(TE_SW_TOP)/te_common.mk
#-include $(TE_SW_TOP)/hwa/res.mk

srcs-y    += $(patsubst $(TE_SW_TOP)/%,%,$(TE_SOURCES))
# filter out the DLIST and HOSAL search paths from TE_CFLAGS
# for the optee_os has its own configs already.
cflags-lib-y  += $(filter-out -I$(SQLIST_DIR) -I$(HOSAL_API_DIR)%,$(TE_CFLAGS))

global-incdirs-y += $(patsubst $(TE_SW_TOP)/%,%,$(TE_INCLUDES))

TE_CLEAN_FILES  := $(patsubst $(TE_SW_TOP)/%,%,$(TEGENFILES))
cleanfiles += $(TE_CLEAN_FILES)
