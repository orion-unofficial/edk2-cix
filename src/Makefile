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
HOST_ARCH := $(if $(filter arm64,$(UNAME)),aarch64,$(UNAME))
UPSTREAM_EDK2_REF ?= refs/heads/main-monorepo-upstream-edk2
BUILD_METADATA_SCRIPT := scripts/resolve_build_metadata.sh
MONOREPO_EDK2_COMMIT_HASH ?= 971cfbc5fa
MONOREPO_EDK2_NON_OSI_COMMIT_HASH ?= c051ce622f
MONOREPO_EDK2_PLATFORMS_COMMIT_HASH ?= 971cfbc5fa
V ?= 0
MONOREPO_EDK2_COMMIT_HASH ?= 898a41ed09
MONOREPO_EDK2_NON_OSI_COMMIT_HASH ?= 50f168f
MONOREPO_EDK2_PLATFORMS_COMMIT_HASH ?= 918cb36f
ARTEFACT_MODE ?= custom
EDK2_BUILD_SILENT_FLAG := $(if $(filter 1,$(V)),,-s)

resolve-path-head = $(strip $(shell git -C .. log -1 --format=%h $(1) -- $(2) 2>/dev/null))

# On main-monorepo-edk2, the imported component trees are regular directories
# overlaid on top of the uplifted main-monorepo-upstream-edk2 history. Prefer
# the latest upstream-edk2 branch commit touching each imported component path
# when it is available locally, then fall back to the recorded abbreviations.
EDK2_COMMIT_HASH := $(or $(call resolve-path-head,$(UPSTREAM_EDK2_REF),src/edk2),$(MONOREPO_EDK2_COMMIT_HASH))
EDK2_NON_OSI_COMMIT_HASH := $(or $(call resolve-path-head,$(UPSTREAM_EDK2_REF),src/edk2-non-osi),$(MONOREPO_EDK2_NON_OSI_COMMIT_HASH))
EDK2_PLATFORMS_COMMIT_HASH := $(or $(call resolve-path-head,$(UPSTREAM_EDK2_REF),src/edk2-platforms),$(MONOREPO_EDK2_PLATFORMS_COMMIT_HASH))
SOURCE_COMMIT ?= $(strip $(shell $(BUILD_METADATA_SCRIPT) source-commit 2>/dev/null || true))
SOURCE_COMMIT_HASH ?= $(strip $(shell $(BUILD_METADATA_SCRIPT) source-commit-short 2>/dev/null || true))
SOURCE_DATE_EPOCH ?= $(strip $(shell $(BUILD_METADATA_SCRIPT) source-date-epoch 2>/dev/null || true))
PM_CONFIG_SOURCE_DATE_EPOCH ?= $(SOURCE_DATE_EPOCH)
BUILD_DATE ?= $(strip $(shell SOURCE_DATE_EPOCH="$(SOURCE_DATE_EPOCH)" $(BUILD_METADATA_SCRIPT) source-date-iso 2>/dev/null || true))
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
SIGNING_CERT_SOURCE_DIR ?=
REQUIRE_SIGNING_CERT_SOURCE ?= $(if $(and $(filter upstream,$(ARTEFACT_MODE)),$(strip $(SIGNING_CERT_SOURCE_DIR))),1,0)
BASETOOLS_C_BIN_DIR := edk2/BaseTools/Source/C/bin
BASETOOLS_C_MARKER := $(BASETOOLS_C_BIN_DIR)/.edk2-cix-build-env
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

CIX_FLASH := $(foreach i, $(patsubst edk2-platforms/Platform/%.dsc,%,$(DSC)), Build/$(lastword $(subst /, ,$(i)))/$(UEFI_TARGET)_GCC5/cix_flash_all.bin) \
			 $(foreach i, $(patsubst edk2-platforms/Platform/%.dsc,%,$(DSC)), Build/$(lastword $(subst /, ,$(i)))/$(UEFI_TARGET)_GCC5/cix_flash_ota.bin)
.PHONY: build host-fiptool
build: preflight $(CIX_FLASH)

host-fiptool: $(FIPTOOL_BIN)

.SILENT: help print-build-metadata clean preflight preflight-% preflight-tools host-fiptool prepare-basetools-c
.PHONY: help
help:
	echo "Supported targets:"
	echo "$(CIX_FLASH)"
	echo "host-fiptool"
	echo "prepare-basetools-c"
	echo "preflight"
	echo "Use V=1 for verbose EDK2 build commands"

$(FIPTOOL_BIN): $(FIPTOOL_SOURCE_INPUTS)
	$(MAKE) -C "$(FIPTOOL_SOURCE_DIR)" HOST_ARCH="$(HOST_ARCH)" all

.PHONY: print-build-metadata
print-build-metadata:
	echo "ARTEFACT_MODE=$(ARTEFACT_MODE)"
	echo "SOURCE_COMMIT=$(SOURCE_COMMIT)"
	echo "SOURCE_COMMIT_HASH=$(SOURCE_COMMIT_HASH)"
	echo "SOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH)"
	echo "PM_CONFIG_SOURCE_DATE_EPOCH=$(PM_CONFIG_SOURCE_DATE_EPOCH)"
	echo "BUILD_DATE=$(BUILD_DATE)"
	echo "EDK2_COMMIT_HASH=$(EDK2_COMMIT_HASH)"
	echo "EDK2_NON_OSI_COMMIT_HASH=$(EDK2_NON_OSI_COMMIT_HASH)"
	echo "EDK2_PLATFORMS_COMMIT_HASH=$(EDK2_PLATFORMS_COMMIT_HASH)"

.PHONY: prepare-basetools-c
prepare-basetools-c:
	@mkdir -p "$(BASETOOLS_C_BIN_DIR)"
	current_env_id="$$( \
		{ source /etc/os-release >/dev/null 2>&1 && true; } || true; \
		printf 'arch=%s;os=%s;version=%s;cc=%s;ldd=%s\n' \
			"$$(uname -sm)" \
			"$${ID:-unknown}" \
			"$${VERSION_ID:-unknown}" \
			"$$(cc --version 2>/dev/null | head -n1 || echo unavailable)" \
			"$$(ldd --version 2>/dev/null | head -n1 || echo unavailable)"; \
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
		$(MAKE) -C edk2/BaseTools -j$(shell nproc) Source/C
		printf '%s\n' "$$current_env_id" > "$(BASETOOLS_C_MARKER)"
	fi

.PHONY: preflight preflight-%
preflight: preflight-tools

preflight-%: preflight-tools
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
	for required_cmd in bash find grep head realpath tr dd python3; do \
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
	for required_bin in \
		"$(PACKAGE_TOOL)/$(PACKAGE_TOOL_ARCH)/cix_package_tool" \
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
Build/%/$(UEFI_TARGET)_GCC5/cix_flash_ota.bin: preflight-% Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img
	@export CFG="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/spi_flash_config_ota.json" -type f)" && \
	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	if [[ -f "$$CFG" ]]; then \
		echo "Found custom flash config."; \
		cp "$$CFG" Build/$*/$(UEFI_TARGET)_GCC5/; \
	else \
		cp "$$PATH_PACKAGE_TOOL/spi_flash_config_ota.json" Build/$*/$(UEFI_TARGET)_GCC5/; \
	fi

	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	printf '[build/%s] Packaging OTA flash image\n' "$*" && \
	pushd "Build/$*/$(UEFI_TARGET)_GCC5/" && \
	tmp_ota="./cix_flash_ota.bin.tmp" && \
	rm -f "$$tmp_ota" && \
	TERM=dumb "$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_package_tool" -c "./spi_flash_config_ota.json" -O "$$tmp_ota" 2>&1 | perl -pe 's/\e\[[0-9;]*m//g' && \
	if [[ ! -s "$$tmp_ota" ]]; then \
		echo "[build/$*] Expected OTA image $$tmp_ota was not created" >&2; \
		exit 1; \
	fi && \
	mv -f "$$tmp_ota" "./cix_flash_ota.bin" && \
	popd

# MEM_CFG_MEMFREQ ?= 2750
MEM_CFG_MEMFREQ ?= 3000

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/cix_flash_all.bin
Build/%/$(UEFI_TARGET)_GCC5/cix_flash_all.bin: preflight-% Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img
	@export MEM="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/mem_config" -type d)" && \
	if [[ -d "$$MEM" ]]; then \
		printf '[build/%s] Regenerating memory configuration\n' "$*" && \
		echo "Found custom memory configuration."; \
		rm -f Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/memory_config.bin; \
		$(MAKE) -C "$$MEM" -j$(shell nproc) -e CFLAG:="-DMEM_CFG_MEMFREQ=$(MEM_CFG_MEMFREQ)" clean main; \
		if [[ ! -s "$$MEM/memory_config.bin" ]]; then \
			echo "[build/$*] Expected memory_config.bin was not generated in $$MEM" >&2; \
			exit 1; \
		fi; \
		mv "$$MEM/memory_config.bin" Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/; \
	fi
	export PM="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/pm_config" -type d)" && \
	if [[ -d "$$PM" ]]; then \
		printf '[build/%s] Regenerating PM configuration\n' "$*" && \
		echo "Found custom PM configuration."; \
		rm -f Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/csu_pm_config.bin; \
		PM_CONFIG_SOURCE_DATE_EPOCH="$(PM_CONFIG_SOURCE_DATE_EPOCH)" $(MAKE) -C "$$PM" -j$(shell nproc); \
		if [[ ! -s "$$PM/csu_pm_config.bin" ]]; then \
			echo "[build/$*] Expected csu_pm_config.bin was not generated in $$PM" >&2; \
			exit 1; \
		fi; \
		mv "$$PM/csu_pm_config.bin" Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/; \
	fi

	export CFG="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/spi_flash_config_all.json" -type f)" && \
	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	if [[ -f "$$CFG" ]]; then \
		echo "Found custom flash config."; \
		cp "$$CFG" Build/$*/$(UEFI_TARGET)_GCC5/; \
	else \
		cp "$$PATH_PACKAGE_TOOL/spi_flash_config_all.json" Build/$*/$(UEFI_TARGET)_GCC5/; \
	fi

	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	printf '[build/%s] Packaging full flash image\n' "$*" && \
	pushd "Build/$*/$(UEFI_TARGET)_GCC5/" && \
	TERM=dumb "$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_package_tool" -c "./spi_flash_config_all.json" -o "./cix_flash_all.raw" 2>&1 | perl -pe 's/\e\[[0-9;]*m//g' && \
	if [[ ! -s "./cix_flash_all.raw" ]]; then \
		echo "[build/$*] Expected cix_flash_all.raw was not created" >&2; \
		exit 1; \
	fi && \
	popd

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

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img
Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img: \
	Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd \
	Build/%/$(UEFI_TARGET)_GCC5/Firmwares/dummy.bin \
	Build/%/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt \
	| $(FIPTOOL_BIN)

	@export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	export SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" && \
	export REQUIRE_SIGNING_CERT_SOURCE="$(REQUIRE_SIGNING_CERT_SOURCE)" && \
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
		"$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cert_uefi_create_rsa" --key-alg rsa --key-size 3072 --hash-alg sha256 -p --ntfw-nvctr 223 \
			--nt-fw-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_cert.crt \
			--nt-fw-key-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_key.crt \
			--nt-fw-key Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
			--non-trusted-world-key Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
			--nt-fw "$(shell realpath "$<")"; \
	fi && \
	"$(FIPTOOL_BIN)" create \
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
		echo "Found custom firmware."; \
		cp -r "$$FW/." Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/; \
	fi; \
	if [[ ! -e "$$FW" ]] || [[ ! -f "$$FW/ec_fw.bin" ]]; then \
		echo "Found EC-less platform."; \
		ln -sf dummy.bin Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/ec_fw.bin; \
	fi

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt
Build/%/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt:
	@export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	mkdir -p Build/$*/$(UEFI_TARGET)_GCC5/Keys Build/$*/$(UEFI_TARGET)_GCC5/certs && \
	cp -a $(PACKAGE_TOOL)/Keys/. Build/$*/$(UEFI_TARGET)_GCC5/Keys/ && \
	cp -a $(PACKAGE_TOOL)/certs/. Build/$*/$(UEFI_TARGET)_GCC5/certs/ && \
	if [[ -f "$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_regen_trusted_key_cert" ]]; then \
    	"$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_regen_trusted_key_cert" \
			-p Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_publickey.pem \
			-s Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
			-o $@; \
	fi

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd tools/acpica/generate/unix/bin/iasl
Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd: preflight-%
	@$(MAKE) prepare-basetools-c
	export WORKSPACE="$(shell pwd)"
	export PACKAGES_PATH="$$WORKSPACE/edk2:$$WORKSPACE/edk2-platforms:$$WORKSPACE/edk2-non-osi:$$WORKSPACE/edk2-platforms/Silicon/Intel/"
	export GCC5_AARCH64_PREFIX="$(GCC5_AARCH64_PREFIX)"
	if [[ -f tools/acpica/generate/unix/bin/iasl ]]; then
		export IASL_PREFIX="$$WORKSPACE/tools/acpica/generate/unix/bin/"
	fi
	unset MAKEFLAGS
	source edk2/edksetup.sh --reconfig
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
	build -a AARCH64 -t GCC5 -p "$(subst edk2-platforms/,,$(filter %/$*.dsc,$(DSC)))" \
		$(EDK2_BUILD_SILENT_FLAG) \
		"$${build_extra_defines[@]}" \
		-b $(UEFI_TARGET) -D BOARD_NAME=$(word 2, $(subst /,, $@)) -D BUILD_DATE=$(BUILD_DATE) \
		-D SMP_ENABLE=1 -D ACPI_BOOT_ENABLE=1 -D FASTBOOT_LOAD= -D VARIABLE_TYPE=SPI -D STANDARD_MM=TRUE \
		-D COMMIT_HASH=$(SOURCE_COMMIT_HASH) \
		-D EDK2_COMMIT_HASH=$(EDK2_COMMIT_HASH) \
		-D EDK2_NON_OSI_COMMIT_HASH=$(EDK2_NON_OSI_COMMIT_HASH) \
		-D EDK2_PLATFORMS_COMMIT_HASH=$(EDK2_PLATFORMS_COMMIT_HASH) \
		-D DEB_VERSION=$(shell cd .. && dpkg-parsechangelog -S Version)
	if [[ ! -s "$@" ]]; then
		echo "[build/$*] Expected firmware image $@ was not produced" >&2
		exit 1
	fi

tools/acpica/generate/unix/bin/iasl:
	@$(MAKE) -C tools/acpica -j$(shell nproc)

.PHONY: clean
clean:
	printf '[clean] Removing build output\n'
	rm -rf Build
	printf '[clean] Cleaning BaseTools\n'
	$(MAKE) -s -C edk2/BaseTools -j$(shell nproc) clean
	if [[ -f tools/acpica/Makefile ]] && grep -Eq '^[[:space:]]*veryclean:' tools/acpica/Makefile; then \
		printf '[clean] Cleaning tools/acpica (veryclean)\n'; \
		$(MAKE) -s -C tools/acpica -j$(shell nproc) veryclean; \
	elif [[ -f tools/acpica/Makefile ]] && grep -Eq '^[[:space:]]*clean:' tools/acpica/Makefile; then \
		printf '[clean] tools/acpica has no veryclean target; using clean instead\n'; \
		$(MAKE) -s -C tools/acpica -j$(shell nproc) clean; \
	else \
		printf '[clean] tools/acpica has no clean target; skipping extra cleanup\n'; \
	fi

.PHONY: distclean
distclean: clean
