/**
 * Copyright (C), 2020, Arm Technology (China) Co., Ltd.
 * All rights reserved
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

/**
 * The OSAL deployment config
 * Currently we only support:
 * 1. Linux kernel
 * 2. Linux user space
 * 3. OPTEE TA space
 * 4. OPTEE OS
 * 5. UBL baremetal environment
 */
#include "osal_common.h"

#if !(defined(OSAL_ENV_LINUX_KERNEL) || defined(OSAL_ENV_LINUX_USER) ||        \
      defined(OSAL_ENV_OPTEE_TA) || defined(OSAL_ENV_OPTEE_OS) ||              \
      defined(OSAL_ENV_UBL) || defined(OSAL_ENV_UBOOT) || defined(OSAL_ENV_LK))
#error "OSAL_ENV not defined!"
#endif
