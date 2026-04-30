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

define PRINT_HELP_SHELL_PROLOGUE
	set -eu; \
	help_pad=31; \
	help_gap=2; \
	help_width=80; \
	desc_width=$$((help_width - help_pad - help_gap)); \
	desc_indent=$$(printf '%*s' "$$((2 + help_pad + help_gap))" ''); \
	print_help_line() { \
		label="$$1"; \
		desc="$$2"; \
		if [ "$${#label}" -le "$$help_pad" ]; then \
			first_indent=$$(printf '  %-*s%*s' "$$help_pad" "$$label" "$$help_gap" ''); \
			printf '%b\n' "$$desc" | fold -s -w "$$desc_width" | \
				awk -v first="$$first_indent" -v cont="$$desc_indent" \
					'NR == 1 { print first $$0; next } { sub(/^ +/, "", $$0); print cont $$0 }'; \
		else \
			printf '  %s\n' "$$label"; \
			printf '%b\n' "$$desc" | fold -s -w "$$desc_width" | \
				awk -v cont="$$desc_indent" \
					'{ sub(/^ +/, "", $$0); print cont $$0 }'; \
		fi; \
	}; \
	print_section() { \
		printf '\n%s\n\n' "$$1"; \
	}
endef

.PHONY: help help-vars help-dev help-releases all build-all install zip targz buildbox-firmware-build buildbox-firmware-stage \
	extract-vendor-delta render-release-branch integrate-source-release import-local-commits \
	verify-release-branch verify-build-matrix check-identity-hygiene \
	extract-vendor-delta-help render-release-branch-help integrate-source-release-help \
	import-local-commits-help verify-release-branch-help verify-build-matrix-help

all: help

help:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	printf '%s\n' 'edk2-cix firmware build targets'; \
	print_section 'Common Targets'; \
	print_help_line 'make help' 'Show this help.'; \
	print_help_line 'make help-vars' 'Show common build variables.'; \
	print_help_line 'make help-releases' 'List configured firmware releases.'; \
	print_help_line 'make build-all' 'Build the latest configured firmware release.'; \
	print_help_line 'make install' 'Build, safety-check, and install firmware.'; \
	print_help_line 'make zip' 'Create a firmware .zip via the buildbox.'; \
	print_help_line 'make targz' 'Create a firmware .tar.gz via the buildbox.'; \
	print_section 'Buildbox Targets'; \
	print_help_line 'make buildbox-firmware-build' 'Delegate firmware build to the selected buildbox.'; \
	print_help_line 'make buildbox-firmware-stage' 'Delegate firmware staging to the selected buildbox.'; \
	print_section 'More Help'; \
	print_help_line 'make help-dev' 'Show source-update and developer tooling targets.'; \
	print_help_line 'make <target>-help' 'Show arguments for a developer tooling target.'

help-vars:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	print_section 'Common Build Variables'; \
	print_help_line 'RELEASE=<release>' 'Select a configured firmware release.\nAccepts short names or full source/release/... branch names.\nDefault: config/releases.json:default_release.'; \
	print_help_line 'V=0|1' 'Verbosity. V=0 is concise; V=1 shows script/build detail.\nDefault: 0.'; \
	print_help_line 'SIGNING_CERT_SOURCE_DIR=<path>' 'Copy exact-replay signing certs into the build worktree.\nDefault: unset.'; \
	print_section 'Install Variables'; \
	print_help_line 'INSTALL_ROOT=<path>' 'Firmware install root.\nDefault: /boot/efi.'; \
	print_help_line 'INSTALL_SOURCE=<path>' 'Optional staged payload path, or path relative to dist/firmware.\nDefault: latest staged firmware payload.'; \
	print_help_line 'FORCE=1' 'Allow make install to replace existing firmware payload files beneath INSTALL_ROOT after the pre-install safety checks pass.\nDefault: 0.'; \
	print_section 'Release Selection'; \
	print_help_line 'make help-releases' 'List configured release names generated from config/releases.json.'; \
	print_help_line 'PERSIST=1' 'For render-release-branch: create or verify a named source/release branch. Without PERSIST=1, build targets use existing refs or cached detached worktrees and do not create a source/release branch.\nDefault: 0.'; \
	printf '\n%s\n' 'For source-update and maintainer variables, run: make help-dev'

help-dev:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	print_section 'Source Update and Developer Targets'; \
	print_help_line 'make render-release-branch' 'Resolve or create a materialised source/release branch.'; \
	print_help_line 'make verify-release-branch' 'Validate a materialised release branch.'; \
	print_help_line 'make verify-build-matrix' 'Validate configured build/source combinations.'; \
	print_help_line 'make extract-vendor-delta' 'Produce a vendor delta report/diff.'; \
	print_help_line 'make integrate-source-release' 'Integrate new upstream/vendor source refs.'; \
	print_help_line 'make import-local-commits' 'Update source/unofficial/current and/or local delta artefacts.'; \
	print_help_line 'make check-identity-hygiene' 'Scan generated files and refs for path/identity leaks.'; \
	print_section 'Per-target Help'; \
	print_help_line 'make render-release-branch-help' 'Show render-release-branch arguments.'; \
	print_help_line 'make integrate-source-release-help' 'Show integrate-source-release arguments.'; \
	print_help_line 'make import-local-commits-help' 'Show import-local-commits arguments.'; \
	print_help_line 'make extract-vendor-delta-help' 'Show extract-vendor-delta arguments.'; \
	print_help_line 'make verify-release-branch-help' 'Show verify-release-branch arguments.'; \
	print_help_line 'make verify-build-matrix-help' 'Show verify-build-matrix arguments.'; \
	print_section 'Source Integration Variables'; \
	print_help_line 'TYPE=upstream|vendor' 'For integrate-source-release. upstream updates base component refs; vendor updates Radxa or CIX-carried source layers.'; \
	print_help_line 'COMPONENT=<name>' 'For TYPE=upstream: edk2, edk2-platforms, edk2-non-osi, tf-a, or op-tee.'; \
	print_help_line 'VENDOR=radxa|cix' 'For TYPE=vendor. Radxa updates the EDK2 vendor layer; CIX updates the TF-A/OP-TEE release bundle.'; \
	print_help_line 'RELEASE=<name>' 'Release, tag, or configured source/release selection.'; \
	print_help_line 'REF=<ref>' 'Input ref/object for source integration.'; \
	print_help_line 'EDK2_BASE=<release>' 'EDK2 base used when integrating Radxa vendor sources.'; \
	print_section 'Ref Update Variables'; \
	print_help_line 'WRITE=1' 'Permit ref creation/advancement in integrate-source-release, import-local-commits, and extract-vendor-delta.'; \
	print_help_line 'ALLOW_REPLACE=1' 'Allow integrate-source-release to replace an existing manifested source ref deliberately.'; \
	print_help_line 'MATERIALISE=0|1' 'Flatten Radxa vendor refs before extracting deltas.\nDefault: 1.'; \
	print_help_line 'BASE_REF=<ref>' 'Base ref for delta extraction/import.'; \
	print_help_line 'TARGET_REF=<ref>' 'Delta artefact output ref.'; \
	print_help_line 'FROM_REF=<ref>' 'Local topic branch/ref for import-local-commits.'; \
	print_help_line 'SOURCE_LOCAL_REF=<ref>' 'Local source branch; defaults to source/unofficial/current.'; \
	print_help_line 'UPDATE_LOCAL_SOURCE=1' 'Allow import-local-commits to advance SOURCE_LOCAL_REF.'; \
	print_help_line 'LOCAL_TARGET_REF=<ref>' 'Local delta target; defaults to source/delta/local/current.'; \
	print_help_line 'SCAN_SOURCE_REFS=1' 'Also scan generated source refs in check-identity-hygiene.'; \
	print_section 'Release Matrix'; \
	print_help_line 'make help-releases' 'Show configured releases.'; \
	print_help_line 'make verify-build-matrix' 'Check release metadata, refs, and build-policy coverage.'

help-releases:
	@$(PYTHON) scripts/list_configured_releases.py

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

verify-build-matrix:
	@V="$(V)" $(PYTHON) scripts/verify_build_matrix.py --v "$(V)"

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

verify-build-matrix-help:
	@$(PYTHON) scripts/verify_build_matrix.py --help

extract-vendor-delta-help:
	@$(PYTHON) scripts/extract_vendor_delta.py --help

integrate-source-release-help:
	@$(PYTHON) scripts/integrate_source_release.py --help

import-local-commits-help:
	@$(PYTHON) scripts/import_local_commits.py --help
