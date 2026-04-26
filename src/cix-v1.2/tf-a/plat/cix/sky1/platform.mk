# Copyright (c) 2021, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include common/fdt_wrappers.mk

RAS_EXTENSION		:=	1

CIX_DST_SUPPORT		:=	1

SDEI_SUPPORT		:=	1

SDEI_WDT_SUPPORT	:=	1

SDEI_TFA_EXCEPTION_SUPPORT	:=	1
SDEI_TEE_EXCEPTION_SUPPORT	:=	1

SKY1_IDM_RAS_SUPPORT	:=	1
SKY1_CPU_RAS_SUPPORT	:=	1
SKY1_TZC400_IDM_SUPPORT	:=	1

SKY1_PM_I2C_HANG_PATCH	:=	1

EL3_EXCEPTION_HANDLING	:=	0

SKY1_EL3_EXCEPTION_HANDLING	:= 1

BL2_AT_EL3              :=      1

FW_BOOT_PERF_SUPPORT	:=	1

HANDLE_EA_EL3_FIRST	:=	0

# CPU will start executing code directly at programmable reset address, both
# on a cold and warm reset
PROGRAMMABLE_RESET_ADDRESS :=   1

# Indicate single-core to boot up bl2 and bl31
COLD_BOOT_SINGLE_CPU 	:=	1

# System coherency is managed in hardware
HW_ASSISTED_COHERENCY	:=	1

# When building for systems with hardware-assisted coherency, there's no need to
# use USE_COHERENT_MEM. Require that USE_COHERENT_MEM must be set to 0 too.
USE_COHERENT_MEM	:=	0

GIC_ENABLE_V4_EXTN	:=      1

# GIC-600 configuration
GICV3_SUPPORT_GIC600	:=	1

# Enable SVE
ENABLE_SVE_FOR_NS	:=	1
ENABLE_SVE_FOR_SWD	:=	1

# enable trace buffer control registers access to NS by default
ENABLE_TRBE_FOR_NS              := 1

# enable trace system registers access to NS by default
ENABLE_SYS_REG_TRACE_FOR_NS     := 1

# enable trace filter control registers access to NS by default
ENABLE_TRF_FOR_NS               := 1

ifeq (${DEBUG},1)
# Enable stack protection
ENABLE_STACK_PROTECTOR		:= strong
endif

ifeq (${SDEI_SUPPORT},0)
SDEI_TFA_EXCEPTION_SUPPORT	:=	0
SDEI_TEE_EXCEPTION_SUPPORT	:=	0
endif

ifeq (${CIX_BOARD}, emu)
$(eval $(call add_define,CIX_BOARD_EMU))
else ifeq (${CIX_BOARD}, fpga)
$(eval $(call add_define,CIX_BOARD_FPGA))
else ifeq (${CIX_BOARD}, evb)
$(eval $(call add_define,CIX_BOARD_EVB))
endif

# Include GICv3 driver files
include drivers/arm/gic/v3/gicv3.mk

TF_MBEDTLS_KEY_ALG := rsa+ecdsa

KEY_SIZE	:=	3072

ENT_GIC_SOURCES		:=	${GICV3_SOURCES}		\
				plat/common/plat_gicv3.c	\
				plat/cix/common/cix_gicv3.c

override NEED_BL2U	:=	no

override ARM_PLAT_MT	:=	0

# Using cix csec lib for trust boot and image parse
CIX_SEC_LIB := 1

SKY1_BASE	=	plat/cix/sky1

PLAT_INCLUDES		+=	-I${SKY1_BASE}/include/	\
				-I${SKY1_BASE}/include/drivers

PLAT_BL_COMMON_SOURCES +=      ${SKY1_BASE}/sky1_plat.c        \
                               ${SKY1_BASE}/sky1_pdc.c		\
			       ${SKY1_BASE}/sky1_board_id_check.c      \
			       ${SKY1_BASE}/sky1_dp_gop.c        \
                               ${SKY1_BASE}/include/sky1_helpers.S

BL2_SOURCES		+=	${SKY1_BASE}/sky1_security.c	\
				${SKY1_BASE}/sky1_err.c		\
				${SKY1_BASE}/sky1_trusted_boot.c		\
				lib/utils/mem_region.c			\
				drivers/arm/tzc/tzc400.c		\
				${SKY1_BASE}/drivers/watchdog/watchdog.c \
				plat/cix/common/cix_tzc400.c

BL31_SOURCES		+=	${ENT_GIC_SOURCES}			\
				${SKY1_BASE}/sky1_bl31_setup.c	\
				${SKY1_BASE}/sky1_topology.c	\
				${SKY1_BASE}/plat_sip.c	\
				lib/fconf/fconf.c			\
				${SKY1_BASE}/sky1_pm.c			\
				lib/fconf/fconf_dyn_cfg_getter.c	\
				drivers/cfi/v2m/v2m_flash.c		\
				lib/utils/mem_region.c			\
				plat/cix/common/cix_nor_psci_mem_protect.c \
				drivers/scmi-msg/base.c			\
				drivers/scmi-msg/clock.c		\
				drivers/scmi-msg/entry.c		\
				drivers/scmi-msg/reset_domain.c		\
				drivers/scmi-msg/smt.c			\
				drivers/scmi-msg/power_domain.c		\
				${SKY1_BASE}/drivers/scmi/scmi.c	\
				${SKY1_BASE}/drivers/scmi/scmi_pd.c	\
				${SKY1_BASE}/drivers/mailbox/mailbox.c \
				${SKY1_BASE}/drivers/hwspinlock/sky1_hwspinlock.c \
				${SKY1_BASE}/drivers/dsu/dsu_portion_pd.c \
				drivers/arm/css/scmi/scmi_common.c \
				drivers/arm/css/scmi/scmi_sys_pwr_proto.c \
                                drivers/mtd/spi-mem/spi_mem.c \
                                drivers/cadence/xspi/xspi.c \
				${SKY1_BASE}/sky1_qos.c \
				$(SKY1_BASE)/sky1_nsaid.c \
				${SKY1_BASE}/sky1_security.c \
				${SKY1_BASE}/sky1_ddrlp.c \
				drivers/arm/tzc/tzc400.c		\
				plat/cix/common/cix_tzc400.c	\
				${SKY1_BASE}/drivers/watchdog/watchdog.c

include ${SKY1_BASE}/dst/dst.mk
include ${SKY1_BASE}/ras/ras.mk

ifeq (${CIX_BOARD}, emu)
BL2_SOURCES	+=	lib/cpus/aarch64/cortex_hayes.S
BL31_SOURCES	+=	lib/cpus/aarch64/cortex_hayes.S
BL2_SOURCES	+=	lib/cpus/aarch64/cortex_hunter.S
BL31_SOURCES	+=	lib/cpus/aarch64/cortex_hunter.S
else ifeq (${CIX_BOARD}, fpga)
BL2_SOURCES	+=	lib/cpus/aarch64/cortex_hayes.S
BL31_SOURCES	+=	lib/cpus/aarch64/cortex_hayes.S
else
BL2_SOURCES	+=	lib/cpus/aarch64/cortex_hayes.S
BL31_SOURCES	+=	lib/cpus/aarch64/cortex_hayes.S
BL2_SOURCES	+=	lib/cpus/aarch64/cortex_hunter.S
BL31_SOURCES	+=	lib/cpus/aarch64/cortex_hunter.S
endif

BL31_SOURCES		+=	${FDT_WRAPPERS_SOURCES}

override CTX_INCLUDE_AARCH32_REGS	:= 0

override CTX_INCLUDE_PAUTH_REGS	:= 0

override ENABLE_SPE_FOR_LOWER_ELS	:= 1

override ENABLE_FEAT_AMUv1 :=1

override ENABLE_FEAT_AMUv1p1 :=1

override ENABLE_AMU := 1

override ENABLE_AMU_AUXILIARY_COUNTERS := 1

override ENABLE_AMU_FCONF := 0

# MPMM which in Group 1 should been enable and used in SCP
override ENABLE_MPMM := 1

override ENABLE_MPMM_FCONF := 0

# Enable MPAM Feature
override ENABLE_MPAM_FOR_LOWER_ELS :=1


include plat/cix/common/cix_common.mk

ifeq (${SMP}, 1)
$(eval $(call add_define,CIX_ARCH_SMP))
endif

ifeq (${BUILD_MODE}, debug)
$(eval $(call add_define,CONFIG_CIX_DEBUG))
endif

ifeq (${CIX_DST_SUPPORT}, 1)
$(eval $(call add_define,CIX_DST_SUPPORT))
endif

ifeq (${SDEI_WDT_SUPPORT}, 1)
$(eval $(call add_define,SDEI_WDT_SUPPORT))
endif

ifeq (${SDEI_TFA_EXCEPTION_SUPPORT}, 1)
$(eval $(call add_define,SDEI_TFA_EXCEPTION_SUPPORT))
endif

ifeq (${SDEI_TEE_EXCEPTION_SUPPORT}, 1)
$(eval $(call add_define,SDEI_TEE_EXCEPTION_SUPPORT))
endif

ifeq (${SKY1_PM_I2C_HANG_PATCH}, 1)
$(eval $(call add_define,SKY1_PM_I2C_HANG_PATCH))
endif

ifeq (${SKY1_EL3_EXCEPTION_HANDLING}, 1)
$(eval $(call add_define,SKY1_EL3_EXCEPTION_HANDLING))
endif

DISABLE_IMG_AUTH ?= 0
ifeq (${DISABLE_IMG_AUTH}, 1)
$(eval $(call add_define,DISABLE_IMG_AUTH))
endif

BOOT_SPI_NOR ?= 1
ifeq (${BOOT_SPI_NOR}, 1)
$(eval $(call add_define,BOOT_SPI_NOR))
BL2_SOURCES	+=	drivers/mtd/nor/spi_nor.c \
				drivers/mtd/spi-mem/spi_mem.c \
				drivers/cadence/xspi/xspi.c
endif

ifeq (${SPD},trusty)
	BL31_CFLAGS    +=      -DPLAT_XLAT_TABLES_DYNAMIC=1
endif

ifneq (${ENABLE_STACK_PROTECTOR},0)
PLAT_BL_COMMON_SOURCES	+=	plat/cix/common/cix_stack_protector.c
endif

#STR WITHOUT PM
CONFIG_CIX_STR_SE	:= 0

ifeq (${CONFIG_CIX_STR_SE}, 1)
$(eval $(call add_define,CONFIG_CIX_STR_SE))
endif

#CONFIG_DEBUG_FOOTPRINT_ENABLE
CONFIG_DEBUG_FOOTPRINT_ENABLE	:= 0

ifeq (${CONFIG_DEBUG_FOOTPRINT_ENABLE}, 1)
$(eval $(call add_define,CONFIG_DEBUG_FOOTPRINT_ENABLE))
endif

#CIX POST CODE DEBUG
CONFIG_CIX_POSTDEBUG_DEBUG	:= 0

ifeq (${CONFIG_CIX_POSTDEBUG_DEBUG}, 1)
$(eval $(call add_define,CONFIG_CIX_POSTDEBUG_DEBUG))
endif

#CIX HW SPINLOCK
CONFIG_CIX_HW_SPINLOCK	:=1

ifeq (${CONFIG_CIX_HW_SPINLOCK}, 1)
$(eval $(call add_define, CONFIG_CIX_HW_SPINLOCK))
endif
