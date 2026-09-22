# Copyright (c) 2025, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

ifeq (${CIX_DST_SUPPORT},1)
BL31_SOURCES		+= 	${SKY1_BASE}/dst/sky1_dst.c \
                    	${SKY1_BASE}/dst/sky1_ci700.c \
						${SKY1_BASE}/dst/sky1_last_stack.c \
						${SKY1_BASE}/dst/sky1_trace.c

ifeq (${DEBUG},1)
BL31_SOURCES		+=	${SKY1_BASE}/dst/sky1_exception_test.c
endif

ifeq (${SDEI_SUPPORT},1)
BL31_SOURCES		+= 	${SKY1_BASE}/dst/sky1_sdei.c

endif # SDEI_SUPPORT

endif # CIX_DST_SUPPORT