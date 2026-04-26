# Copyright (c) 2025, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

ifeq (${RAS_EXTENSION}, 1)
BL31_SOURCES		+=	${SKY1_BASE}/ras/sky1_ras.c
BL31_SOURCES		+=  ${SKY1_BASE}/ras/sky1_esb_ras.c

ifeq (${SKY1_IDM_RAS_SUPPORT}, 1)
$(eval $(call add_define,SKY1_IDM_RAS_SUPPORT))
BL31_SOURCES		+=	${SKY1_BASE}/ras/sky1_idm_ras.c
endif

ifeq (${SKY1_TZC400_IDM_SUPPORT}, 1)
$(eval $(call add_define,SKY1_TZC400_IDM_SUPPORT))
BL31_SOURCES		+=	${SKY1_BASE}/ras/sky1_tzc400_ras.c
endif

ifeq (${SKY1_CPU_RAS_SUPPORT}, 1)
$(eval $(call add_define,SKY1_CPU_RAS_SUPPORT))
BL31_SOURCES		+=	${SKY1_BASE}/ras/sky1_cpu_ras.c
endif

endif