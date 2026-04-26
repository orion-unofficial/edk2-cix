/*
 * Copyright (c) 2023, CIX Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <arch_helpers.h>
#include <common/debug.h>
#include <common/runtime_svc.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>
#include <lib/psci/psci.h>
#include <lib/spinlock.h>
#include <platform_def.h>
#include <plat/common/platform.h>
#include <sky1_pdc.h>

#define SKY1_PDC_BASE			0x1600020c
#define SKY1_SIP_CONFIG_GPD_SET_WAKE	0x02
#define MAX_HW_IRQ_NUM			U(600)
#define S5_GPIO_NUM			3

unsigned int gpio_wake_en[S5_GPIO_NUM];

void sky1_set_wakeup_enable(unsigned int on, unsigned int wakeup_bit)
{
	if (on)
		mmio_setbits_32(SKY1_PDC_BASE, wakeup_bit);
	else
		mmio_clrbits_32(SKY1_PDC_BASE, wakeup_bit);
}

void sky1_gpio_wakeup_enable(unsigned int hwirq, unsigned int on, unsigned int s5_wakeup_bit, unsigned int gpio_wake_bit)
{
	if (on) {
		mmio_setbits_32(SKY1_PDC_BASE, s5_wakeup_bit);
		gpio_wake_en[hwirq - S5_GPIO_U0] = gpio_wake_bit;
	} else
		mmio_clrbits_32(SKY1_PDC_BASE, s5_wakeup_bit);
}

void sky1_pdc_set_wake(unsigned int hwirq, unsigned int on, unsigned int gpio_wake_bit)
{
	if (hwirq >= MAX_HW_IRQ_NUM)
		return;

	switch (hwirq) {
	case USB_C_SSP_0_HOST_IRQ:
		sky1_set_wakeup_enable(on, USB_C_SSP_0_HOST_WKUP);
		break;
	case USB_C_SSP_1_HOST_IRQ:
		sky1_set_wakeup_enable(on, USB_C_SSP_1_HOST_WKUP);
		break;
	case USB_C_SSP_2_HOST_IRQ:
		sky1_set_wakeup_enable(on, USB_C_SSP_2_HOST_WKUP);
		break;
	case USB_C_SSP_3_HOST_IRQ:
		sky1_set_wakeup_enable(on, USB_C_SSP_3_HOST_WKUP);
		break;
	case USB_SSP_0_HOST_IRQ:
		sky1_set_wakeup_enable(on, USB_SSP_0_HOST_WKUP);
		break;
	case USB_SSP_1_HOST_IRQ:
		sky1_set_wakeup_enable(on, USB_SSP_1_HOST_WKUP);
		break;
	case USB2_0_HOST_IRQ:
		sky1_set_wakeup_enable(on, USB2_0_HOST_WKUP);
		break;
	case USB2_1_HOST_IRQ:
		sky1_set_wakeup_enable(on, USB2_1_HOST_WKUP);
		break;
	case USB2_2_HOST_IRQ:
		sky1_set_wakeup_enable(on, USB2_2_HOST_WKUP);
		break;
	case USB2_3_HOST_IRQ:
		sky1_set_wakeup_enable(on, USB2_3_HOST_WKUP);
		break;
	case S5_GPIO_U0:
	case S5_GPIO_U1:
	case S5_GPIO_U2:
		sky1_gpio_wakeup_enable(hwirq, on, S5_GPIO_WKUP, gpio_wake_bit);
		break;
	default:
		break;
	}
}

int sky1_pdc_handler(u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4)
{
	switch(x1) {
	case SKY1_SIP_CONFIG_GPD_SET_WAKE:
		sky1_pdc_set_wake(x2, x3, x4);
		break;
	default:
		return SMC_UNK;
	}

	return 0;
}
