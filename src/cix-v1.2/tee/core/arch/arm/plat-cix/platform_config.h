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
 *                 platform_config.h
 *
 * Filename      : platform_config.h
 * Programmer(s) : China Security team
 * Author        : Shuai Fengyun
 * Mail          : Abel.Shuai@cixcomputing.com
 * Create Time   : 2022-09-05 15:38:27
 **************************************************************************************
 */

#ifndef MOUDLE_PLATFORM_CONFIG_H_
#define MOUDLE_PLATFORM_CONFIG_H_




/*
 *******************************************************************************
 *                                INCLUDE FILES
 *******************************************************************************
*/
#include <mm/generic_ram_layout.h>
#include <stdint.h>






/*
 *******************************************************************************
 *                  MACRO DEFINITION USED ONLY BY THIS MODULE
 *******************************************************************************
*/
#define PLAT_CIX_UART_BASE        UL(0x40d0000)	/* Base address of UART */

#define GICD_BASE        0x0E010000

#define TE_REGS_BASE            (0x05050000UL)    // 0xA0040000 - 0xA0000000 + 0x05010000
#define TE_REGS_SIZE            0x00020000U
#define TE_HOST_ID              1
#define TE_IRQ_ID               473

#define FIRMWARE_VERSION_STR_BASE_ADDR UL(0x83E00000)
#define FIRMWARE_VERSION_STR_SIZE UL(0x2000)
#define TEE_SHMEM_VER_STR	UL(0x83E01200)
#define SHMEM_VER_STR_LEN	128
/*
 *******************************************************************************
 *                STRUCTRUE DEFINITION USED ONLY BY THIS MODULE
 *******************************************************************************
*/




/*
 *******************************************************************************
 *                      VARIABLES SUPPLIED BY THIS MODULE
 *******************************************************************************
*/





/*
 *******************************************************************************
 *                      FUNCTIONS SUPPLIED BY THIS MODULE
 *******************************************************************************
*/




















#endif  /* MOUDLE_platform_config_H*/
