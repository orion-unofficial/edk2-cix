SHELL := /bin/sh
.DEFAULT_GOAL := docs-build

.PHONY: docs-build docs-clean

REPO_ROOT := $(abspath ..)
DOCS_CACHE_ROOT ?= $(REPO_ROOT)/.cache/edk2-cix/docs
DOCS_BOOK_DIR ?= $(DOCS_CACHE_ROOT)/book
DOCS_BOOK_HTML ?= $(DOCS_BOOK_DIR)/html
DOCS_TOOLS_DIR ?= $(DOCS_CACHE_ROOT)/tools
export EDK2_CIX_DOCS_TOOLS_DIR ?= $(DOCS_TOOLS_DIR)
PYTHONPYCACHEPREFIX ?= $(DOCS_CACHE_ROOT)/pycache
export PYTHONPYCACHEPREFIX

docs-build:
	mdbook build --dest-dir "$(DOCS_BOOK_DIR)"
	python3 scripts/validate_docs_artifacts.py "$(DOCS_BOOK_HTML)"

docs-clean:
	rm -rf "$(DOCS_CACHE_ROOT)"
