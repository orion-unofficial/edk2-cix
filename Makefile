SHELL := /bin/sh
.DEFAULT_GOAL := help

PYTHON ?= python3
V ?= 0
RELEASE ?=
PERSIST ?= 0
WORKTREE ?=
TARGET_REF ?= source/delta/local/current

.PHONY: help help-vars all build-all install zip targz buildbox-firmware-build buildbox-firmware-stage \
	extract-vendor-delta render-release-branch integrate-source-release import-local-commits \
	verify-release-branch check-identity-hygiene \
	extract-vendor-delta-help render-release-branch-help integrate-source-release-help \
	import-local-commits-help verify-release-branch-help

all: help

help:
	@printf '%s\n' 'EDK2-CIX reconstruction control branch'
	@printf '%s\n' ''
	@printf '%s\n' 'End-user targets:'
	@printf '%s\n' '  make help                         Show this help.'
	@printf '%s\n' '  make help-vars                    Show variables and configured releases.'
	@printf '%s\n' '  make build-all                    Build the latest configured firmware release.'
	@printf '%s\n' '  make install                      Delegate install to the rendered release worktree.'
	@printf '%s\n' '  make zip                          Delegate zip packaging to the rendered release worktree.'
	@printf '%s\n' '  make targz                        Delegate targz packaging to the rendered release worktree.'
	@printf '%s\n' '  make buildbox-firmware-build      Delegate buildbox firmware build.'
	@printf '%s\n' '  make buildbox-firmware-stage      Delegate buildbox firmware stage.'
	@printf '%s\n' ''
	@printf '%s\n' 'Tooling targets:'
	@printf '%s\n' '  make render-release-branch        Resolve or create a materialized source/release branch.'
	@printf '%s\n' '  make verify-release-branch        Validate rendered-branch invariants.'
	@printf '%s\n' '  make extract-vendor-delta         Produce a vendor delta report/diff.'
	@printf '%s\n' '  make integrate-source-release     Integrate new upstream/vendor source refs.'
	@printf '%s\n' '  make import-local-commits         Explicitly update source/delta/local/current.'
	@printf '%s\n' '  make check-identity-hygiene       Scan generated control files for path/identity leaks.'
	@printf '%s\n' ''
	@printf '%s\n' 'Per-target help:'
	@printf '%s\n' '  make render-release-branch-help'
	@printf '%s\n' '  make integrate-source-release-help'
	@printf '%s\n' '  make import-local-commits-help'
	@printf '%s\n' '  make extract-vendor-delta-help'
	@printf '%s\n' '  make verify-release-branch-help'

help-vars:
	@printf '%s\n' 'Common variables:'
	@printf '%s\n' '  V=0|1                 Verbosity. Defaults to V=0 and propagates to scripts/builds.'
	@printf '%s\n' '  RELEASE=<release>     Short name or full source/release/... branch.'
	@printf '%s\n' '  PERSIST=1             Required to create a persistent rendered release branch.'
	@printf '%s\n' '  TYPE=upstream|vendor  Source integration mode.'
	@printf '%s\n' '  COMPONENT=<name>      Upstream component: edk2, edk2-platforms, edk2-non-osi, tf-a, op-tee.'
	@printf '%s\n' '  VENDOR=radxa|cix      Vendor integration target.'
	@printf '%s\n' '  WRITE=1               Required for targets that create or advance refs.'
	@printf '%s\n' '  TARGET_REF=<ref>      Local import target; defaults to source/delta/local/current.'
	@printf '%s\n' ''
	@printf '%s\n' 'Configured releases:'
	@$(PYTHON) -c 'import json; d=json.load(open("config/releases.json")); print("  default: " + d.get("default_release", "")); [print("  " + (k.removeprefix("source/release/"))) for k in sorted(d.get("releases", {}))]'

define run_release_make
	@wt="$$(RELEASE="$(RELEASE)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --ensure-worktree --print-worktree --v "$(V)")"; \
	if [ "$(V)" = "1" ]; then printf '%s\n' "$(MAKE) -C $$wt $(1) V=$(V)"; fi; \
	$(MAKE) -C "$$wt" $(1) V="$(V)"
endef

build-all:
	$(call run_release_make,build-all)

install:
	$(call run_release_make,install)

zip:
	$(call run_release_make,zip)

targz:
	$(call run_release_make,targz)

buildbox-firmware-build:
	$(call run_release_make,buildbox-firmware-build)

buildbox-firmware-stage:
	$(call run_release_make,buildbox-firmware-stage)

render-release-branch:
	@if [ -z "$(RELEASE)" ]; then $(MAKE) --no-print-directory render-release-branch-help; printf '%s\n' 'missing required variable: RELEASE' >&2; exit 2; fi
	@RELEASE="$(RELEASE)" PERSIST="$(PERSIST)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --require-release --persist "$(PERSIST)" --v "$(V)"

verify-release-branch:
	@if [ -z "$(RELEASE)" ]; then $(MAKE) --no-print-directory verify-release-branch-help; printf '%s\n' 'missing required variable: RELEASE' >&2; exit 2; fi
	@RELEASE="$(RELEASE)" WORKTREE="$(WORKTREE)" V="$(V)" $(PYTHON) scripts/verify_release_branch.py --v "$(V)"

extract-vendor-delta:
	@VENDOR="$(VENDOR)" BASE_REF="$(BASE_REF)" VENDOR_REF="$(VENDOR_REF)" OUTPUT="$(OUTPUT)" PATCH_OUTPUT="$(PATCH_OUTPUT)" V="$(V)" $(PYTHON) scripts/extract_vendor_delta.py --v "$(V)"

integrate-source-release:
	@TYPE="$(TYPE)" COMPONENT="$(COMPONENT)" VENDOR="$(VENDOR)" RELEASE="$(RELEASE)" EDK2_BASE="$(EDK2_BASE)" REF="$(REF)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/integrate_source_release.py --v "$(V)"

import-local-commits:
	@FROM_REF="$(FROM_REF)" TARGET_REF="$(TARGET_REF)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/import_local_commits.py --v "$(V)"

check-identity-hygiene:
	@SCAN_COMMITS="$(SCAN_COMMITS)" V="$(V)" $(PYTHON) scripts/check_identity_hygiene.py --v "$(V)"

render-release-branch-help:
	@$(PYTHON) scripts/render_release_branch.py --help

verify-release-branch-help:
	@$(PYTHON) scripts/verify_release_branch.py --help

extract-vendor-delta-help:
	@$(PYTHON) scripts/extract_vendor_delta.py --help

integrate-source-release-help:
	@$(PYTHON) scripts/integrate_source_release.py --help

import-local-commits-help:
	@$(PYTHON) scripts/import_local_commits.py --help
