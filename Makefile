SHELL := /bin/sh
.DEFAULT_GOAL := help

PYTHON ?= python3
V ?= 0
DEBUG ?= 0
RELEASE ?=
PERSIST ?= 0
WORKTREE ?=
TARGET_REF ?=
SOURCE_UNOFFICIAL_REF ?= source/unofficial/current
BASE_REF ?=
INSTALL_ROOT ?= /boot/efi
INSTALL_SOURCE ?=
FORCE ?= 0
DELETE ?= 0
REPACK ?= 1
KEEP ?= 0
ARTEFACT_MODE ?= custom
FIRMWARE_BOARD ?= O6
FIRMWARE_TARGET ?= RELEASE
FIRMWARE_DISTRO ?=
FIRMWARE_VALIDATE_ON_BUILD ?= 0
BUILDBOX_PLATFORM ?=
ENABLE_FIRMWARE_FIXES ?=
ENABLE_CORE_ORDER ?=
ENABLE_EXPERIMENTAL_UEFI_SETTINGS ?=
DEBUG_ON_UART3 ?=
UART3_ENABLE ?=
DEBUG_VERBOSE ?=
DEBUG_PRINT_ERROR_LEVEL ?=
CIX_RELEASE ?=
QUALITY_IMAGE ?= edk2-cix-build-quality:latest
UPSTREAM_VERSION_MODE ?= policy
UPSTREAM_VERSION_ONLY ?=
UPSTREAM_VERSION_FORMAT ?= text
UPSTREAM_VERSION_SNAPSHOT ?=
ACT_WORKFLOW ?=
ACT_EVENT ?= workflow_dispatch
ACT_JOB ?=
ACT_MATRIX ?=
ACT_SECRET_FILE ?=
ACT_EXTRA_ARGS ?=
ACT_CONTAINER_ARCH ?= linux/amd64
ACT_RUNNER_IMAGE ?= catthehacker/ubuntu:act-latest

define PRINT_HELP_SHELL_PROLOGUE
	set -eu; \
		help_pad=31; \
		nested_help_pad=29; \
		help_gap=2; \
		help_width=80; \
		desc_width=$$((help_width - 2 - help_pad - help_gap)); \
		desc_indent=$$(printf '%*s' "$$((2 + help_pad + help_gap))" ''); \
		nested_desc_indent=$$(printf '%*s' "$$((4 + nested_help_pad + help_gap))" ''); \
		print_help_line() { \
			label="$$1"; \
			desc="$$2"; \
		if [ "$${#label}" -le "$$help_pad" ]; then \
			first_indent=$$(printf '  %-*s%*s' "$$help_pad" "$$label" "$$help_gap" ''); \
			printf '%b\n' "$$desc" | fold -s -w "$$desc_width" | \
				awk -v first="$$first_indent" -v cont="$$desc_indent" \
					'{ sub(/ +$$/, "", $$0) } NR == 1 { print first $$0; next } { sub(/^ +/, "", $$0); print cont $$0 }'; \
		else \
			printf '  %s\n' "$$label"; \
				printf '%b\n' "$$desc" | fold -s -w "$$desc_width" | \
					awk -v cont="$$desc_indent" \
						'{ sub(/^ +/, "", $$0); sub(/ +$$/, "", $$0); print cont $$0 }'; \
			fi; \
		}; \
		print_help_variable() { \
			label="$$1"; \
			desc="$$2"; \
			if [ "$${#label}" -le "$$nested_help_pad" ]; then \
				first_indent=$$(printf '    %-*s%*s' "$$nested_help_pad" "$$label" "$$help_gap" ''); \
				printf '%b\n' "$$desc" | fold -s -w "$$desc_width" | \
					awk -v first="$$first_indent" -v cont="$$nested_desc_indent" \
						'{ sub(/ +$$/, "", $$0) } NR == 1 { print first $$0; next } { sub(/^ +/, "", $$0); print cont $$0 }'; \
			else \
				printf '    %s\n' "$$label"; \
				printf '%b\n' "$$desc" | fold -s -w "$$desc_width" | \
					awk -v cont="$$nested_desc_indent" \
						'{ sub(/^ +/, "", $$0); sub(/ +$$/, "", $$0); print cont $$0 }'; \
			fi; \
		}; \
		print_help_note() { \
			printf '%b\n' "$$1" | fold -s -w "$$desc_width" | \
				awk -v cont="$$desc_indent" \
					'{ sub(/^ +/, "", $$0); sub(/ +$$/, "", $$0); print cont $$0 }'; \
		}; \
		print_subtitle() { \
			printf '\n  %s\n\n' "$$1"; \
		}; \
		print_section() { \
			printf '\n%s\n\n' "$$1"; \
		}
endef

.PHONY: help help-vars help-dev help-source-targets build build-all install zip targz clean realclean prune buildbox-firmware-build buildbox-firmware-stage docs-build docs-workflow-local \
	test lint \
	extract-vendor-delta render-release-branch integrate-source-release import-unofficial-commits \
	verify-release-branch verify-build-matrix verify-manifest-integrity check-ref-integrity verify-minimised-clone check-identity-integrity verify-identity-integrity check-vendor-workflow-drift check-upstream-versions check-help-cache refresh-help-cache ref-report cleanup-report create-minimised-clone \
	gha-act-list gha-act-dry-run gha-act-run \
	extract-vendor-delta-help render-release-branch-help integrate-source-release-help \
	import-unofficial-commits-help verify-release-branch-help verify-build-matrix-help check-vendor-workflow-drift-help check-upstream-versions-help

help:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	printf '%s\n' 'edk2-cix firmware build targets'; \
	print_section 'Build Targets'; \
	print_help_line 'make build' 'Build one firmware image in buildbox for the selected board and source target.'; \
	print_help_line 'make install' 'Build one firmware payload in buildbox, safety-check it on the local host, then install it for the selected board and source target.'; \
	print_help_line 'make build-all' 'Build a distributable archive in buildbox containing all supported firmware build variants for the selected board and source target.'; \
	print_help_note 'See make help-source-targets for the available and default source targets.'; \
	print_help_line 'make zip' 'Create a firmware .zip via the buildbox.'; \
	print_help_line 'make targz' 'Create a firmware .tar.gz via the buildbox.'; \
	printf '\n'; \
	print_help_line 'make clean' 'Remove stale filesystem cache entries.'; \
	print_help_line 'make realclean' 'Remove all filesystem cache entries.'; \
	print_section 'Buildbox Targets'; \
	print_help_line 'make buildbox-firmware-build' 'Delegate firmware build through the rendered firmware tree buildbox wrapper.'; \
	print_help_line 'make buildbox-firmware-stage' 'Delegate firmware staging through the rendered firmware tree buildbox wrapper.'; \
	print_section 'Help Targets'; \
	print_help_line 'make help' 'Show this help.'; \
	print_help_line 'make help-vars' 'Show common build variables.'; \
	print_help_line 'make help-source-targets' 'List configured firmware source targets.'; \
	print_help_line 'make help-dev' 'Show source-update and developer tooling targets.'

help-vars:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	default_release="$$(DEBUG="$(DEBUG)" $(PYTHON) scripts/help_cache.py --print-default-release 2>/dev/null || printf '%s' '<unavailable>')"; \
	print_section 'Common Build Variables'; \
	print_help_line 'RELEASE=<source-target>' "Select a configured firmware source target.\nUse names from 'make help-source-targets' or a full source/cache/release/... branch name.\nSource: latest available EDK2, CIX, Radxa, and unofficial refs."; \
	printf '    Default source target: %s\n' "$$default_release"; \
	print_help_line 'FIRMWARE_BOARD=O6|O6N' 'Select the firmware board.\nDefault: O6.'; \
	print_help_line 'FIRMWARE_TARGET=RELEASE|DEBUG' 'Select the firmware build target.\nDefault: RELEASE.'; \
	print_help_line 'FIRMWARE_DISTRO=bookworm|trixie' 'Select the buildbox distro when the rendered firmware branch supports an override. Leave unset for the selected source-target policy default.'; \
	print_help_line 'ARTEFACT_MODE=custom|upstream' 'Select the firmware artefact mode passed to rendered firmware builds. See README.md, "How do I build the latest firmware?", for the difference.\nDefault: custom.'; \
	print_help_line 'V=0|1' 'Verbosity. V=0 is concise; V=1 shows script/build detail.\nDefault: 0.'; \
	print_help_line 'DEBUG=0|1' 'Show Python tracebacks for unexpected tooling failures.\nDefault: 0.'; \
	print_section 'Install Variables'; \
	print_help_line 'INSTALL_ROOT=<path>' 'Firmware install root.\nDefault: /boot/efi.'; \
	print_help_line 'FORCE=0|1' 'Allow make install to replace existing firmware payload files beneath INSTALL_ROOT after the pre-install safety checks pass.\nDefault: 0.'; \
	print_section 'Source Target Selection'; \
	print_help_line 'make help-source-targets' 'List firmware source-target names generated from source refs.'; \
	printf '\n%s\n' 'For source-update and maintainer variables, run: make help-dev'

help-dev:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	print_section 'Source Integration'; \
	print_help_line 'make extract-vendor-delta' 'Produce a read-only vendor/source comparison report or diff.'; \
	print_help_line 'make integrate-source-release' 'Integrate new upstream/vendor source refs.'; \
	print_help_line 'make import-unofficial-commits' 'Update a source/unofficial source checkpoint explicitly.'; \
	print_subtitle 'Variables:'; \
	print_help_variable 'TYPE=upstream|vendor' 'For integrate-source-release. upstream updates base component refs; vendor updates Radxa source refs or CIX-carried source layers.'; \
	print_help_variable 'COMPONENT=<name>' 'For TYPE=upstream: edk2, edk2-platforms, edk2-non-osi, tf-a, or op-tee.'; \
	print_help_variable 'VENDOR=radxa|cix' 'For TYPE=vendor. Radxa records vendor-published or ported EDK2 source trees; CIX updates the TF-A/OP-TEE release bundle.'; \
	print_help_variable 'RELEASE=<name>' 'For source integration: release, tag, or source version name.'; \
	print_help_variable 'REF=<ref>' 'Input ref/object for source integration.'; \
	print_help_variable 'EDK2_BASE=<release>' 'EDK2 base used when integrating Radxa vendor sources.'; \
	print_help_variable 'ARM_BASE=<release>' 'Arm upstream base used when recording a CIX TF-A or OP-TEE component uplift.'; \
	print_help_variable 'RADXA_SOURCE=auto|vendor|port' 'Select whether a Radxa integration is a vendor-published source tree or this project'\''s port to an EDK2 base.\nDefault: auto.'; \
	print_help_variable 'WRITE=0|1' 'Permit ref creation/advancement in integrate-source-release and import-unofficial-commits.'; \
	print_help_variable 'ALLOW_REPLACE=0|1' 'Allow integrate-source-release to replace an existing manifested source ref deliberately.'; \
	print_help_variable 'MATERIALISE=0|1' 'Flatten Radxa vendor refs before recording source/vendor or source/port refs.\nDefault: 1.'; \
	print_help_variable 'BASE_REF=<ref>' 'Base ref for extract-vendor-delta, or explicit replay base for import-unofficial-commits when automatic inference is ambiguous.'; \
	print_help_variable 'VENDOR_REF=<ref>' 'Vendor ref for extract-vendor-delta.'; \
	print_help_variable 'FROM_REF=<ref>' 'Developer topic branch/ref for import-unofficial-commits. source/unofficial/** refs are rejected by default to avoid accidental replay of an unbounded range.'; \
	print_help_variable 'OUTPUT=<path>' 'Optional extract-vendor-delta metadata output path.'; \
	print_help_variable 'PATCH_OUTPUT=<path>' 'Optional extract-vendor-delta patch output path.'; \
	print_help_variable 'SOURCE_UNOFFICIAL_REF=<ref>' 'Unofficial source branch; defaults to source/unofficial/current.'; \
	print_help_variable 'PROPAGATE_CHECKPOINTS=none|all' 'For import-unofficial-commits. Replay the inferred FROM_REF change range onto every source/unofficial/edk2-stable* checkpoint after preparing all candidates safely.\nDefault: none.'; \
	print_help_variable 'UPDATE_COMPAT_TAGS=0|1' 'For import-unofficial-commits. Move matching source/unofficial/edk2/stable-* tags only after all checkpoint replays succeed.\nDefault: 0.'; \
	print_help_variable 'CONTINUE=0|1' 'Continue a paused import-unofficial-commits operation after conflicts are resolved in the scratch tree.'; \
	print_help_variable 'ABORT=0|1' 'Abort a paused import-unofficial-commits operation and remove its scratch state without moving refs.'; \
	print_help_variable 'OP_ID=<id>' 'Paused import-unofficial-commits operation ID for CONTINUE=1 or ABORT=1.'; \
	print_help_variable 'ALLOW_SOURCE_REF_FROM=0|1' 'Maintainer escape hatch allowing FROM_REF=source/unofficial/** with an explicit BASE_REF.\nDefault: 0.'; \
	print_section 'Rendering and Qualification'; \
	print_help_line 'make render-release-branch' 'Resolve or create a materialised source/cache/release branch.'; \
	print_help_line 'make verify-release-branch' 'Validate a materialised firmware source-target branch.'; \
	print_help_line 'make verify-build-matrix' 'Validate build/source target combinations derived from source refs.'; \
	print_help_line 'make verify-manifest-integrity' 'Validate defaults-expanded tree-ID manifest records.'; \
	print_subtitle 'Variables:'; \
	print_help_variable 'RELEASE=<source-target>' 'Firmware source-target name for render-release-branch and verify-release-branch.'; \
	print_help_variable 'PERSIST=0|1' 'For render-release-branch: create or verify a named source/cache/release branch. Without PERSIST=1, build targets use existing refs or cached detached worktrees and do not create a Git cache branch.'; \
	print_help_variable 'REBUILD=0|1' 'Regenerate a rendered firmware source target from its render plan instead of reusing an existing ref.'; \
	print_help_variable 'FORCE=0|1' 'Allow an explicitly requested ref replacement or install overwrite after the target-specific safety checks pass.'; \
	print_help_variable 'WORKTREE=<path>' 'Existing rendered worktree to use for verify-release-branch history checks.'; \
	print_help_variable 'SIGNING_CERT_SOURCE_DIR=<path>' 'Copy exact-replay signing certs into the build worktree.\nDefault: unset.'; \
	print_help_variable 'INSTALL_SOURCE=<path>' 'Optional staged payload path, or path relative to dist/firmware.\nDefault: latest staged firmware payload.'; \
	print_section 'Minimised Repository'; \
	print_help_line 'make create-minimised-clone' 'Create a bare repo containing only build plus required non-cache source refs and tags.'; \
	print_help_line 'make verify-minimised-clone' 'Create and clone a minimised repository, then verify that it can render the default source target.'; \
	print_subtitle 'Variables:'; \
	print_help_variable 'DIR=<path>' 'Destination directory for create-minimised-clone, or optional workspace directory for verify-minimised-clone.'; \
	print_help_variable 'KEEP=0|1' 'Keep the temporary verification workspace created by verify-minimised-clone.\nDefault: 0.'; \
	print_help_variable 'REPACK=0|1' 'Repack the destination produced by create-minimised-clone.\nDefault: 1.'; \
	print_section 'Repository Maintenance'; \
	print_help_line 'make check-ref-integrity' 'Check persistent source refs do not depend on generated cache refs.'; \
	print_help_line 'make check-identity-integrity' 'Quickly scan build-branch files for path/identity integrity issues.'; \
	print_help_line 'make verify-identity-integrity' 'Deep-scan build-branch files, commit metadata, and persistent source refs for path/identity integrity issues.'; \
	print_help_line 'make check-vendor-workflow-drift' 'Detect vendor .github/workflows changes that may need porting to the build branch CI.'; \
	print_help_line 'make check-upstream-versions' 'Check recorded source refs against external upstream/vendor remotes.'; \
	print_help_line 'make ref-report' 'Report required source refs, generated cache refs, and ref namespace issues.'; \
	print_help_line 'make cleanup-report' 'Report generated cache refs and cautious clean-up guidance.'; \
	print_help_line 'make prune' 'Report generated source/cache refs, or delete them with DELETE=1 after safety checks.'; \
	print_help_line 'make refresh-help-cache' 'Refresh config/help-cache.json after changing Makefile, config/, scripts/, or source refs. No Git hook is installed automatically.'; \
	print_help_line 'make check-help-cache' 'Check config/help-cache.json matches current source refs and help-generation inputs. make test runs this check.'; \
	print_subtitle 'Variables:'; \
	print_help_variable 'SCAN_COMMITS=0|1' 'Legacy override for scripts/check_identity_integrity.py. The make check-identity-integrity target is intentionally quick; use make verify-identity-integrity for the full source-ref and commit scan.'; \
	print_help_variable 'SCAN_SOURCE_REFS=0|1' 'Legacy override for scripts/check_identity_integrity.py. The make check-identity-integrity target is intentionally quick; use make verify-identity-integrity for the full source-ref and commit scan.'; \
	print_help_variable 'UPSTREAM_VERSION_MODE=advisory|policy|strict' 'Version-check failure mode. advisory never fails on stale remotes; policy fails checks marked strict; strict fails any stale or unavailable check.\nDefault: policy.'; \
	print_help_variable 'UPSTREAM_VERSION_ONLY=<id[,id...]>' 'Optional comma-separated source IDs, or source:release/source:commits comparison IDs, for check-upstream-versions.'; \
	print_help_variable 'UPSTREAM_VERSION_FORMAT=text|github|json' 'Upstream versions output format. github emits workflow annotations.\nDefault: text.'; \
	print_help_variable 'UPSTREAM_VERSION_SNAPSHOT=<path>' 'Offline git ls-remote snapshot for upstream version tests.\nDefault: unset.'; \
	print_help_variable 'DELETE=0|1' 'Allow make prune to delete verified source/cache refs.\nDefault: 0.'; \
	print_section 'Local GitHub Actions'; \
	print_help_line 'make gha-act-list' 'List GitHub Actions workflows and jobs through a repo-local act wrapper.'; \
	print_help_line 'make gha-act-dry-run' 'Dry-run a selected GitHub Actions workflow through act.'; \
	print_help_line 'make gha-act-run' 'Execute a selected GitHub Actions workflow through act.'; \
	print_subtitle 'Variables:'; \
	print_help_variable 'ACT_WORKFLOW=.github/workflows/<file>.yaml' 'Workflow file to list, dry-run, or execute through the local act wrapper. Optional for gha-act-list.'; \
	print_help_variable 'ACT_EVENT=workflow_dispatch|push|pull_request|schedule' 'Event name passed to act for gha-act-dry-run and gha-act-run.\nDefault: workflow_dispatch.'; \
	print_help_variable 'ACT_JOB=<job-id>' 'Optional act job filter.'; \
	print_help_variable 'ACT_MATRIX=<name:value>' 'Optional single act matrix filter, for example board:O6.'; \
	print_help_variable 'ACT_SECRET_FILE=<path>' 'Optional act --secret-file path for local workflow runs.'; \
	print_help_variable 'ACT_EXTRA_ARGS=<args>' 'Additional raw flags appended to act.'; \
	print_help_variable 'ACT_CONTAINER_ARCH=<platform>' 'Container architecture used by act.\nDefault: linux/amd64.'; \
	print_help_variable 'ACT_RUNNER_IMAGE=<image>' 'Runner image mapped to ubuntu-latest.\nDefault: catthehacker/ubuntu:act-latest.'; \
	print_section 'Documentation'; \
	print_help_line 'make docs-build' 'Build the mdBook product documentation under docs/.'; \
	print_help_line 'make docs-workflow-local' 'Run the documentation workflow in its local Docker wrapper.'; \
	print_section 'Quality'; \
	print_help_line 'make test' 'Run build-branch tests in the quality container.'; \
	print_help_line 'make lint' 'Run JSON, YAML, Markdown, shell, and Python linting in the quality container.'; \
	print_subtitle 'Variables:'; \
	print_help_variable 'QUALITY_IMAGE=<name>' 'Container image tag used by make test and make lint.\nDefault: edk2-cix-build-quality:latest.'; \
	print_section 'Common Variables'; \
	print_help_variable 'V=0|1' 'Verbosity. V=0 is concise; V=1 shows script/build detail.\nDefault: 0.'; \
	print_help_variable 'DEBUG=0|1' 'Show Python tracebacks for unexpected tooling failures.\nDefault: 0.'; \
	print_section 'Help Targets'; \
	print_help_line 'make help-source-targets' 'Show configured firmware source targets.'; \
	print_help_line 'make render-release-branch-help' 'Show render-release-branch arguments.'; \
	print_help_line 'make integrate-source-release-help' 'Show integrate-source-release arguments.'; \
	print_help_line 'make import-unofficial-commits-help' 'Show import-unofficial-commits arguments.'; \
	print_help_line 'make extract-vendor-delta-help' 'Show extract-vendor-delta arguments.'; \
	print_help_line 'make verify-release-branch-help' 'Show verify-release-branch arguments.'; \
	print_help_line 'make verify-build-matrix-help' 'Show verify-build-matrix arguments.'; \
	print_help_line 'make check-vendor-workflow-drift-help' 'Show check-vendor-workflow-drift arguments.'; \
	print_help_line 'make check-upstream-versions-help' 'Show check-upstream-versions arguments.'

help-source-targets:
	@DEBUG="$(DEBUG)" $(PYTHON) scripts/help_cache.py --print-source-targets

BUILD_VARIABLE_ENV = DEBUG="$(DEBUG)" RELEASE="$(RELEASE)" V="$(V)" SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" ARTEFACT_MODE="$(ARTEFACT_MODE)" FIRMWARE_BOARD="$(FIRMWARE_BOARD)" FIRMWARE_TARGET="$(FIRMWARE_TARGET)" FIRMWARE_DISTRO="$(FIRMWARE_DISTRO)" FIRMWARE_VALIDATE_ON_BUILD="$(FIRMWARE_VALIDATE_ON_BUILD)" BUILDBOX_PLATFORM="$(BUILDBOX_PLATFORM)" ENABLE_FIRMWARE_FIXES="$(ENABLE_FIRMWARE_FIXES)" ENABLE_CORE_ORDER="$(ENABLE_CORE_ORDER)" ENABLE_EXPERIMENTAL_UEFI_SETTINGS="$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS)" DEBUG_ON_UART3="$(DEBUG_ON_UART3)" UART3_ENABLE="$(UART3_ENABLE)" DEBUG_VERBOSE="$(DEBUG_VERBOSE)" DEBUG_PRINT_ERROR_LEVEL="$(DEBUG_PRINT_ERROR_LEVEL)" CIX_RELEASE="$(CIX_RELEASE)" FORCE="$(FORCE)"

DELEGATED_BUILD_ARGS = V="$(V)" ARTEFACT_MODE="$(ARTEFACT_MODE)" FIRMWARE_BOARD="$(FIRMWARE_BOARD)" FIRMWARE_TARGET="$(FIRMWARE_TARGET)" FIRMWARE_DISTRO="$(FIRMWARE_DISTRO)" FIRMWARE_VALIDATE_ON_BUILD="$(FIRMWARE_VALIDATE_ON_BUILD)" BUILDBOX_PLATFORM="$(BUILDBOX_PLATFORM)" ENABLE_FIRMWARE_FIXES="$(ENABLE_FIRMWARE_FIXES)" ENABLE_CORE_ORDER="$(ENABLE_CORE_ORDER)" ENABLE_EXPERIMENTAL_UEFI_SETTINGS="$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS)" DEBUG_ON_UART3="$(DEBUG_ON_UART3)" UART3_ENABLE="$(UART3_ENABLE)" DEBUG_VERBOSE="$(DEBUG_VERBOSE)" DEBUG_PRINT_ERROR_LEVEL="$(DEBUG_PRINT_ERROR_LEVEL)" CIX_RELEASE="$(CIX_RELEASE)"

define run_release_make
	@set -e; \
	$(BUILD_VARIABLE_ENV) $(PYTHON) scripts/validate_build_variables.py --target "$(1)"; \
	release_label="$(RELEASE)"; \
	if [ -z "$$release_label" ]; then release_label="$$(DEBUG="$(DEBUG)" $(PYTHON) scripts/render_release_branch.py --print-default-release)"; fi; \
	printf '[build] Preparing release worktree: %s\n' "$$release_label" >&2; \
	wt="$$(DEBUG="$(DEBUG)" RELEASE="$(RELEASE)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --ensure-worktree --print-worktree --v "$(V)")"; \
	signing_cert_arg="$$(DEBUG="$(DEBUG)" SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" V="$(V)" $(PYTHON) scripts/prepare_release_worktree.py --worktree "$$wt" --print-make-arg --v "$(V)")"; \
	printf '[build] Starting %s: board=%s target=%s release=%s\n' "$(1)" "$(FIRMWARE_BOARD)" "$(FIRMWARE_TARGET)" "$$release_label" >&2; \
	if [ "$(V)" = "1" ]; then printf '%s\n' "$(MAKE) --no-print-directory -C $$wt $(1) $(DELEGATED_BUILD_ARGS)"; fi; \
	$(MAKE) --no-print-directory -C "$$wt" $(1) $(DELEGATED_BUILD_ARGS) $$signing_cert_arg
endef

build:
	$(call run_release_make,buildbox-firmware-build)

build-all:
	$(call run_release_make,build-all)

install:
	@set -e; \
	$(BUILD_VARIABLE_ENV) $(PYTHON) scripts/validate_build_variables.py --target "install"; \
	release_label="$(RELEASE)"; \
	if [ -z "$$release_label" ]; then release_label="$$(DEBUG="$(DEBUG)" $(PYTHON) scripts/render_release_branch.py --print-default-release)"; fi; \
	printf '[build] Preparing release worktree: %s\n' "$$release_label" >&2; \
	wt="$$(DEBUG="$(DEBUG)" RELEASE="$(RELEASE)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --ensure-worktree --print-worktree --v "$(V)")"; \
	signing_cert_arg="$$(DEBUG="$(DEBUG)" SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" V="$(V)" $(PYTHON) scripts/prepare_release_worktree.py --worktree "$$wt" --print-make-arg --v "$(V)")"; \
	printf '[build] Starting buildbox-firmware-stage: board=%s target=%s release=%s\n' "$(FIRMWARE_BOARD)" "$(FIRMWARE_TARGET)" "$$release_label" >&2; \
	if [ "$(V)" = "1" ]; then printf '%s\n' "$(MAKE) --no-print-directory -C $$wt buildbox-firmware-stage $(DELEGATED_BUILD_ARGS)"; fi; \
	$(MAKE) --no-print-directory -C "$$wt" buildbox-firmware-stage $(DELEGATED_BUILD_ARGS) $$signing_cert_arg; \
	force_arg=""; \
	if [ "$(FORCE)" = "1" ]; then force_arg="--force"; fi; \
	DEBUG="$(DEBUG)" INSTALL_SOURCE="$(INSTALL_SOURCE)" FORCE="$(FORCE)" V="$(V)" $(PYTHON) scripts/install_release_payload.py \
		--worktree "$$wt" \
		--install-root "$(INSTALL_ROOT)" \
		$$force_arg \
		--v "$(V)"

zip:
	$(call run_release_make,buildbox-zip)

targz:
	$(call run_release_make,buildbox-targz)

clean:
	@DEBUG="$(DEBUG)" FORCE="$(FORCE)" V="$(V)" $(PYTHON) scripts/clean_cache.py --mode stale --force "$(FORCE)" --v "$(V)"

realclean:
	@DEBUG="$(DEBUG)" FORCE="$(FORCE)" V="$(V)" $(PYTHON) scripts/clean_cache.py --mode all --force "$(FORCE)" --v "$(V)"

prune:
	@DEBUG="$(DEBUG)" DELETE="$(DELETE)" V="$(V)" $(PYTHON) scripts/prune_cache_refs.py --delete "$(DELETE)" --v "$(V)"

create-minimised-clone:
	@if [ -z "$(DIR)" ]; then $(PYTHON) scripts/create_minimised_clone.py --help; printf '%s\n' 'missing required variable: DIR' >&2; exit 2; fi
	@DEBUG="$(DEBUG)" DIR="$(DIR)" REPACK="$(REPACK)" V="$(V)" $(PYTHON) scripts/create_minimised_clone.py --dir "$(DIR)" --repack "$(REPACK)" --v "$(V)"

buildbox-firmware-build:
	$(call run_release_make,buildbox-firmware-build)

buildbox-firmware-stage:
	$(call run_release_make,buildbox-firmware-stage)

docs-build:
	@docs/scripts/run_docs_build.sh

docs-workflow-local:
	@docs/scripts/run_docs_workflow_local.sh

render-release-branch:
	@if [ -z "$(RELEASE)" ]; then $(MAKE) --no-print-directory render-release-branch-help; printf '%s\n' 'missing required variable: RELEASE' >&2; exit 2; fi
	@DEBUG="$(DEBUG)" RELEASE="$(RELEASE)" PERSIST="$(PERSIST)" REBUILD="$(REBUILD)" FORCE="$(FORCE)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --require-release --persist "$(PERSIST)" --rebuild "$(REBUILD)" --force "$(FORCE)" --v "$(V)"

verify-release-branch:
	@if [ -z "$(RELEASE)" ]; then $(MAKE) --no-print-directory verify-release-branch-help; printf '%s\n' 'missing required variable: RELEASE' >&2; exit 2; fi
	@DEBUG="$(DEBUG)" RELEASE="$(RELEASE)" WORKTREE="$(WORKTREE)" V="$(V)" $(PYTHON) scripts/verify_release_branch.py --v "$(V)"

verify-build-matrix:
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/verify_build_matrix.py --v "$(V)"

verify-manifest-integrity:
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/verify_manifest_integrity.py --v "$(V)"

check-ref-integrity:
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/check_ref_integrity.py --v "$(V)"

verify-minimised-clone:
	@DEBUG="$(DEBUG)" DIR="$(DIR)" KEEP="$(KEEP)" REPACK="$(REPACK)" V="$(V)" $(PYTHON) scripts/verify_minimised_clone.py --dir "$(DIR)" --keep "$(KEEP)" --repack "$(REPACK)" --v "$(V)"

extract-vendor-delta:
	@DEBUG="$(DEBUG)" VENDOR="$(VENDOR)" BASE_REF="$(BASE_REF)" VENDOR_REF="$(VENDOR_REF)" OUTPUT="$(OUTPUT)" PATCH_OUTPUT="$(PATCH_OUTPUT)" TARGET_REF="$(TARGET_REF)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/extract_vendor_delta.py --v "$(V)"

integrate-source-release:
	@DEBUG="$(DEBUG)" TYPE="$(TYPE)" COMPONENT="$(COMPONENT)" VENDOR="$(VENDOR)" RELEASE="$(RELEASE)" EDK2_BASE="$(EDK2_BASE)" REF="$(REF)" RADXA_SOURCE="$(RADXA_SOURCE)" WRITE="$(WRITE)" ALLOW_REPLACE="$(ALLOW_REPLACE)" MATERIALISE="$(MATERIALISE)" V="$(V)" $(PYTHON) scripts/integrate_source_release.py --v "$(V)"

import-unofficial-commits:
	@DEBUG="$(DEBUG)" FROM_REF="$(FROM_REF)" BASE_REF="$(BASE_REF)" SOURCE_UNOFFICIAL_REF="$(SOURCE_UNOFFICIAL_REF)" PROPAGATE_CHECKPOINTS="$(PROPAGATE_CHECKPOINTS)" UPDATE_COMPAT_TAGS="$(UPDATE_COMPAT_TAGS)" ALLOW_SOURCE_REF_FROM="$(ALLOW_SOURCE_REF_FROM)" CONTINUE="$(CONTINUE)" ABORT="$(ABORT)" OP_ID="$(OP_ID)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/import_unofficial_commits.py --v "$(V)"

check-identity-integrity:
	@DEBUG="$(DEBUG)" SCAN_COMMITS="0" SCAN_SOURCE_REFS="0" V="$(V)" $(PYTHON) scripts/check_identity_integrity.py --v "$(V)"

verify-identity-integrity:
	@DEBUG="$(DEBUG)" SCAN_COMMITS="1" SCAN_SOURCE_REFS="1" V="$(V)" $(PYTHON) scripts/check_identity_integrity.py --v "$(V)"

check-vendor-workflow-drift:
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/check_vendor_workflow_drift.py --v "$(V)"

check-upstream-versions:
	@DEBUG="$(DEBUG)" UPSTREAM_VERSION_MODE="$(UPSTREAM_VERSION_MODE)" UPSTREAM_VERSION_ONLY="$(UPSTREAM_VERSION_ONLY)" UPSTREAM_VERSION_FORMAT="$(UPSTREAM_VERSION_FORMAT)" UPSTREAM_VERSION_SNAPSHOT="$(UPSTREAM_VERSION_SNAPSHOT)" V="$(V)" $(PYTHON) scripts/check_upstream_versions.py --v "$(V)"

check-help-cache:
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/help_cache.py --verify --v "$(V)"

refresh-help-cache:
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/help_cache.py --refresh --v "$(V)"

ref-report:
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/ref_report.py --v "$(V)"

cleanup-report:
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/ref_report.py --cleanup --v "$(V)"

test:
	@DEBUG="$(DEBUG)" V="$(V)" QUALITY_IMAGE="$(QUALITY_IMAGE)" scripts/run_quality_container.sh test

lint:
	@DEBUG="$(DEBUG)" V="$(V)" QUALITY_IMAGE="$(QUALITY_IMAGE)" scripts/run_quality_container.sh lint

gha-act-list:
	@ACT_WORKFLOW="$(ACT_WORKFLOW)" ACT_EVENT="$(ACT_EVENT)" ACT_JOB="$(ACT_JOB)" ACT_MATRIX="$(ACT_MATRIX)" ACT_SECRET_FILE="$(ACT_SECRET_FILE)" ACT_EXTRA_ARGS="$(ACT_EXTRA_ARGS)" ACT_CONTAINER_ARCH="$(ACT_CONTAINER_ARCH)" ACT_RUNNER_IMAGE="$(ACT_RUNNER_IMAGE)" scripts/run_github_actions_with_act.sh list

gha-act-dry-run:
	@if [ -z "$(ACT_WORKFLOW)" ]; then printf '%s\n' 'missing required variable: ACT_WORKFLOW=.github/workflows/<file>.yaml' >&2; exit 2; fi
	@ACT_WORKFLOW="$(ACT_WORKFLOW)" ACT_EVENT="$(ACT_EVENT)" ACT_JOB="$(ACT_JOB)" ACT_MATRIX="$(ACT_MATRIX)" ACT_SECRET_FILE="$(ACT_SECRET_FILE)" ACT_EXTRA_ARGS="$(ACT_EXTRA_ARGS)" ACT_CONTAINER_ARCH="$(ACT_CONTAINER_ARCH)" ACT_RUNNER_IMAGE="$(ACT_RUNNER_IMAGE)" scripts/run_github_actions_with_act.sh dry-run

gha-act-run:
	@if [ -z "$(ACT_WORKFLOW)" ]; then printf '%s\n' 'missing required variable: ACT_WORKFLOW=.github/workflows/<file>.yaml' >&2; exit 2; fi
	@ACT_WORKFLOW="$(ACT_WORKFLOW)" ACT_EVENT="$(ACT_EVENT)" ACT_JOB="$(ACT_JOB)" ACT_MATRIX="$(ACT_MATRIX)" ACT_SECRET_FILE="$(ACT_SECRET_FILE)" ACT_EXTRA_ARGS="$(ACT_EXTRA_ARGS)" ACT_CONTAINER_ARCH="$(ACT_CONTAINER_ARCH)" ACT_RUNNER_IMAGE="$(ACT_RUNNER_IMAGE)" scripts/run_github_actions_with_act.sh run

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

import-unofficial-commits-help:
	@$(PYTHON) scripts/import_unofficial_commits.py --help

check-vendor-workflow-drift-help:
	@$(PYTHON) scripts/check_vendor_workflow_drift.py --help

check-upstream-versions-help:
	@$(PYTHON) scripts/check_upstream_versions.py --help
