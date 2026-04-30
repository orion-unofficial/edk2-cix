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
UPSTREAM_EDK2_REF ?=
BUILD_METADATA_SCRIPT := scripts/resolve_build_metadata.sh
MONOREPO_EDK2_COMMIT_HASH ?= 971cfbc5fa
MONOREPO_EDK2_NON_OSI_COMMIT_HASH ?= c051ce622f
MONOREPO_EDK2_PLATFORMS_COMMIT_HASH ?= 971cfbc5fa
FIRMWARE_LAYOUT_SCRIPT := $(abspath ../scripts/firmware_layout.py)
EDK2_BUILD_OUTPUT_FILTER := scripts/filter_edk2_build_output.py
MAKE_INPUT_VALIDATOR := $(abspath ../scripts/validate_make_inputs.py)
EDK2_REBASED_DETERMINISTIC_GUARD := $(abspath ../scripts/fail_rebased_edk2_deterministic.sh)
CUSTOM_OVERLAY_SHADOW_CHECK := $(abspath ../scripts/check_custom_overlay_shadowing.py)
CUSTOM_OVERLAY_INF_SYNC_CHECK := $(abspath ../scripts/check_custom_overlay_inf_sync.py)
OFFLINE_AUDIT_BASELINE_CHECK := $(abspath ../scripts/test_offline_audit_baselines.py)
OFFLINE_AUDIT_BASELINE_REFRESH := $(abspath ../scripts/refresh_offline_audit_baselines.py)
FINAL_IMAGE_MANIFEST_AUDIT_SCRIPT := $(abspath ../scripts/audit_final_image_manifest.py)
FINAL_IMAGE_MANIFEST_BASELINE := $(abspath ../validation/final-image-manifests.json)
ACPI_REGRESSION_AUDIT_SCRIPT := $(abspath ../scripts/audit_acpi_regression.py)
ACPI_REGRESSION_AUDIT_BASELINE := $(abspath ../validation/acpi-audit-baselines.json)
MAX_DEBUG_PRINT_ERROR_LEVEL := $(shell python3 "$(MAKE_INPUT_VALIDATOR)" max-debug-print-error-level 2>/dev/null || printf '%s' 0x807B55FF)
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
REPO_ROOT := $(abspath ..)
PREFERRED_TMP_ROOT ?= $(strip $(shell "$(REPO_ROOT)/scripts/resolve_temp_root.sh"))
CCACHE_BIN ?= ccache
CCACHE_DIR ?= $(abspath ../build-cache/ccache)
CCACHE_WRAPPER_ROOT ?= $(abspath ../build-cache/ccache-toolchain)
CCACHE_DISABLE_ORIGIN := $(origin CCACHE_DISABLE)
CCACHE_DISABLED_EFFECTIVE := $(if $(filter-out undefined,$(CCACHE_DISABLE_ORIGIN)),TRUE,)
CCACHE_BIN_RESOLVED := $(if $(CCACHE_DISABLED_EFFECTIVE),,$(strip $(shell command -v "$(CCACHE_BIN)" 2>/dev/null || true)))
CCACHE_ENABLED_EFFECTIVE := $(if $(CCACHE_BIN_RESOLVED),TRUE,)
CIX_RELEASE_CACHE_ROOT ?= $(abspath ../build-cache/cix-release)
ARTEFACT_MODE ?= custom
FIRMWARE_VALIDATION_PROFILE ?= upstream-1.2.1-bookworm
FIRMWARE_VALIDATION_REPORT_ROOT ?= $(abspath ../build-validation)
CUSTOM_OVERLAY_ROOT ?= $(abspath ../custom/overlay)
EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_ROOT ?= $(abspath ../custom/overlay-experimental-uefi-settings)
CUSTOM_EDK2_WORKSPACE_KEY := $(strip $(shell printf '%s' '$(REPO_ROOT)' | cksum | awk '{print $$1}'))
CUSTOM_EDK2_WORKSPACE_ROOT ?= $(PREFERRED_TMP_ROOT)/edk2-cix-custom-workspace-$(CUSTOM_EDK2_WORKSPACE_KEY)
CUSTOM_EDK2_WORKSPACE_FLAVOR := $(shell printf '%s' '$(UEFI_TARGET)' | tr '[:upper:]' '[:lower:]')
CUSTOM_EDK2_WORKSPACE ?= $(CUSTOM_EDK2_WORKSPACE_ROOT)/$(CUSTOM_EDK2_WORKSPACE_FLAVOR)
EDK2_CIX_INCREMENTAL_CUSTOM_WORKSPACE ?= 0
DEBUG_ON_UART3_NORMALIZED_RAW := $(if $(strip $(DEBUG_ON_UART3)),$(shell python3 "$(MAKE_INPUT_VALIDATOR)" normalize-bool --value "$(DEBUG_ON_UART3)" 2>/dev/null || printf '%s' __INVALID__),)
DEBUG_ON_UART3_NORMALIZED := $(if $(filter __INVALID__,$(DEBUG_ON_UART3_NORMALIZED_RAW)),,$(DEBUG_ON_UART3_NORMALIZED_RAW))
UART3_ENABLE_NORMALIZED_RAW := $(if $(strip $(UART3_ENABLE)),$(shell python3 "$(MAKE_INPUT_VALIDATOR)" normalize-bool --value "$(UART3_ENABLE)" 2>/dev/null || printf '%s' __INVALID__),)
UART3_ENABLE_NORMALIZED := $(if $(filter __INVALID__,$(UART3_ENABLE_NORMALIZED_RAW)),,$(UART3_ENABLE_NORMALIZED_RAW))
UART3_ENABLE_EFFECTIVE := $(if $(filter TRUE,$(DEBUG_ON_UART3_NORMALIZED)),TRUE,$(UART3_ENABLE_NORMALIZED))
DEBUG_VERBOSE_NORMALIZED_RAW := $(if $(strip $(DEBUG_VERBOSE)),$(shell python3 "$(MAKE_INPUT_VALIDATOR)" normalize-bool --value "$(DEBUG_VERBOSE)" 2>/dev/null || printf '%s' __INVALID__),)
DEBUG_VERBOSE_NORMALIZED := $(if $(filter __INVALID__,$(DEBUG_VERBOSE_NORMALIZED_RAW)),,$(DEBUG_VERBOSE_NORMALIZED_RAW))
DEBUG_PRINT_ERROR_LEVEL_NORMALIZED_RAW := $(if $(strip $(DEBUG_PRINT_ERROR_LEVEL)),$(shell python3 "$(MAKE_INPUT_VALIDATOR)" normalize-u32 --value "$(DEBUG_PRINT_ERROR_LEVEL)" 2>/dev/null || printf '%s' __INVALID__),)
DEBUG_PRINT_ERROR_LEVEL_NORMALIZED := $(if $(filter __INVALID__,$(DEBUG_PRINT_ERROR_LEVEL_NORMALIZED_RAW)),,$(DEBUG_PRINT_ERROR_LEVEL_NORMALIZED_RAW))
DEBUG_PRINT_ERROR_LEVEL_VERBOSE_DEFAULT := $(if $(filter TRUE,$(DEBUG_VERBOSE_NORMALIZED)),$(MAX_DEBUG_PRINT_ERROR_LEVEL),)
DEBUG_PRINT_ERROR_LEVEL_EFFECTIVE := $(if $(strip $(DEBUG_PRINT_ERROR_LEVEL_NORMALIZED)),$(DEBUG_PRINT_ERROR_LEVEL_NORMALIZED),$(DEBUG_PRINT_ERROR_LEVEL_VERBOSE_DEFAULT))
ENABLE_FIRMWARE_FIXES_NORMALIZED_RAW := $(if $(strip $(ENABLE_FIRMWARE_FIXES)),$(shell python3 "$(MAKE_INPUT_VALIDATOR)" normalize-bool --value "$(ENABLE_FIRMWARE_FIXES)" 2>/dev/null || printf '%s' __INVALID__),)
ENABLE_FIRMWARE_FIXES_NORMALIZED := $(if $(filter __INVALID__,$(ENABLE_FIRMWARE_FIXES_NORMALIZED_RAW)),,$(ENABLE_FIRMWARE_FIXES_NORMALIZED_RAW))
ENABLE_CORE_ORDER ?=
ENABLE_CORE_ORDER_NORMALIZED := $(if $(strip $(ENABLE_CORE_ORDER)),$(shell printf '%s' '$(ENABLE_CORE_ORDER)' | tr '[:upper:]' '[:lower:]'),)
CIX_RELEASE ?=
CIX_RELEASE_VALIDATION_VALUE := $(if $(strip $(CIX_RELEASE)),$(shell printf '%s' '$(CIX_RELEASE)' | tr '[:upper:]' '[:lower:]'),)
CIX_RELEASE_NORMALIZED_RAW := $(if $(strip $(CIX_RELEASE)),$(shell python3 "$(MAKE_INPUT_VALIDATOR)" normalize-cix-release --value "$(CIX_RELEASE)" 2>/dev/null || printf '%s' __INVALID__),)
CIX_RELEASE_NORMALIZED := $(if $(filter __INVALID__,$(CIX_RELEASE_NORMALIZED_RAW)),,$(CIX_RELEASE_NORMALIZED_RAW))
ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED_RAW := $(if $(strip $(ENABLE_EXPERIMENTAL_UEFI_SETTINGS)),$(shell python3 "$(MAKE_INPUT_VALIDATOR)" normalize-bool --value "$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS)" 2>/dev/null || printf '%s' __INVALID__),)
ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED := $(if $(filter __INVALID__,$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED_RAW)),,$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED_RAW))
O6_SMBIOS_ASSET_TAG ?=
O6_SMBIOS_BASEBOARD_ASSET_TAG ?=
O6_SMBIOS_CHASSIS_ASSET_TAG ?=
O6_SMBIOS_BASEBOARD_ASSET_TAG_EFFECTIVE := $(if $(strip $(O6_SMBIOS_BASEBOARD_ASSET_TAG)),$(strip $(O6_SMBIOS_BASEBOARD_ASSET_TAG)),$(strip $(O6_SMBIOS_ASSET_TAG)))
O6_SMBIOS_CHASSIS_ASSET_TAG_EFFECTIVE := $(if $(strip $(O6_SMBIOS_CHASSIS_ASSET_TAG)),$(strip $(O6_SMBIOS_CHASSIS_ASSET_TAG)),$(strip $(O6_SMBIOS_ASSET_TAG)))
O6_SMBIOS_BASEBOARD_ASSET_TAG_C_LITERAL_RAW := $(if $(strip $(O6_SMBIOS_BASEBOARD_ASSET_TAG_EFFECTIVE)),$(shell python3 "$(MAKE_INPUT_VALIDATOR)" emit-c-string-literal --value "$(O6_SMBIOS_BASEBOARD_ASSET_TAG_EFFECTIVE)" 2>/dev/null || printf '%s' __INVALID__),)
O6_SMBIOS_BASEBOARD_ASSET_TAG_C_LITERAL := $(if $(filter __INVALID__,$(O6_SMBIOS_BASEBOARD_ASSET_TAG_C_LITERAL_RAW)),,$(O6_SMBIOS_BASEBOARD_ASSET_TAG_C_LITERAL_RAW))
O6_SMBIOS_CHASSIS_ASSET_TAG_C_LITERAL_RAW := $(if $(strip $(O6_SMBIOS_CHASSIS_ASSET_TAG_EFFECTIVE)),$(shell python3 "$(MAKE_INPUT_VALIDATOR)" emit-c-string-literal --value "$(O6_SMBIOS_CHASSIS_ASSET_TAG_EFFECTIVE)" 2>/dev/null || printf '%s' __INVALID__),)
O6_SMBIOS_CHASSIS_ASSET_TAG_C_LITERAL := $(if $(filter __INVALID__,$(O6_SMBIOS_CHASSIS_ASSET_TAG_C_LITERAL_RAW)),,$(O6_SMBIOS_CHASSIS_ASSET_TAG_C_LITERAL_RAW))
FIRMWARE_LAYOUT_QUERY_ARGS = \
	--artefact-mode "$(ARTEFACT_MODE)" \
	--firmware-target "$(FIRMWARE_TARGET)" \
	$(if $(filter TRUE,$(ENABLE_FIRMWARE_FIXES_NORMALIZED)),--enable-firmware-fixes TRUE) \
	$(if $(strip $(ENABLE_CORE_ORDER_NORMALIZED)),--enable-core-order "$(ENABLE_CORE_ORDER_NORMALIZED)") \
	$(if $(strip $(CIX_RELEASE_NORMALIZED)),--cix-release "$(CIX_RELEASE_NORMALIZED)") \
	$(if $(filter TRUE,$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED)),--enable-experimental-uefi-settings TRUE) \
	$(if $(filter TRUE,$(DEBUG_ON_UART3_NORMALIZED)),--debug-on-uart3 TRUE) \
	$(if $(filter TRUE,$(UART3_ENABLE_NORMALIZED)),--uart3-enable TRUE) \
	$(if $(filter TRUE,$(DEBUG_VERBOSE_NORMALIZED)),--debug-verbose TRUE) \
	$(if $(strip $(DEBUG_PRINT_ERROR_LEVEL_NORMALIZED)),--debug-print-error-level "$(DEBUG_PRINT_ERROR_LEVEL_NORMALIZED)")
EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_PACKAGE_ROOTS := \
	$(wildcard $(EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_ROOT)/edk2) \
	$(wildcard $(EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_ROOT)/edk2-platforms) \
	$(wildcard $(EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_ROOT)/edk2-non-osi)
ACTIVE_EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_PACKAGE_ROOTS := \
	$(if $(and $(filter custom,$(ARTEFACT_MODE)),$(filter TRUE,$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED))),$(EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_PACKAGE_ROOTS))
CUSTOM_OVERLAY_PACKAGE_ROOTS := \
	$(wildcard $(CUSTOM_OVERLAY_ROOT)/edk2) \
	$(wildcard $(CUSTOM_OVERLAY_ROOT)/edk2-platforms) \
	$(wildcard $(CUSTOM_OVERLAY_ROOT)/edk2-non-osi)
EDK2_PACKAGE_ROOTS := \
	$(ACTIVE_EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_PACKAGE_ROOTS) \
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
EXPERIMENTAL_UEFI_SETTINGS_PLATFORM_SEARCH_ROOTS := \
	$(wildcard $(EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_ROOT)/edk2-platforms/Platform) \
	$(wildcard $(EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_ROOT)/edk2-non-osi/Platform)
PLATFORM_SEARCH_ROOTS := \
	$(if $(and $(filter custom,$(ARTEFACT_MODE)),$(filter TRUE,$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED))),$(EXPERIMENTAL_UEFI_SETTINGS_PLATFORM_SEARCH_ROOTS)) \
	$(if $(filter custom,$(ARTEFACT_MODE)),$(CUSTOM_PLATFORM_SEARCH_ROOTS)) \
	edk2-platforms/Platform \
	edk2-non-osi/Platform
EDK2_BUILD_SILENT_FLAG := $(if $(filter 1,$(V)),,-s)

resolve-path-head = $(strip $(shell git -C .. log -1 --format=%h $(1) -- $(2) 2>/dev/null))

# Rendered source/release branches carry a Source-Base trailer. Prefer
# the base commit touching each imported component path when it is
# available locally, then fall back to the recorded abbreviations.
EDK2_COMMIT_HASH := $(or $(call resolve-path-head,$(UPSTREAM_EDK2_REF),src/edk2),$(MONOREPO_EDK2_COMMIT_HASH))
EDK2_NON_OSI_COMMIT_HASH := $(or $(call resolve-path-head,$(UPSTREAM_EDK2_REF),src/edk2-non-osi),$(MONOREPO_EDK2_NON_OSI_COMMIT_HASH))
EDK2_PLATFORMS_COMMIT_HASH := $(or $(call resolve-path-head,$(UPSTREAM_EDK2_REF),src/edk2-platforms),$(MONOREPO_EDK2_PLATFORMS_COMMIT_HASH))
SOURCE_COMMIT ?= $(strip $(shell $(BUILD_METADATA_SCRIPT) source-commit 2>/dev/null || true))
SOURCE_COMMIT_HASH ?= $(strip $(shell $(BUILD_METADATA_SCRIPT) source-commit-short 2>/dev/null || true))
SOURCE_DATE_EPOCH ?= $(strip $(shell $(BUILD_METADATA_SCRIPT) source-date-epoch 2>/dev/null || true))
PM_CONFIG_SOURCE_DATE_EPOCH ?= $(SOURCE_DATE_EPOCH)
BUILD_DATE ?= $(strip $(shell SOURCE_DATE_EPOCH="$(SOURCE_DATE_EPOCH)" $(BUILD_METADATA_SCRIPT) source-date-iso 2>/dev/null || true))
FIRMWARE_VERSION ?= $(strip $(shell python3 "$(FIRMWARE_LAYOUT_SCRIPT)" version --repo-root "$(REPO_ROOT)" 2>/dev/null || true))
UEFI_FW_VERSION ?= $(strip $(shell python3 "$(FIRMWARE_LAYOUT_SCRIPT)" display-version --repo-root "$(REPO_ROOT)" --version "$(FIRMWARE_VERSION)" --source-commit-hash "$(SOURCE_COMMIT_HASH)" $(FIRMWARE_LAYOUT_QUERY_ARGS) 2>/dev/null || true))
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

GCC5_AARCH64_REAL_PREFIX := $(GCC5_AARCH64_PREFIX)
CCACHE_TOOLCHAIN_KEY := $(strip $(shell printf '%s|%s' '$(HOST_ARCH)' '$(GCC5_AARCH64_REAL_PREFIX)' | cksum | awk '{print $$1}'))
CCACHE_WRAPPER_DIR := $(CCACHE_WRAPPER_ROOT)/$(CCACHE_TOOLCHAIN_KEY)
CCACHE_GCC5_AARCH64_PREFIX_EFFECTIVE := $(if $(filter TRUE,$(CCACHE_ENABLED_EFFECTIVE)),$(CCACHE_WRAPPER_DIR)/$(notdir $(GCC5_AARCH64_REAL_PREFIX)),$(GCC5_AARCH64_REAL_PREFIX))

PACKAGE_TOOL := edk2-non-osi/Platform/CIX/Sky1/PackageTool
CIX_RELEASE_SOURCE_DIR := cix-v1.2
CIX_RELEASE_V12_TFA_SOURCE_DIR := $(CIX_RELEASE_SOURCE_DIR)/tf-a
CIX_RELEASE_V12_TEE_SOURCE_DIR := $(CIX_RELEASE_SOURCE_DIR)/tee
CIX_RELEASE_V12_PAYLOAD_DIR := $(CIX_RELEASE_SOURCE_DIR)/release-payloads
CIX_RELEASE_V12_BOOTLOADER1_PAYLOAD := $(CIX_RELEASE_V12_PAYLOAD_DIR)/bootloader1-2026q1.img
CIX_RELEASE_BOOTLOADER2_HELPER := scripts/build_cix_release_bootloader2.sh
CIX_RELEASE_V12_REQUIRED_FILES := \
	$(CIX_RELEASE_V12_TFA_SOURCE_DIR)/Makefile \
	$(CIX_RELEASE_V12_TEE_SOURCE_DIR)/Makefile \
	$(CIX_RELEASE_V12_BOOTLOADER1_PAYLOAD) \
	$(CIX_RELEASE_BOOTLOADER2_HELPER)
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
TRUSTED_KEY_CERT_TOOL_BUILD_DIR := $(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)/build/$(HOST_ARCH)
TRUSTED_KEY_CERT_TOOL_BIN := $(TRUSTED_KEY_CERT_TOOL_BUILD_DIR)/cix_regen_trusted_key_cert
TRUSTED_KEY_CERT_TOOL_SOURCE_INPUTS := \
	$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)/Makefile \
	$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)/cix_regen_trusted_key_cert.c
CERT_UEFI_CREATE_RSA_SOURCE_DIR := $(TOOLS_DIR)/cert_uefi_create_rsa
CERT_UEFI_CREATE_RSA_BUILD_DIR := $(CERT_UEFI_CREATE_RSA_SOURCE_DIR)/build/$(HOST_ARCH)
CERT_UEFI_CREATE_RSA_BIN := $(CERT_UEFI_CREATE_RSA_BUILD_DIR)/cert_uefi_create_rsa
CERT_UEFI_CREATE_RSA_SOURCE_INPUTS := \
	$(CERT_UEFI_CREATE_RSA_SOURCE_DIR)/Makefile \
	$(CERT_UEFI_CREATE_RSA_SOURCE_DIR)/cert_uefi_create_rsa.c
CERT_VALIDATOR := scripts/validate_cix_fip_certs.py
CUSTOM_ACPI_OVERLAY_CHECK := $(abspath ../scripts/check_custom_acpi_overlays.py)
MICROSOFT_SECURE_BOOT_OUTPUT_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/SecureBootDefaults/Microsoft)
MICROSOFT_SECURE_BOOT_MANIFEST := $(MICROSOFT_SECURE_BOOT_OUTPUT_DIR)/manifest.lock.json
MICROSOFT_SECURE_BOOT_RELEASE_CHECK_MODE ?= warn
MICROSOFT_SECURE_BOOT_PAYLOADS := \
	$(MICROSOFT_SECURE_BOOT_OUTPUT_DIR)/PK.bin \
	$(MICROSOFT_SECURE_BOOT_OUTPUT_DIR)/KEK.bin \
	$(MICROSOFT_SECURE_BOOT_OUTPUT_DIR)/DB.bin \
	$(MICROSOFT_SECURE_BOOT_OUTPUT_DIR)/DBX.bin
CUSTOM_O6_DRIVERS_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers)
CUSTOM_O6_ACPI_OVERLAY_DIR := $(CUSTOM_O6_DRIVERS_OVERLAY_DIR)/AcpiPlatfomTables
CUSTOM_SKY1_ACPI_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables)
CUSTOM_SKY1_ACPI_OVERLAY_SOURCES := \
	$(wildcard $(CUSTOM_SKY1_ACPI_OVERLAY_DIR)/*.asl) \
	$(wildcard $(CUSTOM_SKY1_ACPI_OVERLAY_DIR)/*.aslc) \
	$(wildcard $(CUSTOM_SKY1_ACPI_OVERLAY_DIR)/*.h) \
	$(wildcard $(CUSTOM_SKY1_ACPI_OVERLAY_DIR)/*.inf) \
	$(wildcard $(CUSTOM_SKY1_ACPI_OVERLAY_DIR)/tool/python3/*)
CUSTOM_SKY1_BOOT_MANAGER_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Library/PlatformBootManagerLib)
CUSTOM_SKY1_BOOT_MANAGER_OVERLAY_SOURCES := $(wildcard $(CUSTOM_SKY1_BOOT_MANAGER_OVERLAY_DIR)/*)
CUSTOM_SKY1_SETUP_MANAGER_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/SetupManagerDxe)
CUSTOM_SKY1_SETUP_MANAGER_OVERLAY_SOURCES := \
	$(wildcard $(CUSTOM_SKY1_SETUP_MANAGER_OVERLAY_DIR)/*.c) \
	$(wildcard $(CUSTOM_SKY1_SETUP_MANAGER_OVERLAY_DIR)/*.h) \
	$(wildcard $(CUSTOM_SKY1_SETUP_MANAGER_OVERLAY_DIR)/*.inf) \
	$(wildcard $(CUSTOM_SKY1_SETUP_MANAGER_OVERLAY_DIR)/*.uni) \
	$(wildcard $(CUSTOM_SKY1_SETUP_MANAGER_OVERLAY_DIR)/*.vfr) \
	$(wildcard $(CUSTOM_SKY1_SETUP_MANAGER_OVERLAY_DIR)/*.hfr) \
	$(wildcard $(CUSTOM_SKY1_SETUP_MANAGER_OVERLAY_DIR)/MemMenu/*) \
	$(wildcard $(CUSTOM_SKY1_SETUP_MANAGER_OVERLAY_DIR)/PcieMenu/*)
CUSTOM_SKY1_FW_VERSION_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe)
CUSTOM_SKY1_SMBIOS_MISC_LIB_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Library/SmbiosMiscLib)
CUSTOM_SKY1_ACPI_SOC_DXE_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocDxe)
CUSTOM_SKY1_ACPI_PLATFORM_DXE_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiPlatformDxe)
CUSTOM_SKY1_EC_PROTOCOL_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Include/Protocol)
CUSTOM_SKY1_EC_LIBRARY_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Include/Library)
CUSTOM_SKY1_EC_DRIVER_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/EcPlatformDxe)
CUSTOM_SKY1_EC_LIBRARY_SOURCES_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Library/Ite5570EcLib)
CUSTOM_ECLIB_NULL_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Library/EcLibNull)
CUSTOM_COMMON_LIB_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Library/CommonLib)
CUSTOM_CIX_SMBIOS_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/CixSmbiosDxe)
CUSTOM_RADXA_SETUP_INIT_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/RadxaSetupVariableInitDxe)
CUSTOM_RADXA_SETUP_INCLUDE_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Include)
CUSTOM_PLATFORM_CONFIG_DXE_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/PlatformConfigDxe)
CUSTOM_MERAK_ENV_HOOK_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Merak/Library/PlatformEnvHookLib)
CUSTOM_EDGE_ENV_HOOK_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/CIX/Sky1/Edge/Library/PlatformEnvHookLib)
CUSTOM_O6_ENV_HOOK_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib)
CUSTOM_O6_PLATFORM_SMBIOS_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/PlatformSmbios)
CUSTOM_O6N_DRIVERS_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6N/Drivers)
CUSTOM_O6N_PLATFORM_SMBIOS_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6N/Drivers/PlatformSmbios)
CUSTOM_ACPI_PLATFORM_DXE_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Drivers/AcpiPlatformDxe)
CUSTOM_RADXA_SYSTEM_INFO_OVERLAY_DIR := $(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/SystemInfoDxe)
CUSTOM_BASETOOLS_OVERLAY_DIR := $(abspath ../custom/overlay/edk2/BaseTools)
CUSTOM_BASETOOLS_VFR_OVERLAY := $(CUSTOM_BASETOOLS_OVERLAY_DIR)/Source/C/VfrCompile/VfrSyntax.g
CUSTOM_EDK2_OVERLAY_SOURCES := \
	$(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Common.dsc.inc) \
	$(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Common.fdf.inc) \
	$(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Define.dsc.inc) \
	$(CUSTOM_SKY1_ACPI_OVERLAY_SOURCES) \
	$(wildcard $(CUSTOM_SKY1_EC_LIBRARY_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_SKY1_EC_PROTOCOL_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_SKY1_EC_DRIVER_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_SKY1_EC_LIBRARY_SOURCES_DIR)/*) \
	$(CUSTOM_SKY1_SETUP_MANAGER_OVERLAY_SOURCES) \
	$(wildcard $(CUSTOM_SKY1_FW_VERSION_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_SKY1_SMBIOS_MISC_LIB_OVERLAY_DIR)/*.c) \
	$(wildcard $(CUSTOM_SKY1_SMBIOS_MISC_LIB_OVERLAY_DIR)/*.inf) \
	$(wildcard $(CUSTOM_SKY1_ACPI_SOC_DXE_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_SKY1_ACPI_PLATFORM_DXE_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_ECLIB_NULL_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_COMMON_LIB_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_CIX_SMBIOS_OVERLAY_DIR)/*.c) \
	$(wildcard $(CUSTOM_CIX_SMBIOS_OVERLAY_DIR)/*.h) \
	$(wildcard $(CUSTOM_CIX_SMBIOS_OVERLAY_DIR)/*.inf) \
	$(CUSTOM_SKY1_BOOT_MANAGER_OVERLAY_SOURCES) \
	$(wildcard $(CUSTOM_MERAK_ENV_HOOK_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_EDGE_ENV_HOOK_OVERLAY_DIR)/*) \
	$(abspath $(CUSTOM_O6_DRIVERS_OVERLAY_DIR)/LinuxAcpiConfig.h) \
	$(wildcard $(CUSTOM_O6_ACPI_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_O6_ENV_HOOK_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_O6_PLATFORM_SMBIOS_OVERLAY_DIR)/*.c) \
	$(wildcard $(CUSTOM_O6_PLATFORM_SMBIOS_OVERLAY_DIR)/*.h) \
	$(wildcard $(CUSTOM_O6_PLATFORM_SMBIOS_OVERLAY_DIR)/*.inf) \
	$(abspath $(CUSTOM_O6N_DRIVERS_OVERLAY_DIR)/LinuxAcpiConfig.h) \
	$(wildcard $(CUSTOM_O6N_PLATFORM_SMBIOS_OVERLAY_DIR)/*.c) \
	$(wildcard $(CUSTOM_O6N_PLATFORM_SMBIOS_OVERLAY_DIR)/*.h) \
	$(wildcard $(CUSTOM_O6N_PLATFORM_SMBIOS_OVERLAY_DIR)/*.inf) \
	$(wildcard $(CUSTOM_ACPI_PLATFORM_DXE_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_PLATFORM_CONFIG_DXE_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_PLATFORM_CONFIG_DXE_OVERLAY_DIR)/ComplianceMenu/*) \
	$(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLib.inf) \
	$(abspath ../custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLibCustom.c) \
	$(wildcard $(CUSTOM_RADXA_SYSTEM_INFO_OVERLAY_DIR)/*.c) \
	$(wildcard $(CUSTOM_RADXA_SYSTEM_INFO_OVERLAY_DIR)/*.h) \
	$(wildcard $(CUSTOM_RADXA_SYSTEM_INFO_OVERLAY_DIR)/*.inf) \
	$(wildcard $(CUSTOM_RADXA_SYSTEM_INFO_OVERLAY_DIR)/*.vfr) \
	$(wildcard $(CUSTOM_RADXA_SYSTEM_INFO_OVERLAY_DIR)/*.uni) \
	$(wildcard $(CUSTOM_RADXA_SETUP_INCLUDE_OVERLAY_DIR)/*) \
	$(wildcard $(CUSTOM_RADXA_SETUP_INIT_OVERLAY_DIR)/*) \
	$(abspath ../custom/overlay/edk2/ArmPlatformPkg/Library/PL011UartLib/PL011Uart.h) \
	$(abspath ../custom/overlay/edk2/ArmPlatformPkg/Library/PL011UartLib/PL011UartLib.c) \
	$(abspath ../custom/overlay/edk2/ArmPlatformPkg/Library/PL011UartLib/PL011UartLib.inf) \
	$(CUSTOM_BASETOOLS_VFR_OVERLAY) \
	$(abspath ../custom/overlay/edk2/SecurityPkg/Library/SecureBootVariableProvisionLib/SecureBootVariableProvisionLib.c) \
	$(abspath ../custom/overlay/edk2/SecurityPkg/Library/SecureBootVariableProvisionLib/SecureBootVariableProvisionLib.inf)
EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_SOURCES := \
	$(abspath ../custom/overlay-experimental-uefi-settings/README.md) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2/EmbeddedPkg/Drivers/ConsolePrefDxe/ConsolePrefDxe.c) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2/EmbeddedPkg/Drivers/ConsolePrefDxe/ConsolePrefDxe.inf) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/CIX/Sky1/Drivers/SetupManagerDxe/CpuMenu/CpuConfig.uni) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/CIX/Sky1/Drivers/SetupManagerDxe/I2cMenu/I2cConfig.hfr) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/CIX/Sky1/Drivers/SetupManagerDxe/I2cMenu/I2cConfig.uni) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/CIX/Sky1/Drivers/SetupManagerDxe/MemMenu/MemoryConfig.hfr) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/CIX/Sky1/Drivers/SetupManagerDxe/PcieMenu/PcieConfig.hfr) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/CIX/Sky1/Drivers/SetupManagerDxe/PcieMenu/PcieConfig.uni) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/CIX/Sky1/Drivers/SetupManagerDxe/UsbMenu/UsbConfig.uni) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/PlatformEnvHookLib.inf) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/PlatformEnvHookLibCustom.c) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLib.inf) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLibCustom.c) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Common.dsc.inc) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Include/ExperimentalConsoleModeSetupVar.h) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Include/RadxaSetupVar.h) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Include/ExperimentalPcieSrIovSetupVar.h) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/PlatformConfigDxe/ComplianceMenu/ComplianceConfig.hfr) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/PlatformConfigDxe/ComplianceMenu/ComplianceConfig.uni) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/PlatformConfigDxe/ExperimentalUefiSettings.hfr) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/PlatformConfigDxe/ExperimentalUefiSettings.uni) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/PlatformConfigDxe/PlatformConfigDxe.inf) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/PlatformConfigDxe/PlatformConfigHii.uni) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/PlatformConfigDxe/PlatformConfigHii.vfr) \
	$(abspath ../custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Define.dsc.inc)
ACTIVE_EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_SOURCES := \
	$(if $(and $(filter custom,$(ARTEFACT_MODE)),$(filter TRUE,$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED))),$(EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_SOURCES))
PATCHED_EDK2_SOURCE_INPUTS := \
	$(abspath edk2-platforms/Platform/CIX/Sky1/Drivers/SetupManagerDxe/PcieMenu/PcieConfig.hfr) \
	$(abspath edk2-platforms/Platform/CIX/Sky1/Drivers/SetupManagerDxe/PcieMenu/PcieConfig.uni) \
	$(abspath edk2-platforms/Platform/CIX/Sky1/Library/Acpi/CIX/AcpiPpttLibCIX/PpttGenerator.c) \
	$(abspath edk2-platforms/Platform/CIX/Sky1/Library/MemoryInitPeiLib/MemoryInitPeiLib.c) \
	$(abspath edk2-platforms/Platform/Radxa/Drivers/AcpiPlatformDxe/AcpiPlatformDxe.c)
CUSTOM_EDK2_BUILD_INPUTS := \
	$(if $(filter custom,$(ARTEFACT_MODE)),$(ACTIVE_EXPERIMENTAL_UEFI_SETTINGS_OVERLAY_SOURCES) $(CUSTOM_EDK2_OVERLAY_SOURCES) $(MICROSOFT_SECURE_BOOT_PAYLOADS))
CUSTOM_EDK2_ORDER_ONLY_PREREQS := \
	$(if $(filter custom,$(ARTEFACT_MODE)),check-microsoft-secure-boot-release)
HOST_TOOLS_ROOT := Build/HostTools
HOST_TOOLS_DIR := $(HOST_TOOLS_ROOT)/$(HOST_ARCH)
CIX_PACKAGE_TOOL_SOURCE_DIR := $(TOOLS_DIR)/cix_package_tool
CIX_PACKAGE_TOOL_BIN := $(HOST_TOOLS_DIR)/cix_package_tool
CIX_PACKAGE_TOOL_SOURCE_INPUTS := $(CIX_PACKAGE_TOOL_SOURCE_DIR)/cix_package_tool.py
SIGNING_CERT_SOURCE_DIR ?=
REQUIRE_SIGNING_CERT_SOURCE ?= $(if $(and $(filter upstream,$(ARTEFACT_MODE)),$(strip $(SIGNING_CERT_SOURCE_DIR))),1,0)
IMPORTED_BASETOOLS_DIR := $(abspath edk2/BaseTools)
CUSTOM_BASETOOLS_DIR := $(CUSTOM_EDK2_WORKSPACE_ROOT)/BaseTools
ACTIVE_BASETOOLS_DIR := $(if $(filter custom,$(ARTEFACT_MODE)),$(CUSTOM_BASETOOLS_DIR),$(IMPORTED_BASETOOLS_DIR))
BASETOOLS_C_BIN_DIR := $(ACTIVE_BASETOOLS_DIR)/Source/C/bin
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
BUILD_OUTPUT_DIR = Build/$1/$(UEFI_TARGET)_GCC5
BUILD_STAGE_DIR = Build/$1/$(UEFI_TARGET)_GCC5.build
BUILD_CONFIG_STAMP = $(call BUILD_OUTPUT_DIR,$1)/.edk2-cix-build-config
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

.SILENT: help print-build-metadata clean preflight preflight-% preflight-inputs preflight-tools preflight-build-metadata host-fiptool host-cix-regen-trusted-key-cert host-cert-uefi-create-rsa host-cix-package-tool prepare-basetools-c check-custom-module-overlays check-custom-acpi-overlays
.PHONY: help help-vars help-debug
.PHONY: FORCE
FORCE:
define PRINT_SRC_HELP_SHELL_PROLOGUE
	set -eu; \
	help_pad=31; \
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
	print_section() { \
		printf '\n%s\n\n' "$$1"; \
	}
endef

define PRINT_SRC_LOCAL_VARIABLES
	print_help_line '  V=0|1' 'Set V=1 to show raw EDK2 command lines.\nDefault: 0.'; \
	print_help_line '  CIX_RELEASE_CACHE_ROOT=<path>' 'Directory used for cached curated CIX BL31, OP-TEE, and cert_create intermediates.\nDefault: '\''$$(REPO_ROOT)/build-cache/cix-release'\'''; \
	print_help_line '  GCC5_AARCH64_PREFIX=<prefix>' 'Cross-compiler prefix for the AArch64 toolchain.\nDefault: aarch64-linux-gnu- on x86_64; empty on aarch64.'; \
	print_help_line '  SOURCE_DATE_EPOCH=<unix-seconds>' 'Override the compiler-facing reproducible timestamp.\nDefault: resolved from mapped upstream source metadata.'; \
	print_help_line '  PM_CONFIG_SOURCE_DATE_EPOCH=<unix-seconds>' 'Override the PM-config timestamp.\nDefault: SOURCE_DATE_EPOCH.'
endef

help:
	@$(PRINT_SRC_HELP_SHELL_PROLOGUE); \
	printf '%s\n\n' 'edk2-cix raw firmware targets'; \
	print_help_line '  build' 'Build all raw flash images for the supported boards'; \
	print_help_line '  Build/O6/{RELEASE|DEBUG}_GCC5/cix_flash_all.bin' 'Build the full flash image for O6'; \
	print_help_line '  Build/O6/{RELEASE|DEBUG}_GCC5/cix_flash_ota.bin' 'Build the OTA flash image for O6'; \
	print_help_line '  Build/O6N/{RELEASE|DEBUG}_GCC5/cix_flash_all.bin' 'Build the full flash image for O6N'; \
	print_help_line '  Build/O6N/{RELEASE|DEBUG}_GCC5/cix_flash_ota.bin' 'Build the OTA flash image for O6N'; \
	printf '\n%s\n\n' 'Host Tools and Checks'; \
	print_help_line '  preflight' 'Check host tools, metadata, and toolchain prerequisites'; \
	print_help_line '  print-build-metadata' 'Show the resolved reproducible build metadata'; \
	print_help_line '  prepare-basetools-c' 'Build or refresh the EDK2 BaseTools C helpers'; \
	print_help_line '  check-custom-module-overlays' 'Verify custom module overlay INFs keep the imported source dependencies'; \
	print_help_line '  check-custom-acpi-overlays' 'Verify the custom ACPI module overlays still mirror the imported source tree cleanly'; \
	print_help_line '  check-microsoft-secure-boot-release' 'Advisory check for a newer pinned Microsoft Secure Boot firmware release'; \
	print_help_line '  host-fiptool' 'Build the vendored TF-A fiptool from source'; \
	print_help_line '  host-cert-uefi-create-rsa' 'Build the source non-trusted cert helper'; \
	print_help_line '  host-cix-package-tool' 'Refresh the cix_package_tool wrapper in Build/HostTools/<host-arch>/'; \
	print_help_line '  host-cix-regen-trusted-key-cert' 'Build the source trusted-key cert helper'; \
	print_help_line '  clean' 'Remove Build/ output and generated helper artefacts'; \
	print_help_line '  clean-ccache' 'Remove the managed compiler-cache data and generated compiler wrappers'; \
	print_help_line '  clean-cix-cache' 'Remove the cached curated CIX BL31, OP-TEE, and cert_create intermediates'; \
	print_help_line '  help-debug' 'Explain the DEBUG_PRINT_ERROR_LEVEL bit values derived from DebugLib.h'; \
	print_help_line '  help-vars' 'Show raw-build-specific control variables and point back to top-level help for shared options'

help-vars:
	@$(PRINT_SRC_HELP_SHELL_PROLOGUE); \
	printf '%s\n\n' 'Raw Build Variables'; \
	printf '%s\n' 'See make help-vars for common options such as ARTEFACT_MODE, FIRMWARE_BOARD, FIRMWARE_TARGET, CCACHE_DISABLE, ENABLE_FIRMWARE_FIXES, CIX_RELEASE, and the DEBUG_* switches.' | fold -s -w 78 | sed 's/^/  /'; \
	printf '\n'; \
	printf '%s\n' 'The following options are specific to the raw make -C src flow, covering host toolchains and reproducible metadata overrides.' | fold -s -w 78 | sed 's/^/  /'; \
	printf '\n'; \
	$(PRINT_SRC_LOCAL_VARIABLES); \
	printf '\n%s\n' 'For the fuller user-facing explanation of the build variables, see: docs/build-variables.md'; \
	printf '%s\n' 'For top-level wrapper targets and buildbox workflows, run: make help'

help-debug:
	@$(PRINT_SRC_HELP_SHELL_PROLOGUE); \
	printf '%s\n\n' 'Debug Print Levels'; \
	printf '%s\n' 'Set DEBUG_PRINT_ERROR_LEVEL to a decimal or '\
'0x-prefixed 32-bit value to choose which DEBUG() message classes are '\
'printed. Combine entries by ORing or summing their values. Default: '\
'0x80000040 (DEBUG_INFO|DEBUG_ERROR). If DEBUG_VERBOSE=true and you do '\
'not set DEBUG_PRINT_ERROR_LEVEL explicitly, the build uses 0x807B55FF, '\
'which enables all available DEBUG_* message levels by default. Requires '\
'ARTEFACT_MODE=custom.' | fold -s -w 78 | sed 's/^/  /'; \
	printf '\n'; \
	python3 "$(MAKE_INPUT_VALIDATOR)" list-debug-print-error-level-bits | \
	awk '{ mask=$$1; name=$$2; $$1=""; $$2=""; sub(/^ +/, "", $$0); printf "%s\t%s\t%s\n", mask, name, $$0 }' | \
	while IFS=$$(printf '\t') read -r mask name desc; do \
		print_help_line "  $$mask $$name" "$$desc"; \
	done

$(FIPTOOL_BIN): $(FIPTOOL_SOURCE_INPUTS)
	$(Q)$(ENSURE_REPO_LOCK)
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(FIPTOOL_SOURCE_DIR)" HOST_ARCH="$(HOST_ARCH)" V="$(V)" all

$(TRUSTED_KEY_CERT_TOOL_BIN): $(TRUSTED_KEY_CERT_TOOL_SOURCE_INPUTS)
	$(Q)$(ENSURE_REPO_LOCK)
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)" HOST_ARCH="$(HOST_ARCH)" V="$(V)" clean all

$(CERT_UEFI_CREATE_RSA_BIN): $(CERT_UEFI_CREATE_RSA_SOURCE_INPUTS)
	$(Q)$(ENSURE_REPO_LOCK)
	$(PRINT_HOST_TOOL)
	$(Q)$(MAKE) -C "$(CERT_UEFI_CREATE_RSA_SOURCE_DIR)" HOST_ARCH="$(HOST_ARCH)" V="$(V)" clean all

$(CIX_PACKAGE_TOOL_BIN): $(CIX_PACKAGE_TOOL_SOURCE_INPUTS)
	$(Q)$(ENSURE_REPO_LOCK)
	$(PRINT_HOST_TOOL)
	$(Q)mkdir -p "$(HOST_TOOLS_DIR)"
	$(Q)printf '%s\n' '#!/usr/bin/env bash' 'set -euo pipefail' \
		'script_dir="$$(CDPATH= cd -- "$$(dirname -- "$${BASH_SOURCE[0]}")" && pwd)"' \
		'exec python3 "$$script_dir/../../../tools/cix_package_tool/cix_package_tool.py" "$$@"' >"$@"
	$(Q)chmod +x "$@" "$(CIX_PACKAGE_TOOL_SOURCE_DIR)/cix_package_tool.py"

.PHONY: print-build-metadata preflight-build-metadata
print-build-metadata: preflight-build-metadata
	echo "ARTEFACT_MODE=$(ARTEFACT_MODE)"
	echo "FIRMWARE_VERSION=$(FIRMWARE_VERSION)"
	echo "UEFI_FW_VERSION=$(UEFI_FW_VERSION)"
	echo "CCACHE_ENABLED_EFFECTIVE=$(if $(filter TRUE,$(CCACHE_ENABLED_EFFECTIVE)),true,false)"
	echo "CCACHE_BIN=$(CCACHE_BIN)"
	echo "CCACHE_BIN_RESOLVED=$(CCACHE_BIN_RESOLVED)"
	echo "CCACHE_DIR=$(CCACHE_DIR)"
	echo "CCACHE_WRAPPER_ROOT=$(CCACHE_WRAPPER_ROOT)"
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
		check_required "FIRMWARE_VERSION" '$(strip $(FIRMWARE_VERSION))'; \
		check_required "UEFI_FW_VERSION" '$(strip $(UEFI_FW_VERSION))'; \
		check_required "EDK2_COMMIT_HASH" '$(strip $(EDK2_COMMIT_HASH))'; \
		check_required "EDK2_NON_OSI_COMMIT_HASH" '$(strip $(EDK2_NON_OSI_COMMIT_HASH))'; \
		check_required "EDK2_PLATFORMS_COMMIT_HASH" '$(strip $(EDK2_PLATFORMS_COMMIT_HASH))'; \
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
	@set -euo pipefail; \
	base_tools_dir="$(ACTIVE_BASETOOLS_DIR)"; \
	base_tools_bin_dir="$(BASETOOLS_C_BIN_DIR)"; \
	base_tools_marker="$(BASETOOLS_C_MARKER)"; \
	overlay_signature=''; \
	if [[ "$(ARTEFACT_MODE)" == "custom" && -d "$(CUSTOM_BASETOOLS_OVERLAY_DIR)" ]]; then \
		overlay_signature="$$(find "$(CUSTOM_BASETOOLS_OVERLAY_DIR)" -type f -print0 | sort -z | xargs -0 cksum 2>/dev/null || true)"; \
	fi; \
	mkdir -p "$$base_tools_bin_dir"; \
	current_env_id="$$( \
		{ source /etc/os-release >/dev/null 2>&1 && true; } || true; \
		printf 'arch=%s;os=%s;version=%s;cc=%s;ldd=%s;artefact=%s;overlay=%s\n' \
			"$$(uname -sm)" \
			"$${ID:-unknown}" \
			"$${VERSION_ID:-unknown}" \
			"$$(cc --version 2>/dev/null | sed -n '1p' || echo unavailable)" \
			"$$(ldd --version 2>/dev/null | sed -n '1p' || echo unavailable)" \
			"$(ARTEFACT_MODE)" \
			"$$overlay_signature"; \
	)"; \
	rebuild=0; \
	if [[ ! -f "$$base_tools_marker" ]]; then \
		rebuild=1; \
	elif ! grep -qxF "$$current_env_id" "$$base_tools_marker"; then \
		rebuild=1; \
	fi; \
	for tool in $(BASETOOLS_C_REQUIRED_TOOLS); do \
		if [[ ! -x "$$base_tools_bin_dir/$$tool" ]]; then \
			rebuild=1; \
			break; \
		fi; \
	done; \
	if [[ "$$rebuild" -eq 1 ]]; then \
		if [[ "$(V)" == "1" ]]; then \
			printf '[build] Rebuilding BaseTools C binaries for %s\n' "$$current_env_id"; \
			printf '[build] Cleaning stale BaseTools C objects and dependency files\n'; \
		else \
			printf '[build] Rebuilding BaseTools C binaries\n'; \
		fi; \
		if [[ "$(ARTEFACT_MODE)" == "custom" ]]; then \
			rm -rf "$$base_tools_dir"; \
			mkdir -p "$$(dirname "$$base_tools_dir")"; \
			cp -a "$(IMPORTED_BASETOOLS_DIR)" "$$(dirname "$$base_tools_dir")/"; \
			rm -rf "$$(dirname "$$base_tools_dir")/MdePkg"; \
			ln -s "$(abspath edk2/MdePkg)" "$$(dirname "$$base_tools_dir")/MdePkg"; \
			if [[ -d "$(CUSTOM_BASETOOLS_OVERLAY_DIR)" ]]; then \
				while IFS= read -r -d '' overlay_src; do \
					rel_path="$${overlay_src#$(CUSTOM_BASETOOLS_OVERLAY_DIR)/}"; \
					mkdir -p "$$base_tools_dir/$$(dirname "$$rel_path")"; \
					cp "$$overlay_src" "$$base_tools_dir/$$rel_path"; \
				done < <(find "$(CUSTOM_BASETOOLS_OVERLAY_DIR)" -type f -print0); \
			fi; \
		fi; \
		if [[ "$(V)" == "1" ]]; then \
			$(MAKE) -C "$$base_tools_dir" V="$(V)" clean; \
			$(MAKE) -C "$$base_tools_dir" -j$(BASETOOLS_BUILD_JOBS) V="$(V)" Source/C; \
		else \
			basetools_log="$$(mktemp "$(PREFERRED_TMP_ROOT)/edk2-cix-basetools.XXXXXX")"; \
			if ! $(MAKE) -C "$$base_tools_dir" V="$(V)" clean >"$$basetools_log" 2>&1; then \
				cat "$$basetools_log" >&2; \
				rm -f "$$basetools_log"; \
				exit 1; \
			fi; \
			if ! $(MAKE) -C "$$base_tools_dir" -j$(BASETOOLS_BUILD_JOBS) V="$(V)" Source/C >"$$basetools_log" 2>&1; then \
				cat "$$basetools_log" >&2; \
				rm -f "$$basetools_log"; \
				exit 1; \
			fi; \
			rm -f "$$basetools_log"; \
		fi; \
		printf '%s\n' "$$current_env_id" > "$$base_tools_marker"; \
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
	if [ -n "$(strip $(DEB_VERSION))" ]; then \
		echo "[preflight] DEB_VERSION is packaging-only; omit it for firmware builds." >&2; \
		exit 1; \
	fi; \
	if [ -z "$(strip $(CCACHE_DISABLED_EFFECTIVE))" ] && [ -z "$(strip $(CCACHE_BIN_RESOLVED))" ]; then \
		echo "[preflight] warning: ccache was not found as CCACHE_BIN=$(strip $(CCACHE_BIN)); continuing without compiler-cache acceleration." >&2; \
	fi; \
	validate_cmd=(python3 "$(MAKE_INPUT_VALIDATOR)" validate \
		--repo-root "$(abspath ..)" \
		--v "$(V)" \
		--artefact-mode "$(ARTEFACT_MODE)" \
		--firmware-target "$(FIRMWARE_TARGET)"); \
	if [ -n "$(strip $(FIRMWARE_BOARD))" ]; then \
		validate_cmd+=(--firmware-board "$(strip $(FIRMWARE_BOARD))"); \
	fi; \
	if [ -n "$(strip $(ENABLE_FIRMWARE_FIXES_NORMALIZED))" ]; then \
		validate_cmd+=(--enable-firmware-fixes "$(strip $(ENABLE_FIRMWARE_FIXES_NORMALIZED))"); \
	fi; \
	if [ -n "$(strip $(ENABLE_CORE_ORDER_NORMALIZED))" ]; then \
		validate_cmd+=(--enable-core-order "$(strip $(ENABLE_CORE_ORDER_NORMALIZED))"); \
	fi; \
	if [ -n "$(strip $(CIX_RELEASE_VALIDATION_VALUE))" ]; then \
		validate_cmd+=(--cix-release "$(strip $(CIX_RELEASE_VALIDATION_VALUE))"); \
	fi; \
	if [ -n "$(strip $(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED))" ]; then \
		validate_cmd+=(--enable-experimental-uefi-settings "$(strip $(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED))"); \
	fi; \
	if [ -n "$(strip $(DEBUG_ON_UART3))" ]; then \
		validate_cmd+=(--debug-on-uart3 "$(strip $(DEBUG_ON_UART3))"); \
	fi; \
	if [ -n "$(strip $(UART3_ENABLE_EFFECTIVE))" ]; then \
		validate_cmd+=(--uart3-enable "$(strip $(UART3_ENABLE_EFFECTIVE))"); \
	fi; \
	if [ -n "$(strip $(DEBUG_VERBOSE_NORMALIZED))" ]; then \
		validate_cmd+=(--debug-verbose "$(strip $(DEBUG_VERBOSE_NORMALIZED))"); \
	fi; \
	if [ -n "$(strip $(DEBUG_PRINT_ERROR_LEVEL_EFFECTIVE))" ]; then \
		validate_cmd+=(--debug-print-error-level "$(strip $(DEBUG_PRINT_ERROR_LEVEL_EFFECTIVE))"); \
	fi; \
	if [ -n "$(strip $(O6_SMBIOS_ASSET_TAG))" ]; then \
		validate_cmd+=(--o6-smbios-asset-tag "$(strip $(O6_SMBIOS_ASSET_TAG))"); \
	fi; \
	if [ -n "$(strip $(O6_SMBIOS_BASEBOARD_ASSET_TAG))" ]; then \
		validate_cmd+=(--o6-smbios-baseboard-asset-tag "$(strip $(O6_SMBIOS_BASEBOARD_ASSET_TAG))"); \
	fi; \
	if [ -n "$(strip $(O6_SMBIOS_CHASSIS_ASSET_TAG))" ]; then \
		validate_cmd+=(--o6-smbios-chassis-asset-tag "$(strip $(O6_SMBIOS_CHASSIS_ASSET_TAG))"); \
	fi; \
	"$${validate_cmd[@]}"; \
	if [ -n "$(strip $(DEBUG_ON_UART3_NORMALIZED))" ] && [ "$(DEBUG_ON_UART3_NORMALIZED)" = "TRUE" ] && [ "$(strip $(UART3_ENABLE_NORMALIZED))" != "TRUE" ]; then \
		echo "[config] DEBUG_ON_UART3=true implies UART3_ENABLE=true"; \
	fi; \
	if [ -n "$(strip $(DEBUG_VERBOSE_NORMALIZED))" ] && [ "$(DEBUG_VERBOSE_NORMALIZED)" = "TRUE" ] && [ "$(FIRMWARE_TARGET)" = "RELEASE" ]; then \
		echo "[config] DEBUG_VERBOSE=true re-enables DEBUG() logging on RELEASE builds"; \
	fi; \
	if [ -n "$(strip $(DEBUG_PRINT_ERROR_LEVEL_EFFECTIVE))" ]; then \
		echo "[config] $$(python3 "$(MAKE_INPUT_VALIDATOR)" describe-debug-print-error-level --value "$(DEBUG_PRINT_ERROR_LEVEL_EFFECTIVE)")"; \
	fi; \
	if [ -n "$(strip $(ENABLE_FIRMWARE_FIXES_NORMALIZED))" ] && [ "$(ENABLE_FIRMWARE_FIXES_NORMALIZED)" = "TRUE" ]; then \
		echo "[config] ENABLE_FIRMWARE_FIXES=true enables the opt-in firmware fixes"; \
	fi; \
	if [ -n "$(strip $(ENABLE_CORE_ORDER_NORMALIZED))" ] && [ "$(strip $(ENABLE_CORE_ORDER_NORMALIZED))" != "cix" ]; then \
		echo "[config] ENABLE_CORE_ORDER=$(ENABLE_CORE_ORDER_NORMALIZED) remaps exposed CPU UIDs"; \
	fi; \
	if [ -n "$(strip $(CIX_RELEASE_NORMALIZED))" ]; then \
		echo "[config] CIX_RELEASE=$(CIX_RELEASE_NORMALIZED) enables the curated CIX V1.2 source-and-payload path"; \
	fi; \
	if [ -n "$(strip $(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED))" ] && [ "$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED)" = "TRUE" ]; then \
		echo "[config] ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true enables the experimental Radxa UI overlay"; \
	fi; \
	if [ -n "$(strip $(O6_SMBIOS_BASEBOARD_ASSET_TAG_EFFECTIVE))" ]; then \
		echo "[config] O6 Type 2 asset tag = $(O6_SMBIOS_BASEBOARD_ASSET_TAG_EFFECTIVE)"; \
	fi; \
	if [ -n "$(strip $(O6_SMBIOS_CHASSIS_ASSET_TAG_EFFECTIVE))" ]; then \
		echo "[config] O6 Type 3 asset tag = $(O6_SMBIOS_CHASSIS_ASSET_TAG_EFFECTIVE)"; \
	fi; \
	if [ "$(ARTEFACT_MODE)" = "custom" ] && [ -d "$(CUSTOM_OVERLAY_ROOT)" ]; then \
		python3 "$(CUSTOM_OVERLAY_SHADOW_CHECK)" \
			--overlay-root "$(CUSTOM_OVERLAY_ROOT)" \
			--source-root "$(REPO_ROOT)/src"; \
		python3 "$(CUSTOM_OVERLAY_INF_SYNC_CHECK)" \
			--overlay-root "$(CUSTOM_OVERLAY_ROOT)" \
			--source-root "$(REPO_ROOT)/src"; \
		python3 "$(CUSTOM_ACPI_OVERLAY_CHECK)" \
			--repo-root "$(abspath ..)"; \
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
	if [[ -n "$(strip $(CIX_RELEASE_NORMALIZED))" ]]; then \
		for required_file in $(CIX_RELEASE_V12_REQUIRED_FILES); do \
			if [[ ! -e "$$required_file" ]]; then \
				echo "[preflight] Missing curated CIX release input: $$required_file" >&2; \
				missing=1; \
			fi; \
		done; \
	fi; \
	if [[ ! -f "$(CERT_VALIDATOR)" ]]; then \
		echo "[preflight] Missing certificate validation helper: $(CERT_VALIDATOR)" >&2; \
		missing=1; \
	fi; \
	if [[ "$$missing" -ne 0 ]]; then \
		exit 1; \
	fi

.SILENT: $(call BUILD_CONFIG_STAMP,%)
$(call BUILD_CONFIG_STAMP,%): FORCE
	@$(ENSURE_REPO_LOCK)
	@set -euo pipefail; \
	stamp_path="$@"; \
	output_dir="$(call BUILD_OUTPUT_DIR,$*)"; \
	stage_dir="$(call BUILD_STAGE_DIR,$*)"; \
	config_text="$$(printf '%s\n' \
		'ARTEFACT_MODE=$(ARTEFACT_MODE)' \
		'UEFI_TARGET=$(UEFI_TARGET)' \
		'BUILD_DATE=$(BUILD_DATE)' \
		'SOURCE_COMMIT_HASH=$(SOURCE_COMMIT_HASH)' \
		'EDK2_COMMIT_HASH=$(EDK2_COMMIT_HASH)' \
		'EDK2_NON_OSI_COMMIT_HASH=$(EDK2_NON_OSI_COMMIT_HASH)' \
		'EDK2_PLATFORMS_COMMIT_HASH=$(EDK2_PLATFORMS_COMMIT_HASH)' \
		'FIRMWARE_VERSION=$(FIRMWARE_VERSION)' \
		'UEFI_FW_VERSION=$(UEFI_FW_VERSION)' \
		'ENABLE_FIRMWARE_FIXES=$(ENABLE_FIRMWARE_FIXES_NORMALIZED)' \
		'ENABLE_CORE_ORDER=$(ENABLE_CORE_ORDER_NORMALIZED)' \
		'CIX_RELEASE=$(CIX_RELEASE_NORMALIZED)' \
		'ENABLE_EXPERIMENTAL_UEFI_SETTINGS=$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED)' \
		'UART3_ENABLE=$(UART3_ENABLE_EFFECTIVE)' \
		'DEBUG_ON_UART3=$(DEBUG_ON_UART3_NORMALIZED)' \
		'DEBUG_VERBOSE=$(DEBUG_VERBOSE_NORMALIZED)' \
		'DEBUG_PRINT_ERROR_LEVEL=$(DEBUG_PRINT_ERROR_LEVEL_EFFECTIVE)' \
		'O6_SMBIOS_BASEBOARD_ASSET_TAG=$(O6_SMBIOS_BASEBOARD_ASSET_TAG_EFFECTIVE)' \
		'O6_SMBIOS_CHASSIS_ASSET_TAG=$(O6_SMBIOS_CHASSIS_ASSET_TAG_EFFECTIVE)' \
		'SIGNING_CERT_SOURCE_DIR=$(SIGNING_CERT_SOURCE_DIR)' \
		'REQUIRE_SIGNING_CERT_SOURCE=$(REQUIRE_SIGNING_CERT_SOURCE)' \
		'GCC5_AARCH64_PREFIX=$(GCC5_AARCH64_PREFIX)')"; \
	existing_text="$$(cat "$$stamp_path" 2>/dev/null || true)"; \
	if [[ "$$existing_text" != "$$config_text" ]]; then \
		rm -rf "$$output_dir" "$$stage_dir"; \
		mkdir -p "$$output_dir"; \
		printf '%s\n' "$$config_text" > "$$stamp_path"; \
	fi

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/cix_flash_ota.bin
Build/%/$(UEFI_TARGET)_GCC5/cix_flash_ota.bin: $(call BUILD_CONFIG_STAMP,%) preflight-% Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader2.img Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img | $(CIX_PACKAGE_TOOL_BIN)
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
	stage_dir="$(call BUILD_STAGE_DIR,$*)" && \
	final_dir="$(call BUILD_OUTPUT_DIR,$*)" && \
	mkdir -p "$$stage_dir" "$$final_dir" && \
	export CFG="$$(find_platform_match "$*/spi_flash_config_ota.json" f || true)" && \
	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	if [[ -f "$$CFG" ]]; then \
		if [[ "$(V)" == "1" ]]; then echo "Found custom flash config."; fi; \
		cp "$$CFG" "$$stage_dir/"; \
	else \
		cp "$$PATH_PACKAGE_TOOL/spi_flash_config_ota.json" "$$stage_dir/"; \
	fi

	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	export CIX_PACKAGE_TOOL_BIN="$(shell realpath "$(CIX_PACKAGE_TOOL_BIN)")" && \
	printf '[build/%s] Packaging OTA flash image\n' "$*" && \
	( \
		cd "$$stage_dir" && \
		tmp_ota="./cix_flash_ota.bin.tmp" && \
		rm -f "$$tmp_ota" && \
		TERM=dumb "$$CIX_PACKAGE_TOOL_BIN" -c "./spi_flash_config_ota.json" -O "$$tmp_ota" 2>&1 | perl -pe 's/\e\[[0-9;]*m//g' && \
		if [[ ! -s "$$tmp_ota" ]]; then \
			echo "[build/$*] Expected OTA image $$tmp_ota was not created" >&2; \
			exit 1; \
		fi && \
		mv -f "$$tmp_ota" "./cix_flash_ota.bin" \
	) && \
	cp -f "$$stage_dir/cix_flash_ota.bin" "$@" && \
	if [[ -d "$$stage_dir/Firmwares" ]]; then \
		mkdir -p "$$final_dir/Firmwares" && \
		cp -a "$$stage_dir/Firmwares/." "$$final_dir/Firmwares/"; \
	fi && \
	if [[ -d "$$stage_dir/certs" ]]; then \
		mkdir -p "$$final_dir/certs" && \
		cp -a "$$stage_dir/certs/." "$$final_dir/certs/"; \
	fi

# MEM_CFG_MEMFREQ ?= 2750
MEM_CFG_MEMFREQ ?= 3000

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/cix_flash_all.bin
Build/%/$(UEFI_TARGET)_GCC5/cix_flash_all.bin: $(call BUILD_CONFIG_STAMP,%) preflight-% Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader2.img Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img | $(CIX_PACKAGE_TOOL_BIN)
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
	stage_dir="$(call BUILD_STAGE_DIR,$*)" && \
	final_dir="$(call BUILD_OUTPUT_DIR,$*)" && \
	mkdir -p "$$stage_dir/Firmwares" "$$final_dir" && \
	export MEM="$$(find_platform_match "$*/mem_config" d || true)" && \
	if [[ -d "$$MEM" ]]; then \
		printf '[build/%s] Regenerating memory configuration\n' "$*" && \
		if [[ "$(V)" == "1" ]]; then echo "Found custom memory configuration."; fi; \
		rm -f "$$stage_dir/Firmwares/memory_config.bin"; \
		$(MAKE) -C "$$MEM" $(if $(strip $(HELPER_BUILD_JOBS)),-j$(HELPER_BUILD_JOBS)) -e CFLAG:="-DMEM_CFG_MEMFREQ=$(MEM_CFG_MEMFREQ)" clean main; \
		if [[ ! -s "$$MEM/memory_config.bin" ]]; then \
			echo "[build/$*] Expected memory_config.bin was not generated in $$MEM" >&2; \
			exit 1; \
		fi; \
		mv "$$MEM/memory_config.bin" "$$stage_dir/Firmwares/"; \
	fi
	export PM="$$(find_platform_match "$*/pm_config" d || true)" && \
	if [[ -d "$$PM" ]]; then \
		printf '[build/%s] Regenerating PM configuration\n' "$*" && \
		if [[ "$(V)" == "1" ]]; then echo "Found custom PM configuration."; fi; \
		rm -f "$$stage_dir/Firmwares/csu_pm_config.bin"; \
		PM_CONFIG_SOURCE_DATE_EPOCH="$(PM_CONFIG_SOURCE_DATE_EPOCH)" $(MAKE) -C "$$PM" $(if $(strip $(HELPER_BUILD_JOBS)),-j$(HELPER_BUILD_JOBS)); \
		if [[ ! -s "$$PM/csu_pm_config.bin" ]]; then \
			echo "[build/$*] Expected csu_pm_config.bin was not generated in $$PM" >&2; \
			exit 1; \
		fi; \
		mv "$$PM/csu_pm_config.bin" "$$stage_dir/Firmwares/"; \
	fi

	export CFG="$$(find_platform_match "$*/spi_flash_config_all.json" f || true)" && \
	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	export CIX_PACKAGE_TOOL_BIN="$(shell realpath "$(CIX_PACKAGE_TOOL_BIN)")" && \
	if [[ -f "$$CFG" ]]; then \
		if [[ "$(V)" == "1" ]]; then echo "Found custom flash config."; fi; \
		cp "$$CFG" "$$stage_dir/"; \
	else \
		cp "$$PATH_PACKAGE_TOOL/spi_flash_config_all.json" "$$stage_dir/"; \
	fi

	export PATH_PACKAGE_TOOL="$(shell realpath "$(PACKAGE_TOOL)")" && \
	printf '[build/%s] Packaging full flash image\n' "$*" && \
	( \
		cd "$$stage_dir" && \
		TERM=dumb "$$CIX_PACKAGE_TOOL_BIN" -c "./spi_flash_config_all.json" -o "./cix_flash_all.raw" 2>&1 | perl -pe 's/\e\[[0-9;]*m//g' && \
		if [[ ! -s "./cix_flash_all.raw" ]]; then \
			echo "[build/$*] Expected cix_flash_all.raw was not created" >&2; \
			exit 1; \
		fi \
	)

	# \377 = 0xFF
	tmp_all="$@.tmp" && \
	rm -f "$$tmp_all" && \
	head -c 8M < /dev/zero | tr '\000' '\377' > "$$tmp_all" && \
	dd if="$$stage_dir/cix_flash_all.raw" of="$$tmp_all" conv=notrunc status=none && \
	if [[ ! -s "$$tmp_all" ]]; then \
		echo "[build/$*] Expected full flash image $$tmp_all was not created" >&2; \
		exit 1; \
	fi && \
	mv -f "$$tmp_all" "$@" && \
	if [[ -d "$$stage_dir/Firmwares" ]]; then \
		mkdir -p "$$final_dir/Firmwares" && \
		cp -a "$$stage_dir/Firmwares/." "$$final_dir/Firmwares/"; \
	fi && \
	if [[ -d "$$stage_dir/certs" ]]; then \
		mkdir -p "$$final_dir/certs" && \
		cp -a "$$stage_dir/certs/." "$$final_dir/certs/"; \
	fi

.NOTINTERMEDIATE: \
	$(foreach board,$(FIRMWARE_BOARDS),Build/$(board)/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd) \
	$(foreach board,$(FIRMWARE_BOARDS),$(call BUILD_STAGE_DIR,$(board))/Firmwares/dummy.bin) \
	$(foreach board,$(FIRMWARE_BOARDS),$(call BUILD_STAGE_DIR,$(board))/certs/trusted_key_no.crt) \
	$(foreach board,$(FIRMWARE_BOARDS),$(call BUILD_STAGE_DIR,$(board))/Firmwares/bootloader2.img) \
	$(foreach board,$(FIRMWARE_BOARDS),$(call BUILD_STAGE_DIR,$(board))/Firmwares/bootloader3.img) \
	$(foreach board,$(FIRMWARE_BOARDS),Build/$(board)/$(UEFI_TARGET)_GCC5/Firmwares/bootloader2.img) \
	$(foreach board,$(FIRMWARE_BOARDS),Build/$(board)/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img)

.SILENT: $(call BUILD_STAGE_DIR,%)/Firmwares/bootloader2.img
$(call BUILD_STAGE_DIR,%)/Firmwares/bootloader2.img: \
	$(call BUILD_CONFIG_STAMP,%) \
	$(call BUILD_STAGE_DIR,%)/Firmwares/dummy.bin \
	$(call BUILD_STAGE_DIR,%)/certs/trusted_key_no.crt \
	$(abspath $(CIX_RELEASE_BOOTLOADER2_HELPER)) \
	| $(FIPTOOL_BIN)

	@$(ENSURE_REPO_LOCK)
	@if [[ "$(ARTEFACT_MODE)" != "custom" || "$(CIX_RELEASE_NORMALIZED)" != "1.2" ]]; then \
		if [[ ! -s "$@" ]]; then \
			echo "[build/$*] Expected staged bootloader2 image $@ was not found" >&2; \
			exit 1; \
		fi; \
		exit 0; \
	fi; \
	printf '[build/%s] Building bootloader2 from curated CIX V1.2 TF-A and OP-TEE sources\n' "$*" && \
	tmp_output="$@.tmp" && \
	rm -f "$$tmp_output" && \
	bash "$(abspath $(CIX_RELEASE_BOOTLOADER2_HELPER))" \
		--tfa-dir "$(abspath $(CIX_RELEASE_V12_TFA_SOURCE_DIR))" \
		--tee-dir "$(abspath $(CIX_RELEASE_V12_TEE_SOURCE_DIR))" \
		--build-root "$(abspath $(call BUILD_STAGE_DIR,$*))" \
		--fiptool "$(abspath $(FIPTOOL_BIN))" \
		--output "$$tmp_output" \
		--cross-compile "$(GCC5_AARCH64_PREFIX)" \
		--jobs "$(HELPER_BUILD_JOBS)" \
		--mode "$(shell printf '%s' '$(FIRMWARE_TARGET)' | tr '[:upper:]' '[:lower:]')" \
		$(if $(strip $(CIX_RELEASE_CACHE_ROOT)),--cache-root "$(abspath $(CIX_RELEASE_CACHE_ROOT))") \
		$(if $(filter 1,$(V)),--verbose,) && \
	if [[ ! -s "$$tmp_output" ]]; then \
		echo "[build/$*] Expected generated bootloader2 image $$tmp_output was not created" >&2; \
		exit 1; \
	fi && \
	mv -f "$$tmp_output" "$@"

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader2.img
Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader2.img: \
	$(call BUILD_STAGE_DIR,%)/Firmwares/bootloader2.img \
	$(call BUILD_CONFIG_STAMP,%)
	@$(ENSURE_REPO_LOCK)
	@mkdir -p "$(call BUILD_OUTPUT_DIR,$*)/Firmwares"
	@cp -f "$<" "$@"

.SILENT: $(call BUILD_STAGE_DIR,%)/Firmwares/bootloader3.img
$(call BUILD_STAGE_DIR,%)/Firmwares/bootloader3.img: \
	Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd \
	$(call BUILD_CONFIG_STAMP,%) \
	$(call BUILD_STAGE_DIR,%)/Firmwares/dummy.bin \
	$(call BUILD_STAGE_DIR,%)/certs/trusted_key_no.crt \
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
	reused_nt_fw_certs=0; \
	validator_extra_args=(); \
	stage_dir="$(call BUILD_STAGE_DIR,$*)" && \
	mkdir -p "$$stage_dir/Keys" && \
	cp -a $(PACKAGE_TOOL)/Keys/. "$$stage_dir/Keys/" && \
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
			cp "$$trusted_key_cert_src" "$$stage_dir/certs/trusted_key_no.crt"; \
			reused_trusted_key_cert=1; \
		fi; \
		if [[ -n "$$nt_fw_cert_src" && -n "$$nt_fw_key_cert_src" ]]; then \
			echo "Reusing non-trusted FIP certs from $$SIGNING_CERT_SOURCE_DIR"; \
			cp "$$nt_fw_cert_src" "$$stage_dir/certs/nt_fw_cert.crt"; \
			cp "$$nt_fw_key_cert_src" "$$stage_dir/certs/nt_fw_key.crt"; \
			reused_nt_fw_certs=1; \
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
	if [[ "$$reused_nt_fw_certs" != "1" ]]; then \
		rm -f "$$stage_dir/certs/nt_fw_cert.crt" "$$stage_dir/certs/nt_fw_key.crt"; \
		run_success_only "$$SOURCE_CERT_TOOL_BIN" --key-alg rsa --key-size 3072 --hash-alg sha256 -p --ntfw-nvctr 223 \
			--nt-fw-cert "$$stage_dir/certs/nt_fw_cert.crt" \
			--nt-fw-key-cert "$$stage_dir/certs/nt_fw_key.crt" \
			--nt-fw-key "$$stage_dir/Keys/oem_privatekey.pem" \
			--non-trusted-world-key "$$stage_dir/Keys/oem_privatekey.pem" \
			--nt-fw "$(shell realpath "$<")"; \
	fi && \
	run_success_only python3 "$(CERT_VALIDATOR)" \
		--trusted-key-cert "$$stage_dir/certs/trusted_key_no.crt" \
		--nt-fw-key-cert "$$stage_dir/certs/nt_fw_key.crt" \
		--nt-fw-cert "$$stage_dir/certs/nt_fw_cert.crt" \
		--oem-public-key "$$stage_dir/Keys/oem_publickey.pem" \
		--trusted-sign-key "$$stage_dir/Keys/oem_privatekey.pem" \
		--nt-fw-key "$$stage_dir/Keys/oem_privatekey.pem" \
		--non-trusted-world-key "$$stage_dir/Keys/oem_privatekey.pem" \
		--nt-fw "$(shell realpath "$<")" \
		--ntfw-nvctr 223 \
		--fiptool "$(FIPTOOL_BIN)" \
		"$${validator_extra_args[@]}" && \
	run_success_only "$(FIPTOOL_BIN)" create \
		--trusted-key-cert "$$stage_dir/certs/trusted_key_no.crt" \
		--nt-fw-key-cert "$$stage_dir/certs/nt_fw_key.crt" \
		--nt-fw-cert "$$stage_dir/certs/nt_fw_cert.crt" \
		--nt-fw "$(shell realpath "$<")" \
		$@ && \
	if [[ ! -s "$@" ]]; then \
		echo "[build/$*] Expected bootloader image $@ was not created" >&2; \
		exit 1; \
	fi

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img
Build/%/$(UEFI_TARGET)_GCC5/Firmwares/bootloader3.img: \
	$(call BUILD_STAGE_DIR,%)/Firmwares/bootloader3.img \
	$(call BUILD_CONFIG_STAMP,%)
	@$(ENSURE_REPO_LOCK)
	@mkdir -p "$(call BUILD_OUTPUT_DIR,$*)/Firmwares" "$(call BUILD_OUTPUT_DIR,$*)/certs"
	@cp -f "$<" "$@"
	@if [[ -d "$(call BUILD_STAGE_DIR,$*)/certs" ]]; then \
		cp -a "$(call BUILD_STAGE_DIR,$*)/certs/." "$(call BUILD_OUTPUT_DIR,$*)/certs/"; \
	fi

.SILENT: $(call BUILD_STAGE_DIR,%)/Firmwares/dummy.bin
$(call BUILD_STAGE_DIR,%)/Firmwares/dummy.bin: $(call BUILD_CONFIG_STAMP,%)
	@$(ENSURE_REPO_LOCK)
	@mkdir -p "$(call BUILD_STAGE_DIR,$*)"
	@rm -rf "$(call BUILD_STAGE_DIR,$*)/Firmwares"
	@cp -aR $(PACKAGE_TOOL)/Firmwares/ "$(call BUILD_STAGE_DIR,$*)/"
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
		cp -r "$$FW/." "$(call BUILD_STAGE_DIR,$*)/Firmwares/"; \
	fi; \
	if [[ ! -e "$$FW" ]] || [[ ! -f "$$FW/ec_fw.bin" ]]; then \
		echo "Found EC-less platform."; \
		ln -sf dummy.bin "$(call BUILD_STAGE_DIR,$*)/Firmwares/ec_fw.bin"; \
	fi; \
	if [[ "$(ARTEFACT_MODE)" == "custom" && "$(CIX_RELEASE_NORMALIZED)" == "1.2" ]]; then \
		cp -f "$(CIX_RELEASE_V12_BOOTLOADER1_PAYLOAD)" "$(call BUILD_STAGE_DIR,$*)/Firmwares/bootloader1.img"; \
	fi

.SILENT: $(call BUILD_STAGE_DIR,%)/certs/trusted_key_no.crt
$(call BUILD_STAGE_DIR,%)/certs/trusted_key_no.crt: $(call BUILD_CONFIG_STAMP,%) | $(TRUSTED_KEY_CERT_TOOL_BIN)
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
	stage_dir="$(call BUILD_STAGE_DIR,$*)" && \
	rm -rf "$$stage_dir/Keys" "$$stage_dir/certs" && \
	mkdir -p "$$stage_dir/Keys" "$$stage_dir/certs" && \
	cp -a $(PACKAGE_TOOL)/Keys/. "$$stage_dir/Keys/" && \
	cp -a $(PACKAGE_TOOL)/certs/. "$$stage_dir/certs/" && \
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
				-p "$$stage_dir/Keys/oem_publickey.pem" \
				-s "$$stage_dir/Keys/oem_privatekey.pem" \
				-o $@; \
			;; \
		*) \
			echo "Unsupported ARTEFACT_MODE=$(ARTEFACT_MODE); expected custom or upstream" >&2; \
			exit 1; \
			;; \
	esac

.SILENT: Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd
Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd: $(call BUILD_CONFIG_STAMP,%) $(PATCHED_EDK2_SOURCE_INPUTS) $(CUSTOM_EDK2_BUILD_INPUTS) | preflight-% $(CUSTOM_EDK2_ORDER_ONLY_PREREQS)
	@$(ENSURE_REPO_LOCK)
	$(MAKE) prepare-basetools-c
	@set -euo pipefail; \
	if [[ "$(ARTEFACT_MODE)" == "custom" ]]; then \
		export WORKSPACE="$(CUSTOM_EDK2_WORKSPACE)"; \
		export PACKAGES_PATH="$(EDK2_PACKAGES_PATH)"; \
		mkdir -p "$$WORKSPACE"; \
		if [[ "$(EDK2_CIX_INCREMENTAL_CUSTOM_WORKSPACE)" == "1" ]]; then \
			rm -rf "$$WORKSPACE/Conf"; \
		else \
			rm -rf "$$WORKSPACE/Build/$*" "$$WORKSPACE/Conf"; \
		fi; \
		export EDK_TOOLS_PATH="$(CUSTOM_BASETOOLS_DIR)"; \
	else \
		export WORKSPACE="$(shell pwd)"; \
		export PACKAGES_PATH="$(IMPORTED_EDK2_PACKAGES_PATH)"; \
	fi; \
	if [[ "$(CCACHE_ENABLED_EFFECTIVE)" == "TRUE" ]]; then \
		real_gcc_prefix="$(GCC5_AARCH64_REAL_PREFIX)"; \
		wrapper_dir="$(CCACHE_WRAPPER_DIR)"; \
		wrapper_prefix="$(CCACHE_GCC5_AARCH64_PREFIX_EFFECTIVE)"; \
		resolve_host_tool() { \
			local tool_name="$$1"; \
			local tool_path; \
			if ! tool_path="$$(command -v "$$tool_name" 2>/dev/null)"; then \
				echo "[build/$*] Required tool $$tool_name was not found while preparing ccache wrappers" >&2; \
				exit 1; \
			fi; \
			printf '%s\n' "$$tool_path"; \
		}; \
		real_gcc="$$(resolve_host_tool "$${real_gcc_prefix}gcc")"; \
		real_gcc_ar="$$(resolve_host_tool "$${real_gcc_prefix}gcc-ar")"; \
		real_objcopy="$$(resolve_host_tool "$${real_gcc_prefix}objcopy")"; \
		mkdir -p "$(CCACHE_DIR)" "$$wrapper_dir"; \
		printf '%s\n' '#!/usr/bin/env bash' 'set -euo pipefail' "exec \"$(CCACHE_BIN_RESOLVED)\" \"$$real_gcc\" \"\$$@\"" > "$${wrapper_prefix}gcc"; \
		chmod +x "$${wrapper_prefix}gcc"; \
		ln -sf "$$real_gcc_ar" "$${wrapper_prefix}gcc-ar"; \
		ln -sf "$$real_objcopy" "$${wrapper_prefix}objcopy"; \
		export CCACHE_DIR="$(CCACHE_DIR)"; \
		export GCC5_AARCH64_PREFIX="$(CCACHE_GCC5_AARCH64_PREFIX_EFFECTIVE)"; \
	else \
		export GCC5_AARCH64_PREFIX="$(GCC5_AARCH64_REAL_PREFIX)"; \
	fi; \
	if command -v iasl >/dev/null 2>&1; then \
		export IASL_PREFIX="$$(dirname "$$(command -v iasl)")/"; \
	fi; \
	run_edksetup() { \
		local had_nounset=0; \
		case "$$-" in \
			*u*) had_nounset=1; set +u; ;; \
		esac; \
		if [[ "$(V)" == "1" ]]; then \
			source edk2/edksetup.sh --reconfig; \
		else \
			local edksetup_log; \
			edksetup_log="$$(mktemp "$(PREFERRED_TMP_ROOT)/edk2-cix-edksetup.XXXXXX")"; \
			if source edk2/edksetup.sh --reconfig >"$$edksetup_log" 2>&1; then \
				rm -f "$$edksetup_log"; \
			else \
				local status=$$?; \
				cat "$$edksetup_log" >&2; \
				rm -f "$$edksetup_log"; \
				if (( had_nounset )); then \
					set -u; \
				fi; \
				return "$$status"; \
			fi; \
		fi; \
		if (( had_nounset )); then \
			set -u; \
		fi; \
	}; \
	unset MAKEFLAGS; \
	run_edksetup; \
	build_extra_defines=(); \
	tool_def_overrides=(); \
	tools_def_file="$${CONF_PATH:-edk2/Conf}/tools_def.txt"; \
	if [[ ! -f "$$tools_def_file" ]]; then \
		echo "[build/$*] Expected EDK2 tools configuration $$tools_def_file was not produced" >&2; \
		exit 1; \
	fi; \
	if [[ "$(UEFI_TARGET)" == "RELEASE" ]]; then \
		case "$(ARTEFACT_MODE)" in \
			custom) \
				tool_def_overrides+=('RELEASE_*_*_GENFW_FLAGS          = --zero'); \
				build_extra_defines=(-D REPRODUCIBLE_BUILD_METADATA=TRUE); \
				;; \
			upstream) \
				;; \
			*) \
				echo "Unsupported ARTEFACT_MODE=$(ARTEFACT_MODE); expected custom or upstream" >&2; \
				exit 1; \
				;; \
		esac; \
	fi; \
	build_version_defines=(); \
	case "$(ARTEFACT_MODE)" in \
		custom) \
			build_version_defines+=(-D UEFI_FW_VERSION=$(UEFI_FW_VERSION)); \
			;; \
		upstream) \
			build_version_defines+=(-D DEB_VERSION=$(FIRMWARE_VERSION)); \
			;; \
		*) \
			echo "Unsupported ARTEFACT_MODE=$(ARTEFACT_MODE); expected custom or upstream" >&2; \
			exit 1; \
			;; \
	esac; \
	if [[ "$(ARTEFACT_MODE)" == "custom" ]]; then \
		lto_flag='-flto=auto'; \
		rwx_warning_flag=''; \
		if [[ "$(V)" != "1" ]]; then \
			tool_def_overrides+=( \
				'  DEBUG_GCC5_AARCH64_ASL_FLAGS   = -vr -vs' \
				'RELEASE_GCC5_AARCH64_ASL_FLAGS   = -vr -vs' \
			); \
			rwx_warning_flag=' -Wl,--no-warn-rwx-segments'; \
		fi; \
		tool_def_overrides+=( \
			"  DEBUG_GCC5_AARCH64_CC_FLAGS    = DEF(GCC5_AARCH64_CC_FLAGS) $$lto_flag -Wno-unused-but-set-variable -Wno-unused-const-variable" \
			"  DEBUG_GCC5_AARCH64_DLINK_FLAGS = DEF(GCC5_AARCH64_DLINK_FLAGS) $$lto_flag -Os -L\$$(WORKSPACE)/BaseTools/Bin/GccLto -llto-aarch64 -Wl,-plugin-opt=-pass-through=-llto-aarch64 -Wno-lto-type-mismatch$$rwx_warning_flag" \
			'  DEBUG_GCC5_AARCH64_VFRPP_FLAGS   = $$(ARCHCC_FLAGS) $$(PLATFORM_FLAGS) DEF(GCC_VFRPP_FLAGS) -DEDK2_CIX_CUSTOM_BUILD=1' \
			"RELEASE_GCC5_AARCH64_CC_FLAGS    = DEF(GCC5_AARCH64_CC_FLAGS) $$lto_flag -Wno-unused-but-set-variable -Wno-unused-const-variable" \
			"RELEASE_GCC5_AARCH64_DLINK_FLAGS = DEF(GCC5_AARCH64_DLINK_FLAGS) $$lto_flag -Os -L\$$(WORKSPACE)/BaseTools/Bin/GccLto -llto-aarch64 -Wl,-plugin-opt=-pass-through=-llto-aarch64 -Wno-lto-type-mismatch$$rwx_warning_flag" \
			'RELEASE_GCC5_AARCH64_VFRPP_FLAGS   = $$(ARCHCC_FLAGS) $$(PLATFORM_FLAGS) DEF(GCC_VFRPP_FLAGS) -DEDK2_CIX_CUSTOM_BUILD=1' \
		); \
		if [[ "$(ENABLE_FIRMWARE_FIXES_NORMALIZED)" == "TRUE" ]]; then \
			tool_def_overrides+=( \
				'  DEBUG_GCC5_AARCH64_ASLPP_FLAGS   = DEF(GCC_ASLPP_FLAGS) -DENABLE_FIRMWARE_FIXES=1' \
				'RELEASE_GCC5_AARCH64_ASLPP_FLAGS   = DEF(GCC_ASLPP_FLAGS) -DENABLE_FIRMWARE_FIXES=1' \
				'  DEBUG_GCC5_AARCH64_VFRPP_FLAGS   = $$(ARCHCC_FLAGS) $$(PLATFORM_FLAGS) DEF(GCC_VFRPP_FLAGS) -DEDK2_CIX_CUSTOM_BUILD=1 -DENABLE_FIRMWARE_FIXES=1' \
				'RELEASE_GCC5_AARCH64_VFRPP_FLAGS   = $$(ARCHCC_FLAGS) $$(PLATFORM_FLAGS) DEF(GCC_VFRPP_FLAGS) -DEDK2_CIX_CUSTOM_BUILD=1 -DENABLE_FIRMWARE_FIXES=1' \
			); \
		fi; \
	fi; \
	if [[ "$${#tool_def_overrides[@]}" -ne 0 ]]; then \
		printf '\n%s\n' "$${tool_def_overrides[@]}" >> "$$tools_def_file"; \
	fi; \
	if [[ "$(ARTEFACT_MODE)" == "custom" && -n "$(DEBUG_ON_UART3_NORMALIZED)" ]]; then \
		build_extra_defines+=(-D DEBUG_ON_UART3=$(DEBUG_ON_UART3_NORMALIZED)); \
	fi; \
	if [[ "$(ARTEFACT_MODE)" == "custom" && -n "$(UART3_ENABLE_EFFECTIVE)" ]]; then \
		build_extra_defines+=(-D UART3_ENABLE=$(UART3_ENABLE_EFFECTIVE)); \
	fi; \
	if [[ "$(ARTEFACT_MODE)" == "custom" && -n "$(DEBUG_VERBOSE_NORMALIZED)" ]]; then \
		build_extra_defines+=(-D DEBUG_VERBOSE=$(DEBUG_VERBOSE_NORMALIZED)); \
	fi; \
		if [[ "$(ARTEFACT_MODE)" == "custom" && -n "$(DEBUG_PRINT_ERROR_LEVEL_EFFECTIVE)" ]]; then \
			build_extra_defines+=(-D DEBUG_PRINT_ERROR_LEVEL=$(DEBUG_PRINT_ERROR_LEVEL_EFFECTIVE)); \
		fi; \
		if [[ "$(ARTEFACT_MODE)" == "custom" && -n "$(ENABLE_FIRMWARE_FIXES_NORMALIZED)" ]]; then \
			build_extra_defines+=(-D ENABLE_FIRMWARE_FIXES=$(ENABLE_FIRMWARE_FIXES_NORMALIZED)); \
		fi; \
		if [[ "$(ARTEFACT_MODE)" == "custom" && "$(ENABLE_FIRMWARE_FIXES_NORMALIZED)" == "TRUE" && "$(ENABLE_CORE_ORDER_NORMALIZED)" == "conventional" ]]; then \
			build_extra_defines+=(-D ENABLE_CORE_ORDER_CONVENTIONAL=1); \
		fi; \
		if [[ "$(ARTEFACT_MODE)" == "custom" && "$(ENABLE_FIRMWARE_FIXES_NORMALIZED)" == "TRUE" && "$(ENABLE_CORE_ORDER_NORMALIZED)" == "performance" ]]; then \
			build_extra_defines+=(-D ENABLE_CORE_ORDER_PERFORMANCE=1); \
		fi; \
			if [[ "$(ARTEFACT_MODE)" == "custom" && -n "$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED)" ]]; then \
				build_extra_defines+=(-D ENABLE_EXPERIMENTAL_UEFI_SETTINGS=$(ENABLE_EXPERIMENTAL_UEFI_SETTINGS_NORMALIZED)); \
			fi; \
			if [[ "$(ARTEFACT_MODE)" == "custom" && -n "$(O6_SMBIOS_BASEBOARD_ASSET_TAG_C_LITERAL)" ]]; then \
				build_extra_defines+=("-D" "O6_SMBIOS_BASEBOARD_ASSET_TAG=$(O6_SMBIOS_BASEBOARD_ASSET_TAG_C_LITERAL)"); \
			fi; \
			if [[ "$(ARTEFACT_MODE)" == "custom" && -n "$(O6_SMBIOS_CHASSIS_ASSET_TAG_C_LITERAL)" ]]; then \
				build_extra_defines+=("-D" "O6_SMBIOS_CHASSIS_ASSET_TAG=$(O6_SMBIOS_CHASSIS_ASSET_TAG_C_LITERAL)"); \
			fi; \
			run_edk2_build() { \
		export EDK2_CIX_QUIET_PREBUILD="$(if $(filter 1,$(V)),0,1)"; \
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
			"$${build_version_defines[@]}"; \
	}; \
	if [[ "$(V)" == "1" ]]; then \
		run_edk2_build; \
	else \
		run_edk2_build 2>&1 | ARTEFACT_MODE="$(ARTEFACT_MODE)" python3 "$(EDK2_BUILD_OUTPUT_FILTER)"; \
	fi; \
	if [[ "$(ARTEFACT_MODE)" == "custom" ]]; then \
		if [[ ! -d "$$WORKSPACE/Build/$*/$(UEFI_TARGET)_GCC5" ]]; then \
			echo "[build/$*] Expected custom workspace output $$WORKSPACE/Build/$*/$(UEFI_TARGET)_GCC5 was not produced" >&2; \
			exit 1; \
		fi; \
		mkdir -p "Build/$*"; \
		rm -rf "Build/$*/$(UEFI_TARGET)_GCC5"; \
		cp -a "$$WORKSPACE/Build/$*/$(UEFI_TARGET)_GCC5" "Build/$*/"; \
		build_options_path="Build/$*/$(UEFI_TARGET)_GCC5/BuildOptions"; \
		if [[ ! -f "$$build_options_path" ]]; then \
			echo "[build/$*] Expected BuildOptions $$build_options_path was not produced" >&2; \
			exit 1; \
		fi; \
		python3 ../scripts/normalize_build_options.py \
			--build-options "$$build_options_path" \
			--active-platform "src/$(filter %/$*.dsc,$(DSC))" \
			--flash-definition "src/$(patsubst %.dsc,%.fdf,$(filter %/$*.dsc,$(DSC)))"; \
	fi; \
	if [[ ! -s "$@" ]]; then \
		echo "[build/$*] Expected firmware image $@ was not produced" >&2; \
		exit 1; \
	fi

.PHONY: refresh-microsoft-secure-boot-defaults check-microsoft-secure-boot-defaults check-microsoft-secure-boot-release
refresh-microsoft-secure-boot-defaults:
	@python3 ../scripts/generate_microsoft_secure_boot_defaults.py >/dev/null

check-microsoft-secure-boot-defaults:
	@python3 ../scripts/generate_microsoft_secure_boot_defaults.py --check

check-microsoft-secure-boot-release:
	@python3 ../scripts/check_microsoft_secure_boot_release.py \
		--mode "$(MICROSOFT_SECURE_BOOT_RELEASE_CHECK_MODE)"

.PHONY: check-custom-module-overlays check-custom-acpi-overlays
check-custom-module-overlays:
	@python3 "$(CUSTOM_OVERLAY_INF_SYNC_CHECK)" \
		--overlay-root "$(CUSTOM_OVERLAY_ROOT)" \
		--source-root "$(REPO_ROOT)/src"

check-custom-acpi-overlays:
	@python3 "$(CUSTOM_ACPI_OVERLAY_CHECK)" --repo-root "$(abspath ..)"

.PHONY: check-offline-audit-baselines __check-offline-audit-baselines
check-offline-audit-baselines:
	@"$(EDK2_REBASED_DETERMINISTIC_GUARD)"

__check-offline-audit-baselines:
	@python3 "$(OFFLINE_AUDIT_BASELINE_CHECK)"

.PHONY: audit-final-image-manifest audit-final-image-manifest-% __audit-final-image-manifest-%
audit-final-image-manifest:
	@"$(EDK2_REBASED_DETERMINISTIC_GUARD)"

audit-final-image-manifest-%:
	@"$(EDK2_REBASED_DETERMINISTIC_GUARD)"

__audit-final-image-manifest-%: Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd
	@mkdir -p "$(FIRMWARE_VALIDATION_REPORT_ROOT)"
	@python3 "$(OFFLINE_AUDIT_BASELINE_CHECK)"
	@python3 "$(FINAL_IMAGE_MANIFEST_AUDIT_SCRIPT)" \
		--repo-root "$(abspath ..)" \
		--board "$*" \
		--target "$(UEFI_TARGET)_GCC5" \
		--profile "$(FIRMWARE_VALIDATION_PROFILE)" \
		--baseline-file "$(FINAL_IMAGE_MANIFEST_BASELINE)" \
		--report-json "$(FIRMWARE_VALIDATION_REPORT_ROOT)/$*-$(UEFI_TARGET)_GCC5-$(FIRMWARE_VALIDATION_PROFILE)-manifest-audit.json"

.PHONY: audit-acpi-regression audit-acpi-regression-% __audit-acpi-regression-%
audit-acpi-regression:
	@"$(EDK2_REBASED_DETERMINISTIC_GUARD)"

audit-acpi-regression-%:
	@"$(EDK2_REBASED_DETERMINISTIC_GUARD)"

__audit-acpi-regression-%: Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd
	@mkdir -p "$(FIRMWARE_VALIDATION_REPORT_ROOT)"
	@python3 "$(OFFLINE_AUDIT_BASELINE_CHECK)"
	@python3 "$(ACPI_REGRESSION_AUDIT_SCRIPT)" \
		--repo-root "$(abspath ..)" \
		--board "$*" \
		--target "$(UEFI_TARGET)_GCC5" \
		--profile "$(FIRMWARE_VALIDATION_PROFILE)" \
		--baseline-file "$(ACPI_REGRESSION_AUDIT_BASELINE)" \
		--report-json "$(FIRMWARE_VALIDATION_REPORT_ROOT)/$*-$(UEFI_TARGET)_GCC5-$(FIRMWARE_VALIDATION_PROFILE)-acpi-audit.json"

.PHONY: refresh-offline-audit-baselines refresh-offline-audit-baselines-% __refresh-offline-audit-baselines-%
refresh-offline-audit-baselines:
	@"$(EDK2_REBASED_DETERMINISTIC_GUARD)"

refresh-offline-audit-baselines-%:
	@"$(EDK2_REBASED_DETERMINISTIC_GUARD)"

__refresh-offline-audit-baselines-%:
	@mkdir -p "$(FIRMWARE_VALIDATION_REPORT_ROOT)"
	@python3 "$(OFFLINE_AUDIT_BASELINE_CHECK)"
	@python3 "$(OFFLINE_AUDIT_BASELINE_REFRESH)" \
		--repo-root "$(abspath ..)" \
		--board "$*" \
		--target "$(UEFI_TARGET)_GCC5" \
		--profile "$(FIRMWARE_VALIDATION_PROFILE)" \
		--report-dir "$(FIRMWARE_VALIDATION_REPORT_ROOT)"
	@python3 "$(OFFLINE_AUDIT_BASELINE_CHECK)"

.PHONY: validate-secure-boot-defaults validate-secure-boot-defaults-%
validate-secure-boot-defaults: $(foreach board,$(FIRMWARE_BOARDS),validate-secure-boot-defaults-$(board))

validate-secure-boot-defaults-%: Build/%/$(UEFI_TARGET)_GCC5/FV/SKY1_BL33_UEFI.fd
	@python3 ../scripts/validate_secure_boot_defaults.py \
		--repo-root "$(abspath ..)" \
		--board "$*" \
		--target "$(UEFI_TARGET)_GCC5"

$(MICROSOFT_SECURE_BOOT_PAYLOADS) &: ../scripts/generate_microsoft_secure_boot_defaults.py $(MICROSOFT_SECURE_BOOT_MANIFEST)
	@python3 ../scripts/generate_microsoft_secure_boot_defaults.py >/dev/null

.PHONY: clean clean-ccache clean-cix-cache __clean __clean-ccache __clean-cix-cache
clean:
	$(Q)$(LOCKED_MAKE) __clean V="$(V)"

clean-ccache:
	$(Q)$(LOCKED_MAKE) __clean-ccache V="$(V)"

clean-cix-cache:
	$(Q)$(LOCKED_MAKE) __clean-cix-cache V="$(V)"

.SILENT: __clean __clean-ccache __clean-cix-cache __distclean
__clean:
	printf '[clean] Removing build output\n'
	rm -rf Build
	rm -rf "$(CUSTOM_EDK2_WORKSPACE_ROOT)"
	printf '[clean] Removing host-built helper tools\n'
	rm -rf "$(FIPTOOL_SOURCE_DIR)/build"
	rm -rf "$(TRUSTED_KEY_CERT_TOOL_SOURCE_DIR)/build" "$(CERT_UEFI_CREATE_RSA_SOURCE_DIR)/build"
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

__clean-ccache:
	printf '[clean] Removing compiler cache data\n'
	rm -rf "$(CCACHE_DIR)" "$(CCACHE_WRAPPER_ROOT)"

__clean-cix-cache:
	printf '[clean] Removing curated CIX intermediate cache data\n'
	rm -rf "$(CIX_RELEASE_CACHE_ROOT)"

.PHONY: distclean __distclean
distclean:
	$(Q)$(LOCKED_MAKE) __distclean V="$(V)"

__distclean: __clean __clean-ccache
