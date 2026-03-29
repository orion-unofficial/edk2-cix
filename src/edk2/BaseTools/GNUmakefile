## @file
# GNU/Linux makefile for Base Tools project build.
#
# Copyright (c) 2009 - 2017, Intel Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#

MAKEFLAGS += --no-print-directory
V ?= 0

ifeq ($(V),1)
Q :=
define PRINT_STATUS
@:
endef
else
Q := @
define PRINT_STATUS
@printf '  %-7s %s\n' '$(1)' '$(2)'
endef
endif

all: subdirs

LANGUAGES = C Python

SOURCE_SUBDIRS := $(patsubst %,Source/%,$(sort $(LANGUAGES)))
SUBDIRS := $(SOURCE_SUBDIRS) Tests
CLEAN_SUBDIRS := $(patsubst %,%-clean,$(sort $(SUBDIRS)))

.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS):
	$(call PRINT_STATUS,MAKE,$@)
	$(Q)$(MAKE) -C $@

Tests: $(SOURCE_SUBDIRS)

.PHONY: $(CLEAN_SUBDIRS)
$(CLEAN_SUBDIRS):
	$(call PRINT_STATUS,CLEAN,$(@:-clean=))
	$(Q)-$(MAKE) -C $(@:-clean=) clean

clean:  $(CLEAN_SUBDIRS)
