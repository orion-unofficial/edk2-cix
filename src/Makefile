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
UPSTREAM_SOURCE_REF ?= refs/upstream-source/main
BUILD_METADATA_SCRIPT := scripts/resolve_build_metadata.sh
MONOREPO_EDK2_COMMIT_HASH ?= 898a41ed09
MONOREPO_EDK2_NON_OSI_COMMIT_HASH ?= 50f168f
MONOREPO_EDK2_PLATFORMS_COMMIT_HASH ?= 918cb36f
ARTEFACT_MODE ?= custom

# The firmware UI historically displayed the abbreviated hashes emitted by the
# original upstream superproject and its submodule repos. Recomputing
# `git rev-parse --short` inside the monorepo can produce different lengths, so
# keep the recorded upstream abbreviations unless the caller overrides them.
EDK2_COMMIT_HASH ?= $(MONOREPO_EDK2_COMMIT_HASH)
EDK2_NON_OSI_COMMIT_HASH ?= $(MONOREPO_EDK2_NON_OSI_COMMIT_HASH)
EDK2_PLATFORMS_COMMIT_HASH ?= $(MONOREPO_EDK2_PLATFORMS_COMMIT_HASH)
SOURCE_COMMIT := $(strip $(shell $(BUILD_METADATA_SCRIPT) source-commit 2>/dev/null || git rev-parse HEAD))
SOURCE_COMMIT_HASH := $(strip $(shell $(BUILD_METADATA_SCRIPT) source-commit-short 2>/dev/null || git rev-parse --short=10 HEAD))
SOURCE_DATE_EPOCH ?= $(strip $(shell $(BUILD_METADATA_SCRIPT) source-date-epoch 2>/dev/null || git log -1 --format=%ct HEAD))
PM_CONFIG_SOURCE_DATE_EPOCH ?= $(SOURCE_DATE_EPOCH)
BUILD_DATE := $(strip $(shell SOURCE_DATE_EPOCH="$(SOURCE_DATE_EPOCH)" $(BUILD_METADATA_SCRIPT) source-date-iso 2>/dev/null || git log -1 --format=%cI HEAD))
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
SIGNING_CERT_SOURCE_DIR ?=
DSC := $(shell find edk2-platforms/Platform/Radxa -mindepth 3 -maxdepth 3 -name "*.dsc" -type "f") \
	#    $(shell find edk2-platforms/Platform/CIX -mindepth 3 -maxdepth 3 -name "*.dsc" -type "f")

CIX_FLASH := $(foreach i, $(patsubst edk2-platforms/Platform/%.dsc,%,$(DSC)), Build/$(lastword $(subst /, ,$(i)))/$(UEFI_TARGET)_GCC5/cix_flash_all.bin) \
			 $(foreach i, $(patsubst edk2-platforms/Platform/%.dsc,%,$(DSC)), Build/$(lastword $(subst /, ,$(i)))/$(UEFI_TARGET)_GCC5/cix_flash_ota.bin)
.PHONY: build
build: preflight $(CIX_FLASH)

.SILENT: help print-build-metadata clean preflight preflight-% preflight-tools
.PHONY: help
help:
	echo "Supported targets:"
	echo "$(CIX_FLASH)"
	echo "preflight"

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
	if [[ -n "$(GCC5_AARCH64_PREFIX)" ]] && ! command -v "$(GCC5_AARCH64_PREFIX)gcc" >/dev/null 2>&1; then \
		echo "[preflight] Missing cross-compiler: $(GCC5_AARCH64_PREFIX)gcc" >&2; \
		missing=1; \
	fi; \
	for required_bin in \
		"$(PACKAGE_TOOL)/$(PACKAGE_TOOL_ARCH)/cix_package_tool" \
		"$(PACKAGE_TOOL)/$(PACKAGE_TOOL_ARCH)/fiptool" \
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
	"$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_package_tool" -c "./spi_flash_config_ota.json" -O "$$tmp_ota" && \
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
		mv "$$MEM/memory_config.bin" Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/; \
	fi
	export PM="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/pm_config" -type d)" && \
	if [[ -d "$$PM" ]]; then \
		printf '[build/%s] Regenerating PM configuration\n' "$*" && \
		echo "Found custom PM configuration."; \
		rm -f Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/csu_pm_config.bin; \
		PM_CONFIG_SOURCE_DATE_EPOCH="$(PM_CONFIG_SOURCE_DATE_EPOCH)" $(MAKE) -C "$$PM" -j$(shell nproc); \
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
	"$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_package_tool" -c "./spi_flash_config_all.json" -o "./cix_flash_all.raw" && \
	popd

	# \377 = 0xFF
	tmp_all="Build/$*/$(UEFI_TARGET)_GCC5/cix_flash_all.bin.tmp" && \
	rm -f "$$tmp_all" && \
	head -c 8M < /dev/zero | tr '\000' '\377' > "$$tmp_all" && \
	dd if="Build/$*/$(UEFI_TARGET)_GCC5/cix_flash_all.raw" of="$$tmp_all" conv=notrunc status=none && \
	mv -f "$$tmp_all" "Build/$*/$(UEFI_TARGET)_GCC5/cix_flash_all.bin"

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img
Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img: \
	Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd \
	Build/%/$(UEFI_TARGET)_GCC5/Firmwares/dummy.bin \
	Build/%/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt

	@export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	export SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" && \
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
		trusted_key_cert_src="$$(select_cert_source "$$SIGNING_CERT_SOURCE_DIR" trusted_key_no.crt trusted-key-cert.bin || true)"; \
		nt_fw_cert_src="$$(select_cert_source "$$SIGNING_CERT_SOURCE_DIR" nt_fw_cert.crt nt-fw-cert.bin || true)"; \
		nt_fw_key_cert_src="$$(select_cert_source "$$SIGNING_CERT_SOURCE_DIR" nt_fw_key.crt nt-fw-key-cert.bin || true)"; \
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
	"$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/fiptool" create \
		--trusted-key-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt \
		--nt-fw-key-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_key.crt \
		--nt-fw-cert Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_cert.crt \
		--nt-fw "$(shell realpath "$<")" \
		$@

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
	@cp -aR $(PACKAGE_TOOL)/Keys/ $(PACKAGE_TOOL)/certs/ Build/$*/$(UEFI_TARGET)_GCC5/
	if [[ -f "$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_regen_trusted_key_cert" ]]; then \
    	"$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_regen_trusted_key_cert" \
			-p Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_publickey.pem \
			-s Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
			-o $@; \
	fi

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd tools/acpica/generate/unix/bin/iasl
Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd: preflight-%
	@$(MAKE) -C edk2/BaseTools -j$(shell nproc) Source/C
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
		"$${build_extra_defines[@]}" \
		-b $(UEFI_TARGET) -D BOARD_NAME=$(word 2, $(subst /,, $@)) -D BUILD_DATE=$(BUILD_DATE) \
		-D SMP_ENABLE=1 -D ACPI_BOOT_ENABLE=1 -D FASTBOOT_LOAD= -D VARIABLE_TYPE=SPI -D STANDARD_MM=TRUE \
		-D COMMIT_HASH=$(SOURCE_COMMIT_HASH) \
		-D EDK2_COMMIT_HASH=$(EDK2_COMMIT_HASH) \
		-D EDK2_NON_OSI_COMMIT_HASH=$(EDK2_NON_OSI_COMMIT_HASH) \
		-D EDK2_PLATFORMS_COMMIT_HASH=$(EDK2_PLATFORMS_COMMIT_HASH) \
		-D DEB_VERSION=$(shell cd .. && dpkg-parsechangelog -S Version)

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
