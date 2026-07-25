/*
 * Copyright (c) 2020-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLATFORM_DEF_H
#define PLATFORM_DEF_H

#include <lib/utils_def.h>
#include <lib/xlat_tables/xlat_tables_defs.h>
#include <plat/arm/board/common/v2m_def.h>
#include <plat/common/common_def.h>
#include <arch.h>
#include <common/tbbr/tbbr_img_def.h>
#ifndef __ASSEMBLER__
#include <lib/mmio.h>
#endif /* __ASSEMBLER__ */

#define PLATFORM_MAX_CPUS_PER_CLUSTER   U(12)

#define PLATFORM_SYSTEM_COUNT		U(1)
#define PLATFORM_CLUSTER_COUNT		U(1)
#define PLATFORM_MAX_PE_PER_CPU		U(1)
#define PLATFORM_CLUSTER0_CORE_COUNT    PLATFORM_MAX_CPUS_PER_CLUSTER
#define PLATFORM_CORE_COUNT		PLATFORM_CLUSTER0_CORE_COUNT
#define PLAT_NUM_PWR_DOMAINS            (PLATFORM_SYSTEM_COUNT + \
					 PLATFORM_CLUSTER_COUNT + \
                                         PLATFORM_CORE_COUNT)

#define CSS_SYSTEM_PWR_DMN_LVL		MPIDR_AFFLVL2
#define PLAT_MAX_PWR_LVL		MPIDR_AFFLVL2


#define CIX_TRUSTED_SRAM_BASE		UL(0x04000000)
#define CIX_TRUSTED_SRAM_SIZE		0x00080000	/* 512 KB */

/* the mailbox is at the bottom of SRAM */
#define PLAT_CIX_TRUSTED_MAILBOX_BASE	CIX_TRUSTED_SRAM_BASE

#define CIX_SHARED_RAM_BASE		CIX_TRUSTED_SRAM_BASE
#define CIX_SHARED_RAM_SIZE		UL(0x00001000)	/* 4 KB */

#define SKY1_DRAM1_BASE			ULL(0x80000000)
#define SKY1_DRAM1_SIZE			ULL(0x400000000)
#define SKY1_DRAM1_END			(SKY1_DRAM1_BASE + SKY1_DRAM1_SIZE - 1ULL)

#define SKY1_DRAM2_BASE			ULL(0x8000000000)
#define SKY1_DRAM2_SIZE			ULL(0x8000000000)
#define SKY1_DRAM2_END			(SKY1_DRAM2_BASE + SKY1_DRAM2_SIZE - 1ULL)

#define SKY1_TZC_DRAM1_BASE		SKY1_DRAM1_BASE
#define SKY1_TZC_DRAM1_SIZE		UL(0x2500000)	/* 37 MB */
#define SKY1_TZC_DRAM1_END		(SKY1_TZC_DRAM1_BASE +		\
					 SKY1_TZC_DRAM1_SIZE - 1)

#define SKY1_NS_DRAM1_BASE		ULL(0x82500000)
#define SKY1_NS_DRAM1_SIZE		(SKY1_DRAM1_SIZE -		\
					 SKY1_TZC_DRAM1_SIZE)

#define SKY1_NS_DRAM1_END		(SKY1_NS_DRAM1_BASE +		\
					 SKY1_NS_DRAM1_SIZE - 1)

#define SKY1_AP_ATF_NS_BASE		ULL(0x84380000)
#define SKY1_AP_ATF_NS_SIZE		0x1000

#define AP_SW_CNT_DDR_PHY_ADDR_BASE	ULL(0x83BF0000)
#define AP_SW_CNT_DDR_PHY_SIZE	0x1000

#define SKY1_REBOOT_REASON_UEFI ULL(0x83E02080)

#define SKY1_ATF_TEE_SHM_BASE	ULL(0x824FF000)
#define SKY1_ATF_TEE_SHM_SIZE	0x1000

#define SKY1_TEE_OS_BASE		ULL(0x80500000)

/*RCSU mapping*/
#define AUDIO_RCSU_BASE_REG	ULL(0x07000000)
#define AUDIO_RCSU_PD_REG	ULL(0x07000020)
#define PCIE_X8_RCSU_BASE_REG	ULL(0x0a000000)
#define PCIE_X8_RCSU_PD_REG	ULL(0x0a000020)
#define PCIEHUB_SMMU_RCSU_BASE_REG	ULL(0x0b000000)
#define MMHUB_SMMU_RCSU_BASE	ULL(0x0b1A0000)
#define PCIEHUB_RCSU_PD_REG	ULL(0x0d008000)
#define MMHUB_RCSU_BASE_REG	ULL(0x0d030000)
#define MMHUB_RCSU_PD_REG	ULL(0x0d038000)
#define GIC_RCSU_BASE	ULL(0x0e000000)
#define DPU_RCSU_BASE_REG	ULL(0x14000000)
#define DPU0_RCSU_PD_REG	ULL(0x14000210)
#define DPU1_RCSU_PD_REG	ULL(0x14070210)
#define DPU2_RCSU_PD_REG	ULL(0x140e0210)
#define DPU3_RCSU_PD_REG ULL(0x14150210)
#define DPU4_RCSU_PD_REG ULL(0x141c0210)
#define VPU_RCSU_BASE_REG	ULL(0x14230000)
#define VPU_RCSU_PD_REG	ULL(0x1423021c)
#define NPU_RCSU_BASE_REG ULL(0x14250000)
#define NPU_CORE0_PD_REG	ULL(0x14250200)
#define NPU_CORE1_PD_REG	ULL(0x14250204)
#define NPU_CORE2_PD_REG	ULL(0x14250208)
#define NPU_TOP_PD_REG	ULL(0x1425020c)
#define ISP0_RCSU_BASE_REG	ULL(0x14330000)
#define ISP0_RCSU_PD_REG	ULL(0x14330020)
#define GPU_RCSU_BASE_REG	ULL(0x15000000)
#define GPU_RCSU_PD_REG	ULL(0x15000218)
#define DBG_RCSU_PD_REG ULL(0x1a000b00)

/*reserved regsiter for s5 domain*/
#define SW_USED_REG0	ULL(0x16000500)
#define SW_USED_REG1	ULL(0x16000504)
#define SW_USED_REG2	ULL(0x16000508)
#define SW_USED_REG3	ULL(0x1600050c)
#define SW_USED_REG4	ULL(0x16000510)
#define SW_USED_REG5	ULL(0x16000514)
#define SW_USED_REG6	ULL(0x16000518)
#define SW_USED_REG7	ULL(0x1600051c)

#define SKY1_REBOOT_REASON_ADDR SW_USED_REG0

/*
 * Mappings for SKY1 DRAM1 (non-secure) and SKY1 TZC DRAM1 (secure)
 */
#define SKY1_MAP_NS_DRAM1		MAP_REGION_FLAT(		\
						SKY1_NS_DRAM1_BASE,	\
						SKY1_NS_DRAM1_SIZE,	\
						MT_MEMORY | MT_RW | MT_NS)


#define SKY1_MAP_TZC_DRAM1		MAP_REGION_FLAT(		\
						SKY1_TZC_DRAM1_BASE,	\
						SKY1_TZC_DRAM1_SIZE,	\
						MT_MEMORY | MT_RW | MT_SECURE)

#define PLAT_SKY1_PRIMARY_CPU U(0x400)
#define PLAT_SKY1_CPU_MASK MPIDR_CLUSTER_MASK

/*
 * PLAT_ARM_MMAP_ENTRIES depends on the number of entries in the
 * plat_sky1_mmap array defined for each BL stage.
 */
#if defined(IMAGE_BL31)
# if SPM_MM
#  define PLAT_ARM_MMAP_ENTRIES		9
#  define MAX_XLAT_TABLES		7
#  define PLAT_SP_IMAGE_MMAP_REGIONS	7
#  define PLAT_SP_IMAGE_MAX_XLAT_TABLES	10
# else
#  define PLAT_ARM_MMAP_ENTRIES		8
#  define MAX_XLAT_TABLES		12
# endif
#elif defined(IMAGE_BL32)
# define PLAT_ARM_MMAP_ENTRIES		8
# define MAX_XLAT_TABLES		5
#elif !USE_ROMLIB
# define PLAT_ARM_MMAP_ENTRIES		11
# define MAX_XLAT_TABLES		7
#else
# define PLAT_ARM_MMAP_ENTRIES		12
# define MAX_XLAT_TABLES		6
#endif

/*
 * Size of cacheable stacks
 */
#if defined(IMAGE_BL2)
# if TRUSTED_BOARD_BOOT
#  define PLATFORM_STACK_SIZE		0x2000
# else
#  define PLATFORM_STACK_SIZE		0x1000
# endif
#elif defined(IMAGE_BL31)
# if SPM_MM
#  define PLATFORM_STACK_SIZE		0x500
# else
#  define PLATFORM_STACK_SIZE		0x2000
# endif
#elif defined(IMAGE_BL32)
# define PLATFORM_STACK_SIZE		0x440
#endif


// SKY1 peripherals address memory
#define SKY1_DEVICE_BASE			0x00000000
#define SKY1_DEVICE_SIZE			0x20000000

// SKY1_MAP_DEVICE covers different peripherals
// available to the platform
#define SKY1_MAP_DEVICE	MAP_REGION_FLAT(		\
					SKY1_DEVICE_BASE,	\
					SKY1_DEVICE_SIZE,	\
					MT_DEVICE | MT_RW | MT_SECURE)

/* for gicv3 */
#define ARM_IRQ_SEC_SGI_0		8
#define ARM_IRQ_SEC_SGI_1		9
#define ARM_IRQ_SEC_SGI_2		10
#define ARM_IRQ_SEC_SGI_3		11
#define ARM_IRQ_SEC_SGI_4		12
#define ARM_IRQ_SEC_SGI_5		13
#define ARM_IRQ_SEC_SGI_6		14
#define ARM_IRQ_SEC_SGI_7		15

/* define for SDEI WDT Intr */
#define CIX_SDEI_WDT_EVENT		U(100)
#define CIX_AP_WDT_INTR			U(408)
#define CIX_TEE_EXCEPTION_EVENT		(0xFE)
#define CIX_TFA_EXCEPTION_EVENT		U(0xFF)
#define PLAT_PRI_BITS			U(0x03)
#define PLAT_SDEI_SGI_PRIVATE		U(9)

/* define for SDEI idm event */
#define SDEI_SKY1_IDM_MMHUB0_EVENT	U(200)
#define SDEI_SKY1_IDM_MMHUB1_EVENT	U(201)
#define SDEI_SKY1_IDM_PCIE0_EVENT	U(202)
#define SDEI_SKY1_IDM_PCIE1_EVENT	U(203)
#define SDEI_SKY1_IDM_SYSHUB0_EVENT	U(204)
#define SDEI_SKY1_IDM_SYSHUB1_EVENT	U(205)
#define SDEI_SKY1_IDM_SMN0_EVENT	U(206)
#define SDEI_SKY1_IDM_SMN1_EVENT	U(207)

#define SKY1_IDM_MMHUB0_INTR U(93)
#define SKY1_IDM_MMHUB1_INTR U(94)
#define SKY1_IDM_PCIE0_INTR U(97)
#define SKY1_IDM_PCIE1_INTR U(98)
#define SKY1_IDM_SYSHUB0_INTR U(101)
#define SKY1_IDM_SYSHUB1_INTR U(102)
#define SKY1_IDM_SMN0_INTR U(104)
#define SKY1_IDM_SMN1_INTR U(105)

/* define for SDEI tzc400 Event */
#define SDEI_SKY1_TZC400_0_EVENT		(210)
#define SDEI_SKY1_TZC400_1_EVENT		(211)
#define SDEI_SKY1_TZC400_2_EVENT		(212)
#define SDEI_SKY1_TZC400_3_EVENT		(213)

#define SKY1_TZC400_0_INTR		(241)
#define SKY1_TZC400_1_INTR		(244)
#define SKY1_TZC400_2_INTR		(247)
#define SKY1_TZC400_3_INTR		(250)

/* define for CPU RAS event */
#define SDEI_SKY1_CLUSTER_ERR_EVENT 150
#define SDEI_SKY1_CLUSTER_FAULT_EVENT 151
/* cpu0~cpu3 */
#define SDEI_SKY1_COMPLEX_ERR_EVENT(cpu) (153 + (cpu))
#define SDEI_SKY1_COMPLEX_FAULT_EVENT(cpu) (157 + (cpu))
/* core0~core11 */
#define SDEI_SKY1_CORE_ERR_EVENT(cpu) (161 + (cpu))
#define SDEI_SKY1_CORE_FAULT_EVENT(cpu) (173 + (cpu))

/*cluster*/
#define SKY1_CLUSTER_ERR_INTR (0 + MIN_SPI_ID)
#define SKY1_CLUSTER_FAULT_INTR (1 + MIN_SPI_ID)
/* cpu0~cpu3 */
#define SKY1_COMPLEX_ERR_INTR(cpu) (3 + MIN_SPI_ID + (cpu))
#define SKY1_COMPLEX_FAULT_INTR(cpu) (7 + MIN_SPI_ID + (cpu))
/* core0~core11 */
#define SKY1_CORE_ERR_INTR(cpu) (11 + MIN_SPI_ID + (cpu))
#define SKY1_CORE_FAULT_INTR(cpu) (23 + MIN_SPI_ID + (cpu))

/* Priority levels for ARM platforms */
#define PLAT_RAS_PRI			0x10
#define PLAT_SDEI_CRITICAL_PRI		0x60
#define PLAT_SDEI_NORMAL_PRI		0x70

#if SDEI_SUPPORT
#define ARM_G0_IRQ_PROPS(grp) \
       INTR_PROP_DESC(PLAT_SDEI_SGI_PRIVATE, PLAT_SDEI_NORMAL_PRI, \
                      INTR_GROUP0, GIC_INTR_CFG_LEVEL)
#define PLAT_ARM_G0_IRQ_PROPS(grp)	ARM_G0_IRQ_PROPS(grp)
#endif

/* Interrupt handling constants */
#define CSS_G1S_IRQ_PROPS(grp) \
	INTR_PROP_DESC(ARM_IRQ_SEC_SGI_0, GIC_HIGHEST_SEC_PRIORITY, grp, \
			GIC_INTR_CFG_LEVEL)

#define PLAT_ARM_G1S_IRQ_PROPS(grp)	CSS_G1S_IRQ_PROPS(grp)

#define PLAT_ARM_SP_IMAGE_STACK_BASE	(PLAT_SP_IMAGE_NS_BUF_BASE +	\
					 PLAT_SP_IMAGE_NS_BUF_SIZE)

/*******************************************************************************
 * Memprotect definitions
 ******************************************************************************/
/* PSCI memory protect definitions:
 * This variable is stored in a non-secure flash because some ARM reference
 * platforms do not have secure NVRAM. Real systems that provided MEM_PROTECT
 * support must use a secure NVRAM to store the PSCI MEM_PROTECT definitions.
 */
#define PLAT_ARM_MEM_PROT_ADDR		(V2M_FLASH0_BASE + \
					 V2M_FLASH0_SIZE - V2M_FLASH_BLOCK_SIZE)

/*Secure Watchdog Constants */
#define SBSA_SECURE_WDOG_BASE		UL(0x2A480000)
#define SBSA_SECURE_WDOG_TIMEOUT	UL(100)

#define PLAT_ARM_SCMI_CHANNEL_COUNT	1

/* TODO */
#define PLAT_MHUV2_BASE			    UL(0x45400000)

/*
 * Physical and virtual address space limits for MMU in AARCH64
 */
#define PLAT_PHY_ADDR_SPACE_SIZE	(1ULL << 40)
#define PLAT_VIRT_ADDR_SPACE_SIZE	(1ULL << 40)

/* GIC related constants */
#define PLAT_SKY1_GICD_BASE		UL(0x0e010000)
#define PLAT_SKY1_GICD_ITS		UL(0x0e050000)
#define PLAT_SKY1_GICR_BASE		UL(0x0e090000)

#define GICR_AFF(n)	            UL((n * 4) * 0x10000)

#define CIX_SYS_COUNTER_BASE	0x16002000
#define CIX_GENERIC_TIMER_BASE	0x12000000

#define CIX_PMCTL_S5_BASE		UL(0x16000000)
#define ENABLE_TSGEN_100M_CLK   UL(0x018)

#define CIX_IOMUX_S5_BASE       UL(0x16007000)

/* TZC Related Constants */
#define PLAT_CIX_TZC_BASE		UL(0x0C0B0000)
#define TZC400_OFFSET			UL(0x20000)
#define TZC400_BASE(n)			(PLAT_CIX_TZC_BASE + (n * TZC400_OFFSET))

/*******************************************************
 *  index          usage            status
 *
 *	filter-0       noc              used
 *
 *	filter-1       mmhub            used
 *
 *	filter-2       N/A              free
 *
 *	filter-3       N/A              free
 *
 * *****************************************************/
#define PLAT_CIX_TZC_FILTERS	(TZC_400_REGION_ATTR_FILTER_BIT(0) |\
                                 TZC_400_REGION_ATTR_FILTER_BIT(1))

#define TZC400_COUNT			(4)

#define TZC_NSAID_DEFAULT		U(0)

#define PLAT_CIX_TZC_NS_DEV_ACCESS	\
		(TZC_REGION_ACCESS_RDWR(TZC_NSAID_DEFAULT))

/* Define the Access permissions for Secure peripherals to NS_DRAM */
#if ARM_CRYPTOCELL_INTEG
/*
 * Allow Secure peripheral to read NS DRAM when integrated with CryptoCell.
 * This is required by CryptoCell to authenticate BL33 which is loaded
 * into the Non Secure DDR.
 */
#define ARM_TZC_NS_DRAM_S_ACCESS	TZC_REGION_S_RD
#else
#define ARM_TZC_NS_DRAM_S_ACCESS	TZC_REGION_S_NONE
#endif

/*******************************************************************
 *
 * TZC400 regions index defines
 *
 * *****************************************************************/
#define	TZC400_REGION_DEFAULT              U(0x0) /*default region(also named bg region),configued in se-fw*/
#define	TZC400_REGION_1                    U(0x1) /*region 1 ,configured by se-fw*/
#define	TZC400_REGION_2                    U(0x2) /*region 2 ,configured by BL2*/
#define	TZC400_REGION_3                    U(0x3) /*region 2 ,configured by BL2*/
#define	TZC400_REGION_4                    U(0x4) /*region 2 ,configured by BL2*/
#define	TZC400_REGION_5                    U(0x5) /*region 2 ,configured by BL2*/
#define	TZC400_REGION_6                    U(0x6) /*free */
#define	TZC400_REGION_7                    U(0x7) /*free */
#define	TZC400_REGION_8                    U(0x8) /*free */

/*******************************************************************
 *
 * NSAID defines
 *
 * *****************************************************************/

#define	CIX_NSAID_DEFAULT                  U(0x0)
#define	CIX_NSAID_VPU_0                    U(0x1)
#define	CIX_NSAID_VPU_1                    U(0x2)
#define	CIX_NSAID_VPU_2                    U(0x3)
#define	CIX_NSAID_GPU                      U(0x4)
#define	CIX_NSAID_DPU                      U(0x5)
#define	CIX_NSAID_ISP                      U(0x6)
#define	CIX_NSAID_NPU                      U(0x7)
#define	CIX_NSAID_REV0                     U(0x8)
#define	CIX_NSAID_REV1                     U(0x9)
#define	CIX_NSAID_REV2                     U(0xA)
#define	CIX_NSAID_REV3                     U(0xB)
#define	CIX_NSAID_REV4                     U(0xC)
#define	CIX_NSAID_FCH                      U(0xD)
#define	CIX_NSAID_HIFI                     U(0xE)
#define	CIX_NSAID_SFU                      U(0xF)

#define CIX_NSAID_GROUP0                (TZC_REGION_ACCESS_RDWR(CIX_NSAID_DEFAULT) | \
                                         TZC_REGION_ACCESS_RD(CIX_NSAID_VPU_2) | \
										 TZC_REGION_ACCESS_RDWR(CIX_NSAID_FCH) | \
										 TZC_REGION_ACCESS_RDWR(CIX_NSAID_HIFI) | \
										 TZC_REGION_ACCESS_RDWR(CIX_NSAID_SFU) | \
                                         TZC_REGION_ACCESS_RD(CIX_NSAID_DPU) | \
										 TZC_REGION_ACCESS_RDWR(CIX_NSAID_NPU))
#define CIX_NSAID_GROUP1                (0UL)
#define CIX_NSAID_GROUP2                ( TZC_REGION_ACCESS_RDWR(CIX_NSAID_VPU_0) )
#define CIX_NSAID_GROUP3                ( TZC_REGION_ACCESS_RDWR(CIX_NSAID_VPU_1) )
#define CIX_NSAID_GROUP4                ( TZC_REGION_ACCESS_WR(CIX_NSAID_DEFAULT) | \
                                          TZC_REGION_ACCESS_RDWR(CIX_NSAID_VPU_2) | \
                                          TZC_REGION_ACCESS_RDWR(CIX_NSAID_GPU) | \
                                          TZC_REGION_ACCESS_RDWR(CIX_NSAID_DPU) \
                                        )
#define CIX_NSAID_GROUP5                ( TZC_REGION_ACCESS_WR(CIX_NSAID_DEFAULT) | \
                                          TZC_REGION_ACCESS_RDWR(CIX_NSAID_GPU) \
                                        )

#define CIX_REGION_NULL                 (0x77777777UL)
#define CIX_REGION_TEMP                 (0x66666666UL)

/***********************************************************************************************************************
 * NSAID defines as follow
 *
 * component:  VPU0  VPU1  VPU2  GPU  DPU  ISP  NPU
 *
 * NSAID    :  1     2     3     4    5    6    7
 *
 * tzc400 regions defines as follow
 *
 *
 * region-start    region-end     permmit-secure       permit-NSAID       Comment                  init-config firmware
 *
 * 0x80000000      0x824FFFFF     R/W                  N/A                secure access only       SE-FW
 *
 * 0xBDE00000      0xBE5FFFFF     R/W                  1-RW               vpu-firmware code        BL2
 *
 * 0xBE600000      0xBFDFFFFF     R/W                  2-RW               data after de-cipher     BL2
 *
 * 0xBFE00000      0xCCDFFFFF     N/A                  0-WO               yuv data                 BL2
 *                                                     3-RW
 *                                                     4-RW
 *                                                     5-RW
 *
 * 0xBCE0000       0xBDDFFFFF     N/A                  0-WO               GPU internal data        BL2
 *                                                     4-RW
 *
 *************************************************************************************************************************/
#define SKY1_TZC_REGIONS_DEF	\
	/* begin          end                        secure-perrmit            non-secure-permit          region-idx   */ \
	{0x00000000,      0xFFFFFFFFFF,              TZC_REGION_S_RDWR,        CIX_NSAID_GROUP0,          TZC400_REGION_DEFAULT}, \
	{0x80000000,      0x824FFFFF,                TZC_REGION_S_RDWR,        CIX_NSAID_GROUP1,          TZC400_REGION_1}, \
	{CIX_REGION_NULL, CIX_REGION_NULL,           0,                        0,                         TZC400_REGION_2}

#define SKY1_TZC_REGIONS_DEF_DEBUG	\
	/* begin          end                        secure-perrmit            non-secure-permit          region-idx   */ \
	{0x00000000,      0xFFFFFFFFFF,              TZC_REGION_S_RDWR,        CIX_NSAID_GROUP0,          TZC400_REGION_DEFAULT}, \
	{0x80000000,      0x824FFFFF,                TZC_REGION_S_RDWR,        CIX_NSAID_GROUP1,          TZC400_REGION_1}, \
        {0x0,		  0x7FFFFFFF,                TZC_REGION_S_NONE,        CIX_NSAID_GROUP1,          TZC400_REGION_2}, \
	{SKY1_DRAM2_BASE, 0xFFFFFFFFFF,              TZC_REGION_S_NONE,        CIX_NSAID_GROUP1,          TZC400_REGION_4}, \
	{CIX_REGION_TEMP, 0x7FFFFFFFFF,              TZC_REGION_S_NONE,        CIX_NSAID_GROUP1,          TZC400_REGION_3}, \
	{CIX_REGION_NULL, CIX_REGION_NULL,           0,                        0,                         TZC400_REGION_5}

#define SKY1_TZC_REGIONS_FULL    \
	/* begin          end                        secure-perrmit            non-secure-permit          region-idx   */ \
	{0x00000000,      0xFFFFFFFFFF,              TZC_REGION_S_RDWR,        CIX_NSAID_GROUP0,          TZC400_REGION_DEFAULT}, \
	{0x80000000,      0x824FFFFF,                TZC_REGION_S_RDWR,        CIX_NSAID_GROUP1,          TZC400_REGION_1}, \
	{0xBDE00000,      0xBE5FFFFF,                TZC_REGION_S_RDWR,        CIX_NSAID_GROUP2,          TZC400_REGION_2}, \
	{0xBE600000,      0xBFDFFFFF,                TZC_REGION_S_RDWR,        CIX_NSAID_GROUP3,          TZC400_REGION_3},	\
	{0xBFE00000,      0xCCDFFFFF,                TZC_REGION_S_NONE,        CIX_NSAID_GROUP4,          TZC400_REGION_4}, \
	{0xBCE00000,      0xBDDFFFFF,                TZC_REGION_S_NONE,        CIX_NSAID_GROUP5,          TZC400_REGION_5}, \
	{CIX_REGION_NULL, CIX_REGION_NULL,           0,                        0,                         TZC400_REGION_6}

/* virtual address used by dynamic mem_protect for chunk_base */
#define PLAT_ARM_MEM_PROTEC_VA_FRAME	UL(0xc0000000)

/* UART related constants */
#ifdef CIX_BOARD_FPGA
#define PLAT_CIX_BOOT_UART_BASE        UL(0x40d0000)
#define PLAT_CIX_RUN_UART_BASE         UL(0x40d0000)
#define PLAT_CIX_CRASH_UART_BASE       UL(0x40d0000)
#define CIX_CONSOLE_BAUDRATE           115200
#define PLAT_CIX_BOOT_UART_CLK_IN_HZ   5000000
#define PLAT_CIX_RUN_UART_CLK_IN_HZ    5000000
#define PLAT_CIX_CRASH_UART_CLK_IN_HZ  5000000
#elif defined(CIX_BOARD_EMU)
#define PLAT_CIX_BOOT_UART_BASE        UL(0x40d0000)
#define PLAT_CIX_RUN_UART_BASE         UL(0x40d0000)
#define PLAT_CIX_CRASH_UART_BASE       UL(0x40d0000)
#define CIX_CONSOLE_BAUDRATE           921600
#define PLAT_CIX_BOOT_UART_CLK_IN_HZ   25000000
#define PLAT_CIX_RUN_UART_CLK_IN_HZ    25000000
#define PLAT_CIX_CRASH_UART_CLK_IN_HZ  25000000
#else
#define PLAT_CIX_BOOT_UART_BASE        UL(0x40d0000)
#define PLAT_CIX_RUN_UART_BASE         UL(0x40d0000)
#define PLAT_CIX_CRASH_UART_BASE       UL(0x40d0000)
#define CIX_CONSOLE_BAUDRATE           115200
#define PLAT_CIX_BOOT_UART_CLK_IN_HZ   100000000
#define PLAT_CIX_RUN_UART_CLK_IN_HZ    100000000
#define PLAT_CIX_CRASH_UART_CLK_IN_HZ  100000000
#endif

#define PLAT_CIX_FLASH_IMAGE_BASE			UL(0x83000000)
#define PLAT_CIX_FLASH_IMAGE_MAX_SIZE			0x01000000

#define PLAT_CIX_SPI_NOR_FIP_IMAGE_BASE			0x288000
#define PLAT_CIX_SPI_NOR_FIP_UEFI_IMAGE_BASE		0x3f0000
#ifdef CIX_BOARD_FPGA
#define PLAT_CIX_SPI_NOR_FLASH_IMAGE_MAX_SIZE			0x300000
#elif defined(CIX_BOARD_EMU)
#define PLAT_CIX_SPI_NOR_FLASH_IMAGE_MAX_SIZE			0x500000
#else
#define PLAT_CIX_SPI_NOR_FLASH_IMAGE_MAX_SIZE			0x300000
#endif

#define MAX_IO_MTD_DEVICES		U(2)
#define MAX_FIP_DEVICES		U(2)

#define SKY1_PBL_SIZE 	UL(0x200000)

 /* BL2 specific defines */
#define BL2_SIZE 		SKY1_PBL_SIZE
#define BL2_BASE 		SKY1_DRAM1_BASE
#define BL2_LIMIT 		(BL2_BASE + BL2_SIZE)

  /* BL2 & SE_FW share space */
#define BL2_SE_FW_SHARE_SIZE	0x1000
#define BL2_SE_FW_SHARE_BASE	(BL2_BASE + 0x200000 - BL2_SE_FW_SHARE_SIZE)

 /* BL31 specific defines */
#define BL31_SIZE 		UL(0x200000)
#define BL31_BASE 		BL2_LIMIT
#define BL31_LIMIT 		(BL31_BASE + BL31_SIZE)

 /* BL32 specific defines */
#define BL32_SIZE 		UL(0x2000000)
#ifndef SPD_none
#define BL32_BASE 		SKY1_TEE_OS_BASE
#define BL32_LIMIT 		(BL32_BASE + BL32_SIZE)

#define TSP_SEC_MEM_BASE	BL32_DRAM_BASE
#define TSP_SEC_MEM_SIZE	(BL32_LIMIT - BL32_BASE + 1)
#endif /* SPD_none */

 /* BL33 specific defines */
#define BL33_SIZE 		UL(0x400000)
#define BL33_BASE 		UL(0x84400000)
#define BL33_LIMIT 		UL(0x84C00000)

/*
 * This macro defines the deepest retention state possible. A higher state
 * id will represent an invalid or a power down state.
 */
#define PLAT_MAX_RET_STATE		U(1)

/*
 * This macro defines the deepest power down states possible. Any state ID
 * higher than this is invalid.
 */
#define PLAT_MAX_OFF_STATE		U(2)

/*
 * Some data must be aligned on the biggest cache line size in the platform.
 * This is known only to the platform as it might have a combination of
 * integrated and external caches.
 */
#define CIX_CACHE_WRITEBACK_SHIFT	6
#define CACHE_WRITEBACK_GRANULE		(U(1) << CIX_CACHE_WRITEBACK_SHIFT)

/* Local power state for power domains in Run state. */
#define CIX_LOCAL_STATE_RUN	U(0)
/* Local power state for retention. Valid only for CPU power domains */
#define CIX_LOCAL_STATE_RET	U(1)
/* Local power state for OFF/power-down. Valid for CPU and cluster power
   domains */
#define CIX_LOCAL_STATE_OFF	U(2)

#define MAX_IO_DEVICES			3
#define MAX_IO_HANDLES			4

/* Load address of Non-Secure Image */
#define PLAT_CIX_NS_IMAGE_BASE		BL33_BASE

/* Non-volatile counters */
/* TODO */
#define SOC_TRUSTED_NVCTR_BASE		0x7fe70000
#define TFW_NVCTR_BASE			(SOC_TRUSTED_NVCTR_BASE + 0x0000)
#define TFW_NVCTR_SIZE			4
#define NTFW_CTR_BASE			(SOC_TRUSTED_NVCTR_BASE + 0x0004)
#define NTFW_CTR_SIZE			4

#define ARM_BL_REGIONS			7
#define MAX_MMAP_REGIONS		(PLAT_ARM_MMAP_ENTRIES +	\
					 ARM_BL_REGIONS)

#define POSTCODE_REGISTER_BASE		UL(0x85000000)
#define POSTCODE_BL2_RANGE			U(0xb000)
#define POSTCODE_BL31_RANGE			U(0xc000)

#ifndef CIX_BOARD_EVB
#define DEBUG_REGISTER_PLAT_CONTROL	UL(0x05040100)
#define PLAT_BL_MODE_POS		(20U)
#define PLAT_BL_MODE_MASK		(1 << PLAT_BL_MODE_POS)

#define PLAT_REG_BASE                   (DEBUG_REGISTER_PLAT_CONTROL)
#define PLAT_IP_INFO                    (PLAT_REG_BASE + 0x04U)
#define PLAT_IP_INFO_CLUSTER_POS        (28U)
#define PLAT_IP_INFO_CLUSTER_MSK        (7 << 28)
#endif

#ifdef CIX_BOARD_FPGA
#define CIX_UART_CLK_DEBUG_REGISTER 1
#elif defined(CIX_BOARD_EMU)
#define CIX_UART_CLK_DEBUG_REGISTER 0
#else
#define CIX_UART_CLK_DEBUG_REGISTER 0
#endif

#ifdef IMAGE_BL2
#define POSTCODE_RANGE POSTCODE_BL2_RANGE
#endif

#ifdef IMAGE_BL31
#define POSTCODE_RANGE POSTCODE_BL31_RANGE
#endif

#define BL2_SHMEM_VER_STR	UL(0x83E01080)
#define BL31_SHMEM_VER_STR	UL(0x83E01100)
#define SHMEM_VER_STR_LEN	128

#if RAM_LOG_SUPPORT
#define RAM_LOG_BL2_ADDR 0x83DBC000
#define RAM_LOG_BL2_SIZE 0x4000
#define RAM_LOG_BL31_ADDR 0x83DAC000
#define RAM_LOG_BL31_SIZE 0x4000
#endif

#endif /* PLATFORM_DEF_H */
