SHELL := /bin/sh
.DEFAULT_GOAL := help

PYTHON ?= python3
V ?= 0
RELEASE ?=
PERSIST ?= 0
WORKTREE ?=
TARGET_REF ?=
LOCAL_TARGET_REF ?= source/delta/local/current
SOURCE_LOCAL_REF ?= source/unofficial/current
UPDATE_LOCAL_SOURCE ?= 0
BASE_REF ?=
INSTALL_ROOT ?= /boot/efi
INSTALL_SOURCE ?=
FORCE ?= 0

.PHONY: help help-vars all build-all install zip targz buildbox-firmware-build buildbox-firmware-stage \
	extract-vendor-delta render-release-branch integrate-source-release import-local-commits \
	verify-release-branch check-identity-hygiene \
	extract-vendor-delta-help render-release-branch-help integrate-source-release-help \
	import-local-commits-help verify-release-branch-help

all: help

help:
	@printf '%s\n' 'EDK2-CIX reconstruction build branch'
	@printf '%s\n' ''
	@printf '%s\n' 'End-user targets:'
	@printf '%s\n' '  make help                         Show this help.'
	@printf '%s\n' '  make help-vars                    Show variables and configured releases.'
	@printf '%s\n' '  make build-all                    Build the latest configured firmware release.'
	@printf '%s\n' '  make install                      Build, safety-check, and install firmware.'
	@printf '%s\n' '  make zip                          Create a firmware .zip via the buildbox.'
	@printf '%s\n' '  make targz                        Create a firmware .tar.gz via the buildbox.'
	@printf '%s\n' '  make buildbox-firmware-build      Delegate buildbox firmware build.'
	@printf '%s\n' '  make buildbox-firmware-stage      Delegate buildbox firmware stage.'
	@printf '%s\n' ''
	@printf '%s\n' 'Tooling targets:'
	@printf '%s\n' '  make render-release-branch        Resolve or create a materialised source/release branch.'
	@printf '%s\n' '  make verify-release-branch        Validate rendered-branch invariants.'
	@printf '%s\n' '  make extract-vendor-delta         Produce a vendor delta report/diff.'
	@printf '%s\n' '  make integrate-source-release     Integrate new upstream/vendor source refs.'
	@printf '%s\n' '  make import-local-commits         Explicitly update source/unofficial/current and/or local delta artefacts.'
	@printf '%s\n' '  make check-identity-hygiene       Scan generated reconstruction files for path/identity leaks.'
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
	@printf '%s\n' '  ALLOW_REPLACE=1       Allow integrate-source-release to move an existing immutable ref.'
	@printf '%s\n' '  MATERIALISE=1         Flatten Radxa vendor refs before extracting deltas. Defaults to 1.'
	@printf '%s\n' '  TARGET_REF=<ref>      Delta artefact output ref.'
	@printf '%s\n' '  SOURCE_LOCAL_REF=<ref> Local source branch; defaults to source/unofficial/current.'
	@printf '%s\n' '  UPDATE_LOCAL_SOURCE=1  Allow import-local-commits to advance SOURCE_LOCAL_REF.'
	@printf '%s\n' '  LOCAL_TARGET_REF=<ref> Local delta target; defaults to source/delta/local/current.'
	@printf '%s\n' '  BASE_REF=<ref>        Base ref for delta extraction/import.'
	@printf '%s\n' '  INSTALL_ROOT=<path>   Firmware install root. Defaults to /boot/efi.'
	@printf '%s\n' '  INSTALL_SOURCE=<path> Optional staged payload path or path relative to dist/firmware.'
	@printf '%s\n' '  SIGNING_CERT_SOURCE_DIR=<path> Copy exact-replay signing certs into the rendered worktree before building.'
	@printf '%s\n' '  SCAN_SOURCE_REFS=1   Also scan generated source refs in check-identity-hygiene.'
	@printf '%s\n' '  FORCE=1               Allow install to replace existing destination files.'
	@printf '%s\n' ''
	@printf '%s\n' 'Configured releases:'
	@$(PYTHON) -c 'import json; d=json.load(open("config/releases.json")); print("  default: " + d.get("default_release", "")); [print("  " + (k.removeprefix("source/release/"))) for k in sorted(d.get("releases", {}))]'

define run_release_make
	@wt="$$(RELEASE="$(RELEASE)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --ensure-worktree --print-worktree --v "$(V)")"; \
	signing_cert_arg="$$(SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" V="$(V)" $(PYTHON) scripts/prepare_release_worktree.py --worktree "$$wt" --print-make-arg --v "$(V)")"; \
	if [ "$(V)" = "1" ]; then printf '%s\n' "$(MAKE) --no-print-directory -C $$wt $(1) V=$(V)"; fi; \
	$(MAKE) --no-print-directory -C "$$wt" $(1) V="$(V)" $$signing_cert_arg
endef

build-all:
	$(call run_release_make,build-all)

install:
	@wt="$$(RELEASE="$(RELEASE)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --ensure-worktree --print-worktree --v "$(V)")"; \
	signing_cert_arg="$$(SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" V="$(V)" $(PYTHON) scripts/prepare_release_worktree.py --worktree "$$wt" --print-make-arg --v "$(V)")"; \
	if [ "$(V)" = "1" ]; then printf '%s\n' "$(MAKE) --no-print-directory -C $$wt buildbox-firmware-stage V=$(V)"; fi; \
	$(MAKE) --no-print-directory -C "$$wt" buildbox-firmware-stage V="$(V)" $$signing_cert_arg; \
	force_arg=""; \
	if [ "$(FORCE)" = "1" ]; then force_arg="--force"; fi; \
	INSTALL_SOURCE="$(INSTALL_SOURCE)" FORCE="$(FORCE)" V="$(V)" $(PYTHON) scripts/install_release_payload.py \
		--worktree "$$wt" \
		--install-root "$(INSTALL_ROOT)" \
		$$force_arg \
		--v "$(V)"

zip:
	$(call run_release_make,buildbox-zip)

targz:
	$(call run_release_make,buildbox-targz)

buildbox-firmware-build:
	$(call run_release_make,buildbox-firmware-build)

buildbox-firmware-stage:
	$(call run_release_make,buildbox-firmware-stage)

render-release-branch:
	@if [ -z "$(RELEASE)" ]; then $(MAKE) --no-print-directory render-release-branch-help; printf '%s\n' 'missing required variable: RELEASE' >&2; exit 2; fi
	@RELEASE="$(RELEASE)" PERSIST="$(PERSIST)" REBUILD="$(REBUILD)" FORCE="$(FORCE)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --require-release --persist "$(PERSIST)" --rebuild "$(REBUILD)" --force "$(FORCE)" --v "$(V)"

verify-release-branch:
	@if [ -z "$(RELEASE)" ]; then $(MAKE) --no-print-directory verify-release-branch-help; printf '%s\n' 'missing required variable: RELEASE' >&2; exit 2; fi
	@RELEASE="$(RELEASE)" WORKTREE="$(WORKTREE)" V="$(V)" $(PYTHON) scripts/verify_release_branch.py --v "$(V)"

extract-vendor-delta:
	@VENDOR="$(VENDOR)" BASE_REF="$(BASE_REF)" VENDOR_REF="$(VENDOR_REF)" OUTPUT="$(OUTPUT)" PATCH_OUTPUT="$(PATCH_OUTPUT)" TARGET_REF="$(TARGET_REF)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/extract_vendor_delta.py --v "$(V)"

integrate-source-release:
	@TYPE="$(TYPE)" COMPONENT="$(COMPONENT)" VENDOR="$(VENDOR)" RELEASE="$(RELEASE)" EDK2_BASE="$(EDK2_BASE)" REF="$(REF)" WRITE="$(WRITE)" ALLOW_REPLACE="$(ALLOW_REPLACE)" MATERIALISE="$(MATERIALISE)" V="$(V)" $(PYTHON) scripts/integrate_source_release.py --v "$(V)"

import-local-commits:
	@FROM_REF="$(FROM_REF)" BASE_REF="$(BASE_REF)" SOURCE_LOCAL_REF="$(SOURCE_LOCAL_REF)" UPDATE_LOCAL_SOURCE="$(UPDATE_LOCAL_SOURCE)" TARGET_REF="$(if $(TARGET_REF),$(TARGET_REF),$(LOCAL_TARGET_REF))" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/import_local_commits.py --v "$(V)"

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
