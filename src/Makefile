UEFI_TARGET ?= RELEASE
# Specify DEBUG build for development
# UEFI_TARGET ?= DEBUG

.PHONY: all
all: build

.SHELLFLAGS := -ec
.ONESHELL:
SHELL := bash
UNAME := $(shell uname -m)
UPSTREAM_EDK2_REF ?= refs/heads/main-monorepo-upstream-edk2
BUILD_METADATA_SCRIPT := scripts/resolve_build_metadata.sh
MONOREPO_EDK2_COMMIT_HASH ?= 971cfbc5fa
MONOREPO_EDK2_NON_OSI_COMMIT_HASH ?= c051ce622f
MONOREPO_EDK2_PLATFORMS_COMMIT_HASH ?= 971cfbc5fa

resolve-path-head = $(strip $(shell git -C .. log -1 --format=%h $(1) -- $(2) 2>/dev/null))

# On main-monorepo-edk2, the imported component trees are regular directories
# overlaid on top of the uplifted main-monorepo-upstream-edk2 history. Prefer
# the latest upstream-edk2 branch commit touching each imported component path
# when it is available locally, then fall back to the recorded abbreviations.
EDK2_COMMIT_HASH := $(or $(call resolve-path-head,$(UPSTREAM_EDK2_REF),src/edk2),$(MONOREPO_EDK2_COMMIT_HASH))
EDK2_NON_OSI_COMMIT_HASH := $(or $(call resolve-path-head,$(UPSTREAM_EDK2_REF),src/edk2-non-osi),$(MONOREPO_EDK2_NON_OSI_COMMIT_HASH))
EDK2_PLATFORMS_COMMIT_HASH := $(or $(call resolve-path-head,$(UPSTREAM_EDK2_REF),src/edk2-platforms),$(MONOREPO_EDK2_PLATFORMS_COMMIT_HASH))
SOURCE_COMMIT := $(strip $(shell $(BUILD_METADATA_SCRIPT) source-commit 2>/dev/null || git rev-parse HEAD))
SOURCE_COMMIT_HASH := $(strip $(shell $(BUILD_METADATA_SCRIPT) source-commit-short 2>/dev/null || git rev-parse --short=10 HEAD))
SOURCE_DATE_EPOCH ?= $(strip $(shell $(BUILD_METADATA_SCRIPT) source-date-epoch 2>/dev/null || git log -1 --format=%ct HEAD))
BUILD_DATE := $(strip $(shell SOURCE_DATE_EPOCH="$(SOURCE_DATE_EPOCH)" $(BUILD_METADATA_SCRIPT) source-date-iso 2>/dev/null || git log -1 --format=%cI HEAD))
export SOURCE_DATE_EPOCH

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
build: $(CIX_FLASH)

.SILENT: help
.PHONY: help
help:
	echo "Supported targets:"
	echo "$(CIX_FLASH)"

.PHONY: print-build-metadata
print-build-metadata:
	echo "SOURCE_COMMIT=$(SOURCE_COMMIT)"
	echo "SOURCE_COMMIT_HASH=$(SOURCE_COMMIT_HASH)"
	echo "SOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH)"
	echo "BUILD_DATE=$(BUILD_DATE)"
	echo "EDK2_COMMIT_HASH=$(EDK2_COMMIT_HASH)"
	echo "EDK2_NON_OSI_COMMIT_HASH=$(EDK2_NON_OSI_COMMIT_HASH)"
	echo "EDK2_PLATFORMS_COMMIT_HASH=$(EDK2_PLATFORMS_COMMIT_HASH)"

Build/%/$(UEFI_TARGET)_GCC5/cix_flash_ota.bin: Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img
	export CFG="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/spi_flash_config_ota.json" -type f)" && \
	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	if [[ -f "$$CFG" ]]; then \
		echo "Found custom flash config."; \
		cp "$$CFG" Build/$*/$(UEFI_TARGET)_GCC5/; \
	else \
		cp "$$PATH_PACKAGE_TOOL/spi_flash_config_ota.json" Build/$*/$(UEFI_TARGET)_GCC5/; \
	fi

	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	pushd "Build/$*/$(UEFI_TARGET)_GCC5/" && \
	"$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_package_tool" -c "./spi_flash_config_ota.json" -O "./cix_flash_ota.bin" && \
	popd

# MEM_CFG_MEMFREQ ?= 2750
MEM_CFG_MEMFREQ ?= 3000

Build/%/$(UEFI_TARGET)_GCC5/cix_flash_all.bin: Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img
	export MEM="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/mem_config" -type d)" && \
	if [[ -d "$$MEM" ]]; then \
		echo "Found custom memory configuration."; \
		rm -f Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/memory_config.bin; \
		make -C "$$MEM" -j$(shell nproc) -e CFLAG:="-DMEM_CFG_MEMFREQ=$(MEM_CFG_MEMFREQ)" clean main; \
		mv "$$MEM/memory_config.bin" Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/; \
	fi
	export PM="$(shell find edk2*/Platform/ -mindepth 4 -maxdepth 4 -path "*/$*/pm_config" -type d)" && \
	if [[ -d "$$PM" ]]; then \
		echo "Found custom PM configuration."; \
		rm -f Build/$*/$(UEFI_TARGET)_GCC5/Firmwares/csu_pm_config.bin; \
		make -C "$$PM" -j$(shell nproc); \
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
	pushd "Build/$*/$(UEFI_TARGET)_GCC5/" && \
	"$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_package_tool" -c "./spi_flash_config_all.json" -o "./cix_flash_all.raw" && \
	popd

	# \377 = 0xFF
	head -c 8M < /dev/zero | tr '\000' '\377' > "Build/$*/$(UEFI_TARGET)_GCC5/cix_flash_all.bin"
	dd if="Build/$*/$(UEFI_TARGET)_GCC5/cix_flash_all.raw" of="Build/$*/$(UEFI_TARGET)_GCC5/cix_flash_all.bin" conv=notrunc

Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img: \
	Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd \
	Build/%/$(UEFI_TARGET)_GCC5/Firmwares/dummy.bin \
	Build/%/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt

	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	export SIGNING_CERT_SOURCE_DIR="$(SIGNING_CERT_SOURCE_DIR)" && \
	if [[ -n "$$SIGNING_CERT_SOURCE_DIR" ]]; then \
		cp "$$SIGNING_CERT_SOURCE_DIR/nt_fw_cert.crt" Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_cert.crt; \
		cp "$$SIGNING_CERT_SOURCE_DIR/nt_fw_key.crt" Build/$*/$(UEFI_TARGET)_GCC5/certs/nt_fw_key.crt; \
	else \
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

Build/%/$(UEFI_TARGET)_GCC5/Firmwares/dummy.bin:
	cp -aR $(PACKAGE_TOOL)/Firmwares/ Build/$*/$(UEFI_TARGET)_GCC5/
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

Build/%/$(UEFI_TARGET)_GCC5/certs/trusted_key_no.crt:
	cp -aR $(PACKAGE_TOOL)/Keys/ $(PACKAGE_TOOL)/certs/ Build/$*/$(UEFI_TARGET)_GCC5/
	if [[ -f "$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_regen_trusted_key_cert" ]]; then \
    	"$$PATH_PACKAGE_TOOL/$(PACKAGE_TOOL_ARCH)/cix_regen_trusted_key_cert" \
			-p Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_publickey.pem \
			-s Build/$*/$(UEFI_TARGET)_GCC5/Keys/oem_privatekey.pem \
			-o $@; \
	fi

Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd:
	make -C edk2/BaseTools -j$(shell nproc) Source/C
	export WORKSPACE="$(shell pwd)"
	export PACKAGES_PATH="$$WORKSPACE/edk2:$$WORKSPACE/edk2-platforms:$$WORKSPACE/edk2-non-osi:$$WORKSPACE/edk2-platforms/Silicon/Intel/"
	export GCC5_AARCH64_PREFIX="$(GCC5_AARCH64_PREFIX)"
	if [[ -f tools/acpica/generate/unix/bin/iasl ]]; then
		export IASL_PREFIX="$$WORKSPACE/tools/acpica/generate/unix/bin/"
	fi
	unset MAKEFLAGS
	source edk2/edksetup.sh --reconfig
	build -a AARCH64 -t GCC5 -p "$(subst edk2-platforms/,,$(filter %/$*.dsc,$(DSC)))" \
		-b $(UEFI_TARGET) -D BOARD_NAME=$(word 2, $(subst /,, $@)) -D BUILD_DATE=$(BUILD_DATE) \
		-D SMP_ENABLE=1 -D ACPI_BOOT_ENABLE=1 -D FASTBOOT_LOAD= -D VARIABLE_TYPE=SPI -D STANDARD_MM=TRUE \
		-D COMMIT_HASH=$(SOURCE_COMMIT_HASH) \
		-D EDK2_COMMIT_HASH=$(EDK2_COMMIT_HASH) \
		-D EDK2_NON_OSI_COMMIT_HASH=$(EDK2_NON_OSI_COMMIT_HASH) \
		-D EDK2_PLATFORMS_COMMIT_HASH=$(EDK2_PLATFORMS_COMMIT_HASH) \
		-D DEB_VERSION=$(shell cd .. && dpkg-parsechangelog -S Version)

tools/acpica/generate/unix/bin/iasl:
	make -C tools/acpica -j$(shell nproc)

.PHONY: clean
clean:
	rm -rf Build
	make -C edk2/BaseTools -j$(shell nproc) clean
	make -C tools/acpica -j$(shell nproc) veryclean || true

.PHONY: distclean
distclean: clean
