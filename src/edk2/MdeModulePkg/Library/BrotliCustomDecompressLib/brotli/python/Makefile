# Copyright 2016 The Brotli Authors. All rights reserved.
#
# Distributed under MIT license.
# See file LICENSE for detail or copy at https://opensource.org/licenses/MIT


# Default (Build)
.PHONY: all
# Build
.PHONY: build
# Develop
.PHONY: develop
# Install
.PHONY: install
# Test
.PHONY: test
# Clean
.PHONY: clean
# Format
.PHONY: fix


PYTHON ?= python
YAPF ?= yapf
V ?= 0

ifeq ($(V),1)
Q :=
else
Q := @
endif

EXT_SUFFIX=$(shell $(PYTHON) -c 'import sysconfig; print(sysconfig.get_config_var("EXT_SUFFIX"))')
EXT_SOURCES=$(shell find . -name '*.cc')
EXTENSIONS=$(EXT_SOURCES:%.cc=%$(EXT_SUFFIX))


all: build

build: $(EXTENSIONS)

$(EXTENSIONS): $(EXT_SOURCES)
	@cd .. && $(PYTHON) setup.py build_ext --inplace

develop:
	@cd .. && $(PYTHON) setup.py develop

install:
	@cd .. && $(PYTHON) setup.py install

test: build
	@echo 'running tests'
	@$(PYTHON) -m unittest discover -v -p '*_test.py'

clean:
	$(Q)cd .. && $(PYTHON) setup.py clean
	$(Q)find .. -name '*.pyc' -exec rm -f {} +
	$(Q)find .. -name '*.so' -exec rm -f {} +
	$(Q)find .. -type d -name '__pycache__' -exec rm -rf {} +
	$(Q)find .. -type d -name '*.egg-info' -exec rm -rf {} +

fix:
	@echo 'formatting code'
	-@$(YAPF) --in-place --recursive --verify .
