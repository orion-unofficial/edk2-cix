UEFI_TARGET ?= RELEASE
# Specify DEBUG build for development
# UEFI_TARGET ?= DEBUG

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
UPSTREAM_SOURCE_REF ?= refs/upstream-source/main
BUILD_METADATA_SCRIPT := scripts/resolve_build_metadata.sh
EDK2_BUILD_OUTPUT_FILTER := scripts/filter_edk2_build_output.py
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
MONOREPO_EDK2_COMMIT_HASH ?= 898a41ed09
MONOREPO_EDK2_NON_OSI_COMMIT_HASH ?= 50f168f
MONOREPO_EDK2_PLATFORMS_COMMIT_HASH ?= 918cb36f
ARTEFACT_MODE ?= custom
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
# GCC5_AARCH64_PREFIX := $(shell pwd)/tools/gcc/gcc-arm-10.2-2020.11-aarch64-aarch64-none-elf/bin/aarch64-none-elf-gcc
else
PACKAGE_TOOL_ARCH := X86_64
GCC5_AARCH64_PREFIX := aarch64-linux-gnu-
# GCC5_AARCH64_PREFIX := $(shell pwd)/tools/gcc/gcc-arm-10.2-2020.11-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
endif

PACKAGE_TOOL := edk2-non-osi/Platform/CIX/Sky1/PackageTool
FIPTOOL_SOURCE_DIR := tools/arm-trusted-firmware-fiptool
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
TRUSTED_KEY_CERT_TOOL_SOURCE_DIR := $(PACKAGE_TOOL)/source_tools/cix_regen_trusted_key_cert
TRUSTED_KEY_CERT_TOOL_BIN := $(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)/cix_regen_trusted_key_cert
TRUSTED_KEY_CERT_TOOL_SOURCE_INPUTS := \
	$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)/Makefile \
	$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)/cix_regen_trusted_key_cert.c
HOST_TOOLS_DIR := Build/HostTools
CIX_PACKAGE_TOOL_SOURCE_DIR := $(PACKAGE_TOOL)/source_tools/cix_package_tool
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
.PHONY: build __build host-fiptool host-cix-regen-trusted-key-cert host-cix-package-tool
build:
	$(Q)$(LOCKED_MAKE) __build V="$(V)"

__build: preflight $(CIX_FLASH)

host-fiptool: $(FIPTOOL_BIN)
host-cix-regen-trusted-key-cert:
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)" V="$(V)" clean all
host-cix-package-tool: $(CIX_PACKAGE_TOOL_BIN)

.SILENT: help print-build-metadata clean preflight preflight-% preflight-tools preflight-build-metadata host-fiptool host-cix-regen-trusted-key-cert host-cix-package-tool prepare-basetools-c
.PHONY: help
help:
	echo "Supported targets:"
	echo "$(CIX_FLASH)"
	echo "host-fiptool"
	echo "host-cix-regen-trusted-key-cert"
	echo "host-cix-package-tool"
	echo "prepare-basetools-c"
	echo "preflight"
	echo "Use V=1 for verbose EDK2 build commands"

$(FIPTOOL_BIN): $(FIPTOOL_SOURCE_INPUTS)
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(FIPTOOL_SOURCE_DIR)" HOST_ARCH="$(HOST_ARCH)" V="$(V)" all

$(TRUSTED_KEY_CERT_TOOL_BIN): $(TRUSTED_KEY_CERT_TOOL_SOURCE_INPUTS)
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)" V="$(V)" clean all

$(CIX_PACKAGE_TOOL_BIN): $(CIX_PACKAGE_TOOL_SOURCE_INPUTS)
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
		case "$(ARTEFACT_MODE)" in \
			custom|upstream) \
				;; \
			*) \
				echo "[preflight/metadata] Unsupported ARTEFACT_MODE=$(ARTEFACT_MODE); expected custom or upstream" >&2; \
				missing=1; \
				;; \
		esac; \
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
		$(MAKE) -C edk2/BaseTools -j$(BASETOOLS_BUILD_JOBS) V="$(V)" Source/C
		printf '%s\n' "$$current_env_id" > "$(BASETOOLS_C_MARKER)"
	fi

.PHONY: preflight preflight-%
preflight: preflight-tools preflight-build-metadata

preflight-%: preflight-tools preflight-build-metadata
	@dsc_path="$(filter %/$*.dsc,$(DSC))" && \
	if [[ -z "$$dsc_path" ]]; then \
		echo "[preflight/$*] No matching DSC found under edk2-platforms/Platform/Radxa" >&2; \
		exit 1; \
	fi

.PHONY: preflight-tools
preflight-tools:
	@missing=0; \
	for required_dir in edk2 edk2-platforms edk2-non-osi "$(PACKAGE_TOOL)"; do \
		if [[ ! -e "$$required_dir" ]]; then \
			echo "[preflight] Missing required path: $$required_dir" >&2; \
			missing=1; \
		fi; \
	done; \
	for required_cmd in bash find grep head realpath tr dd python3 iasl; do \
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
	for required_bin in \
		"$(PACKAGE_TOOL)/$(PACKAGE_TOOL_ARCH)/cert_uefi_create_rsa"; do \
		if [[ ! -x "$$required_bin" ]]; then \
			echo "[preflight] Missing required package-tool binary: $$required_bin" >&2; \
			missing=1; \
		fi; \
	done; \
	if [[ "$$missing" -ne 0 ]]; then \
		exit 1; \
	fi

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/cix_flash_ota.bin
Build/%/$(UEFI_TARGET)_GCC5/cix_flash_ota.bin: preflight-% Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img | $(CIX_PACKAGE_TOOL_BIN)
	@export CFG="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/spi_flash_config_ota.json" -type f)" && \
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
	@export MEM="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/mem_config" -type d)" && \
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
	export PM="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/pm_config" -type d)" && \
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

	export CFG="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/spi_flash_config_all.json" -type f)" && \
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
	| $(FIPTOOL_BIN)

	@export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
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
	if [[ -n "$$SIGNING_CERT_SOURCE_DIR" ]]; then \
		if [[ ! -d "$$SIGNING_CERT_SOURCE_DIR" ]]; then \
			echo "SIGNING_CERT_SOURCE_DIR=$$SIGNING_CERT_SOURCE_DIR does not exist" >&2; \
			exit 1; \
		fi; \
		trusted_key_cert_src="$$(select_cert_source "$$SIGNING_CERT_SOURCE_DIR" trusted_key_no.crt trusted-key-cert.bin || true)"; \
		nt_fw_cert_src="$$(select_cert_source "$$SIGNING_CERT_SOURCE_DIR" nt_fw_cert.crt nt-fw-cert.bin || true)"; \
		nt_fw_key_cert_src="$$(select_cert_source "$$SIGNING_CERT_SOURCE_DIR" nt_fw_key.crt nt-fw-key-cert.bin || true)"; \
		if [[ "$$REQUIRE_SIGNING_CERT_SOURCE" == "1" ]]; then \
			if [[ -z "$$trusted_key_cert_src" || -z "$$nt_fw_cert_src" || -z "$$nt_fw_key_cert_src" ]]; then \
				echo "ARTEFACT_MODE=upstream requires SIGNING_CERT_SOURCE_DIR to provide trusted_key_no.crt|trusted-key-cert.bin, nt_fw_cert.crt|nt-fw-cert.bin, and nt_fw_key.crt|nt-fw-key-cert.bin" >&2; \
				exit 1; \
			fi; \
		fi; \
		if [[ -n "$$trusted_key_cert_src" ]]; then \
			echo "Reusing trusted-key cert from $$trusted_key_cert_src"; \
			cp "$$trusted_key_cert_src" Build/$*/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt; \
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
	if [[ ! -f Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_cert.crt || ! -f Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_key.crt ]]; then \
		run_success_only "$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cert_uefi_create_rsa" --key-alg rsa --key-size 3072 --hash-alg sha256 -p --ntfw-nvctr 223 \
			--nt-fw-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_cert.crt \
			--nt-fw-key-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_key.crt \
			--nt-fw-key Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
			--non-trusted-world-key Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
			--nt-fw "$(shell realpath "$<")"; \
	fi && \
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
	@cp -aR $(PACKAGE_TOOL)/Firmwares/ Build/$*/$(UEFI_TARGET)_GCC5/
	# \377 = 0xFF
	head -c 8192 < /dev/zero | tr '\000' '\377' > $@

	export FW="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/Firmwares" -type d)" && \
	if [[ -d "$$FW" ]]; then \
		if [[ "$(V)" == "1" ]]; then echo "Found custom firmware."; fi; \
		cp -r "$$FW/." Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/; \
	fi; \
	if [[ ! -e "$$FW" ]] || [[ ! -f "$$FW/ec_fw.bin" ]]; then \
		echo "Found EC-less platform."; \
		ln -sf dummy.bin Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/ec_fw.bin; \
	fi

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt
Build/%/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt:
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
		custom) \
			$(MAKE) --no-print-directory host-cix-regen-trusted-key-cert V="$(V)"; \
			run_success_only "$$SOURCE_TOOL_BIN" \
				-p Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_publickey.pem \
				-s Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
				-o $@; \
			;; \
		upstream) \
			if [[ -f "$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_regen_trusted_key_cert" ]]; then \
				run_success_only "$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_regen_trusted_key_cert" \
					-p Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_publickey.pem \
					-s Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
					-o $@; \
			else \
				echo "ARTEFACT_MODE=upstream requires SIGNING_CERT_SOURCE_DIR or the bundled cix_regen_trusted_key_cert binary" >&2; \
				exit 1; \
			fi; \
			;; \
		*) \
			echo "Unsupported ARTEFACT_MODE=$(ARTEFACT_MODE); expected custom or upstream" >&2; \
			exit 1; \
			;; \
	esac

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd
Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd: preflight-%
	@$(MAKE) prepare-basetools-c
	export WORKSPACE="$(shell pwd)"
	export PACKAGES_PATH="$$WORKSPACE/edk2:$$WORKSPACE/edk2-platforms:$$WORKSPACE/edk2-non-osi:$$WORKSPACE/edk2-platforms/Silicon/Intel/"
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
		run_edk2_build 2>&1 | python3 "$(EDK2_BUILD_OUTPUT_FILTER)"
	fi
	if [[ ! -s "$@" ]]; then
		echo "[build/$*] Expected firmware image $@ was not produced" >&2
		exit 1
	fi

.PHONY: clean __clean
clean:
	$(Q)$(LOCKED_MAKE) __clean V="$(V)"

.SILENT: __clean __distclean
__clean:
	printf '[clean] Removing build output\n'
	rm -rf Build
	printf '[clean] Removing host-built helper tools\n'
	rm -rf "$(FIPTOOL_SOURCE_DIR)/build"
	rm -f "$(TRUSTED_KEY_CERT_TOOL_BIN)" "$(CIX_PACKAGE_TOOL_BIN)"
	rm -f "$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)"/*.o
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
