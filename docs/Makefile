SHELL := /bin/sh
.DEFAULT_GOAL := docs-build

.PHONY: docs-build docs-clean

docs-build:
	mdbook build
	python3 scripts/validate_docs_artifacts.py book/html

docs-clean:
	rm -rf book .docs-tools
