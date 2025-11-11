/**************************************************************************************/
/*                          COPYRIGHT INFORMATION                                     */
/*     Copyright 2024 Cix Technology Group Co., Ltd.                             */
/*     All Rights Reserved.                                                           */
/*                                                                                    */
/*     The following programs are the sole property of Cix Technology Group      */
/*     Co., Ltd., and contain its proprietary and confidential information.           */
/*                                                                                    */
/*                                                                                    */
/**************************************************************************************/
/*
 **************************************************************************************
 *
 *                 main.c
 *
 * Filename      : main.c
 * Programmer(s) : China Security team
 * Author        : Shuai Fengyun
 * Mail          : Abel.Shuai@cixcomputing.com
 * Create Time   : 2022-09-05 15:58:38
 **************************************************************************************
 */

#define MOUDLE_CIX_MAIN_C_


/*
 *******************************************************************************
 *                                INCLUDE FILES
 *******************************************************************************
*/
#include <arm.h>
#include <console.h>
#include <io.h>
#include <kernel/interrupt.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <platform_config.h>
#include <stdint.h>
#include <drivers/cix_uart_pl011.h>
#include <drivers/gic.h>
#include <drivers/ipc.h>
#include <kernel/tee_common_otp.h>
#include <kernel/linker.h>
#include <string_ext.h>

/*
 *******************************************************************************
 *                         FUNCTIONS SUPPLIED BY THIS MODULE
 *******************************************************************************
*/
void cix_postcode_debug(unsigned int val);
void set_cix_version_str(void);

#if defined(CFG_GIC)
void main_init_gic(void);
void main_secondary_init_gic(void);
#endif

/*
 *******************************************************************************
 *                          VARIABLES SUPPLIED BY THIS MODULE
 *******************************************************************************
*/


/*
 *******************************************************************************
 *                          FUNCTIONS USED ONLY BY THIS MODULE
 *******************************************************************************
*/


/*
 *******************************************************************************
 *                          VARIABLES USED ONLY BY THIS MODULE
 *******************************************************************************
*/

static struct cix_pl011_data console_data;
static struct gic_data gic_data;

/* Register TEE UART memory region */
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, PLAT_CIX_UART_BASE, PL011_REG_SIZE);
register_phys_mem(MEM_AREA_IO_NSEC, FIRMWARE_VERSION_STR_BASE_ADDR, FIRMWARE_VERSION_STR_SIZE);

#if defined(CFG_TZ_TF_A_SHM)
register_phys_mem(MEM_AREA_RAM_SEC, TZ_TFA_SHM_BASE, TZ_TFA_SHM_SIZE);
#endif

#if defined(CFG_TEE_KEY_META_SHM)
register_phys_mem(MEM_AREA_RAM_SEC, TEE_KEY_META_SHM_BASE, TEE_KEY_META_SHM_SIZE);
#endif

register_phys_mem(MEM_AREA_RAM_NSEC, 0x85F00000, 0x100000);


/*
 *******************************************************************************
 *                               FUNCTIONS IMPLEMENT
 *******************************************************************************
*/
void console_init(void)
{
	/** 1) AP UART was init in blx, so in TEE os just only need to register it */
    cix_pl011_register(&console_data, PLAT_CIX_UART_BASE);
#ifndef CONFIG_CIX_DEBUG
    trace_set_level(TRACE_ERROR);
#endif
	/** 2) Register cix uart handle into console struct */
    register_serial_console(&console_data.chip);
    EMSG("Cix uart register successful\n");
}

void set_cix_version_str(void)
{
	char* dst = NULL;
	dst = phys_to_virt(TEE_SHMEM_VER_STR, MEM_AREA_IO_NSEC, SHMEM_VER_STR_LEN);
	strlcpy(dst, core_cix_tee_str, SHMEM_VER_STR_LEN);
}

#if defined(CFG_CIX_HUK_ENABLE)
TEE_Result tee_otp_get_hw_unique_key(struct tee_hw_unique_key *hwkey)
{
	uint32_t len = 0U;
	uint32_t ta_flag = 0U;
	uint8_t key_data[32] = {0};
	uint32_t key_len = 32;
	cix_get_key_info(KEY_ID_DEVICE_KEY, KM_DEVICE_KEY_SIZE, hwkey->data, &len);

	return TEE_SUCCESS;
}
#endif

#if defined(CFG_GIC)
void itr_core_handler(void)
{
	gic_it_handle(&gic_data);
}

register_phys_mem_pgdir(MEM_AREA_IO_SEC, GICD_BASE, CORE_MMU_PGDIR_SIZE);

void main_init_gic(void)
{
    paddr_t gicd_base = GICD_BASE;

#if defined(CFG_WITH_ARM_TRUSTED_FW)
    /* On ARMv8, GIC configuration is initialized in ARM-TF */
    gic_init_base_addr(&gic_data, 0, gicd_base);
#else
    /* Initialize GIC */
    gic_init(&gic_data, 0, gicd_base);
#endif
    itr_init(&gic_data.chip);

#ifdef CFG_FT_TZC
    register_tzc_test();
#endif
}

#if CFG_TEE_CORE_NB_CORE > 1
void main_secondary_init_gic(void)
{
    gic_cpu_init(&gic_data);
}
#endif
#endif

#ifdef CFG_TE_DRIVER
register_phys_mem_pgdir(MEM_AREA_IO_SEC, TE_REGS_BASE, TE_REGS_SIZE);

/*
 * Collect the configurations for the trust engine, including
 * base address, irq line number, and host id.
 */
void platform_get_te_configs(void **base, int *irq, int *host)
{
	if (base)
		*base = phys_to_virt(TE_REGS_BASE, MEM_AREA_IO_SEC, DEFAULT_PHY_TO_VIRT_SIZE);
	if (irq)
		*irq  = TE_IRQ_ID;
	if (host)
		*host = TE_HOST_ID;
}
#endif

TEE_Result tee_otp_get_ta_enc_key(uint32_t key_type __maybe_unused,
					 uint8_t *buffer, size_t len)
{
	size_t key_len = 32;
	TEE_Result ret;

	ret = cix_get_key_info(KEY_ID_MODEL_KEY, key_len, buffer, &len);
	EMSG_RAW("Cix_en_key: %d\n", key_len);
	DHEXDUMP(buffer, 32);

	return TEE_SUCCESS;
}



