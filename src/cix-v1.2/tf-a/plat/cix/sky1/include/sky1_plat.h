/*
 * Copyright (c) 2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SKY1_PLAT_H
#define SKY1_PLAT_H

void sky1_bl31_common_platform_setup(void);
void *sky1_get_scmi_handle(void);
void cix_sky1_qos_setting_init(void);
void cix_sky1_qos_setting_dump(void);
void cix_sky1_nsaid_setting_init(void);
void cix_sky1_mmhub_config_save(void);
void cix_sky1_mmhub_config_restore(void);
void sky1_ras_setup_resume(void);
void sky1_ras_setup(void);
#ifdef SKY1_CPU_RAS_SUPPORT
void sky1_ras_dsu_cache_resume(void);
#endif
int sky1_set_cpu_boost_trigger(int set);

#endif /* SKY1_PLAT_H */
