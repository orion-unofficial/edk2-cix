/*
 * Copyright (c) 2018-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <platform_def.h>

#include <common/debug.h>
#include <cix_uart.h>
#include <drivers/console.h>

#pragma weak cix_console_runtime_init
#pragma weak cix_console_runtime_end

/*******************************************************************************
 * Functions that set up the console
 ******************************************************************************/
static console_t cix_boot_console;
static console_t cix_runtime_console;

/* Initialize the console to provide early debug support */
void __init cix_console_boot_init(void)
{
#if CIX_UART_CLK_DEBUG_REGISTER
	int clk_in_hz = (mmio_read_32(DEBUG_REGISTER_PLAT_CONTROL) & 0x1ffff) * 1000;
#else
	int clk_in_hz = PLAT_CIX_BOOT_UART_CLK_IN_HZ;
#endif
	int rc = console_cix_uart_register(PLAT_CIX_BOOT_UART_BASE,
					clk_in_hz,
					CIX_CONSOLE_BAUDRATE,
					&cix_boot_console);
	if (rc == 0) {
		/*
		 * The crash console doesn't use the multi console API, it uses
		 * the core console functions directly. It is safe to call panic
		 * and let it print debug information.
		 */
		panic();
	}

	console_set_scope(&cix_boot_console, CONSOLE_FLAG_BOOT);
#ifndef CONFIG_CIX_DEBUG
	tf_log_set_max_level(LOG_LEVEL_NOTICE);
#endif
	console_switch_state(CONSOLE_FLAG_BOOT);
}

void cix_console_boot_end(void)
{
	console_flush();
	(void)console_unregister(&cix_boot_console);
}

/* Initialize the runtime console */
void cix_console_runtime_init(void)
{
	uint32_t log_ctl;

#if CIX_UART_CLK_DEBUG_REGISTER
	int clk_in_hz = (mmio_read_32(DEBUG_REGISTER_PLAT_CONTROL) & 0x1ffff) * 1000;
#else
	int clk_in_hz = PLAT_CIX_BOOT_UART_CLK_IN_HZ;
#endif
	int rc = console_cix_uart_register(PLAT_CIX_RUN_UART_BASE,
					clk_in_hz,
					CIX_CONSOLE_BAUDRATE,
					&cix_runtime_console);
	if (rc == 0)
		panic();

	console_set_scope(&cix_runtime_console, CONSOLE_FLAG_RUNTIME);

	/*in runtime, default notice log will print*/
	log_ctl = mmio_read_32(SW_USED_REG5);
	log_ctl = (log_ctl >> 0xf) & 0x1;

	if (log_ctl)
		tf_log_set_max_level(LOG_LEVEL);
	else
		tf_log_set_max_level(LOG_LEVEL_NOTICE);
}

void cix_console_runtime_end(void)
{
	console_flush();
}

void cix_console_print_logo()
{
	printf("#################################################################################\n");
	printf("#                                           ,----,                              #\n");
	printf("#                                         ,/   .`|                              #\n");
	printf("#      ,----..                          ,`   .'  :                    ,---,     #\n");
	printf("#     /   /   \\  ,--,                 ;     ;    /                  ,--.' |     #\n");
	printf("#    |   :     :,--.'|              .'___,/    ,'                   |  |  :     #\n");
	printf("#    .   |  ;. /|  |,     ,--,  ,--,|    :     |                    :  :  :     #\n");
	printf("#    .   ; /--` `--'_     |'. \\/ .`|;    |.';  ;   ,---.     ,---.  :  |  |,--. #\n");
	printf("#    ;   | ;    ,' ,'|    '  \\/  / ;`----'  |  |  /     \\   /     \\ |  :  '   | #\n");
	printf("#    |   : |    '  | |     \\  \\.' /     '   :  ; /    /  | /    / ' |  |   /' : #\n");
	printf("#    .   | '___ |  | :      \\  ;  ;     |   |  '.    ' / |.    ' /  '  :  | | | #\n");
	printf("#    '   ; : .'|'  : |__   / \\  \\  \\    '   :  |'   ;   /|'   ; :__ |  |  ' | : #\n");
	printf("#    '   | '/  :|  | '.'|./__; ;    \\   ;   |.' '   |  / |'   | '.'||  :  :_:,' #\n");
	printf("#    |   :    / ;  :    ;|   :/\\  \\ ;   '---'   |   :    ||   :    :|  | ,'     #\n");
	printf("#    \\    \\ .'  |  ,   / `---'  `--`             \\   \\  /  \\   \\   / `--''      #\n");
	printf("#     `---`      ---`-'                           `----'     `----'             #\n");
	printf("#################################################################################\n");
}