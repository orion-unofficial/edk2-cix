#
# Copyright (c) 2015-2022, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include common/fdt_wrappers.mk

ifeq (${ARCH}, aarch64)
  # On ARM standard platorms, the TSP can execute from Trusted SRAM, Trusted
  # DRAM (if available) or the TZC secured area of DRAM.
  # TZC secured DRAM is the default.

  ARM_TSP_RAM_LOCATION	?=	dram

  ifeq (${ARM_TSP_RAM_LOCATION}, tsram)
    ARM_TSP_RAM_LOCATION_ID = ARM_TRUSTED_SRAM_ID
  else ifeq (${ARM_TSP_RAM_LOCATION}, tdram)
    ARM_TSP_RAM_LOCATION_ID = ARM_TRUSTED_DRAM_ID
  else ifeq (${ARM_TSP_RAM_LOCATION}, dram)
    ARM_TSP_RAM_LOCATION_ID = ARM_DRAM_ID
  else
    $(error "Unsupported ARM_TSP_RAM_LOCATION value")
  endif

  # Process flags
  # Process ARM_BL31_IN_DRAM flag
  ARM_BL31_IN_DRAM		:=	0
  $(eval $(call assert_boolean,ARM_BL31_IN_DRAM))
  $(eval $(call add_define,ARM_BL31_IN_DRAM))
else
  ARM_TSP_RAM_LOCATION_ID = ARM_TRUSTED_SRAM_ID
endif

$(eval $(call add_define,ARM_TSP_RAM_LOCATION_ID))


# For the original power-state parameter format, the State-ID can be encoded
# according to the recommended encoding or zero. This flag determines which
# State-ID encoding to be parsed.
ARM_RECOM_STATE_ID_ENC := 0

# If the PSCI_EXTENDED_STATE_ID is set, then ARM_RECOM_STATE_ID_ENC need to
# be set. Else throw a build error.
ifeq (${PSCI_EXTENDED_STATE_ID}, 1)
  ifeq (${ARM_RECOM_STATE_ID_ENC}, 0)
    $(error Build option ARM_RECOM_STATE_ID_ENC needs to be set if \
            PSCI_EXTENDED_STATE_ID is set for ARM platforms)
  endif
endif

# Process ARM_RECOM_STATE_ID_ENC flag
$(eval $(call assert_boolean,ARM_RECOM_STATE_ID_ENC))
$(eval $(call add_define,ARM_RECOM_STATE_ID_ENC))

# Process ARM_DISABLE_TRUSTED_WDOG flag
# By default, Trusted Watchdog is always enabled unless
# SPIN_ON_BL1_EXIT or ENABLE_RME is set
ARM_DISABLE_TRUSTED_WDOG	:=	0
ifneq ($(filter 1,${SPIN_ON_BL1_EXIT} ${ENABLE_RME}),)
ARM_DISABLE_TRUSTED_WDOG	:=	1
endif
$(eval $(call assert_boolean,ARM_DISABLE_TRUSTED_WDOG))
$(eval $(call add_define,ARM_DISABLE_TRUSTED_WDOG))

# Process ARM_BL31_IN_DRAM flag
ARM_BL31_IN_DRAM		:=	0
$(eval $(call assert_boolean,ARM_BL31_IN_DRAM))
$(eval $(call add_define,ARM_BL31_IN_DRAM))

# Process ARM_PLAT_MT flag
ARM_PLAT_MT			:=	0
$(eval $(call assert_boolean,ARM_PLAT_MT))
$(eval $(call add_define,ARM_PLAT_MT))

# Use translation tables library v2 by default
ARM_XLAT_TABLES_LIB_V1		:=	0
$(eval $(call assert_boolean,ARM_XLAT_TABLES_LIB_V1))
$(eval $(call add_define,ARM_XLAT_TABLES_LIB_V1))

# Arm Ethos-N NPU SiP service
ARM_ETHOSN_NPU_DRIVER			:=	0
$(eval $(call assert_boolean,ARM_ETHOSN_NPU_DRIVER))
$(eval $(call add_define,ARM_ETHOSN_NPU_DRIVER))

# Use an implementation of SHA-256 with a smaller memory footprint but reduced
# speed.
$(eval $(call add_define,MBEDTLS_SHA256_SMALLER))

# Enable PSCI_STAT_COUNT/RESIDENCY APIs on ARM platforms
ENABLE_PSCI_STAT		:=	0
ENABLE_PMF			:=	0

# Override the standard libc with optimised libc_asm
OVERRIDE_LIBC			:=	1
ifeq (${OVERRIDE_LIBC},1)
    include lib/libc/libc_asm.mk
endif

# On ARM platforms, separate the code and read-only data sections to allow
# mapping the former as executable and the latter as execute-never.
SEPARATE_CODE_AND_RODATA	:=	1

# On ARM platforms, disable SEPARATE_NOBITS_REGION by default. Both PROGBITS
# and NOBITS sections of BL31 image are adjacent to each other and loaded
# into Trusted SRAM.
SEPARATE_NOBITS_REGION		:=	0

# In order to support SEPARATE_NOBITS_REGION for Arm platforms, we need to load
# BL31 PROGBITS into secure DRAM space and BL31 NOBITS into SRAM. Hence mandate
# the build to require that ARM_BL31_IN_DRAM is enabled as well.
ifeq ($(SEPARATE_NOBITS_REGION),1)
    ifneq ($(ARM_BL31_IN_DRAM),1)
         $(error For SEPARATE_NOBITS_REGION, ARM_BL31_IN_DRAM must be enabled)
    endif
    ifneq ($(RECLAIM_INIT_CODE),0)
          $(error For SEPARATE_NOBITS_REGION, RECLAIM_INIT_CODE cannot be supported)
    endif
endif

# Disable ARM Cryptocell by default
ARM_CRYPTOCELL_INTEG		:=	0
$(eval $(call assert_boolean,ARM_CRYPTOCELL_INTEG))
$(eval $(call add_define,ARM_CRYPTOCELL_INTEG))

# Enable PIE support for RESET_TO_BL31/RESET_TO_SP_MIN case
ifneq ($(filter 1,${RESET_TO_BL31} ${RESET_TO_SP_MIN}),)
	ENABLE_PIE			:=	1
endif

# CryptoCell integration relies on coherent buffers for passing data from
# the AP CPU to the CryptoCell
ifeq (${ARM_CRYPTOCELL_INTEG},1)
    ifeq (${USE_COHERENT_MEM},0)
        $(error "ARM_CRYPTOCELL_INTEG needs USE_COHERENT_MEM to be set.")
    endif
endif

# Enable CRC instructions via extension for ARMv8-A CPUs.
# For ARMv8.1-A, and onwards CRC instructions are default enabled.
# Enable HW computed CRC support unconditionally in BL2 component.
ifeq (${ARM_ARCH_MINOR},0)
  BL2_CPPFLAGS += -march=armv8-a+crc
endif

ifeq (${ARCH}, aarch64)
PLAT_INCLUDES		+=	-Iinclude/plat/arm/common/aarch64
endif

CIX_BASE       =       plat/cix/
PLAT_INCLUDES           +=      -I${CIX_BASE}/common/include/

PLAT_BL_COMMON_SOURCES	+=	plat/cix/common/${ARCH}/cix_helpers.S		\
				plat/cix/common/cix_common.c			\
				plat/cix/common/cix_uart.S			\
				plat/cix/common/cix_console.c     \

ifeq (${ARM_XLAT_TABLES_LIB_V1}, 1)
PLAT_BL_COMMON_SOURCES 	+=	lib/xlat_tables/xlat_tables_common.c	      \
				lib/xlat_tables/${ARCH}/xlat_tables.c
else
ifeq (${XLAT_MPU_LIB_V1}, 1)
include lib/xlat_mpu/xlat_mpu.mk
PLAT_BL_COMMON_SOURCES	+=	${XLAT_MPU_LIB_V1_SRCS}
else
include lib/xlat_tables_v2/xlat_tables.mk
PLAT_BL_COMMON_SOURCES	+=      ${XLAT_TABLES_LIB_SRCS}
endif
endif

CIX_IO_SOURCES		+=	plat/cix/common/cix_io_storage.c		\

ifeq (${SPD},spmd)
    ifeq (${BL2_ENABLE_SP_LOAD},1)
         CIX_IO_SOURCES		+=	plat/cix/common/fconf/cix_fconf_sp.c
    endif
endif


BL2_SOURCES		+=	drivers/delay_timer/delay_timer.c		\
				drivers/delay_timer/generic_delay_timer.c	\
				drivers/io/io_fip.c				\
				drivers/io/io_memmap.c				\
				drivers/io/io_storage.c				\
				drivers/io/io_mtd.c					\
				drivers/mtd/nor/spi_nor.c					\
				drivers/mtd/spi-mem/spi_mem.c					\
				drivers/cadence/xspi/xspi.c              \
				common/tf_crc32.c				\
				${CIX_IO_SOURCES}

# Firmware Configuration Framework sources
include lib/fconf/fconf.mk

BL2_SOURCES		+=	${FCONF_SOURCES} ${FCONF_DYN_SOURCES}

# Add `libfdt` and Arm common helpers required for Dynamic Config
include lib/libfdt/libfdt.mk

ifeq (${BL2_AT_EL3},1)
BL2_SOURCES		+=	plat/cix/sky1/sky1_bl2_el3_setup.c
endif

# Because BL1/BL2 execute in AArch64 mode but BL32 in AArch32 we need to use
# the AArch32 descriptors.
BL2_SOURCES		+=	plat/cix/common/${ARCH}/cix_bl2_mem_params_desc.c		\
				          plat/cix/common/cix_image_load.c		\
				          common/desc_image_load.c

ifeq (${SPD},opteed)
BL2_SOURCES		+=	lib/optee/optee_utils.c
endif

BL31_SOURCES		+=	plat/cix/common/cix_topology.c			\
				plat/common/plat_psci_common.c			\
				drivers/delay_timer/delay_timer.c		\
				drivers/delay_timer/generic_delay_timer.c       \
				plat/cix/common/aarch64/execution_state_switch.c\
				plat/cix/common/cix_sip_svc.c

ifeq (${CIX_DST_SUPPORT},1)
BL31_SOURCES		+= plat/cix/common/cix_dst.c
endif

ifneq ($(filter 1,${ENABLE_PMF} ${ARM_ETHOSN_NPU_DRIVER}),)
ARM_SVC_HANDLER_SRCS :=

ifeq (${ENABLE_PMF},1)
ARM_SVC_HANDLER_SRCS	+=	lib/pmf/pmf_smc.c
endif

ifeq (${ARCH}, aarch64)
BL31_SOURCES		+=	plat/cix/common/aarch64/execution_state_switch.c\
				plat/cix/common/cix_sip_svc.c			\
				${ARM_SVC_HANDLER_SRCS}
else
BL32_SOURCES		+=	plat/cix/common/cix_sip_svc.c			\
				${ARM_SVC_HANDLER_SRCS}
endif
endif

ifeq (${EL3_EXCEPTION_HANDLING},1)
BL31_SOURCES		+=	plat/cix/common/cix_ehf.c
endif

EL3_EXCEPTION_HANDLING := $(SDEI_SUPPORT)
ifeq (${SDEI_SUPPORT},1)
BL31_SOURCES		+=	plat/cix/common/cix_ehf.c

ifeq (${SDEI_TFA_EXCEPTION_SUPPORT},1)
BL31_SOURCES		+= plat/cix/common/cix_err.c \
					plat/cix/common/cix_bl31_crash.S
endif

ifeq (${SDEI_TEE_EXCEPTION_SUPPORT},1)
BL31_SOURCES		+= plat/cix/common/cix_tee_err.c
endif

ifeq (${SDEI_IN_FCONF},1)
BL31_SOURCES		+=	plat/cix/common/fconf/fconf_sdei_getter.c
endif

endif # SDEI_SUPPORT

ifeq (${FW_BOOT_PERF_SUPPORT},1)
$(eval $(call add_define,FW_BOOT_PERF_SUPPORT))
BL2_SOURCES		+=	plat/cix/common/cix_fw_boot_perf.c
BL31_SOURCES		+=	plat/cix/common/cix_fw_boot_perf.c
endif

# RAS sources
ifeq (${RAS_EXTENSION},1)
BL31_SOURCES		+=	lib/extensions/ras/std_err_record.c		\
				lib/extensions/ras/ras_common.c
endif

# Pointer Authentication sources
ifeq (${ENABLE_PAUTH}, 1)
PLAT_BL_COMMON_SOURCES	+=	plat/cix/common/aarch64/arm_pauth.c	\
				lib/extensions/pauth/pauth_helpers.S
endif

ifeq (${SPD},spmd)
BL31_SOURCES		+=	plat/common/plat_spmd_manifest.c	\
				common/uuid.c				\
				${LIBFDT_SRCS}

BL31_SOURCES		+=	${FDT_WRAPPERS_SOURCES}
endif

ifneq (${TRUSTED_BOARD_BOOT},0)

    BOOT_SPI_NOR := 1

    # Include common TBB sources
    AUTH_SOURCES 	:= 	drivers/auth/auth_mod.c	\
				drivers/auth/img_parser_mod.c

    # Include the selected chain of trust sources.
    ifeq (${COT},tbbr)
        ifneq (${COT_DESC_IN_DTB},0)
            BL2_SOURCES	+=	lib/fconf/fconf_cot_getter.c
        else
            BL2_SOURCES	+=	drivers/auth/tbbr/tbbr_cot_common.c	\
				drivers/auth/tbbr/tbbr_cot_bl2.c
        endif
    else ifeq (${COT},dualroot)
        AUTH_SOURCES	+=	drivers/auth/dualroot/cot.c
    else
        $(error Unknown chain of trust ${COT})
    endif


    BL2_SOURCES		+=	${AUTH_SOURCES}					\
				plat/common/tbbr/plat_tbbr.c

    $(eval $(call TOOL_ADD_IMG,ns_bl2u,--fwu,FWU_))
endif

# Include Measured Boot makefile before any Crypto library makefile.
# Crypto library makefile may need default definitions of Measured Boot build
# flags present in Measured Boot makefile.
ifeq (${MEASURED_BOOT},1)
    MEASURED_BOOT_MK := drivers/measured_boot/event_log/event_log.mk
    $(info Including ${MEASURED_BOOT_MK})
    include ${MEASURED_BOOT_MK}
endif

ifneq ($(filter 1,${MEASURED_BOOT} ${TRUSTED_BOARD_BOOT}),)
    CRYPTO_SOURCES	:=	drivers/auth/crypto_mod.c 	\
				lib/fconf/fconf_tbbr_getter.c
    BL2_SOURCES		+=	${CRYPTO_SOURCES}
endif

ifeq (${RECLAIM_INIT_CODE}, 1)
    ifeq (${ARM_XLAT_TABLES_LIB_V1}, 1)
        $(error "To reclaim init code xlat tables v2 must be used")
    endif
endif
