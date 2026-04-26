#ifndef PLAT_PWRC_H
#define PLAT_PWRC_H

#include <drivers/arm/css/css_scp.h>
#include <lib/psci/psci.h>
#include <assert.h>

#define CORE_PWR_STATE(state) \
	((state)->pwr_domain_state[MPIDR_AFFLVL0])
#define CLUSTER_PWR_STATE(state) \
	((state)->pwr_domain_state[MPIDR_AFFLVL1])
#define SYSTEM_PWR_STATE(state) \
	((state)->pwr_domain_state[PLAT_MAX_PWR_LVL])

void sky1_enter_core_idle(unsigned int cluster, unsigned int core);
int sky1_test_pwrdn_allcores(unsigned int cluster, unsigned int core);
int sky1_test_ap_suspend_flag(void);
void sky1_enter_ap_suspend(unsigned int cluster, unsigned int core);
void sky1_enter_cluster_idle(unsigned int cluster, unsigned int core);
void sky1_enable_pdc(unsigned int cluster);
void sky1_set_sec_entrypoint(void);
void sky1_powerup_core(unsigned int cluster, unsigned int core);
void sky1_powerdn_core(unsigned int cluster, unsigned int core);
void sky1_powerup_cluster(unsigned int cluster, unsigned int core);

void sky1_powerdn_cluster(unsigned int cluster, unsigned int core);
void sky1_set_el3_to_el1(void);
int cluster_is_powered_on(unsigned int cluster);
void sky1_gicv2_cpuif_disable(void);


void plat_cpu_standby(plat_local_state_t cpu_state);
void plat_scp_on(u_register_t mpidr);
void plat_power_down_common(const psci_power_state_t *target_state);

#endif
