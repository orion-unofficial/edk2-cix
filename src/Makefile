FIRMWARE_TARGET ?= RELEASE
# Specify DEBUG build for development
# FIRMWARE_TARGET ?= DEBUG
override UEFI_TARGET := $(FIRMWARE_TARGET)

.PHONY: all
all: build

.SHELLFLAGS := -eo pipefail -c
.ONESHELL:
.DELETE_ON_ERROR:
SHELL := bash
MAKEFLAGS += --no-print-directory
UNAME := $(shell uname -m)
HOST_OS ?= $(if $(EDK2_CIX_HOST_OS),$(EDK2_CIX_HOST_OS),$(shell uname -s))
HOST_ARCH := $(if $(filter arm64,$(UNAME)),aarch64,$(UNAME))
EMPTY :=
SPACE := $(EMPTY) $(EMPTY)
UPSTREAM_SOURCE_REF ?= refs/upstream-source/main
BUILD_METADATA_SCRIPT := scripts/resolve_build_metadata.sh
EDK2_BUILD_OUTPUT_FILTER := scripts/filter_edk2_build_output.py
MAKE_INPUT_VALIDATOR := $(abspath ../scripts/validate_make_inputs.py)
V ?= 0
ifeq ($(V),1)
Q :=
PRINT_HOST_TOOL = @:
else
Q := @
PRINT_HOST_TOOL = @printf '  HOST    %s\n' '$(@F)'
endif
WITH_REPO_LOCK := "$(abspath ../scripts/with_repo_lock.sh)"
LOCKED_MAKE = $(if $(filter 1,$(EDK2_CIX_REPO_LOCK_HELD)),$(MAKE) --no-print-directory,$(WITH_REPO_LOCK) $(MAKE) --no-print-directory)
define ENSURE_REPO_LOCK
if [[ "$(EDK2_CIX_REPO_LOCK_HELD)" != "1" ]]; then \
	$(WITH_REPO_LOCK) $(MAKE) --no-print-directory "$@"; \
	exit $$?; \
fi
endef
MONOREPO_EDK2_COMMIT_HASH ?= 898a41ed09
MONOREPO_EDK2_NON_OSI_COMMIT_HASH ?= 50f168f
MONOREPO_EDK2_PLATFORMS_COMMIT_HASH ?= 918cb36f
ARTEFACT_MODE ?= custom
CUSTOM_OVERLAY_ROOT ?= $(abspath ../custom/overlay)
CUSTOM_EDK2_WORKSPACE ?= /tmp/edk2-cix-custom-workspace
DEBUG_ON_UART3_NORMALIZED_RAW := $(if $(strip $(DEBUG_ON_UART3)),$(shell python3 "$(MAKE_INPUT_VALIDATOR)" normalize-bool --value "$(DEBUG_ON_UART3)" 2>/dev/null || printf '%s' __INVALID__),)
DEBUG_ON_UART3_NORMALIZED := $(if $(filter __INVALID__,$(DEBUG_ON_UART3_NORMALIZED_RAW)),,$(DEBUG_ON_UART3_NORMALIZED_RAW))
CUSTOM_OVERLAY_PACKAGE_ROOTS := \
	$(wildcard $(CUSTOM_OVERLAY_ROOT)/edk2) \
	$(wildcard $(CUSTOM_OVERLAY_ROOT)/edk2-platforms) \
	$(wildcard $(CUSTOM_OVERLAY_ROOT)/edk2-non-osi)
EDK2_PACKAGE_ROOTS := \
	$(if $(filter custom,$(ARTEFACT_MODE)),$(CUSTOM_OVERLAY_PACKAGE_ROOTS)) \
	$(abspath edk2) \
	$(abspath edk2-platforms) \
	$(abspath edk2-non-osi) \
	$(abspath edk2-platforms/Silicon/Intel)
EDK2_PACKAGES_PATH := $(subst $(SPACE),:,$(strip $(EDK2_PACKAGE_ROOTS)))
IMPORTED_EDK2_PACKAGE_ROOTS := \
	$(abspath edk2) \
	$(abspath edk2-platforms) \
	$(abspath edk2-non-osi) \
	$(abspath edk2-platforms/Silicon/Intel)
IMPORTED_EDK2_PACKAGES_PATH := $(subst $(SPACE),:,$(strip $(IMPORTED_EDK2_PACKAGE_ROOTS)))
CUSTOM_PLATFORM_SEARCH_ROOTS := \
	$(wildcard $(CUSTOM_OVERLAY_ROOT)/edk2-platforms/Platform) \
	$(wildcard $(CUSTOM_OVERLAY_ROOT)/edk2-non-osi/Platform)
PLATFORM_SEARCH_ROOTS := \
	$(if $(filter custom,$(ARTEFACT_MODE)),$(CUSTOM_PLATFORM_SEARCH_ROOTS)) \
	edk2-platforms/Platform \
	edk2-non-osi/Platform
EDK2_BUILD_SILENT_FLAG := $(if $(filter 1,$(V)),,-s)

# The firmware UI historically displayed the abbreviated hashes emitted by the
# original upstream superproject and its submodule repos. Recomputing
# `git rev-parse --short` inside the monorepo can produce different lengths, so
# keep the recorded upstream abbreviations unless the caller overrides them.
EDK2_COMMIT_HASH ?= $(MONOREPO_EDK2_COMMIT_HASH)
EDK2_NON_OSI_COMMIT_HASH ?= $(MONOREPO_EDK2_NON_OSI_COMMIT_HASH)
EDK2_PLATFORMS_COMMIT_HASH ?= $(MONOREPO_EDK2_PLATFORMS_COMMIT_HASH)
SOURCE_COMMIT_HASH ?= $(strip $(shell $(BUILD_METADATA_SCRIPT) source-commit-short 2>/dev/null || true))
SOURCE_COMMIT ?= $(strip $(shell $(BUILD_METADATA_SCRIPT) source-commit 2>/dev/null || true))
SOURCE_DATE_EPOCH ?= $(strip $(shell $(BUILD_METADATA_SCRIPT) source-date-epoch 2>/dev/null || true))
PM_CONFIG_SOURCE_DATE_EPOCH ?= $(SOURCE_DATE_EPOCH)
BUILD_DATE ?= $(strip $(shell SOURCE_DATE_EPOCH="$(SOURCE_DATE_EPOCH)" $(BUILD_METADATA_SCRIPT) source-date-iso 2>/dev/null || true))
DEB_VERSION ?= $(strip $(shell cd .. && (dpkg-parsechangelog -S Version 2>/dev/null || sed -n '1s/^[^(]*(\([^)]*\)).*/\1/p' debian/changelog)))
export SOURCE_DATE_EPOCH
export PM_CONFIG_SOURCE_DATE_EPOCH

# The first GCC5_AARCH64_PREFIX value uses distro-supplied cross compiler
# The second GCC5_AARCH64_PREFIX value uses CIX-verified cross compiler
ifeq ($(UNAME), aarch64)
PACKAGE_TOOL_ARCH := AARCH64
GCC5_AARCH64_PREFIX :=
# GCC5_AARCH64_PREFIX := $(CURDIR)/tools/gcc/gcc-arm-10.3-2021.07-aarch64-aarch64-none-elf/bin/aarch64-none-elf-
else
PACKAGE_TOOL_ARCH := X86_64
GCC5_AARCH64_PREFIX := aarch64-linux-gnu-
# GCC5_AARCH64_PREFIX := $(CURDIR)/tools/gcc/gcc-arm-10.2-2020.11-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
endif

PACKAGE_TOOL := edk2-non-osi/Platform/CIX/Sky1/PackageTool
TOOLS_DIR := tools
FIPTOOL_SOURCE_DIR := $(TOOLS_DIR)/arm-trusted-firmware-fiptool
FIPTOOL_BIN := $(FIPTOOL_SOURCE_DIR)/build/$(HOST_ARCH)/fiptool
FIPTOOL_SOURCE_INPUTS := \
	$(FIPTOOL_SOURCE_DIR)/Makefile \
	$(FIPTOOL_SOURCE_DIR)/src/fiptool.c \
	$(FIPTOOL_SOURCE_DIR)/src/fiptool.h \
	$(FIPTOOL_SOURCE_DIR)/src/fiptool_platform.h \
	$(FIPTOOL_SOURCE_DIR)/src/tbbr_config.c \
	$(FIPTOOL_SOURCE_DIR)/src/tbbr_config.h \
	$(FIPTOOL_SOURCE_DIR)/include/tools_share/firmware_image_package.h \
	$(FIPTOOL_SOURCE_DIR)/include/tools_share/uuid.h \
	$(FIPTOOL_SOURCE_DIR)/upstream.json
TRUSTED_KEY_CERT_TOOL_SOURCE_DIR := $(TOOLS_DIR)/cix_regen_trusted_key_cert
TRUSTED_KEY_CERT_TOOL_BIN := $(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)/cix_regen_trusted_key_cert
TRUSTED_KEY_CERT_TOOL_SOURCE_INPUTS := \
	$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)/Makefile \
	$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)/cix_regen_trusted_key_cert.c
CERT_UEFI_CREATE_RSA_SOURCE_DIR := $(TOOLS_DIR)/cert_uefi_create_rsa
CERT_UEFI_CREATE_RSA_BIN := $(CERT_UEFI_CREATE_RSA_SOURCE_DIR)/cert_uefi_create_rsa
CERT_UEFI_CREATE_RSA_SOURCE_INPUTS := \
	$(CERT_UEFI_CREATE_RSA_SOURCE_DIR)/Makefile \
	$(CERT_UEFI_CREATE_RSA_SOURCE_DIR)/cert_uefi_create_rsa.c
CERT_VALIDATOR := scripts/validate_cix_fip_certs.py
MICROSOFT_SECURE_BOOT_OUTPUT_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/SecureBootDxe/Microsoft)
MICROSOFT_SECURE_BOOT_MANIFEST := $(MICROSOFT_SECURE_BOOT_OUTPUT_DIR)/manifest.lock.json
MICROSOFT_SECURE_BOOT_RELEASE_CHECK_MODE ?= warn
MICROSOFT_SECURE_BOOT_PAYLOADS := \
	$(MICROSOFT_SECURE_BOOT_OUTPUT_DIR)/PK.bin \
	$(MICROSOFT_SECURE_BOOT_OUTPUT_DIR)/KEK.bin \
	$(MICROSOFT_SECURE_BOOT_OUTPUT_DIR)/DB.bin \
	$(MICROSOFT_SECURE_BOOT_OUTPUT_DIR)/DBX.bin
CUSTOM_EDK2_OVERLAY_SOURCES := \
	$(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Common.dsc.inc) \
	$(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Define.dsc.inc) \
	$(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/tool/python3/ParseIomuxTemplate.py) \
	$(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/tool/python3/pads-sky1.m4) \
	$(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/PlatformEnvHookLib.inf) \
	$(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/PlatformEnvHookLibCustom.c) \
	$(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLib.inf) \
	$(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLibCustom.c) \
	$(abspath ../custom/overlay/edk2/SecurityPkg/Library/SecureBootVariableProvisionLib/SecureBootVariableProvisionLib.c) \
	$(abspath ../custom/overlay/edk2/SecurityPkg/Library/SecureBootVariableProvisionLib/SecureBootVariableProvisionLib.inf)
CUSTOM_EDK2_BUILD_INPUTS := \
	$(if $(filter custom,$(ARTEFACT_MODE)),$(CUSTOM_EDK2_OVERLAY_SOURCES) $(MICROSOFT_SECURE_BOOT_PAYLOADS))
CUSTOM_EDK2_ORDER_ONLY_PREREQS := \
	$(if $(filter custom,$(ARTEFACT_MODE)),refresh-microsoft-secure-boot-defaults check-microsoft-secure-boot-release)
HOST_TOOLS_DIR := Build/HostTools
CIX_PACKAGE_TOOL_SOURCE_DIR := $(TOOLS_DIR)/cix_package_tool
CIX_PACKAGE_TOOL_BIN := $(HOST_TOOLS_DIR)/cix_package_tool
CIX_PACKAGE_TOOL_SOURCE_INPUTS := $(CIX_PACKAGE_TOOL_SOURCE_DIR)/cix_package_tool.py
SIGNING_CERT_SOURCE_DIR ?=
REQUIRE_SIGNING_CERT_SOURCE ?= $(if $(and $(filter upstream,$(ARTEFACT_MODE)),$(strip $(SIGNING_CERT_SOURCE_DIR))),1,0)
BASETOOLS_C_BIN_DIR := edk2/BaseTools/Source/C/bin
BASETOOLS_C_MARKER := $(BASETOOLS_C_BIN_DIR)/.edk2-cix-build-env
# Docker Desktop bind mounts on Darwin have been flaky with EDK2's default
# parallel build fan-out, so default to a single build thread there while
# leaving Linux behavior unchanged. Callers can still override this explicitly.
EDK2_BUILD_JOBS ?= $(if $(filter Darwin,$(HOST_OS)),1,)
EDK2_GENFDS_MULTI_THREAD_FLAG := $(if $(filter Darwin,$(HOST_OS)),--no-genfds-multi-thread,)
BASETOOLS_BUILD_JOBS ?= $(if $(filter Darwin,$(HOST_OS)),1,$(shell nproc))
HELPER_BUILD_JOBS ?= $(if $(filter Darwin,$(HOST_OS)),1,$(shell nproc))
BASETOOLS_C_REQUIRED_TOOLS := \
	BrotliCompress \
	DevicePath \
	EfiRom \
	GenCrc32 \
	GenFfs \
	GenFv \
	GenFw \
	GenSec \
	LzmaCompress \
	TianoCompress \
	VfrCompile \
	VolInfo
DSC := $(shell find edk2-platforms/Platform/Radxa -mindepth 3 -maxdepth 3 -name "*.dsc" -type "f") \
	#    $(shell find edk2-platforms/Platform/CIX -mindepth 3 -maxdepth 3 -name "*.dsc" -type "f")

FIRMWARE_BOARDS := $(foreach i,$(patsubst edk2-platforms/Platform/%.dsc,%,$(DSC)),$(lastword $(subst /, ,$(i))))
CIX_FLASH := $(foreach i, $(patsubst edk2-platforms/Platform/%.dsc,%,$(DSC)), Build/$(lastword $(subst /, ,$(i)))/$(UEFI_TARGET)_GCC5/cix_flash_all.bin) \
				 $(foreach i, $(patsubst edk2-platforms/Platform/%.dsc,%,$(DSC)), Build/$(lastword $(subst /, ,$(i)))/$(UEFI_TARGET)_GCC5/cix_flash_ota.bin)
.PHONY: build __build host-fiptool host-cix-regen-trusted-key-cert host-cert-uefi-create-rsa host-cix-package-tool
build:
	$(Q)$(LOCKED_MAKE) __build V="$(V)"

__build: preflight $(CIX_FLASH)

host-fiptool:
	$(Q)$(ENSURE_REPO_LOCK)
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(FIPTOOL_SOURCE_DIR)" HOST_ARCH="$(HOST_ARCH)" V="$(V)" clean all
host-cix-regen-trusted-key-cert:
	$(Q)$(ENSURE_REPO_LOCK)
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)" V="$(V)" clean all
host-cert-uefi-create-rsa:
	$(Q)$(ENSURE_REPO_LOCK)
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(CERT_UEFI_CREATE_RSA_SOURCE_DIR)" V="$(V)" clean all
host-cix-package-tool: $(CIX_PACKAGE_TOOL_BIN)

.SILENT: help print-build-metadata clean preflight preflight-% preflight-inputs preflight-tools preflight-build-metadata host-fiptool host-cix-regen-trusted-key-cert host-cert-uefi-create-rsa host-cix-package-tool prepare-basetools-c
.PHONY: help vars
define PRINT_SRC_VARIABLES
	@set -eu; \
	help_pad=28; \
	help_gap=2; \
	help_width=80; \
	desc_width=$$((help_width - help_pad - help_gap)); \
	desc_indent=$$(printf '%*s' "$$((help_pad + help_gap))" ''); \
	print_help_line() { \
		label="$$1"; \
		desc="$$2"; \
		if [ "$${#label}" -le "$$help_pad" ]; then \
			first_indent=$$(printf '%-*s%*s' "$$help_pad" "$$label" "$$help_gap" ''); \
			printf '%b\n' "$$desc" | fold -s -w "$$desc_width" | \
				awk -v first="$$first_indent" -v cont="$$desc_indent" \
					'NR == 1 { print first $$0; next } { sub(/^ +/, "", $$0); print cont $$0 }'; \
		else \
			printf '%s\n' "$$label"; \
			printf '%b\n' "$$desc" | fold -s -w "$$desc_width" | \
				awk -v cont="$$desc_indent" \
					'{ sub(/^ +/, "", $$0); print cont $$0 }'; \
		fi; \
	}; \
	printf '%s\n\n' 'useful variables'; \
	print_help_line '  FIRMWARE_TARGET=RELEASE|DEBUG' 'Select RELEASE or DEBUG build.\nDefault: RELEASE.'; \
	print_help_line '  ARTEFACT_MODE=custom|upstream' 'Generate unmodified upstream firmware images or allow custom enhancements.\nDefault: custom.'; \
	print_help_line '  DEBUG_ON_UART3=true|false' 'Custom-only: route firmware DEBUG() output to UART3; consumes GPIO105 and GPIO106.\nDefault: unset; custom builds keep DEBUG() on UART2.'; \
	print_help_line '  DEBUG_PRINT_ERROR_LEVEL=<u32>' 'Custom-only: override the firmware debug mask with a decimal or 0x-prefixed 32-bit value.\nDefault: unset; custom builds use 0x80000040.'; \
	print_help_line '  GCC5_AARCH64_PREFIX=<prefix>' 'Cross-compiler prefix for the AArch64 toolchain.\nDefault: aarch64-linux-gnu- on x86_64; empty on aarch64.'; \
	print_help_line '  SIGNING_CERT_SOURCE_DIR=<dir>' 'Path to extracted or built cert blobs for exact replay.\nDefault: unset.'; \
	print_help_line '  SOURCE_DATE_EPOCH=<unix-seconds>' 'Override the compiler-facing reproducible timestamp.\nDefault: resolved from mapped upstream source metadata.'; \
	print_help_line '  PM_CONFIG_SOURCE_DATE_EPOCH=<unix-seconds>' 'Override the PM-config timestamp.\nDefault: SOURCE_DATE_EPOCH.'; \
	print_help_line '  V=0|1' 'Set V=1 to show raw EDK2 command lines.\nDefault: 0.'
endef

help:
	@set -eu; \
	help_pad=28; \
	help_gap=2; \
	help_width=80; \
	desc_width=$$((help_width - help_pad - help_gap)); \
	desc_indent=$$(printf '%*s' "$$((help_pad + help_gap))" ''); \
	print_help_line() { \
		label="$$1"; \
		desc="$$2"; \
		if [ "$${#label}" -le "$$help_pad" ]; then \
			first_indent=$$(printf '%-*s%*s' "$$help_pad" "$$label" "$$help_gap" ''); \
			printf '%b\n' "$$desc" | fold -s -w "$$desc_width" | \
				awk -v first="$$first_indent" -v cont="$$desc_indent" \
					'NR == 1 { print first $$0; next } { sub(/^ +/, "", $$0); print cont $$0 }'; \
		else \
			printf '%s\n' "$$label"; \
			printf '%b\n' "$$desc" | fold -s -w "$$desc_width" | \
				awk -v cont="$$desc_indent" \
					'{ sub(/^ +/, "", $$0); print cont $$0 }'; \
		fi; \
	}; \
	printf '%s\n\n' 'edk2-cix raw firmware targets'; \
	print_help_line '  build' 'Build all raw flash images for the supported boards'; \
	print_help_line '  Build/O6/{RELEASE|DEBUG}_GCC5/cix_flash_all.bin' 'Build the full flash image for O6'; \
	print_help_line '  Build/O6/{RELEASE|DEBUG}_GCC5/cix_flash_ota.bin' 'Build the OTA flash image for O6'; \
	print_help_line '  Build/O6N/{RELEASE|DEBUG}_GCC5/cix_flash_all.bin' 'Build the full flash image for O6N'; \
	print_help_line '  Build/O6N/{RELEASE|DEBUG}_GCC5/cix_flash_ota.bin' 'Build the OTA flash image for O6N'; \
	printf '\n%s\n\n' 'host tools and checks'; \
	print_help_line '  host-fiptool' 'Build the vendored TF-A fiptool from source'; \
	print_help_line '  host-cix-regen-trusted-key-cert' 'Build the source trusted-key cert helper'; \
	print_help_line '  host-cert-uefi-create-rsa' 'Build the source non-trusted cert helper'; \
	print_help_line '  host-cix-package-tool' 'Refresh the cix_package_tool wrapper in Build/HostTools/'; \
	print_help_line '  prepare-basetools-c' 'Build or refresh the EDK2 BaseTools C helpers'; \
	print_help_line '  preflight' 'Check host tools, metadata, and toolchain prerequisites'; \
	print_help_line '  print-build-metadata' 'Show the resolved reproducible build metadata'; \
	print_help_line '  clean' 'Remove Build/ output and generated helper artefacts'; \
	printf '\n%s\n' 'For accepted control variables, run: make -C src vars'

vars:
	$(PRINT_SRC_VARIABLES)

$(FIPTOOL_BIN): $(FIPTOOL_SOURCE_INPUTS)
	$(Q)$(ENSURE_REPO_LOCK)
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(FIPTOOL_SOURCE_DIR)" HOST_ARCH="$(HOST_ARCH)" V="$(V)" all

$(TRUSTED_KEY_CERT_TOOL_BIN): $(TRUSTED_KEY_CERT_TOOL_SOURCE_INPUTS)
	$(Q)$(ENSURE_REPO_LOCK)
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)" V="$(V)" clean all

$(CERT_UEFI_CREATE_RSA_BIN): $(CERT_UEFI_CREATE_RSA_SOURCE_INPUTS)
	$(Q)$(ENSURE_REPO_LOCK)
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(CERT_UEFI_CREATE_RSA_SOURCE_DIR)" V="$(V)" clean all

$(CIX_PACKAGE_TOOL_BIN): $(CIX_PACKAGE_TOOL_SOURCE_INPUTS)
	$(Q)$(ENSURE_REPO_LOCK)
	$(PRINT_HOST_TOOL)
	$(Q)mkdir -p "$(HOST_TOOLS_DIR)"
	$(Q)printf '%s\n' '#!/usr/bin/env bash' 'set -euo pipefail' \
		'exec python3 "$(abspath $(CIX_PACKAGE_TOOL_SOURCE_DIR))/cix_package_tool.py" "$$@"' >"$@"
	$(Q)chmod +x "$@" "$(CIX_PACKAGE_TOOL_SOURCE_DIR)/cix_package_tool.py"

.PHONY: print-build-metadata preflight-build-metadata
print-build-metadata: preflight-build-metadata
	echo "ARTEFACT_MODE=$(ARTEFACT_MODE)"
	echo "SOURCE_COMMIT=$(SOURCE_COMMIT)"
	echo "SOURCE_COMMIT_HASH=$(SOURCE_COMMIT_HASH)"
	echo "SOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH)"
	echo "PM_CONFIG_SOURCE_DATE_EPOCH=$(PM_CONFIG_SOURCE_DATE_EPOCH)"
	echo "BUILD_DATE=$(BUILD_DATE)"
	echo "EDK2_COMMIT_HASH=$(EDK2_COMMIT_HASH)"
	echo "EDK2_NON_OSI_COMMIT_HASH=$(EDK2_NON_OSI_COMMIT_HASH)"
	echo "EDK2_PLATFORMS_COMMIT_HASH=$(EDK2_PLATFORMS_COMMIT_HASH)"

preflight-build-metadata:
	@{ \
		missing=0; \
		check_required() { \
			local name="$$1"; \
			local value="$$2"; \
			if [[ -z "$$value" ]]; then \
				echo "[preflight/metadata] Missing required build metadata: $$name" >&2; \
				missing=1; \
			fi; \
		}; \
		check_required "SOURCE_COMMIT" '$(strip $(SOURCE_COMMIT))'; \
		check_required "SOURCE_COMMIT_HASH" '$(strip $(SOURCE_COMMIT_HASH))'; \
		check_required "SOURCE_DATE_EPOCH" '$(strip $(SOURCE_DATE_EPOCH))'; \
		check_required "PM_CONFIG_SOURCE_DATE_EPOCH" '$(strip $(PM_CONFIG_SOURCE_DATE_EPOCH))'; \
		check_required "BUILD_DATE" '$(strip $(BUILD_DATE))'; \
		check_required "EDK2_COMMIT_HASH" '$(strip $(EDK2_COMMIT_HASH))'; \
		check_required "EDK2_NON_OSI_COMMIT_HASH" '$(strip $(EDK2_NON_OSI_COMMIT_HASH))'; \
		check_required "EDK2_PLATFORMS_COMMIT_HASH" '$(strip $(EDK2_PLATFORMS_COMMIT_HASH))'; \
		check_required "DEB_VERSION" '$(strip $(DEB_VERSION))'; \
		if [[ -n '$(strip $(SOURCE_DATE_EPOCH))' ]] && [[ ! '$(strip $(SOURCE_DATE_EPOCH))' =~ ^[0-9]+$$ ]]; then \
			echo "[preflight/metadata] SOURCE_DATE_EPOCH must be an integer Unix timestamp, got: $(SOURCE_DATE_EPOCH)" >&2; \
			missing=1; \
		fi; \
		if [[ -n '$(strip $(PM_CONFIG_SOURCE_DATE_EPOCH))' ]] && [[ ! '$(strip $(PM_CONFIG_SOURCE_DATE_EPOCH))' =~ ^[0-9]+$$ ]]; then \
			echo "[preflight/metadata] PM_CONFIG_SOURCE_DATE_EPOCH must be an integer Unix timestamp, got: $(PM_CONFIG_SOURCE_DATE_EPOCH)" >&2; \
			missing=1; \
		fi; \
		if [[ "$$missing" -ne 0 ]]; then \
			echo "[preflight/metadata] Refusing to continue with incomplete or invalid build metadata." >&2; \
			if [[ "$(ARTEFACT_MODE)" == "custom" ]]; then \
				echo "[preflight/metadata] In ARTEFACT_MODE=custom these values should resolve from the mapped upstream source commit in Git history." >&2; \
				echo "[preflight/metadata] Ensure the checkout is a usable Git repository inside the current environment, or pass explicit BUILD_DATE, SOURCE_DATE_EPOCH, PM_CONFIG_SOURCE_DATE_EPOCH, SOURCE_COMMIT, and SOURCE_COMMIT_HASH overrides." >&2; \
			fi; \
			exit 1; \
		fi; \
	}

.PHONY: prepare-basetools-c
prepare-basetools-c:
	@$(ENSURE_REPO_LOCK)
	@mkdir -p "$(BASETOOLS_C_BIN_DIR)"
	current_env_id="$$( \
		{ source /etc/os-release >/dev/null 2>&1 && true; } || true; \
		printf 'arch=%s;os=%s;version=%s;cc=%s;ldd=%s\n' \
			"$$(uname -sm)" \
			"$${ID:-unknown}" \
			"$${VERSION_ID:-unknown}" \
			"$$(cc --version 2>/dev/null | sed -n '1p' || echo unavailable)" \
			"$$(ldd --version 2>/dev/null | sed -n '1p' || echo unavailable)"; \
	)"
	rebuild=0
	if [[ ! -f "$(BASETOOLS_C_MARKER)" ]]; then
		rebuild=1
	elif ! grep -qxF "$$current_env_id" "$(BASETOOLS_C_MARKER)"; then
		rebuild=1
	fi
	for tool in $(BASETOOLS_C_REQUIRED_TOOLS); do
		if [[ ! -x "$(BASETOOLS_C_BIN_DIR)/$$tool" ]]; then
			rebuild=1
			break
		fi
	done
	if [[ "$$rebuild" -eq 1 ]]; then
		printf '[build] Rebuilding BaseTools C binaries for %s\n' "$$current_env_id"
		printf '[build] Cleaning stale BaseTools C objects and dependency files\n'
		$(MAKE) -C edk2/BaseTools V="$(V)" clean
		if [[ "$(V)" == "1" ]]; then
			$(MAKE) -C edk2/BaseTools -j$(BASETOOLS_BUILD_JOBS) V="$(V)" Source/C
		else
			$(MAKE) -C edk2/BaseTools -j$(BASETOOLS_BUILD_JOBS) V="$(V)" Source/C 2>&1 | \
				ARTEFACT_MODE="$(ARTEFACT_MODE)" \
				python3 "$(EDK2_BUILD_OUTPUT_FILTER)"
		fi
		printf '%s\n' "$$current_env_id" > "$(BASETOOLS_C_MARKER)"
	fi

.PHONY: preflight preflight-% preflight-inputs
preflight: preflight-inputs preflight-tools preflight-build-metadata

preflight-%: preflight-inputs preflight-tools preflight-build-metadata
	@dsc_path="$(filter %/$*.dsc,$(DSC))" && \
	if [[ -z "$$dsc_path" ]]; then \
		echo "[preflight/$*] No matching DSC found under edk2-platforms/Platform/Radxa" >&2; \
		exit 1; \
	fi

preflight-inputs:
	@set -euo pipefail; \
	validate_cmd=(python3 "$(MAKE_INPUT_VALIDATOR)" validate \
		--repo-root "$(abspath ..)" \
		--v "$(V)" \
		--artefact-mode "$(ARTEFACT_MODE)" \
		--firmware-target "$(FIRMWARE_TARGET)"); \
	if [ -n "$(strip $(DEBUG_ON_UART3))" ]; then \
		validate_cmd+=(--debug-on-uart3 "$(strip $(DEBUG_ON_UART3))"); \
	fi; \
	if [ -n "$(strip $(DEBUG_PRINT_ERROR_LEVEL))" ]; then \
		validate_cmd+=(--debug-print-error-level "$(strip $(DEBUG_PRINT_ERROR_LEVEL))"); \
	fi; \
	"$${validate_cmd[@]}"

.PHONY: preflight-tools
preflight-tools:
	@missing=0; \
	for required_dir in edk2 edk2-platforms edk2-non-osi "$(PACKAGE_TOOL)"; do \
		if [[ ! -e "$$required_dir" ]]; then \
			echo "[preflight] Missing required path: $$required_dir" >&2; \
			missing=1; \
		fi; \
	done; \
	for required_cmd in bash find grep head realpath tr dd python3 iasl openssl; do \
		if ! command -v "$$required_cmd" >/dev/null 2>&1; then \
			echo "[preflight] Missing required host tool: $$required_cmd" >&2; \
			missing=1; \
		fi; \
	done; \
	for required_cmd in make cc perl; do \
		if ! command -v "$$required_cmd" >/dev/null 2>&1; then \
			echo "[preflight] Missing required host tool: $$required_cmd" >&2; \
			missing=1; \
		fi; \
	done; \
	if [[ -n "$(GCC5_AARCH64_PREFIX)" ]] && ! command -v "$(GCC5_AARCH64_PREFIX)gcc" >/dev/null 2>&1; then \
		echo "[preflight] Missing cross-compiler: $(GCC5_AARCH64_PREFIX)gcc" >&2; \
		missing=1; \
	fi; \
	for required_file in $(FIPTOOL_SOURCE_INPUTS); do \
		if [[ ! -f "$$required_file" ]]; then \
			echo "[preflight] Missing fiptool source input: $$required_file" >&2; \
			missing=1; \
		fi; \
	done; \
	for required_file in $(CIX_PACKAGE_TOOL_SOURCE_INPUTS); do \
		if [[ ! -f "$$required_file" ]]; then \
			echo "[preflight] Missing cix_package_tool source input: $$required_file" >&2; \
			missing=1; \
		fi; \
	done; \
	for required_file in $(CERT_UEFI_CREATE_RSA_SOURCE_INPUTS); do \
		if [[ ! -f "$$required_file" ]]; then \
			echo "[preflight] Missing cert_uefi_create_rsa source input: $$required_file" >&2; \
			missing=1; \
		fi; \
	done; \
	if [[ ! -f "$(CERT_VALIDATOR)" ]]; then \
		echo "[preflight] Missing certificate validation helper: $(CERT_VALIDATOR)" >&2; \
		missing=1; \
	fi; \
	if [[ "$$missing" -ne 0 ]]; then \
		exit 1; \
	fi

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/cix_flash_ota.bin
Build/%/$(UEFI_TARGET)_GCC5/cix_flash_ota.bin: preflight-% Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img | $(CIX_PACKAGE_TOOL_BIN)
	@$(ENSURE_REPO_LOCK)
	@find_platform_match() { \
		local relative_path="$$1"; \
		local node_type="$$2"; \
		local search_root; \
		local found; \
		for search_root in $(PLATFORM_SEARCH_ROOTS); do \
			[[ -d "$$search_root" ]] || continue; \
			found="$$(find "$$search_root" -mindepth 4 -maxdepth 4 -path "*/$$relative_path" -type "$$node_type" | head -n 1)"; \
			if [[ -n "$$found" ]]; then \
				printf '%s\n' "$$found"; \
				return 0; \
			fi; \
		done; \
		return 1; \
	}; \
	export CFG="$$(find_platform_match "$*/spi_flash_config_ota.json" f || true)" && \
	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	if [[ -f "$$CFG" ]]; then \
		if [[ "$(V)" == "1" ]]; then echo "Found custom flash config."; fi; \
		cp "$$CFG" Build/$*/$(UEFI_TARGET)_GCC5/; \
	else \
		cp "$$PATH_PACKAGE_TOOL/spi_flash_config_ota.json" Build/$*/$(UEFI_TARGET)_GCC5/; \
	fi

	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	export CIX_PACKAGE_TOOL_BIN="$(shell realpath "$(CIX_PACKAGE_TOOL_BIN)")" && \
	printf '[build/%s] Packaging OTA flash image\n' "$*" && \
	( \
		cd "Build/$*/$(UEFI_TARGET)_GCC5/" && \
		tmp_ota="./cix_flash_ota.bin.tmp" && \
		rm -f "$$tmp_ota" && \
		TERM=dumb "$$CIX_PACKAGE_TOOL_BIN" -c "./spi_flash_config_ota.json" -O "$$tmp_ota" 2>&1 | perl -pe 's/\e\[[0-9;]*m//g' && \
		if [[ ! -s "$$tmp_ota" ]]; then \
			echo "[build/$*] Expected OTA image $$tmp_ota was not created" >&2; \
			exit 1; \
		fi && \
		mv -f "$$tmp_ota" "./cix_flash_ota.bin" \
	)

# MEM_CFG_MEMFREQ ?= 2750
MEM_CFG_MEMFREQ ?= 3000

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/cix_flash_all.bin
Build/%/$(UEFI_TARGET)_GCC5/cix_flash_all.bin: preflight-% Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img | $(CIX_PACKAGE_TOOL_BIN)
	@$(ENSURE_REPO_LOCK)
	@find_platform_match() { \
		local relative_path="$$1"; \
		local node_type="$$2"; \
		local search_root; \
		local found; \
		for search_root in $(PLATFORM_SEARCH_ROOTS); do \
			[[ -d "$$search_root" ]] || continue; \
			found="$$(find "$$search_root" -mindepth 4 -maxdepth 4 -path "*/$$relative_path" -type "$$node_type" | head -n 1)"; \
			if [[ -n "$$found" ]]; then \
				printf '%s\n' "$$found"; \
				return 0; \
			fi; \
		done; \
		return 1; \
	}; \
	export MEM="$$(find_platform_match "$*/mem_config" d || true)" && \
	if [[ -d "$$MEM" ]]; then \
		printf '[build/%s] Regenerating memory configuration\n' "$*" && \
		if [[ "$(V)" == "1" ]]; then echo "Found custom memory configuration."; fi; \
		rm -f Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/memory_config.bin; \
		$(MAKE) -C "$$MEM" $(if $(strip $(HELPER_BUILD_JOBS)),-j$(HELPER_BUILD_JOBS)) -e CFLAG:="-DMEM_CFG_MEMFREQ=$(MEM_CFG_MEMFREQ)" clean main; \
		if [[ ! -s "$$MEM/memory_config.bin" ]]; then \
			echo "[build/$*] Expected memory_config.bin was not generated in $$MEM" >&2; \
			exit 1; \
		fi; \
		mv "$$MEM/memory_config.bin" Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/; \
	fi
	export PM="$$(find_platform_match "$*/pm_config" d || true)" && \
	if [[ -d "$$PM" ]]; then \
		printf '[build/%s] Regenerating PM configuration\n' "$*" && \
		if [[ "$(V)" == "1" ]]; then echo "Found custom PM configuration."; fi; \
		rm -f Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/csu_pm_config.bin; \
		PM_CONFIG_SOURCE_DATE_EPOCH="$(PM_CONFIG_SOURCE_DATE_EPOCH)" $(MAKE) -C "$$PM" $(if $(strip $(HELPER_BUILD_JOBS)),-j$(HELPER_BUILD_JOBS)); \
		if [[ ! -s "$$PM/csu_pm_config.bin" ]]; then \
			echo "[build/$*] Expected csu_pm_config.bin was not generated in $$PM" >&2; \
			exit 1; \
		fi; \
		mv "$$PM/csu_pm_config.bin" Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/; \
	fi

	export CFG="$$(find_platform_match "$*/spi_flash_config_all.json" f || true)" && \
	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	export CIX_PACKAGE_TOOL_BIN="$(shell realpath "$(CIX_PACKAGE_TOOL_BIN)")" && \
	if [[ -f "$$CFG" ]]; then \
		if [[ "$(V)" == "1" ]]; then echo "Found custom flash config."; fi; \
		cp "$$CFG" Build/$*/$(UEFI_TARGET)_GCC5/; \
	else \
		cp "$$PATH_PACKAGE_TOOL/spi_flash_config_all.json" Build/$*/$(UEFI_TARGET)_GCC5/; \
	fi

	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	printf '[build/%s] Packaging full flash image\n' "$*" && \
	( \
		cd "Build/$*/$(UEFI_TARGET)_GCC5/" && \
		TERM=dumb "$$CIX_PACKAGE_TOOL_BIN" -c "./spi_flash_config_all.json" -o "./cix_flash_all.raw" 2>&1 | perl -pe 's/\e\[[0-9;]*m//g' && \
		if [[ ! -s "./cix_flash_all.raw" ]]; then \
			echo "[build/$*] Expected cix_flash_all.raw was not created" >&2; \
			exit 1; \
		fi \
	)

	# \377 = 0xFF
	tmp_all="Build/$*/$(UEFI_TARGET)_GCC5/cix_flash_all.bin.tmp" && \
	rm -f "$$tmp_all" && \
	head -c 8M < /dev/zero | tr '\000' '\377' > "$$tmp_all" && \
	dd if="Build/$*/$(UEFI_TARGET)_GCC5/cix_flash_all.raw" of="$$tmp_all" conv=notrunc status=none && \
	if [[ ! -s "$$tmp_all" ]]; then \
		echo "[build/$*] Expected full flash image $$tmp_all was not created" >&2; \
		exit 1; \
	fi && \
	mv -f "$$tmp_all" "Build/$*/$(UEFI_TARGET)_GCC5/cix_flash_all.bin"

.NOTINTERMEDIATE: \
	$(foreach board,$(FIRMWARE_BOARDS),Build/$(board)/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd) \
	$(foreach board,$(FIRMWARE_BOARDS),Build/$(board)/$(UEFI_TARGET)_GCC5/Firmwares/dummy.bin) \
	$(foreach board,$(FIRMWARE_BOARDS),Build/$(board)/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt) \
	$(foreach board,$(FIRMWARE_BOARDS),Build/$(board)/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img)

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img
Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img: \
	Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd \
	Build/%/$(UEFI_TARGET)_GCC5/Firmwares/dummy.bin \
	Build/%/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt \
	| $(FIPTOOL_BIN) $(CERT_UEFI_CREATE_RSA_BIN)

	@$(ENSURE_REPO_LOCK)
	@export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	export SOURCE_CERT_TOOL_BIN="$(abspath $(CERT_UEFI_CREATE_RSA_BIN))" && \
	export SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" && \
	export REQUIRE_SIGNING_CERT_SOURCE="$(REQUIRE_SIGNING_CERT_SOURCE)" && \
	run_success_only() { \
		if [[ "$(V)" == "1" ]]; then \
			"$$@"; \
			return $$?; \
		fi; \
		local tool_output; \
		if ! tool_output="$$("$$@" 2>&1)"; then \
			[[ -n "$$tool_output" ]] && printf '%s\n' "$$tool_output" >&2; \
			return 1; \
		fi; \
		return 0; \
	}; \
	select_cert_source() { \
		local dir="$$1"; \
		shift; \
		local candidate; \
		for candidate in "$$@"; do \
			if [[ -f "$$dir/$$candidate" ]]; then \
				printf '%s\n' "$$dir/$$candidate"; \
				return 0; \
			fi; \
		done; \
		return 1; \
	}; \
	reused_trusted_key_cert=0; \
	validator_extra_args=(); \
	if [[ -n "$$SIGNING_CERT_SOURCE_DIR" ]]; then \
		if [[ ! -d "$$SIGNING_CERT_SOURCE_DIR" ]]; then \
			echo "Exact upstream replay requested with SIGNING_CERT_SOURCE_DIR=$$SIGNING_CERT_SOURCE_DIR, but that directory does not exist." >&2; \
			echo "Point SIGNING_CERT_SOURCE_DIR at the extracted cert bundle generated by src/scripts/replay_o6_release.py," >&2; \
			echo "or unset SIGNING_CERT_SOURCE_DIR if you only want a semantically compatible upstream build instead of a byte-identical replay." >&2; \
			exit 1; \
		fi; \
		trusted_key_cert_src="$$(select_cert_source "$$SIGNING_CERT_SOURCE_DIR" trusted_key_no.crt trusted-key-cert.bin || true)"; \
		nt_fw_cert_src="$$(select_cert_source "$$SIGNING_CERT_SOURCE_DIR" nt_fw_cert.crt nt-fw-cert.bin || true)"; \
		nt_fw_key_cert_src="$$(select_cert_source "$$SIGNING_CERT_SOURCE_DIR" nt_fw_key.crt nt-fw-key-cert.bin || true)"; \
		if [[ "$$REQUIRE_SIGNING_CERT_SOURCE" == "1" ]]; then \
			missing_exact_replay_certs=(); \
			if [[ -z "$$trusted_key_cert_src" ]]; then \
				missing_exact_replay_certs+=(trusted_key_no.crt\|trusted-key-cert.bin); \
			fi; \
			if [[ -z "$$nt_fw_cert_src" ]]; then \
				missing_exact_replay_certs+=(nt_fw_cert.crt\|nt-fw-cert.bin); \
			fi; \
			if [[ -z "$$nt_fw_key_cert_src" ]]; then \
				missing_exact_replay_certs+=(nt_fw_key.crt\|nt-fw-key-cert.bin); \
			fi; \
			if [[ "$${#missing_exact_replay_certs[@]}" -ne 0 ]]; then \
				echo "ARTEFACT_MODE=upstream with SIGNING_CERT_SOURCE_DIR requires the full extracted exact-replay cert bundle." >&2; \
				echo "Missing from $$SIGNING_CERT_SOURCE_DIR:" >&2; \
				for missing_cert in "$${missing_exact_replay_certs[@]}"; do \
					echo "  - $$missing_cert" >&2; \
				done; \
				echo "Provide the cert bundle extracted by src/scripts/replay_o6_release.py," >&2; \
				echo "or unset SIGNING_CERT_SOURCE_DIR if you want a semantically compatible upstream build instead of a byte-identical replay." >&2; \
				exit 1; \
			fi; \
		fi; \
		if [[ -n "$$trusted_key_cert_src" ]]; then \
			echo "Reusing trusted-key cert from $$trusted_key_cert_src"; \
			cp "$$trusted_key_cert_src" Build/$*/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt; \
			reused_trusted_key_cert=1; \
		fi; \
		if [[ -n "$$nt_fw_cert_src" && -n "$$nt_fw_key_cert_src" ]]; then \
			echo "Reusing non-trusted FIP certs from $$SIGNING_CERT_SOURCE_DIR"; \
			cp "$$nt_fw_cert_src" Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_cert.crt; \
			cp "$$nt_fw_key_cert_src" Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_key.crt; \
		elif [[ -n "$$nt_fw_cert_src" || -n "$$nt_fw_key_cert_src" ]]; then \
			echo "SIGNING_CERT_SOURCE_DIR must provide both nt-fw cert blobs together" >&2; \
			exit 1; \
		else \
			echo "SIGNING_CERT_SOURCE_DIR does not provide nt-fw cert blobs; generating them"; \
		fi; \
	fi; \
	if [[ "$(ARTEFACT_MODE)" == "upstream" && "$$reused_trusted_key_cert" == "1" ]]; then \
		validator_extra_args+=(--allow-reused-trusted-key-cert); \
	fi; \
	if [[ ! -f Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_cert.crt || ! -f Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_key.crt ]]; then \
		run_success_only "$$SOURCE_CERT_TOOL_BIN" --key-alg rsa --key-size 3072 --hash-alg sha256 -p --ntfw-nvctr 223 \
			--nt-fw-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_cert.crt \
			--nt-fw-key-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_key.crt \
			--nt-fw-key Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
			--non-trusted-world-key Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
			--nt-fw "$(shell realpath "$<")"; \
	fi && \
	run_success_only python3 "$(CERT_VALIDATOR)" \
		--trusted-key-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt \
		--nt-fw-key-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_key.crt \
		--nt-fw-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_cert.crt \
		--oem-public-key Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_publickey.pem \
		--trusted-sign-key Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
		--nt-fw-key Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
		--non-trusted-world-key Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
		--nt-fw "$(shell realpath "$<")" \
		--ntfw-nvctr 223 \
		--fiptool "$(FIPTOOL_BIN)" \
		"$${validator_extra_args[@]}" && \
	run_success_only "$(FIPTOOL_BIN)" create \
		--trusted-key-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt \
		--nt-fw-key-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_key.crt \
		--nt-fw-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_cert.crt \
		--nt-fw "$(shell realpath "$<")" \
		$@ && \
	if [[ ! -s "$@" ]]; then \
		echo "[build/$*] Expected bootloader image $@ was not created" >&2; \
		exit 1; \
	fi

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/Firmwares/dummy.bin
Build/%/$(UEFI_TARGET)_GCC5/Firmwares/dummy.bin:
	@$(ENSURE_REPO_LOCK)
	@cp -aR $(PACKAGE_TOOL)/Firmwares/ Build/$*/$(UEFI_TARGET)_GCC5/
	# \377 = 0xFF
	head -c 8192 < /dev/zero | tr '\000' '\377' > $@

	find_platform_match() { \
		local relative_path="$$1"; \
		local node_type="$$2"; \
		local search_root; \
		local found; \
		for search_root in $(PLATFORM_SEARCH_ROOTS); do \
			[[ -d "$$search_root" ]] || continue; \
			found="$$(find "$$search_root" -mindepth 4 -maxdepth 4 -path "*/$$relative_path" -type "$$node_type" | head -n 1)"; \
			if [[ -n "$$found" ]]; then \
				printf '%s\n' "$$found"; \
				return 0; \
			fi; \
		done; \
		return 1; \
	}; \
	export FW="$$(find_platform_match "$*/Firmwares" d || true)" && \
	if [[ -d "$$FW" ]]; then \
		if [[ "$(V)" == "1" ]]; then echo "Found custom firmware."; fi; \
		cp -r "$$FW/." Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/; \
	fi; \
	if [[ ! -e "$$FW" ]] || [[ ! -f "$$FW/ec_fw.bin" ]]; then \
		echo "Found EC-less platform."; \
		ln -sf dummy.bin Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/ec_fw.bin; \
	fi

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt
Build/%/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt: | $(TRUSTED_KEY_CERT_TOOL_BIN)
	@$(ENSURE_REPO_LOCK)
	@export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	export SOURCE_TOOL_BIN="$(abspath $(TRUSTED_KEY_CERT_TOOL_BIN))" && \
	run_success_only() { \
		if [[ "$(V)" == "1" ]]; then \
			"$$@"; \
			return $$?; \
		fi; \
		local tool_output; \
		if ! tool_output="$$("$$@" 2>&1)"; then \
			[[ -n "$$tool_output" ]] && printf '%s\n' "$$tool_output" >&2; \
			return 1; \
		fi; \
		return 0; \
	}; \
	mkdir -p Build/$*/$(UEFI_TARGET)_GCC5/Keys Build/$*/$(UEFI_TARGET)_GCC5/certs && \
	cp -a $(PACKAGE_TOOL)/Keys/. Build/$*/$(UEFI_TARGET)_GCC5/Keys/ && \
	cp -a $(PACKAGE_TOOL)/certs/. Build/$*/$(UEFI_TARGET)_GCC5/certs/ && \
	select_cert_source() { \
		local base_dir="$$1"; \
		shift; \
		local candidate; \
		for candidate in "$$@"; do \
			if [[ -f "$$base_dir/$$candidate" ]]; then \
				printf '%s\n' "$$base_dir/$$candidate"; \
				return 0; \
			fi; \
		done; \
		return 1; \
	}; \
	if [[ -n "$$SIGNING_CERT_SOURCE_DIR" ]]; then \
		trusted_key_cert_src="$$(select_cert_source "$$SIGNING_CERT_SOURCE_DIR" trusted_key_no.crt trusted-key-cert.bin || true)"; \
		if [[ -n "$$trusted_key_cert_src" ]]; then \
			cp "$$trusted_key_cert_src" "$@"; \
			exit 0; \
		fi; \
	fi; \
	case "$(ARTEFACT_MODE)" in \
		custom|upstream) \
			run_success_only "$$SOURCE_TOOL_BIN" \
				-p Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_publickey.pem \
				-s Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
				-o $@; \
			;; \
		*) \
			echo "Unsupported ARTEFACT_MODE=$(ARTEFACT_MODE); expected custom or upstream" >&2; \
			exit 1; \
			;; \
	esac

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd
Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd: $(CUSTOM_EDK2_BUILD_INPUTS) | preflight-% $(CUSTOM_EDK2_ORDER_ONLY_PREREQS)
	@$(ENSURE_REPO_LOCK)
	@$(MAKE) prepare-basetools-c
	if [[ "$(ARTEFACT_MODE)" == "custom" ]]; then
		export WORKSPACE="$(CUSTOM_EDK2_WORKSPACE)"
		export PACKAGES_PATH="$(EDK2_PACKAGES_PATH)"
		mkdir -p "$$WORKSPACE"
		rm -rf "$$WORKSPACE/Build/$*" "$$WORKSPACE/Conf"
		export EDK_TOOLS_PATH="$(abspath edk2/BaseTools)"
	else
		export WORKSPACE="$(shell pwd)"
		export PACKAGES_PATH="$(IMPORTED_EDK2_PACKAGES_PATH)"
	fi
	export GCC5_AARCH64_PREFIX="$(GCC5_AARCH64_PREFIX)"
	if command -v iasl >/dev/null 2>&1; then
		export IASL_PREFIX="$$(dirname "$$(command -v iasl)")/"
	fi
	unset MAKEFLAGS
	if [[ "$(V)" == "1" ]]; then
		source edk2/edksetup.sh --reconfig
	else
		edksetup_log="$$(mktemp "$${TMPDIR:-/tmp}/edk2-cix-edksetup.XXXXXX")"
		if ! source edk2/edksetup.sh --reconfig >"$$edksetup_log" 2>&1; then
			cat "$$edksetup_log" >&2
			rm -f "$$edksetup_log"
			exit 1
		fi
		rm -f "$$edksetup_log"
	fi
	build_extra_defines=()
	if [[ "$(UEFI_TARGET)" == "RELEASE" ]]; then
		case "$(ARTEFACT_MODE)" in \
			custom) \
				printf '\nRELEASE_*_*_GENFW_FLAGS          = --zero\n' >> edk2/Conf/tools_def.txt; \
				build_extra_defines=(-D REPRODUCIBLE_BUILD_METADATA=TRUE); \
				;; \
			upstream) \
				;; \
			*) \
				echo "Unsupported ARTEFACT_MODE=$(ARTEFACT_MODE); expected custom or upstream" >&2; \
				exit 1; \
				;; \
		esac
	fi
	if [[ "$(ARTEFACT_MODE)" == "custom" && -n "$(DEBUG_ON_UART3_NORMALIZED)" ]]; then
		build_extra_defines+=(-D DEBUG_ON_UART3=$(DEBUG_ON_UART3_NORMALIZED))
	fi
	if [[ "$(ARTEFACT_MODE)" == "custom" && -n "$(DEBUG_PRINT_ERROR_LEVEL)" ]]; then
		build_extra_defines+=(-D DEBUG_PRINT_ERROR_LEVEL=$(DEBUG_PRINT_ERROR_LEVEL))
	fi
	run_edk2_build() {
		export EDK2_CIX_QUIET_PREBUILD="$(if $(filter 1,$(V)),0,1)"
		build -a AARCH64 -t GCC5 -p "$(subst edk2-platforms/,,$(filter %/$*.dsc,$(DSC)))" \
			$(if $(strip $(EDK2_BUILD_JOBS)),-n $(EDK2_BUILD_JOBS)) \
			$(EDK2_GENFDS_MULTI_THREAD_FLAG) \
			$(EDK2_BUILD_SILENT_FLAG) \
			"$${build_extra_defines[@]}" \
			-b $(UEFI_TARGET) -D BOARD_NAME=$(word 2, $(subst /,, $@)) -D BUILD_DATE=$(BUILD_DATE) \
			-D SMP_ENABLE=1 -D ACPI_BOOT_ENABLE=1 -D FASTBOOT_LOAD= -D VARIABLE_TYPE=SPI -D STANDARD_MM=TRUE \
			-D COMMIT_HASH=$(SOURCE_COMMIT_HASH) \
			-D EDK2_COMMIT_HASH=$(EDK2_COMMIT_HASH) \
			-D EDK2_NON_OSI_COMMIT_HASH=$(EDK2_NON_OSI_COMMIT_HASH) \
			-D EDK2_PLATFORMS_COMMIT_HASH=$(EDK2_PLATFORMS_COMMIT_HASH) \
			-D DEB_VERSION=$(DEB_VERSION)
	}
	if [[ "$(V)" == "1" ]]; then
		run_edk2_build
	else
		run_edk2_build 2>&1 | ARTEFACT_MODE="$(ARTEFACT_MODE)" python3 "$(EDK2_BUILD_OUTPUT_FILTER)"
	fi
	if [[ "$(ARTEFACT_MODE)" == "custom" ]]; then
		if [[ ! -d "$$WORKSPACE/Build/$*/$(UEFI_TARGET)_GCC5" ]]; then
			echo "[build/$*] Expected custom workspace output $$WORKSPACE/Build/$*/$(UEFI_TARGET)_GCC5 was not produced" >&2
			exit 1
		fi
		mkdir -p "Build/$*"
		rm -rf "Build/$*/$(UEFI_TARGET)_GCC5"
		cp -a "$$WORKSPACE/Build/$*/$(UEFI_TARGET)_GCC5" "Build/$*/"
	fi
	if [[ ! -s "$@" ]]; then
		echo "[build/$*] Expected firmware image $@ was not produced" >&2
		exit 1
	fi

.PHONY: refresh-microsoft-secure-boot-defaults check-microsoft-secure-boot-defaults check-microsoft-secure-boot-release
refresh-microsoft-secure-boot-defaults:
	@python3 ../scripts/generate_microsoft_secure_boot_defaults.py >/dev/null

check-microsoft-secure-boot-defaults:
	@python3 ../scripts/generate_microsoft_secure_boot_defaults.py --check

check-microsoft-secure-boot-release:
	@python3 ../scripts/check_microsoft_secure_boot_release.py \
		--mode "$(MICROSOFT_SECURE_BOOT_RELEASE_CHECK_MODE)"

.PHONY: validate-secure-boot-defaults validate-secure-boot-defaults-%
validate-secure-boot-defaults: $(foreach board,$(FIRMWARE_BOARDS),validate-secure-boot-defaults-$(board))

validate-secure-boot-defaults-%: Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd
	@python3 ../scripts/validate_secure_boot_defaults.py \
		--repo-root "$(abspath ..)" \
		--board "$*" \
		--target "$(UEFI_TARGET)_GCC5"

$(MICROSOFT_SECURE_BOOT_PAYLOADS): ../scripts/generate_microsoft_secure_boot_defaults.py $(MICROSOFT_SECURE_BOOT_MANIFEST)
	@python3 ../scripts/generate_microsoft_secure_boot_defaults.py >/dev/null

.PHONY: clean __clean
clean:
	$(Q)$(LOCKED_MAKE) __clean V="$(V)"

.SILENT: __clean __distclean
__clean:
	printf '[clean] Removing build output\n'
	rm -rf Build
	rm -rf "$(CUSTOM_EDK2_WORKSPACE)"
	printf '[clean] Removing host-built helper tools\n'
	rm -rf "$(FIPTOOL_SOURCE_DIR)/build"
	rm -f "$(TRUSTED_KEY_CERT_TOOL_BIN)" "$(CERT_UEFI_CREATE_RSA_BIN)" "$(CIX_PACKAGE_TOOL_BIN)"
	rm -f "$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)"/*.o "$(CERT_UEFI_CREATE_RSA_SOURCE_DIR)"/*.o
	find "$(CIX_PACKAGE_TOOL_SOURCE_DIR)" -type f -name '*.pyc' -delete
	find "$(CIX_PACKAGE_TOOL_SOURCE_DIR)" -type d -name '__pycache__' -prune -exec rm -rf {} +
	printf '[clean] Cleaning BaseTools output without invoking host toolchain checks\n'
	rm -rf "$(BASETOOLS_C_BIN_DIR)" edk2/BaseTools/Source/C/libs
	printf '[clean] Removing generated EDK2 Conf state\n'
	rm -f "$(BASETOOLS_C_MARKER)" \
		edk2/Conf/.AutoGenIdFile.txt \
		edk2/Conf/BuildEnv.sh \
		edk2/Conf/build_rule.txt \
		edk2/Conf/tools_def.txt \
		edk2/Conf/target.txt
	rm -f \
		edk2/BaseTools/Source/C/VfrCompile/Pccts/antlr/antlr \
		edk2/BaseTools/Source/C/VfrCompile/Pccts/dlg/dlg \
		edk2/BaseTools/Source/C/VfrCompile/EfiVfrParser.cpp \
		edk2/BaseTools/Source/C/VfrCompile/EfiVfrParser.h \
		edk2/BaseTools/Source/C/VfrCompile/VfrParser.dlg \
		edk2/BaseTools/Source/C/VfrCompile/VfrTokens.h \
		edk2/BaseTools/Source/C/VfrCompile/VfrLexer.cpp \
		edk2/BaseTools/Source/C/VfrCompile/VfrLexer.h \
		edk2/BaseTools/Source/C/VfrCompile/VfrSyntax.cpp \
		edk2/BaseTools/Source/C/VfrCompile/tokens.h
	find edk2/BaseTools/Source/C -type f \( \
		-name '*.o' -o -name '*.d' -o -name '*.a' -o -name '*.so' -o \
		-name '*.dll' -o -name '*.obj' -o -name '*.exe' \
	\) -delete
	find edk2/BaseTools -type f -name '*.pyc' -delete
	find edk2/BaseTools -type d -name '__pycache__' -prune -exec rm -rf {} +

.PHONY: distclean __distclean
distclean:
	$(Q)$(LOCKED_MAKE) __distclean V="$(V)"

__distclean: __clean
