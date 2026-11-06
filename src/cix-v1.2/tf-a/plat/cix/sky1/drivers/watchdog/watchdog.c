/*
 * Copyright (c) 2024, Cix Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <platform_def.h>
#include <lib/mmio.h>
#include "watchdog.h"

/*PMCTRL_S5 base */
#define PMCTRL_S5_BASE		0x16000000	/* PMCTRL_S5 base */
#define WDT_TM_OPTION_OFFSET	0x604		/* Watchdog timeout option select */
#define WDT_TO_PMCTRL_S5	0x0		/* Second interrupt to pmctrl_s5 */
#define WDT_TO_CSU_SE		0x1		/* Second interrupt to cus_se */
#define WDT_TO_CSU_PM		0x2		/* Second interrupt to csu_pm */

/* Control frame registers */
#define SKY1_WDT_CRTL_BASE	0x16003000	/* Watchdog control base register */
#define SKY1_WDT_WCS_RW         0x000		/* Watchdog control and status register */
#define SKY1_WDT_WCS_ENABLE     BIT(0)		/* Watchdog Enable */
#define SKY1_WDT_WCS_WS0        BIT(1)		/* Watchdog Signal0 */
#define SKY1_WDT_WCS_WS1        BIT(2)		/* Watchdog Signal1 */

#define SKY1_WDT_WOR_LOW_RW     0x008		/* Watchdog offset register */
#define SKY1_WDT_WOR_HIGH_RW    0x00C		/* Watchdog offset register */
#define SKY1_WDT_WCV_LOW_RW     0x010		/* Watchdog compare value */
#define SKY1_WDT_WCV_HIGH_RW    0x014		/* Watchdog compare value */

/* Refresh frame registers */
#define SKY1_WDT_WRR_BASE	0x16008000	/* watchdog refresh base register */
#define SKY1_WDT_WRR_RW         0x000		/* Watchdog refresh register */
#define SKY1_WDT_WRR_REFRESH    BIT(0)		/* Explicit watchdog refresh ocurrs */
#define SKY1_WDT_W_IID_RO       0xFCC		/* W_IIDR is a 32-bit read-only register */

#define SKY1_WDT_MAX_TIME       0x44B82
#define SKY1_WDT_DEFAULT_TIME   60		/* in seconds */
#define WDG_FEED_MOMENT_ADJUST  1

#define MSECS_TO_JIFFIES	1000
#define WDOG_SEC_TO_COUNT	1000000000	/* sky1 global watchdog frequency 1G */

void plat_watchdog_set_timeout(unsigned int timeout_sec)
{
	uint64_t wor_val;
	uint32_t wor_low_val, wor_high_val;
	uint32_t enable_ctrl;
	uint32_t val32;

	enable_ctrl = mmio_read_32(SW_USED_REG5);
	enable_ctrl = (enable_ctrl & BIT(5));
	if (!enable_ctrl)
		return;

	/* Keep Watchdog Disabled */
	val32 = mmio_read_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WCS_RW);
	val32 &= ~(SKY1_WDT_WCS_ENABLE | SKY1_WDT_WCS_WS0 | SKY1_WDT_WCS_WS1);
	mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WCS_RW, val32);

	/* Strip the old watchdog Time-Out value */
	mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_LOW_RW, 0);
	mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_HIGH_RW, 0);

	/* Set the watchdog's Time-Out value */
	wor_low_val = mmio_read_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_LOW_RW);
	wor_high_val = mmio_read_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_HIGH_RW);
	wor_val = wor_high_val;
	wor_val = wor_val << 32 | wor_low_val;

	wor_val += (uint64_t)(timeout_sec)*WDOG_SEC_TO_COUNT;
	wor_low_val = (uint32_t)wor_val;
	wor_high_val = wor_val >> 32;
	mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_LOW_RW, wor_low_val);
	mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_HIGH_RW, wor_high_val);

	/* enable the watchdog */
	val32 = mmio_read_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WCS_RW);
	val32 |= SKY1_WDT_WCS_ENABLE;
	mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WCS_RW, val32);

	val32 = mmio_read_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WCS_RW);
}

/* Initialize & enable watchdog */
void plat_watchdog_init(void)
{
	uint32_t val32;
	uint32_t enable_ctrl;
	uint64_t wor_val;
	uint32_t wor_low_val, wor_high_val;

	enable_ctrl = mmio_read_32(SW_USED_REG5);
	enable_ctrl = (enable_ctrl & BIT(5));
	if (enable_ctrl) {
		/* Keep Watchdog Disabled */
		val32 = mmio_read_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WCS_RW);
		val32 &= ~(SKY1_WDT_WCS_ENABLE|SKY1_WDT_WCS_WS0|SKY1_WDT_WCS_WS1);
		mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WCS_RW, val32);

		/* Strip the old watchdog Time-Out value */
		mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_LOW_RW, 0);
		mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_HIGH_RW, 0);

		/* Set the watchdog's Time-Out value */
		wor_low_val = mmio_read_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_LOW_RW);
		wor_high_val = mmio_read_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_HIGH_RW);
		wor_val = wor_high_val;
		wor_val = wor_val << 32 | wor_low_val;

		wor_val += (uint64_t)(SKY1_WDT_DEFAULT_TIME) * WDOG_SEC_TO_COUNT;
		wor_low_val = (uint32_t)wor_val;
		wor_high_val = wor_val >> 32;
		mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_LOW_RW, wor_low_val);
		mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WOR_HIGH_RW, wor_high_val);


		/* config watchdog second timetout to csu_se */
		val32 = mmio_read_32(PMCTRL_S5_BASE + WDT_TM_OPTION_OFFSET);
		val32 = WDT_TO_CSU_SE;
		mmio_write_32(PMCTRL_S5_BASE + WDT_TM_OPTION_OFFSET, val32);

		/* enable the watchdog */
		val32 = mmio_read_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WCS_RW);
		val32 |= SKY1_WDT_WCS_ENABLE;
		mmio_write_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WCS_RW, val32);

		val32 = mmio_read_32(SKY1_WDT_CRTL_BASE + SKY1_WDT_WCS_RW);
		VERBOSE("global watchdog is enabled!\n");
	} else {
		VERBOSE("global watchdog is disabled!\n");
	}
}
