SHELL := /bin/sh
.DEFAULT_GOAL := help

PYTHON ?= python3
PYTHONPYCACHEPREFIX ?= $(CURDIR)/.cache/edk2-cix/pycache
export PYTHONPYCACHEPREFIX
V ?= 0
DEBUG ?= 0
RELEASE ?=
PERSIST ?= 0
WORKTREE ?=
TARGET_REF ?=
SOURCE_UNOFFICIAL_REF ?=
BASE_REF ?=
COMMIT_MESSAGE ?=
COMMIT_MESSAGE_FILE ?=
SIGNOFF ?= 0
SOURCE_LIFECYCLE_NORMALISE ?= exact
INSTALL_ROOT ?= /boot/efi
INSTALL_SOURCE ?=
FORCE ?= 0
WRITE ?= 0
CHECK ?= 0
VERIFY ?= 1
RENDER_GENERATED ?= 0
UPDATE_RELEASE_TAGS ?= 0
DELETE ?= 0
REPACK ?= 1
KEEP ?= 0
ARTEFACT_MODE ?= custom
FIRMWARE_BOARD ?= O6
FIRMWARE_PRODUCT ?= $(if $(filter O6N,$(FIRMWARE_BOARD)),orion-o6n,orion-o6)
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
ACT_CONTAINER_ARCH ?= auto
ACT_RUNNER_IMAGE ?=
REPLAY_SOURCE_TARGET ?= edk2-202208/radxa-1.2.1/unofficial-1.2.1
REPLAY_UPSTREAM_REPOSITORY ?= radxa-pkg/edk2-cix
REPLAY_DOWNLOAD ?= 1
REPLAY_INPUT ?=
REPLAY_BUILD_OPTIONS ?=
REPLAY_BUILD_DATE ?=
REPLAY_VERSION ?= 1.2.1
DOCS_BUILD_MODE ?= auto
CONFLICT_PATHS ?=
CONFLICT_EDITOR ?=
PRESERVE_SYMLINKS ?= 1
ALLOW_CONFLICT_MARKERS ?= 0
FIRST_OUTPUT_PROBE ?= 0
BUILD_DIST_ROOT ?= $(CURDIR)/dist
FIRMWARE_CACHE_ROOT ?= $(CURDIR)/.cache/edk2-cix/firmware

define PROGRESS_PROBE
	@printf '%s\n' '$(1)' >&2; if [ "$(FIRST_OUTPUT_PROBE)" = "1" ]; then exit 0; fi
endef

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

.PHONY: help help-vars help-dev help-dev-source help-dev-verify help-dev-maintenance help-source-targets build build-all deterministic-replay install zip targz clean realclean prune buildbox-firmware-build buildbox-firmware-stage docs-build docs-workflow-local \
	test test-local lint \
	extract-vendor-delta render-release-branch uplift-edk2-release uplift-radxa-release select-unofficial-line integrate-source-release import-changes import-unofficial-commits inspect-import-conflicts resolve-conflicts \
	propagate-release-branches promote-unofficial-compatibility promote-unofficial-release update-release-tags \
	verify-release-branch verify-build-matrix verify-manifest-integrity check-ref-integrity verify-minimised-clone check-identity-integrity verify-identity-integrity check-vendor-workflow-drift refresh-vendor-workflow-baseline check-upstream-versions check-source-metadata check-help-cache check-first-output-latency refresh-source-metadata refresh-help-cache ref-report cleanup-report create-minimised-clone \
	gha-act-list gha-act-dry-run gha-act-run \
	extract-vendor-delta-help render-release-branch-help uplift-edk2-release-help uplift-radxa-release-help select-unofficial-line-help integrate-source-release-help \
	import-changes-help import-unofficial-commits-help inspect-import-conflicts-help resolve-conflicts-help promote-unofficial-compatibility-help promote-unofficial-release-help update-release-tags-help \
	verify-release-branch-help verify-build-matrix-help verify-source-policy-help verify-source-lifecycle-help \
	check-ref-integrity-help check-identity-integrity-help verify-identity-integrity-help check-vendor-workflow-drift-help check-upstream-versions-help check-source-metadata-help refresh-source-metadata-help \
	create-minimised-clone-help verify-minimised-clone-help prune-help refresh-help-cache-help check-help-cache-help check-first-output-latency-help ref-report-help cleanup-report-help

help:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	printf '%s\n' 'edk2-cix firmware build targets'; \
	print_section 'Build Targets'; \
	print_help_line 'make build' 'Build one firmware image in buildbox for the selected board and source target.'; \
	print_help_line 'make deterministic-replay' 'Render the replay-capable vendor source target and run byte-identical replay for the selected board.'; \
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
	print_help_line 'make help-dev' 'Show the developer tooling help index.'; \
	print_help_line 'make help-dev-source' 'Show source integration and import workflow targets.'; \
	print_help_line 'make help-dev-verify' 'Show source rendering, verification, and integrity-check targets.'; \
	print_help_line 'make help-dev-maintenance' 'Show minimised-repo, cache, CI, docs, and quality targets.'

help-vars:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	print_section 'Common Build Variables'; \
	print_help_line 'RELEASE=<source-target>' "Select a configured firmware source target.\nUse names from 'make help-source-targets' or a full source/cache/release/... branch name.\nSource: latest available EDK2, CIX, Radxa, and unofficial refs."; \
	print_help_note 'See make help-source-targets for the available and default source targets.'; \
	print_help_line 'FIRMWARE_BOARD=O6|O6N' 'Select the firmware board.\nDefault: O6.'; \
	print_help_line 'FIRMWARE_PRODUCT=<name>' 'Set the output product name.\nDefault: orion-o6 for O6 and orion-o6n for O6N.'; \
	print_help_line 'FIRMWARE_TARGET=RELEASE|DEBUG' 'Select the firmware build target.\nDefault: RELEASE.'; \
	print_help_line 'FIRMWARE_DISTRO=trixie|bookworm' 'Select the buildbox distro when the rendered firmware branch supports an override. Leave unset for the selected source-target policy default.'; \
	print_help_line 'ARTEFACT_MODE=custom|upstream' 'Select the firmware artefact mode passed to rendered firmware builds. See README.md, "How do I build the latest firmware?", for the difference.\nDefault: custom.'; \
	print_section 'Custom Build Gates'; \
	print_help_line 'CIX_RELEASE=v1.2' 'Use public CIX BIOS V1.2 bootloader1 payload with TF-A and OP-TEE sources for bootloader2 when the selected rendered source target supports it.\nDefault: unset.'; \
	print_help_line 'ENABLE_FIRMWARE_FIXES=true|false' 'Enable opt-in custom firmware fixes for O6/O6N. This changes firmware metadata and setup behavior; see FIXES.md in the rendered source target for details.\nDefault: false.'; \
	print_help_line 'ENABLE_CORE_ORDER=cix|conventional|performance' 'Choose how custom firmware numbers CPUs exposed to the OS. cix keeps vendor order; conventional puts A520 cores before A720 cores; performance puts A720 cores first.\nDefault: unset, which behaves like cix.\nRequires ENABLE_FIRMWARE_FIXES=true for conventional and performance.'; \
	print_help_line 'ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true|false' 'Enable the experimental Radxa settings overlay for O6/O6N, including RTC wakeup and selected power controls, with SR-IOV remaining O6-only.\nDefault: false.'; \
	print_help_line 'UART3_ENABLE=true|false' 'Expose UART3 to ACPI and mux its header pins as UART instead of GPIO. This consumes header GPIO105/GPIO106 while enabled.\nDefault: false.'; \
	print_help_line 'DEBUG_ON_UART3=true|false' 'Route firmware DEBUG() output to UART3; implies UART3_ENABLE=true.\nDefault: unset; custom builds keep DEBUG() on UART2.'; \
	print_help_line 'DEBUG_VERBOSE=true|false' 'On RELEASE builds, re-enable DEBUG() logging without switching the whole firmware image to DEBUG. If DEBUG_PRINT_ERROR_LEVEL is unset, the rendered source build uses its verbose debug mask default.\nDefault: false.'; \
	print_help_line 'DEBUG_PRINT_ERROR_LEVEL=<u32>' 'Override the firmware debug message mask with a decimal or 0x-prefixed 32-bit value.\nDefault: unset; rendered source builds use their normal default, or their verbose default when DEBUG_VERBOSE=true.'; \
	print_section 'Generic Build Controls'; \
	print_help_line 'V=0|1' 'Verbosity. V=0 is concise; V=1 shows script/build detail.\nDefault: 0.'; \
	print_help_line 'DEBUG=0|1' 'Show Python tracebacks for unexpected tooling failures.\nDefault: 0.'; \
	print_section 'Build Output Locations'; \
	print_help_line 'BUILD_DIST_ROOT=<path>' 'Directory where build-branch builds mirror rendered worktree archives, staged payloads, and key raw firmware images.\nDefault: ./dist.'; \
	print_help_line 'FIRMWARE_CACHE_ROOT=<path>' 'Persistent build-branch firmware cache root shared by rendered worktree builds, including ccache, buildbox temporary state, and CIX release caches.\nDefault: ./.cache/edk2-cix/firmware.'; \
	print_section 'Replay Variables'; \
	print_help_line 'REPLAY_INPUT=<path>' 'Published edk2-cix .deb, extracted release directory, or cix_flash_all.bin to replay. If unset, make deterministic-replay downloads REPLAY_VERSION from REPLAY_UPSTREAM_REPOSITORY unless REPLAY_DOWNLOAD=0.\nDefault: unset.'; \
	print_help_line 'REPLAY_SOURCE_TARGET=<target>' 'Replay-capable source target rendered before delegating to the firmware tree deterministic-replay target.\nDefault: edk2-202208/radxa-1.2.1/unofficial-1.2.1.'; \
	print_help_line 'REPLAY_UPSTREAM_REPOSITORY=<owner/name>' 'GitHub repository used to resolve and download the release package when REPLAY_INPUT is unset.\nDefault: radxa-pkg/edk2-cix.'; \
	print_help_line 'REPLAY_DOWNLOAD=0|1' 'When REPLAY_INPUT is unset, download the REPLAY_VERSION package before replay. Set to 0 to reuse an existing rendered replay cache instead.\nDefault: 1.'; \
	print_help_line 'REPLAY_BUILD_OPTIONS=<path>' 'BuildOptions file used when REPLAY_INPUT points directly at cix_flash_all.bin.\nDefault: unset.'; \
	print_help_line 'REPLAY_BUILD_DATE=<iso8601>' 'Fallback build timestamp when replay inputs do not include BuildOptions.\nDefault: unset.'; \
	print_help_line 'REPLAY_VERSION=<version>' 'Release tag to download and replay validation profile passed to the rendered firmware tree. Keep it matched to REPLAY_SOURCE_TARGET.\nDefault: 1.2.1.'; \
	print_section 'Install Variables'; \
	print_help_line 'INSTALL_ROOT=<path>' 'Firmware install root.\nDefault: /boot/efi.'; \
	print_help_line 'FORCE=0|1' 'Allow make install to replace existing firmware payload files beneath INSTALL_ROOT after the pre-install safety checks pass.\nDefault: 0.'; \
	print_section 'Source Target Selection'; \
	print_help_line 'make help-source-targets' 'List firmware source-target names generated from source refs.'; \
	printf '\n%s\n' 'For source-update and maintainer variables, run: make help-dev'

help-dev:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	print_section 'Developer Help'; \
	print_help_line 'make help-dev-source' 'Show source integration, import, conflict-resolution, and release-tag update targets.'; \
	print_help_line 'make help-dev-verify' 'Show rendering, source-model verification, metadata refresh, and integrity-check targets.'; \
	print_help_line 'make help-dev-maintenance' 'Show minimised-repository, cache/reporting, local CI, documentation, and quality targets.'; \
	print_section 'Common Variables'; \
	print_help_variable 'V=0|1' 'Verbosity. V=0 is concise; V=1 shows script/build detail.\nDefault: 0.'; \
	print_help_variable 'DEBUG=0|1' 'Show Python tracebacks for unexpected tooling failures.\nDefault: 0.'; \
	print_section 'Help Targets'; \
	print_help_line 'make help-source-targets' 'Show configured firmware source targets.'; \
	printf '\n'; \
	print_help_line 'make render-release-branch-help' 'Show render-release-branch arguments.'; \
	print_help_line 'make verify-release-branch-help' 'Show verify-release-branch arguments.'; \
	print_help_line 'make verify-build-matrix-help' 'Show verify-build-matrix arguments.'; \
	print_help_line 'make verify-source-policy-help' 'Show verify-source-policy arguments.'; \
	print_help_line 'make verify-source-lifecycle-help' 'Show verify-source-lifecycle arguments.'; \
	print_help_line 'make extract-vendor-delta-help' 'Show extract-vendor-delta arguments.'; \
	print_help_line 'make uplift-edk2-release-help' 'Show uplift-edk2-release arguments.'; \
	print_help_line 'make uplift-radxa-release-help' 'Show uplift-radxa-release arguments.'; \
	print_help_line 'make integrate-source-release-help' 'Show integrate-source-release arguments.'; \
	print_help_line 'make import-changes-help' 'Show import-changes arguments.'; \
	print_help_line 'make import-unofficial-commits-help' 'Show import-unofficial-commits arguments.'; \
	print_help_line 'make inspect-import-conflicts-help' 'Show inspect-import-conflicts arguments.'; \
	print_help_line 'make resolve-conflicts-help' 'Show resolve-conflicts arguments.'; \
	print_help_line 'make promote-unofficial-compatibility-help' 'Show retained EDK2 compatibility promotion arguments.'; \
	print_help_line 'make promote-unofficial-release-help' 'Show promote-unofficial-release arguments.'; \
	print_help_line 'make update-release-tags-help' 'Show update-release-tags arguments.'; \
	print_help_line 'make check-ref-integrity-help' 'Show check-ref-integrity arguments.'; \
	print_help_line 'make check-identity-integrity-help' 'Show check-identity-integrity arguments.'; \
	print_help_line 'make verify-identity-integrity-help' 'Show verify-identity-integrity arguments.'; \
	print_help_line 'make check-vendor-workflow-drift-help' 'Show check-vendor-workflow-drift arguments.'; \
	print_help_line 'make check-upstream-versions-help' 'Show check-upstream-versions arguments.'; \
	print_help_line 'make check-source-metadata-help' 'Show check-source-metadata arguments.'; \
	print_help_line 'make refresh-source-metadata-help' 'Show refresh-source-metadata arguments.'; \
	print_help_line 'make create-minimised-clone-help' 'Show create-minimised-clone arguments.'; \
	print_help_line 'make verify-minimised-clone-help' 'Show verify-minimised-clone arguments.'; \
	print_help_line 'make prune-help' 'Show prune arguments.'; \
	print_help_line 'make ref-report-help' 'Show ref-report arguments.'; \
	print_help_line 'make cleanup-report-help' 'Show cleanup-report arguments.'

help-dev-source:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	print_section 'Source Integration'; \
	print_help_line 'make extract-vendor-delta' 'Produce a read-only vendor/source comparison report or diff.'; \
	print_help_line 'make uplift-edk2-release' 'Run the mechanical stages for a new upstream EDK2 stable release.'; \
	print_help_line 'make uplift-radxa-release' 'Carry one unofficial firmware line onto the next Radxa release.'; \
	print_help_line 'make select-unofficial-line' 'Select a validated Unofficial line as the default source target.'; \
	print_help_line 'make integrate-source-release' 'Integrate new upstream/vendor source refs.'; \
	print_help_line 'make import-changes' 'Extract a patch from a materialised, legacy, or broader source tree into source/unofficial refs.'; \
	print_help_line 'make import-unofficial-commits' 'Update a source/unofficial source branch from an already unofficial-based topic branch.'; \
	print_help_line 'make inspect-import-conflicts' 'Inspect a paused import operation with symlink-aware conflict reporting.'; \
	print_help_line 'make resolve-conflicts' 'Batch-resolve paused import conflicts in scratch trees with symlink-aware vimdiff panes.'; \
	print_help_line 'make propagate-release-branches' 'Replay the policy-selected Unofficial line-tip changes onto retained legacy EDK2 branches.'; \
	print_help_line 'make promote-unofficial-compatibility' 'Port the retained Unofficial compatibility source onto a newer EDK2 base.'; \
	print_help_line 'make promote-unofficial-release' 'Port a policy-selected Unofficial line onto a newer EDK2 base and record an exact checkpoint.'; \
	print_help_line 'make update-release-tags' 'Move source/unofficial/edk2/stable-* tags to matching release-branch heads after validation.'; \
	print_subtitle 'Variables:'; \
	print_help_variable 'TYPE=upstream|vendor' 'For integrate-source-release. upstream updates base component refs; vendor updates Radxa source refs or CIX-carried source layers.'; \
	print_help_variable 'COMPONENT=<name>' 'For TYPE=upstream: edk2, edk2-platforms, edk2-non-osi, tf-a, or op-tee.'; \
	print_help_variable 'VENDOR=radxa|cix' 'For TYPE=vendor. Radxa records vendor-published or ported EDK2 source trees; CIX updates the TF-A/OP-TEE release bundle.'; \
	print_help_variable 'RELEASE=<name>' 'For source integration: release, tag, or source version name.'; \
	print_help_variable 'REF=<ref>' 'Input ref/object for source integration, or source ref for verify-source-policy.'; \
	print_help_variable 'EDK2_REF=<ref>' 'For uplift-edk2-release. Explicit EDK2 upstream object to record instead of the matching release tag.'; \
	print_help_variable 'EDK2_PLATFORMS_REF=<ref>' 'For uplift-edk2-release. Explicit edk2-platforms companion object to record.'; \
	print_help_variable 'EDK2_NON_OSI_REF=<ref>' 'For uplift-edk2-release. Explicit edk2-non-osi companion object to record.'; \
	print_help_variable 'COMPATIBILITY_REF=<ref>' 'For uplift-edk2-release. Resolved retained EDK2 compatibility source-port commit from a conflict handoff.'; \
	print_help_variable 'EDK2_BASE=<release>' 'EDK2 base used when integrating Radxa vendor sources.'; \
	print_help_variable 'FROM_EDK2_BASE=<release>' 'Previous EDK2 base used when porting a Radxa or unofficial source tree to a newer EDK2 base.'; \
	print_help_variable 'ARM_BASE=<release>' 'Arm upstream base used when recording a CIX TF-A or OP-TEE component uplift.'; \
	print_help_variable 'RADXA_RELEASE=<release>' 'For uplift-edk2-release. Radxa release to carry forward.\nDefault: config/policies.json current_radxa_release.'; \
	print_help_variable 'FROM_RELEASE=<release>' 'For uplift-radxa-release. Previous Radxa release for the adjacent vendor delta.'; \
	print_help_variable 'TO_RELEASE=<release>' 'For uplift-radxa-release. New Radxa release to integrate.'; \
	print_help_variable 'LINE=<major.minor>' 'For EDK2 or Radxa uplift, the Unofficial development line to advance; for select-unofficial-line, the validated line to make the default.\nDefault for EDK2 uplift: policy default_line. Default for Radxa uplift: TO_RELEASE major.minor.'; \
	print_help_variable 'FROM_UNOFFICIAL_REF=<ref>' 'For uplift-radxa-release. Reviewed source that deliberately overrides the previous exact checkpoint, normally to initialise a new line.'; \
	print_help_variable 'PORT_REF=<ref>' 'For uplift-radxa-release. Resolved Radxa port commit from a conflict handoff.'; \
	print_help_variable 'UNOFFICIAL_REF_STAGE=auto|source|overlay|final' 'For uplift-radxa-release. Resume stage represented by UNOFFICIAL_REF; auto reads the conflict-stage trailer when available.\nDefault: auto.'; \
	print_help_variable 'RESOLVED_REF_STAGE=auto|source|overlay|final' 'For promote-unofficial-release. Resume stage represented by RESOLVED_REF; use final only for a reviewed complete tree.\nDefault: auto.'; \
	print_help_variable 'MAKE_DEFAULT=0|1' 'For uplift-radxa-release. Select the updated line as the default source target.'; \
	print_help_note 'After validating an existing line, use make select-unofficial-line LINE=<major.minor> [WRITE=1] to promote it without replaying the uplift.'; \
	print_help_variable 'CIX_RELEASE=<release>' 'For uplift-edk2-release. CIX release to use in the rendered source target.\nDefault: config/policies.json current_cix_release.'; \
	print_help_variable 'RADXA_SOURCE=auto|vendor|port' 'Select whether a Radxa integration is a vendor-published source tree or this project'\''s port to an EDK2 base.\nDefault: auto.'; \
	print_help_variable 'RADXA_REF=<ref>' 'For uplift-edk2-release. Resolved Radxa source-port commit from a conflict handoff.'; \
	print_help_variable 'UNOFFICIAL_REF=<ref>' 'For EDK2 or Radxa uplift. Resolved Unofficial source-port commit from a conflict handoff.'; \
	print_help_variable 'WRITE=0|1' 'Permit ref, tag, or policy updates in uplift-edk2-release, uplift-radxa-release, select-unofficial-line, integrate-source-release, import targets, propagation, and tag updates.'; \
	print_help_variable 'ALLOW_REPLACE=0|1' 'Allow an EDK2/Radxa uplift or source integration to replace an existing manifested source ref deliberately.'; \
	print_help_variable 'MATERIALISE=0|1' 'Flatten Radxa vendor refs before recording source/vendor or source/port refs.\nDefault: 1.'; \
	print_help_variable 'BASE_REF=<ref>' 'Base ref for extract-vendor-delta, or explicit override for import-changes/import-unofficial-commits when automatic inference is ambiguous.'; \
	print_help_variable 'VENDOR_REF=<ref>' 'Vendor ref for extract-vendor-delta.'; \
	print_help_variable 'FROM_REF=<ref>' 'Developer topic branch/ref for import-changes or import-unofficial-commits. Use import-changes for source/cache/**, legacy, or broader source trees.'; \
	print_help_variable 'OUTPUT=<path>' 'Optional extract-vendor-delta metadata output path.'; \
	print_help_variable 'PATCH_OUTPUT=<path>' 'Optional extract-vendor-delta patch output path.'; \
	print_help_variable 'SOURCE_UNOFFICIAL_REF=<ref>' 'Unofficial source branch for import-changes or import-unofficial-commits; defaults to the policy-selected source/unofficial/<line>/current ref.'; \
	print_help_variable 'PROPAGATE_RELEASE_BRANCHES=none|all' 'For import targets. Replay or apply the imported change onto every source/unofficial/edk2-stable* release branch after preparing all candidates safely.\nDefault: none.'; \
	print_help_variable 'UPDATE_RELEASE_TAGS=0|1' 'For import targets, only with PROPAGATE_RELEASE_BRANCHES=all. Move matching source/unofficial/edk2/stable-* tags after all requested release-branch imports succeed. The safer staged workflow is to run make update-release-tags separately after validation.\nDefault: 0.'; \
	print_help_variable 'SOURCE_LIFECYCLE_NORMALISE=off|validate|mirror|exact' 'For import targets and verify-source-lifecycle. Control deterministic overlay/source lifecycle handling when an imported overlay path points at a source file that moved or disappeared in another release branch. validate reports required rewrites without changing the scratch tree; mirror rewrites mirror symlinks only; exact also rewrites exact regular overlay renames.\nDefault: exact.'; \
	print_help_variable 'COMMIT_MESSAGE=<text>' 'For import-changes. Commit message for the extracted patch. Literal \\n sequences become separate git commit -m paragraphs.\nDefault: inherited from the FROM_REF tip commit.'; \
	print_help_variable 'COMMIT_MESSAGE_FILE=<path>' 'For import-changes. Read the imported patch commit message from a file. Mutually exclusive with COMMIT_MESSAGE.'; \
	print_help_variable 'SIGNOFF=0|1' 'For import-changes. Add a Signed-off-by trailer with git commit -s.\nDefault: 0.'; \
	print_help_variable 'CONTINUE=0|1' 'Continue a paused import operation after conflicts are resolved in the scratch tree.'; \
	print_help_variable 'ABORT=0|1' 'Abort a paused import operation and remove its scratch state without moving refs.'; \
	print_help_variable 'ABORT_ALL=0|1' 'For import-changes. Abort all paused import-changes operations and remove their scratch state without moving refs.'; \
	print_help_variable 'OP_ID=<id>' 'Paused import operation ID for CONTINUE=1 or ABORT=1.'; \
	print_help_variable 'IMPORT_TOOL=import-changes|import-unofficial' 'Optional operation namespace for inspect-import-conflicts when OP_ID is ambiguous.'; \
	print_help_variable 'SCRATCH=<path>' 'Optional scratch tree path for inspect-import-conflicts or resolve-conflicts when operating on a tree directly.'; \
	print_help_variable 'REPORT=<path>' 'Optional report path for inspect-import-conflicts SCRATCH mode.'; \
	print_help_variable 'CONFLICT_PATHS=<path[,path...]>' 'Optional logical conflict path filter for resolve-conflicts.'; \
	print_help_variable 'CONFLICT_EDITOR=<command>' 'Editor command for resolve-conflicts.\nDefault: vimdiff -f.'; \
	print_help_variable 'PRESERVE_SYMLINKS=0|1' 'For resolve-conflicts. If the resolved content exactly matches an expanded conflicted symlink target, restore the symlink rather than materialising a regular file.\nDefault: 1.'; \
	print_help_variable 'ALLOW_CONFLICT_MARKERS=0|1' 'For resolve-conflicts. Permit conflict-marker text in a resolved file.\nDefault: 0.'; \
	print_help_variable 'ALLOW_SOURCE_REF_FROM=0|1' 'Maintainer escape hatch allowing FROM_REF=source/unofficial/** with an explicit BASE_REF.\nDefault: 0.'; \
	print_help_variable 'UPDATE_CURRENT=0|1' 'For promote-unofficial-release. Move the selected source/unofficial/<line>/current ref to the promoted source tree.\nDefault: 1.'; \
	print_help_variable 'UPDATE_POLICY=0|1' 'For promote-unofficial-release. Update config/policies.json current_edk2_release.\nDefault: 1.'; \
	print_help_variable 'SKIP_RENDER=0|1' 'For EDK2 or Radxa uplift. Skip the rendered source-target refresh.\nDefault: 0.'; \
	print_help_variable 'VERIFY=0|1' 'For EDK2 or Radxa uplift. Run verify-build-matrix after rendering.\nDefault: 1.'; \
	print_help_variable 'TARGET_REF=<ref[,ref...]>' 'Optional comma-separated target refs for update-release-tags. Leave unset to check every source/unofficial/edk2-stable* release branch.'; \
	print_section 'Help Targets'; \
	print_help_line 'make extract-vendor-delta-help' 'Show extract-vendor-delta arguments.'; \
	print_help_line 'make uplift-edk2-release-help' 'Show uplift-edk2-release arguments.'; \
	print_help_line 'make integrate-source-release-help' 'Show integrate-source-release arguments.'; \
	print_help_line 'make import-changes-help' 'Show import-changes arguments.'; \
	print_help_line 'make import-unofficial-commits-help' 'Show import-unofficial-commits arguments.'; \
	print_help_line 'make inspect-import-conflicts-help' 'Show inspect-import-conflicts arguments.'; \
	print_help_line 'make resolve-conflicts-help' 'Show resolve-conflicts arguments.'; \
	print_help_line 'make promote-unofficial-compatibility-help' 'Show retained EDK2 compatibility promotion arguments.'; \
	print_help_line 'make promote-unofficial-release-help' 'Show promote-unofficial-release arguments.'; \
	print_help_line 'make update-release-tags-help' 'Show update-release-tags arguments.'

help-dev-verify:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	print_section 'Rendering and Qualification'; \
	print_help_line 'make render-release-branch' 'Resolve or create a materialised source/cache/release branch.'; \
	print_help_line 'make verify-release-branch' 'Validate a materialised firmware source-target branch.'; \
	print_help_line 'make verify-build-matrix' 'Validate build/source target combinations derived from source refs.'; \
	print_help_line 'make verify-manifest-integrity' 'Validate defaults-expanded tree-ID manifest records.'; \
	print_help_line 'make verify-source-policy' 'Validate shared source-tree policy checks, including overlay symlink rules.'; \
	print_help_line 'make verify-source-lifecycle' 'Validate deterministic overlay/source lifecycle projection across unofficial release branches.'; \
	print_section 'Source Metadata and Integrity'; \
	print_help_line 'make check-ref-integrity' 'Check persistent source refs do not depend on generated cache refs.'; \
	print_help_line 'make check-source-metadata' 'Check source-ref hashes, source-target cache tree IDs, and optionally unofficial release tags for drift.'; \
	print_help_line 'make refresh-source-metadata' 'Refresh source-ref hashes and source-target cache tree IDs from current refs.'; \
	print_help_line 'make check-identity-integrity' 'Quickly scan build-branch files for path/identity integrity issues.'; \
	print_help_line 'make verify-identity-integrity' 'Deep-scan build-branch files, commit metadata, and persistent source refs for path/identity integrity issues.'; \
	print_help_line 'make check-vendor-workflow-drift' 'Detect vendor .github/workflows changes that may need porting to the build branch CI.'; \
	print_help_line 'make refresh-vendor-workflow-baseline' 'Record reviewed vendor workflow snapshots after relevant CI changes are ported.'; \
	print_help_line 'make check-upstream-versions' 'Check recorded source refs and tooling pins against external upstream/vendor remotes.'; \
	print_subtitle 'Variables:'; \
	print_help_variable 'RELEASE=<source-target>' 'Firmware source-target name for render-release-branch and verify-release-branch.'; \
	print_help_variable 'FROM_REF=<ref>' 'Source ref for verify-source-lifecycle projection.\nDefault: the policy-selected source/unofficial/<line>/current ref.'; \
	print_help_variable 'TARGET_REF=<ref[,ref...]>' 'Optional comma-separated target refs for verify-source-lifecycle. Leave unset to check every source/unofficial/edk2-stable* release branch.'; \
	print_help_variable 'REF=<ref>' 'Optional source ref for verify-source-policy.'; \
	print_help_variable 'PERSIST=0|1' 'For render-release-branch: create or verify a named source/cache/release branch. Without PERSIST=1, build targets use existing refs or cached detached worktrees and do not create a Git cache branch.'; \
	print_help_variable 'REBUILD=0|1' 'Regenerate a rendered firmware source target from its render plan instead of reusing an existing ref.'; \
	print_help_variable 'FORCE=0|1' 'Allow an explicitly requested ref replacement or install overwrite after the target-specific safety checks pass.'; \
	print_help_variable 'WORKTREE=<path>' 'Existing rendered worktree to use for verify-release-branch history checks.'; \
	print_help_variable 'SOURCE_LIFECYCLE_NORMALISE=off|validate|mirror|exact' 'For verify-source-lifecycle. Control deterministic overlay/source lifecycle handling when an imported overlay path points at a source file that moved or disappeared in another release branch.\nDefault: exact.'; \
	print_help_variable 'SCAN_COMMITS=0|1' 'Legacy override for scripts/check_identity_integrity.py. The make check-identity-integrity target is intentionally quick; use make verify-identity-integrity for the full source-ref and commit scan.'; \
	print_help_variable 'SCAN_SOURCE_REFS=0|1' 'Legacy override for scripts/check_identity_integrity.py. The make check-identity-integrity target is intentionally quick; use make verify-identity-integrity for the full source-ref and commit scan.'; \
	print_help_variable 'REVIEWED=0|1' 'For refresh-vendor-workflow-baseline. Confirm vendor workflow changes were reviewed and relevant build-branch CI updates were ported.'; \
	print_help_variable 'UPSTREAM_VERSION_MODE=advisory|policy|strict' 'Version-check failure mode. advisory never fails on stale remotes; policy fails checks marked strict; strict fails stale or unavailable non-advisory checks while still reporting advisory drift.\nDefault: policy.'; \
	print_help_variable 'UPSTREAM_VERSION_ONLY=<id[,id...]>' 'Optional comma-separated source IDs, or source:release/source:commits comparison IDs, for check-upstream-versions.'; \
	print_help_variable 'UPSTREAM_VERSION_FORMAT=text|github|json' 'Upstream versions output format. github emits workflow annotations and a job summary table when available.\nDefault: text.'; \
	print_help_variable 'UPSTREAM_VERSION_SNAPSHOT=<path>' 'Offline remote-ref snapshot for upstream version tests.\nDefault: unset.'; \
	print_help_variable 'WRITE=0|1' 'For refresh-source-metadata. Required before config metadata or requested release tags are updated.\nDefault: 0.'; \
	print_help_variable 'RENDER_GENERATED=0|1' 'For check-source-metadata and refresh-source-metadata. Re-render generated source/cache/release entries whose tree cannot be derived directly from retained source refs. Use 1 for full post-rewrite cache regeneration.\nDefault: 0.'; \
	print_help_variable 'UPDATE_RELEASE_TAGS=0|1' 'For check-source-metadata and refresh-source-metadata. Include refs/tags/source/unofficial/edk2/stable-* in the check or refresh.\nDefault: 0.'; \
	print_section 'Help Targets'; \
	print_help_line 'make render-release-branch-help' 'Show render-release-branch arguments.'; \
	print_help_line 'make verify-release-branch-help' 'Show verify-release-branch arguments.'; \
	print_help_line 'make verify-build-matrix-help' 'Show verify-build-matrix arguments.'; \
	print_help_line 'make verify-source-policy-help' 'Show verify-source-policy arguments.'; \
	print_help_line 'make verify-source-lifecycle-help' 'Show verify-source-lifecycle arguments.'; \
	print_help_line 'make check-ref-integrity-help' 'Show check-ref-integrity arguments.'; \
	print_help_line 'make check-source-metadata-help' 'Show check-source-metadata arguments.'; \
	print_help_line 'make refresh-source-metadata-help' 'Show refresh-source-metadata arguments.'; \
	print_help_line 'make check-identity-integrity-help' 'Show check-identity-integrity arguments.'; \
	print_help_line 'make verify-identity-integrity-help' 'Show verify-identity-integrity arguments.'; \
	print_help_line 'make check-vendor-workflow-drift-help' 'Show check-vendor-workflow-drift arguments.'; \
	print_help_line 'make check-upstream-versions-help' 'Show check-upstream-versions arguments.'

help-dev-maintenance:
	@$(PRINT_HELP_SHELL_PROLOGUE); \
	print_section 'Minimised Repository'; \
	print_help_line 'make create-minimised-clone' 'Create a bare repo containing only build plus required non-cache source refs and tags.'; \
	print_help_line 'make verify-minimised-clone' 'Create and clone a minimised repository, then verify that it can render the default source target.'; \
	print_subtitle 'Variables:'; \
	print_help_variable 'DIR=<path>' 'Destination directory for create-minimised-clone, or optional workspace directory for verify-minimised-clone.'; \
	print_help_variable 'KEEP=0|1' 'Keep the temporary verification workspace created by verify-minimised-clone.\nDefault: 0.'; \
	print_help_variable 'REPACK=0|1' 'Repack the destination produced by create-minimised-clone.\nDefault: 1.'; \
	print_section 'Cache and Reports'; \
	print_help_line 'make ref-report' 'Report required source refs, generated cache refs, and ref namespace issues.'; \
	print_help_line 'make cleanup-report' 'Report generated cache refs and cautious clean-up guidance.'; \
	print_help_line 'make prune' 'Report generated source/cache refs, or delete them with DELETE=1 after safety checks.'; \
	print_subtitle 'Variables:'; \
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
	print_help_variable 'ACT_CONTAINER_ARCH=auto|<platform>' 'Container architecture used by act. auto selects linux/arm64 on arm64 hosts and linux/amd64 on x86_64 hosts.\nDefault: auto.'; \
	print_help_variable 'ACT_RUNNER_IMAGE=<image>' 'Runner image mapped to ubuntu-latest.\nDefault: ghcr.io/catthehacker/ubuntu:act-24.04-20260508.'; \
	print_help_note 'For workflow_dispatch inputs and matrix examples, see docs/src/build.md, "Test GitHub Actions Locally".'; \
	print_section 'Documentation'; \
	print_help_line 'make docs-build' 'Build product docs. DOCS_BUILD_MODE=auto uses host tools when available and otherwise delegates to the internal docs container workflow.'; \
	print_subtitle 'Variables:'; \
	print_help_variable 'DOCS_BUILD_MODE=auto|host|container' 'Documentation build environment. auto uses host tools when available and the docs container otherwise.\nDefault: auto.'; \
	print_section 'Quality'; \
	print_help_line 'make test' 'Run build-branch tests in the quality container.'; \
	print_help_line 'make test-local' 'Run the self-contained build-branch test gate directly on the host.'; \
	print_help_line 'make lint' 'Run JSON, YAML, Markdown, shell, and Python linting in the quality container.'; \
	print_subtitle 'Variables:'; \
	print_help_variable 'QUALITY_IMAGE=<name>' 'Container image tag used by make test and make lint.\nDefault: edk2-cix-build-quality:latest.'; \
	print_section 'Common Variables'; \
	print_help_variable 'V=0|1' 'Verbosity. V=0 is concise; V=1 shows script/build detail.\nDefault: 0.'; \
	print_help_variable 'DEBUG=0|1' 'Show Python tracebacks for unexpected tooling failures.\nDefault: 0.'; \
	print_section 'Help Targets'; \
	print_help_line 'make create-minimised-clone-help' 'Show create-minimised-clone arguments.'; \
	print_help_line 'make verify-minimised-clone-help' 'Show verify-minimised-clone arguments.'; \
	print_help_line 'make prune-help' 'Show prune arguments.'; \
	print_help_line 'make ref-report-help' 'Show ref-report arguments.'; \
	print_help_line 'make cleanup-report-help' 'Show cleanup-report arguments.'

help-source-targets:
	@DEBUG="$(DEBUG)" $(PYTHON) scripts/help_cache.py --print-source-targets

BUILD_VARIABLE_ENV = DEBUG="$(DEBUG)" RELEASE="$(RELEASE)" V="$(V)" SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" ARTEFACT_MODE="$(ARTEFACT_MODE)" FIRMWARE_BOARD="$(FIRMWARE_BOARD)" FIRMWARE_PRODUCT="$(FIRMWARE_PRODUCT)" FIRMWARE_TARGET="$(FIRMWARE_TARGET)" FIRMWARE_DISTRO="$(FIRMWARE_DISTRO)" FIRMWARE_VALIDATE_ON_BUILD="$(FIRMWARE_VALIDATE_ON_BUILD)" BUILDBOX_PLATFORM="$(BUILDBOX_PLATFORM)" ENABLE_FIRMWARE_FIXES="$(ENABLE_FIRMWARE_FIXES)" ENABLE_CORE_ORDER="$(ENABLE_CORE_ORDER)" ENABLE_EXPERIMENTAL_UEFI_SETTINGS="$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS)" DEBUG_ON_UART3="$(DEBUG_ON_UART3)" UART3_ENABLE="$(UART3_ENABLE)" DEBUG_VERBOSE="$(DEBUG_VERBOSE)" DEBUG_PRINT_ERROR_LEVEL="$(DEBUG_PRINT_ERROR_LEVEL)" CIX_RELEASE="$(CIX_RELEASE)" FORCE="$(FORCE)"

DELEGATED_BUILD_ARGS = V="$(V)" ARTEFACT_MODE="$(ARTEFACT_MODE)" FIRMWARE_BOARD="$(FIRMWARE_BOARD)" FIRMWARE_PRODUCT="$(FIRMWARE_PRODUCT)" FIRMWARE_TARGET="$(FIRMWARE_TARGET)" FIRMWARE_DISTRO="$(FIRMWARE_DISTRO)" FIRMWARE_VALIDATE_ON_BUILD="$(FIRMWARE_VALIDATE_ON_BUILD)" BUILDBOX_PLATFORM="$(BUILDBOX_PLATFORM)" ENABLE_FIRMWARE_FIXES="$(ENABLE_FIRMWARE_FIXES)" ENABLE_CORE_ORDER="$(ENABLE_CORE_ORDER)" ENABLE_EXPERIMENTAL_UEFI_SETTINGS="$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS)" DEBUG_ON_UART3="$(DEBUG_ON_UART3)" UART3_ENABLE="$(UART3_ENABLE)" DEBUG_VERBOSE="$(DEBUG_VERBOSE)" DEBUG_PRINT_ERROR_LEVEL="$(DEBUG_PRINT_ERROR_LEVEL)" CIX_RELEASE="$(CIX_RELEASE)"

define run_release_make
	@set -e; \
	printf '[build] Resolving source target for %s\n' "$(1)" >&2; \
	if [ "$(FIRST_OUTPUT_PROBE)" = "1" ]; then exit 0; fi; \
	release_label="$(RELEASE)"; \
	if [ -z "$$release_label" ]; then release_label="$$(DEBUG="$(DEBUG)" $(PYTHON) scripts/help_cache.py --print-default-release 2>/dev/null || printf '%s' '<default source target>')"; fi; \
	printf '[build] Preparing release worktree: %s\n' "$$release_label" >&2; \
	$(BUILD_VARIABLE_ENV) $(PYTHON) scripts/validate_build_variables.py --target "$(1)"; \
	wt="$$(DEBUG="$(DEBUG)" RELEASE="$(RELEASE)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --ensure-worktree --print-worktree --v "$(V)")"; \
	signing_cert_arg="$$(DEBUG="$(DEBUG)" SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" V="$(V)" $(PYTHON) scripts/prepare_release_worktree.py --worktree "$$wt" --print-make-arg --v "$(V)")"; \
	cache_root="$(FIRMWARE_CACHE_ROOT)"; \
	container_cache_root="/hosttmp"; \
	printf '[build] Starting %s: board=%s target=%s release=%s\n' "$(1)" "$(FIRMWARE_BOARD)" "$(FIRMWARE_TARGET)" "$$release_label" >&2; \
	if [ "$(V)" = "1" ]; then printf '%s\n' "$(MAKE) --no-print-directory -C $$wt $(1) $(DELEGATED_BUILD_ARGS) BUILDBOX_HOST_TMPDIR=$$cache_root/buildbox CCACHE_DIR=$$container_cache_root/ccache"; fi; \
	EDK2_CIX_BUILDBOX_NAME_FILE="$$cache_root/buildbox/buildbox-name" \
	$(MAKE) --no-print-directory -C "$$wt" $(1) $(DELEGATED_BUILD_ARGS) $$signing_cert_arg \
		BUILDBOX_HOST_TMPDIR="$$cache_root/buildbox" \
		BUILDBOX_CONTAINER_TMPDIR="$$container_cache_root" \
		CCACHE_DIR="$$container_cache_root/ccache" \
		CCACHE_WRAPPER_ROOT="$$container_cache_root/ccache-toolchain" \
		CIX_RELEASE_CACHE_ROOT="$$container_cache_root/cix-release" \
		FIPTOOL_DISTRO_STAMP="$$cache_root/buildbox/fiptool/.buildbox-distro" \
		BUILD_LOG_ROOT="$$cache_root/build-logs" \
		FIRMWARE_VALIDATION_REPORT_ROOT="$$cache_root/build-validation"; \
	DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/mirror_build_outputs.py \
		--repo-root "$(CURDIR)" \
		--worktree "$$wt" \
		--dist-root "$(BUILD_DIST_ROOT)" \
		--release "$$release_label" \
		--build-target "$(1)" \
		--board "$(FIRMWARE_BOARD)" \
		--firmware-target "$(FIRMWARE_TARGET)" \
		--artefact-mode "$(ARTEFACT_MODE)" \
		--v "$(V)"
endef

build:
	$(call run_release_make,buildbox-firmware-build)

build-all:
	$(call run_release_make,build-all)

deterministic-replay:
	@set -eu; \
	printf '[replay] Preparing replay-capable source target: %s\n' "$(REPLAY_SOURCE_TARGET)" >&2; \
	if [ "$(FIRST_OUTPUT_PROBE)" = "1" ]; then exit 0; fi; \
	cache_root="$(abspath $(FIRMWARE_CACHE_ROOT))"; \
	replay_input="$(REPLAY_INPUT)"; \
	replay_build_options="$(REPLAY_BUILD_OPTIONS)"; \
	replay_build_date="$(REPLAY_BUILD_DATE)"; \
	replay_version="$(REPLAY_VERSION)"; \
	mkdir -p "$$cache_root/buildbox" "$$cache_root/replay/downloads"; \
	wt="$$(DEBUG="$(DEBUG)" RELEASE="$(REPLAY_SOURCE_TARGET)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --ensure-worktree --print-worktree --v "$(V)")"; \
	if [ -n "$$replay_input" ]; then \
		replay_input="$$( $(PYTHON) -c 'from pathlib import Path; import sys; print(Path(sys.argv[1]).expanduser().resolve())' "$$replay_input" )"; \
	elif [ "$(REPLAY_DOWNLOAD)" != "0" ]; then \
		release_json="$$cache_root/replay/release-$$replay_version.json"; \
		printf '[replay] Resolving release %s from %s\n' "$$replay_version" "$(REPLAY_UPSTREAM_REPOSITORY)" >&2; \
		$(PYTHON) scripts/resolve_release_asset.py \
			--github-repository "$(REPLAY_UPSTREAM_REPOSITORY)" \
			--tag "$$replay_version" \
			>"$$release_json"; \
		asset_name="$$( $(PYTHON) -c 'import json, sys; print(json.load(open(sys.argv[1], encoding="utf-8"))[sys.argv[2]])' "$$release_json" asset_name )"; \
		asset_url="$$( $(PYTHON) -c 'import json, sys; print(json.load(open(sys.argv[1], encoding="utf-8"))[sys.argv[2]])' "$$release_json" asset_download_url )"; \
		if [ -z "$$asset_url" ] || [ "$$asset_url" = "None" ]; then \
			printf '[replay] Resolved release %s did not include a download URL\n' "$$replay_version" >&2; \
			exit 2; \
		fi; \
		replay_input="$$cache_root/replay/downloads/$$asset_name"; \
		if [ ! -f "$$replay_input" ]; then \
			printf '[replay] Downloading %s\n' "$$asset_name" >&2; \
			curl --fail --location --retry 5 --retry-delay 2 "$$asset_url" --output "$$replay_input"; \
		else \
			printf '[replay] Reusing downloaded release package: %s\n' "$$replay_input" >&2; \
		fi; \
	else \
		printf '[replay] REPLAY_DOWNLOAD=0 and REPLAY_INPUT is unset; rendered target will reuse cached replay inputs if present.\n' >&2; \
	fi; \
	if [ -n "$$replay_build_options" ]; then \
		replay_build_options="$$( $(PYTHON) -c 'from pathlib import Path; import sys; print(Path(sys.argv[1]).expanduser().resolve())' "$$replay_build_options" )"; \
	fi; \
	replay_key="$(FIRMWARE_BOARD)-$${replay_version:-manual}"; \
	printf '[replay] Running deterministic replay in %s\n' "$$wt" >&2; \
	EDK2_CIX_BUILDBOX_NAME_FILE="$$cache_root/buildbox/buildbox-name" \
	$(MAKE) --no-print-directory -C "$$wt" deterministic-replay V="$(V)" \
		FIRMWARE_BOARD="$(FIRMWARE_BOARD)" \
		FIRMWARE_TARGET="$(FIRMWARE_TARGET)" \
		FIRMWARE_DISTRO="$(if $(strip $(FIRMWARE_DISTRO)),$(FIRMWARE_DISTRO),bookworm)" \
		BUILDBOX_PLATFORM="$(BUILDBOX_PLATFORM)" \
		BUILDBOX_HOST_TMPDIR="$$cache_root/buildbox/tmp" \
		CCACHE_DIR="/hosttmp/ccache" \
		CCACHE_WRAPPER_ROOT="/hosttmp/ccache-toolchain" \
		CIX_RELEASE_CACHE_ROOT="/hosttmp/cix-release" \
		FIPTOOL_DISTRO_STAMP="$$cache_root/buildbox/fiptool/.buildbox-distro" \
		FIRMWARE_VALIDATION_REPORT_ROOT="$$cache_root/build-validation" \
		DETERMINISTIC_REPLAY_ROOT="$$cache_root/buildbox/replay/$$replay_key" \
		DETERMINISTIC_REPLAY_MOUNT_ROOT="$$cache_root/buildbox" \
		REPLAY_VERSION="$$replay_version" \
		REPLAY_INPUT="$$replay_input" \
		REPLAY_BUILD_OPTIONS="$$replay_build_options" \
		REPLAY_BUILD_DATE="$$replay_build_date"

install:
	@set -e; \
	printf '[build] Resolving source target for install\n' >&2; \
	if [ "$(FIRST_OUTPUT_PROBE)" = "1" ]; then exit 0; fi; \
	release_label="$(RELEASE)"; \
	if [ -z "$$release_label" ]; then release_label="$$(DEBUG="$(DEBUG)" $(PYTHON) scripts/help_cache.py --print-default-release 2>/dev/null || printf '%s' '<default source target>')"; fi; \
	printf '[build] Preparing release worktree: %s\n' "$$release_label" >&2; \
	$(BUILD_VARIABLE_ENV) $(PYTHON) scripts/validate_build_variables.py --target "install"; \
	wt="$$(DEBUG="$(DEBUG)" RELEASE="$(RELEASE)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --ensure-worktree --print-worktree --v "$(V)")"; \
	signing_cert_arg="$$(DEBUG="$(DEBUG)" SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" V="$(V)" $(PYTHON) scripts/prepare_release_worktree.py --worktree "$$wt" --print-make-arg --v "$(V)")"; \
	cache_root="$(FIRMWARE_CACHE_ROOT)"; \
	container_cache_root="/hosttmp"; \
	printf '[build] Starting buildbox-firmware-stage: board=%s target=%s release=%s\n' "$(FIRMWARE_BOARD)" "$(FIRMWARE_TARGET)" "$$release_label" >&2; \
	if [ "$(V)" = "1" ]; then printf '%s\n' "$(MAKE) --no-print-directory -C $$wt buildbox-firmware-stage $(DELEGATED_BUILD_ARGS) BUILDBOX_HOST_TMPDIR=$$cache_root/buildbox CCACHE_DIR=$$container_cache_root/ccache"; fi; \
	EDK2_CIX_BUILDBOX_NAME_FILE="$$cache_root/buildbox/buildbox-name" \
	$(MAKE) --no-print-directory -C "$$wt" buildbox-firmware-stage $(DELEGATED_BUILD_ARGS) $$signing_cert_arg \
		BUILDBOX_HOST_TMPDIR="$$cache_root/buildbox" \
		BUILDBOX_CONTAINER_TMPDIR="$$container_cache_root" \
		CCACHE_DIR="$$container_cache_root/ccache" \
		CCACHE_WRAPPER_ROOT="$$container_cache_root/ccache-toolchain" \
		CIX_RELEASE_CACHE_ROOT="$$container_cache_root/cix-release" \
		FIPTOOL_DISTRO_STAMP="$$cache_root/buildbox/fiptool/.buildbox-distro" \
		BUILD_LOG_ROOT="$$cache_root/build-logs" \
		FIRMWARE_VALIDATION_REPORT_ROOT="$$cache_root/build-validation"; \
	DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/mirror_build_outputs.py \
		--repo-root "$(CURDIR)" \
		--worktree "$$wt" \
		--dist-root "$(BUILD_DIST_ROOT)" \
		--release "$$release_label" \
		--build-target "buildbox-firmware-stage" \
		--board "$(FIRMWARE_BOARD)" \
		--firmware-target "$(FIRMWARE_TARGET)" \
		--artefact-mode "$(ARTEFACT_MODE)" \
		--v "$(V)"; \
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
	$(call PROGRESS_PROBE,[clean] Removing stale filesystem cache entries)
	@DEBUG="$(DEBUG)" FORCE="$(FORCE)" V="$(V)" $(PYTHON) scripts/clean_cache.py --mode stale --force "$(FORCE)" --v "$(V)"

realclean:
	$(call PROGRESS_PROBE,[clean] Removing all filesystem cache entries)
	@DEBUG="$(DEBUG)" FORCE="$(FORCE)" V="$(V)" $(PYTHON) scripts/clean_cache.py --mode all --force "$(FORCE)" --v "$(V)"

prune:
	$(call PROGRESS_PROBE,[prune] Checking generated source/cache refs)
	@DEBUG="$(DEBUG)" DELETE="$(DELETE)" V="$(V)" $(PYTHON) scripts/prune_cache_refs.py --delete "$(DELETE)" --v "$(V)"

create-minimised-clone:
	@if [ -z "$(DIR)" ]; then $(PYTHON) scripts/create_minimised_clone.py --help; printf '%s\n' 'missing required variable: DIR' >&2; exit 2; fi
	$(call PROGRESS_PROBE,[repo] Creating minimised clone in $(DIR))
	@DEBUG="$(DEBUG)" DIR="$(DIR)" REPACK="$(REPACK)" V="$(V)" $(PYTHON) scripts/create_minimised_clone.py --dir "$(DIR)" --repack "$(REPACK)" --v "$(V)"

buildbox-firmware-build:
	$(call run_release_make,buildbox-firmware-build)

buildbox-firmware-stage:
	$(call run_release_make,buildbox-firmware-stage)

docs-build:
	$(call PROGRESS_PROBE,[docs] Starting documentation build)
	@DOCS_BUILD_MODE="$(DOCS_BUILD_MODE)" docs/scripts/run_docs_build.sh

docs-workflow-local:
	$(call PROGRESS_PROBE,[docs] Starting local documentation workflow)
	@docs/scripts/run_docs_workflow_local.sh

render-release-branch:
	@if [ -z "$(RELEASE)" ]; then $(MAKE) --no-print-directory render-release-branch-help; printf '%s\n' 'missing required variable: RELEASE' >&2; exit 2; fi
	$(call PROGRESS_PROBE,[render] Resolving source target: $(RELEASE))
	@DEBUG="$(DEBUG)" RELEASE="$(RELEASE)" PERSIST="$(PERSIST)" REBUILD="$(REBUILD)" FORCE="$(FORCE)" V="$(V)" $(PYTHON) scripts/render_release_branch.py --require-release --persist "$(PERSIST)" --rebuild "$(REBUILD)" --force "$(FORCE)" --v "$(V)"

verify-release-branch:
	@if [ -z "$(RELEASE)" ]; then $(MAKE) --no-print-directory verify-release-branch-help; printf '%s\n' 'missing required variable: RELEASE' >&2; exit 2; fi
	$(call PROGRESS_PROBE,[verify] Checking release branch: $(RELEASE))
	@DEBUG="$(DEBUG)" RELEASE="$(RELEASE)" WORKTREE="$(WORKTREE)" V="$(V)" $(PYTHON) scripts/verify_release_branch.py --v "$(V)"

verify-build-matrix:
	$(call PROGRESS_PROBE,[verify] Checking build matrix)
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/verify_build_matrix.py --v "$(V)"

verify-manifest-integrity:
	$(call PROGRESS_PROBE,[verify] Checking manifest integrity)
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/verify_manifest_integrity.py --v "$(V)"

verify-source-policy:
	$(call PROGRESS_PROBE,[verify] Checking source policy)
	@DEBUG="$(DEBUG)" REF="$(REF)" V="$(V)" $(PYTHON) scripts/verify_source_policy.py --ref "$(REF)" --v "$(V)"

verify-source-lifecycle:
	$(call PROGRESS_PROBE,[verify] Checking source lifecycle)
	@DEBUG="$(DEBUG)" FROM_REF="$(FROM_REF)" TARGET_REF="$(TARGET_REF)" SOURCE_LIFECYCLE_NORMALISE="$(SOURCE_LIFECYCLE_NORMALISE)" V="$(V)" $(PYTHON) scripts/verify_source_lifecycle.py --from-ref "$(FROM_REF)" --target-ref "$(TARGET_REF)" --normalise-mode "$(SOURCE_LIFECYCLE_NORMALISE)" --v "$(V)"

check-ref-integrity:
	$(call PROGRESS_PROBE,[check] Checking ref integrity)
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/check_ref_integrity.py --v "$(V)"

verify-minimised-clone:
	$(call PROGRESS_PROBE,[verify] Checking minimised clone reconstruction)
	@DEBUG="$(DEBUG)" DIR="$(DIR)" KEEP="$(KEEP)" REPACK="$(REPACK)" V="$(V)" $(PYTHON) scripts/verify_minimised_clone.py --dir "$(DIR)" --keep "$(KEEP)" --repack "$(REPACK)" --v "$(V)"

extract-vendor-delta:
	$(call PROGRESS_PROBE,[delta] Extracting vendor delta)
	@DEBUG="$(DEBUG)" VENDOR="$(VENDOR)" BASE_REF="$(BASE_REF)" VENDOR_REF="$(VENDOR_REF)" OUTPUT="$(OUTPUT)" PATCH_OUTPUT="$(PATCH_OUTPUT)" TARGET_REF="$(TARGET_REF)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/extract_vendor_delta.py --v "$(V)"

uplift-edk2-release: FORCE = 1
uplift-edk2-release:
	@if [ -z "$(EDK2_BASE)" ]; then $(MAKE) --no-print-directory uplift-edk2-release-help; printf '%s\n' 'missing required variable: EDK2_BASE' >&2; exit 2; fi
	$(call PROGRESS_PROBE,[uplift] Starting EDK2 release uplift)
	@DEBUG="$(DEBUG)" EDK2_BASE="$(EDK2_BASE)" FROM_EDK2_BASE="$(FROM_EDK2_BASE)" RADXA_RELEASE="$(RADXA_RELEASE)" LINE="$(LINE)" CIX_RELEASE="$(CIX_RELEASE)" EDK2_REF="$(EDK2_REF)" EDK2_PLATFORMS_REF="$(EDK2_PLATFORMS_REF)" EDK2_NON_OSI_REF="$(EDK2_NON_OSI_REF)" COMPATIBILITY_REF="$(COMPATIBILITY_REF)" RADXA_REF="$(RADXA_REF)" UNOFFICIAL_REF="$(UNOFFICIAL_REF)" FROM_REF="$(FROM_REF)" RELEASE="$(RELEASE)" SKIP_RENDER="$(SKIP_RENDER)" VERIFY="$(VERIFY)" WRITE="$(WRITE)" FORCE="$(FORCE)" ALLOW_REPLACE="$(ALLOW_REPLACE)" V="$(V)" $(PYTHON) scripts/uplift_edk2_release.py --v "$(V)"

uplift-radxa-release:
	@if [ -z "$(FROM_RELEASE)" ] || [ -z "$(TO_RELEASE)" ]; then $(MAKE) --no-print-directory uplift-radxa-release-help; printf '%s\n' 'missing required variable(s): FROM_RELEASE TO_RELEASE' >&2; exit 2; fi
	$(call PROGRESS_PROBE,[uplift] Starting Radxa release uplift)
	@DEBUG="$(DEBUG)" FROM_RELEASE="$(FROM_RELEASE)" TO_RELEASE="$(TO_RELEASE)" LINE="$(LINE)" EDK2_BASE="$(EDK2_BASE)" CIX_RELEASE="$(CIX_RELEASE)" FROM_UNOFFICIAL_REF="$(FROM_UNOFFICIAL_REF)" PORT_REF="$(PORT_REF)" UNOFFICIAL_REF="$(UNOFFICIAL_REF)" UNOFFICIAL_REF_STAGE="$(UNOFFICIAL_REF_STAGE)" MAKE_DEFAULT="$(MAKE_DEFAULT)" SKIP_RENDER="$(SKIP_RENDER)" VERIFY="$(VERIFY)" WRITE="$(WRITE)" ALLOW_REPLACE="$(ALLOW_REPLACE)" V="$(V)" $(PYTHON) scripts/uplift_radxa_release.py --v "$(V)"

select-unofficial-line:
	@if [ -z "$(LINE)" ]; then $(MAKE) --no-print-directory select-unofficial-line-help; printf '%s\n' 'missing required variable: LINE' >&2; exit 2; fi
	$(call PROGRESS_PROBE,[select] Selecting unofficial default line)
	@DEBUG="$(DEBUG)" LINE="$(LINE)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/select_unofficial_line.py --v "$(V)"

integrate-source-release:
	$(call PROGRESS_PROBE,[integrate] Starting source integration)
	@DEBUG="$(DEBUG)" TYPE="$(TYPE)" COMPONENT="$(COMPONENT)" VENDOR="$(VENDOR)" RELEASE="$(RELEASE)" EDK2_BASE="$(EDK2_BASE)" FROM_EDK2_BASE="$(FROM_EDK2_BASE)" REF="$(REF)" RADXA_SOURCE="$(RADXA_SOURCE)" WRITE="$(WRITE)" ALLOW_REPLACE="$(ALLOW_REPLACE)" MATERIALISE="$(or $(MATERIALISE),1)" V="$(V)" $(PYTHON) scripts/integrate_source_release.py --v "$(V)"

import-unofficial-commits:
	$(call PROGRESS_PROBE,[import-unofficial] Starting unofficial import)
	@if [ -n "$(PROPAGATE_CHECKPOINTS)" ]; then printf '%s\n' 'PROPAGATE_CHECKPOINTS was renamed to PROPAGATE_RELEASE_BRANCHES; use PROPAGATE_RELEASE_BRANCHES=all.' >&2; exit 2; fi
	@DEBUG="$(DEBUG)" FROM_REF="$(FROM_REF)" BASE_REF="$(BASE_REF)" SOURCE_UNOFFICIAL_REF="$(SOURCE_UNOFFICIAL_REF)" PROPAGATE_RELEASE_BRANCHES="$(PROPAGATE_RELEASE_BRANCHES)" UPDATE_RELEASE_TAGS="$(UPDATE_RELEASE_TAGS)" SOURCE_LIFECYCLE_NORMALISE="$(SOURCE_LIFECYCLE_NORMALISE)" ALLOW_SOURCE_REF_FROM="$(ALLOW_SOURCE_REF_FROM)" CONTINUE="$(CONTINUE)" ABORT="$(ABORT)" OP_ID="$(OP_ID)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/import_unofficial_commits.py --v "$(V)"

import-changes:
	$(if $(filter 1 true yes on,$(ABORT) $(ABORT_ALL)),,$(call PROGRESS_PROBE,[import-changes] Starting change import))
	@if [ -n "$(PROPAGATE_CHECKPOINTS)" ]; then printf '%s\n' 'PROPAGATE_CHECKPOINTS was renamed to PROPAGATE_RELEASE_BRANCHES; use PROPAGATE_RELEASE_BRANCHES=all.' >&2; exit 2; fi
	@DEBUG="$(DEBUG)" FROM_REF="$(FROM_REF)" BASE_REF="$(BASE_REF)" SOURCE_UNOFFICIAL_REF="$(SOURCE_UNOFFICIAL_REF)" PROPAGATE_RELEASE_BRANCHES="$(PROPAGATE_RELEASE_BRANCHES)" UPDATE_RELEASE_TAGS="$(UPDATE_RELEASE_TAGS)" COMMIT_MESSAGE="$(COMMIT_MESSAGE)" COMMIT_MESSAGE_FILE="$(COMMIT_MESSAGE_FILE)" SIGNOFF="$(SIGNOFF)" SOURCE_LIFECYCLE_NORMALISE="$(SOURCE_LIFECYCLE_NORMALISE)" CONTINUE="$(CONTINUE)" ABORT="$(ABORT)" ABORT_ALL="$(ABORT_ALL)" OP_ID="$(OP_ID)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/import_changes.py --v "$(V)"

inspect-import-conflicts:
	$(call PROGRESS_PROBE,[inspect] Inspecting import conflicts)
	@DEBUG="$(DEBUG)" OP_ID="$(OP_ID)" IMPORT_TOOL="$(IMPORT_TOOL)" SCRATCH="$(SCRATCH)" REPORT="$(REPORT)" V="$(V)" $(PYTHON) scripts/inspect_import_conflicts.py

resolve-conflicts:
	$(call PROGRESS_PROBE,[resolve] Starting conflict resolver)
	@DEBUG="$(DEBUG)" OP_ID="$(OP_ID)" IMPORT_TOOL="$(IMPORT_TOOL)" SCRATCH="$(SCRATCH)" CONFLICT_PATHS="$(CONFLICT_PATHS)" CONFLICT_EDITOR="$(CONFLICT_EDITOR)" PRESERVE_SYMLINKS="$(PRESERVE_SYMLINKS)" ALLOW_CONFLICT_MARKERS="$(ALLOW_CONFLICT_MARKERS)" V="$(V)" $(PYTHON) scripts/resolve_import_conflicts.py --v "$(V)"

propagate-release-branches:
	$(call PROGRESS_PROBE,[propagate] Propagating the selected Unofficial line tip to retained legacy EDK2 branches)
	@DEBUG="$(DEBUG)" FROM_REF="$(FROM_REF)" BASE_REF="$(BASE_REF)" SOURCE_UNOFFICIAL_REF="$(SOURCE_UNOFFICIAL_REF)" PROPAGATE_RELEASE_BRANCHES="all" UPDATE_RELEASE_TAGS="0" SOURCE_LIFECYCLE_NORMALISE="$(SOURCE_LIFECYCLE_NORMALISE)" ALLOW_SOURCE_REF_FROM="1" CONTINUE="$(CONTINUE)" ABORT="$(ABORT)" OP_ID="$(OP_ID)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/import_unofficial_commits.py --v "$(V)"

promote-unofficial-compatibility:
	@if [ -z "$(EDK2_BASE)" ] || [ -z "$(FROM_EDK2_BASE)" ]; then $(MAKE) --no-print-directory promote-unofficial-compatibility-help; printf '%s\n' 'missing required variable(s): EDK2_BASE FROM_EDK2_BASE' >&2; exit 2; fi
	$(call PROGRESS_PROBE,[promote] Promoting retained EDK2 compatibility source to $(EDK2_BASE))
	@DEBUG="$(DEBUG)" EDK2_BASE="$(EDK2_BASE)" FROM_EDK2_BASE="$(FROM_EDK2_BASE)" FROM_REF="$(FROM_REF)" RESOLVED_REF="$(or $(RESOLVED_REF),$(REF))" ALLOW_REPLACE="$(ALLOW_REPLACE)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/promote_unofficial_compatibility.py --v "$(V)"

promote-unofficial-release:
	@if [ -z "$(EDK2_BASE)" ] || [ -z "$(FROM_EDK2_BASE)" ]; then $(MAKE) --no-print-directory promote-unofficial-release-help; printf '%s\n' 'missing required variable(s): EDK2_BASE FROM_EDK2_BASE' >&2; exit 2; fi
	$(call PROGRESS_PROBE,[promote] Promoting unofficial source to $(EDK2_BASE))
	@DEBUG="$(DEBUG)" EDK2_BASE="$(EDK2_BASE)" FROM_EDK2_BASE="$(FROM_EDK2_BASE)" RADXA_RELEASE="$(RADXA_RELEASE)" LINE="$(LINE)" CIX_RELEASE="$(CIX_RELEASE)" FROM_REF="$(FROM_REF)" RESOLVED_REF="$(or $(RESOLVED_REF),$(REF))" RESOLVED_REF_STAGE="$(RESOLVED_REF_STAGE)" UPDATE_CURRENT="$(or $(UPDATE_CURRENT),1)" UPDATE_POLICY="$(or $(UPDATE_POLICY),1)" ALLOW_REPLACE="$(ALLOW_REPLACE)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/promote_unofficial_release.py --v "$(V)"

update-release-tags:
	$(call PROGRESS_PROBE,[tags] Checking unofficial release tags)
	@DEBUG="$(DEBUG)" TARGET_REF="$(TARGET_REF)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/update_release_tags.py --v "$(V)"

check-identity-integrity:
	$(call PROGRESS_PROBE,[check] Checking identity integrity)
	@DEBUG="$(DEBUG)" SCAN_COMMITS="0" SCAN_SOURCE_REFS="0" V="$(V)" $(PYTHON) scripts/check_identity_integrity.py --v "$(V)"

verify-identity-integrity:
	$(call PROGRESS_PROBE,[verify] Checking identity integrity)
	@DEBUG="$(DEBUG)" SCAN_COMMITS="1" SCAN_SOURCE_REFS="1" V="$(V)" $(PYTHON) scripts/check_identity_integrity.py --v "$(V)"

check-vendor-workflow-drift:
	$(call PROGRESS_PROBE,[check] Checking vendor workflow drift)
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/check_vendor_workflow_drift.py --v "$(V)"

refresh-vendor-workflow-baseline:
	$(call PROGRESS_PROBE,[metadata] Refreshing reviewed vendor workflow baseline)
	@DEBUG="$(DEBUG)" REVIEWED="$(REVIEWED)" WRITE="$(WRITE)" V="$(V)" $(PYTHON) scripts/check_vendor_workflow_drift.py --refresh 1 --reviewed "$(REVIEWED)" --write "$(WRITE)" --v "$(V)"

check-upstream-versions:
	$(call PROGRESS_PROBE,[check] Checking upstream versions)
	@DEBUG="$(DEBUG)" UPSTREAM_VERSION_MODE="$(UPSTREAM_VERSION_MODE)" UPSTREAM_VERSION_ONLY="$(UPSTREAM_VERSION_ONLY)" UPSTREAM_VERSION_FORMAT="$(UPSTREAM_VERSION_FORMAT)" UPSTREAM_VERSION_SNAPSHOT="$(UPSTREAM_VERSION_SNAPSHOT)" V="$(V)" $(PYTHON) scripts/check_upstream_versions.py --v "$(V)"

check-source-metadata:
	$(call PROGRESS_PROBE,[check] Checking source metadata refresh state)
	@DEBUG="$(DEBUG)" CHECK="1" WRITE="0" RENDER_GENERATED="$(RENDER_GENERATED)" UPDATE_RELEASE_TAGS="$(UPDATE_RELEASE_TAGS)" V="$(V)" $(PYTHON) scripts/refresh_source_metadata.py --check 1 --write 0 --render-generated "$(RENDER_GENERATED)" --update-release-tags "$(UPDATE_RELEASE_TAGS)" --v "$(V)"

refresh-source-metadata:
	$(call PROGRESS_PROBE,[metadata] Refreshing source metadata)
	@DEBUG="$(DEBUG)" WRITE="$(WRITE)" RENDER_GENERATED="$(RENDER_GENERATED)" UPDATE_RELEASE_TAGS="$(UPDATE_RELEASE_TAGS)" V="$(V)" $(PYTHON) scripts/refresh_source_metadata.py --write "$(WRITE)" --render-generated "$(RENDER_GENERATED)" --update-release-tags "$(UPDATE_RELEASE_TAGS)" --v "$(V)"

check-help-cache:
	$(call PROGRESS_PROBE,[check] Checking help cache)
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/help_cache.py --verify --v "$(V)"

check-first-output-latency:
	$(call PROGRESS_PROBE,[check] Checking first-output latency)
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/check_first_output_latency.py --v "$(V)"

refresh-help-cache:
	$(call PROGRESS_PROBE,[cache] Refreshing help cache)
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/help_cache.py --refresh --v "$(V)"

ref-report:
	$(call PROGRESS_PROBE,[report] Generating ref report)
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/ref_report.py --v "$(V)"

cleanup-report:
	$(call PROGRESS_PROBE,[report] Generating cleanup report)
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/ref_report.py --cleanup --v "$(V)"

test:
	$(call PROGRESS_PROBE,[quality] Starting tests)
	@DEBUG="$(DEBUG)" V="$(V)" QUALITY_IMAGE="$(QUALITY_IMAGE)" scripts/run_quality_container.sh test

test-local:
	$(call PROGRESS_PROBE,[quality] Starting local tests)
	@DEBUG="$(DEBUG)" V="$(V)" $(PYTHON) scripts/quality_checks.py test

lint:
	$(call PROGRESS_PROBE,[quality] Starting lint checks)
	@DEBUG="$(DEBUG)" V="$(V)" QUALITY_IMAGE="$(QUALITY_IMAGE)" scripts/run_quality_container.sh lint

gha-act-list:
	$(call PROGRESS_PROBE,[gha] Listing local GitHub Actions jobs)
	@ACT_WORKFLOW="$(ACT_WORKFLOW)" ACT_EVENT="$(ACT_EVENT)" ACT_JOB="$(ACT_JOB)" ACT_MATRIX="$(ACT_MATRIX)" ACT_SECRET_FILE="$(ACT_SECRET_FILE)" ACT_EXTRA_ARGS="$(ACT_EXTRA_ARGS)" ACT_CONTAINER_ARCH="$(ACT_CONTAINER_ARCH)" ACT_RUNNER_IMAGE="$(ACT_RUNNER_IMAGE)" scripts/run_github_actions_with_act.sh list

gha-act-dry-run:
	@if [ -z "$(ACT_WORKFLOW)" ]; then printf '%s\n' 'missing required variable: ACT_WORKFLOW=.github/workflows/<file>.yaml' >&2; exit 2; fi
	$(call PROGRESS_PROBE,[gha] Dry-running local GitHub Actions workflow: $(ACT_WORKFLOW))
	@ACT_WORKFLOW="$(ACT_WORKFLOW)" ACT_EVENT="$(ACT_EVENT)" ACT_JOB="$(ACT_JOB)" ACT_MATRIX="$(ACT_MATRIX)" ACT_SECRET_FILE="$(ACT_SECRET_FILE)" ACT_EXTRA_ARGS="$(ACT_EXTRA_ARGS)" ACT_CONTAINER_ARCH="$(ACT_CONTAINER_ARCH)" ACT_RUNNER_IMAGE="$(ACT_RUNNER_IMAGE)" scripts/run_github_actions_with_act.sh dry-run

gha-act-run:
	@if [ -z "$(ACT_WORKFLOW)" ]; then printf '%s\n' 'missing required variable: ACT_WORKFLOW=.github/workflows/<file>.yaml' >&2; exit 2; fi
	$(call PROGRESS_PROBE,[gha] Running local GitHub Actions workflow: $(ACT_WORKFLOW))
	@ACT_WORKFLOW="$(ACT_WORKFLOW)" ACT_EVENT="$(ACT_EVENT)" ACT_JOB="$(ACT_JOB)" ACT_MATRIX="$(ACT_MATRIX)" ACT_SECRET_FILE="$(ACT_SECRET_FILE)" ACT_EXTRA_ARGS="$(ACT_EXTRA_ARGS)" ACT_CONTAINER_ARCH="$(ACT_CONTAINER_ARCH)" ACT_RUNNER_IMAGE="$(ACT_RUNNER_IMAGE)" scripts/run_github_actions_with_act.sh run

render-release-branch-help:
	@$(PYTHON) scripts/render_release_branch.py --help

verify-release-branch-help:
	@$(PYTHON) scripts/verify_release_branch.py --help

verify-build-matrix-help:
	@$(PYTHON) scripts/verify_build_matrix.py --help

verify-source-policy-help:
	@$(PYTHON) scripts/verify_source_policy.py --help

verify-source-lifecycle-help:
	@$(PYTHON) scripts/verify_source_lifecycle.py --help

extract-vendor-delta-help:
	@$(PYTHON) scripts/extract_vendor_delta.py --help

uplift-edk2-release-help:
	@$(PYTHON) scripts/uplift_edk2_release.py --help

uplift-radxa-release-help:
	@$(PYTHON) scripts/uplift_radxa_release.py --help

select-unofficial-line-help:
	@$(PYTHON) scripts/select_unofficial_line.py --help

integrate-source-release-help:
	@$(PYTHON) scripts/integrate_source_release.py --help

import-unofficial-commits-help:
	@$(PYTHON) scripts/import_unofficial_commits.py --help

import-changes-help:
	@$(PYTHON) scripts/import_changes.py --help

inspect-import-conflicts-help:
	@$(PYTHON) scripts/inspect_import_conflicts.py --help

resolve-conflicts-help:
	@$(PYTHON) scripts/resolve_import_conflicts.py --help

promote-unofficial-compatibility-help:
	@$(PYTHON) scripts/promote_unofficial_compatibility.py --help

promote-unofficial-release-help:
	@$(PYTHON) scripts/promote_unofficial_release.py --help

update-release-tags-help:
	@$(PYTHON) scripts/update_release_tags.py --help

check-ref-integrity-help:
	@$(PYTHON) scripts/check_ref_integrity.py --help

check-identity-integrity-help:
	@$(PYTHON) scripts/check_identity_integrity.py --help

verify-identity-integrity-help:
	@$(PYTHON) scripts/check_identity_integrity.py --help

check-vendor-workflow-drift-help:
	@$(PYTHON) scripts/check_vendor_workflow_drift.py --help

check-upstream-versions-help:
	@$(PYTHON) scripts/check_upstream_versions.py --help

check-source-metadata-help:
	@$(PYTHON) scripts/refresh_source_metadata.py --help

refresh-source-metadata-help:
	@$(PYTHON) scripts/refresh_source_metadata.py --help

create-minimised-clone-help:
	@$(PYTHON) scripts/create_minimised_clone.py --help

verify-minimised-clone-help:
	@$(PYTHON) scripts/verify_minimised_clone.py --help

prune-help:
	@$(PYTHON) scripts/prune_cache_refs.py --help

refresh-help-cache-help:
	@$(PYTHON) scripts/help_cache.py --help

check-help-cache-help:
	@$(PYTHON) scripts/help_cache.py --help

check-first-output-latency-help:
	@$(PYTHON) scripts/check_first_output_latency.py --help

ref-report-help:
	@$(PYTHON) scripts/ref_report.py --help

cleanup-report-help:
	@$(PYTHON) scripts/ref_report.py --help
