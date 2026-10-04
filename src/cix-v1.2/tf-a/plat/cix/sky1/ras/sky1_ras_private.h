/*
 * Copyright 2025 Cix Technology Group Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SKY1_RAS_H
#define SKY1_RAS_H

#define INVALID_HWID ULONG_MAX

#define INTR_LEVEL 0x00
#define INTR_EDGE 0x02

#define RAS_ARGS_DEF(name, records, cookie, trigger_type, mpidr)
#define RAS_ARGSID_DEF(name, records, cookie, trigger_type, mpidr, id)

#if SKY1_IDM_RAS_SUPPORT
#define IDM_INTR_LIST                                                          \
	RAS_ARGS_DEF(IDM_MMHUB0, idm_records, NULL, INTR_LEVEL, INVALID_HWID), \
		RAS_ARGS_DEF(IDM_MMHUB1, idm_records, NULL, INTR_LEVEL,        \
			     INVALID_HWID),                                    \
		RAS_ARGS_DEF(IDM_PCIE0, idm_records, NULL, INTR_LEVEL,         \
			     INVALID_HWID),                                    \
		RAS_ARGS_DEF(IDM_PCIE1, idm_records, NULL, INTR_LEVEL,         \
			     INVALID_HWID),                                    \
		RAS_ARGS_DEF(IDM_SYSHUB0, idm_records, NULL, INTR_LEVEL,       \
			     INVALID_HWID),                                    \
		RAS_ARGS_DEF(IDM_SYSHUB1, idm_records, NULL, INTR_LEVEL,       \
			     INVALID_HWID),                                    \
		RAS_ARGS_DEF(IDM_SMN0, idm_records, NULL, INTR_LEVEL,          \
			     INVALID_HWID),                                    \
		RAS_ARGS_DEF(IDM_SMN1, idm_records, NULL, INTR_LEVEL,          \
			     INVALID_HWID)
#endif

#if SKY1_TZC400_IDM_SUPPORT
#define TZC400_INTR_LIST                                                     \
	RAS_ARGS_DEF(TZC400_0, &tzc400_records[0], NULL, INTR_LEVEL,         \
		     INVALID_HWID),                                          \
		RAS_ARGS_DEF(TZC400_1, &tzc400_records[1], NULL, INTR_LEVEL, \
			     INVALID_HWID),                                  \
		RAS_ARGS_DEF(TZC400_2, &tzc400_records[2], NULL, INTR_LEVEL, \
			     INVALID_HWID),                                  \
		RAS_ARGS_DEF(TZC400_3, &tzc400_records[3], NULL, INTR_LEVEL, \
			     INVALID_HWID)
#endif

#ifdef SKY1_CPU_RAS_SUPPORT
#define CPU_RAS_INTR_LIST                                                \
	RAS_ARGS_DEF(CLUSTER_ERR, &cpu_ras_records[0], NULL, INTR_LEVEL, \
		     INVALID_HWID),                                      \
		RAS_ARGS_DEF(CLUSTER_FAULT, &cpu_ras_records[0], NULL,   \
			     INTR_LEVEL, INVALID_HWID),                  \
		RAS_ARGSID_DEF(COMPLEX_ERR, &cpu_ras_records[2], NULL,   \
			       INTR_LEVEL, 0x000, 0),                    \
		RAS_ARGSID_DEF(COMPLEX_ERR, &cpu_ras_records[2], NULL,   \
			       INTR_LEVEL, 0x100, 1),                    \
		RAS_ARGSID_DEF(COMPLEX_ERR, &cpu_ras_records[2], NULL,   \
			       INTR_LEVEL, 0x200, 2),                    \
		RAS_ARGSID_DEF(COMPLEX_ERR, &cpu_ras_records[2], NULL,   \
			       INTR_LEVEL, 0x300, 3),                    \
		RAS_ARGSID_DEF(COMPLEX_FAULT, &cpu_ras_records[2], NULL, \
			       INTR_LEVEL, 0x000, 0),                    \
		RAS_ARGSID_DEF(COMPLEX_FAULT, &cpu_ras_records[2], NULL, \
			       INTR_LEVEL, 0x100, 1),                    \
		RAS_ARGSID_DEF(COMPLEX_FAULT, &cpu_ras_records[2], NULL, \
			       INTR_LEVEL, 0x200, 2),                    \
		RAS_ARGSID_DEF(COMPLEX_FAULT, &cpu_ras_records[2], NULL, \
			       INTR_LEVEL, 0x300, 3),                    \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[2], NULL,      \
			       INTR_LEVEL, 0x000, 0),                    \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[2], NULL,      \
			       INTR_LEVEL, 0x100, 1),                    \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[2], NULL,      \
			       INTR_LEVEL, 0x200, 2),                    \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[2], NULL,      \
			       INTR_LEVEL, 0x300, 3),                    \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[1], NULL,      \
			       INTR_LEVEL, 0x400, 4),                    \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[1], NULL,      \
			       INTR_LEVEL, 0x500, 5),                    \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[1], NULL,      \
			       INTR_LEVEL, 0x600, 6),                    \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[1], NULL,      \
			       INTR_LEVEL, 0x700, 7),                    \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[1], NULL,      \
			       INTR_LEVEL, 0x800, 8),                    \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[1], NULL,      \
			       INTR_LEVEL, 0x900, 9),                    \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[1], NULL,      \
			       INTR_LEVEL, 0xa00, 10),                   \
		RAS_ARGSID_DEF(CORE_ERR, &cpu_ras_records[1], NULL,      \
			       INTR_LEVEL, 0xb00, 11),                   \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[2], NULL,    \
			       INTR_LEVEL, 0x000, 0),                    \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[2], NULL,    \
			       INTR_LEVEL, 0x100, 1),                    \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[2], NULL,    \
			       INTR_LEVEL, 0x200, 2),                    \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[2], NULL,    \
			       INTR_LEVEL, 0x300, 3),                    \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[1], NULL,    \
			       INTR_LEVEL, 0x400, 4),                    \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[1], NULL,    \
			       INTR_LEVEL, 0x500, 5),                    \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[1], NULL,    \
			       INTR_LEVEL, 0x600, 6),                    \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[1], NULL,    \
			       INTR_LEVEL, 0x700, 7),                    \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[1], NULL,    \
			       INTR_LEVEL, 0x800, 8),                    \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[1], NULL,    \
			       INTR_LEVEL, 0x900, 9),                    \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[1], NULL,    \
			       INTR_LEVEL, 0xa00, 10),                   \
		RAS_ARGSID_DEF(CORE_FAULT, &cpu_ras_records[1], NULL,    \
			       INTR_LEVEL, 0xb00, 11)
#endif

struct intr_info {
	char *name;
	int intr; /* Physical intr number */
	u_register_t mpidr;
	int trigger_type;
	int cpu;
};

struct sky1_ras_ev_map {
	int sdei_ev_num; /* SDEI Event number */
	struct intr_info intr;
};

const struct sky1_ras_ev_map *find_ras_event_map_by_intr(uint32_t intr_num);

/*idm*/
#if SKY1_IDM_RAS_SUPPORT
extern struct err_record_info idm_records[];
void sky1_idm_init(void);
#endif

/*tzc400*/
#if SKY1_TZC400_IDM_SUPPORT
extern struct err_record_info tzc400_records[];
#endif

#ifdef SKY1_CPU_RAS_SUPPORT
extern struct err_record_info cpu_ras_records[];
void sky1_ras_cpu_cache_resume(void);
void sky1_ras_dsu_cache_resume(void);
#endif

int sky1_esb_ras_err_record_handler(const struct err_record_info *info,
				    int probe_data,
				    const struct err_handler_data *const data);
int sky1_esb_ras_err_record_probe(const struct err_record_info *info,
				  int *probe_data);

#endif
