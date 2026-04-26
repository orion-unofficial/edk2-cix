/**
 * Copyright (C), 2018-2020, Arm Technology (China) Co., Ltd.
 * All rights reserved
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and
 * any derivative work shall retain this copyright notice.
 *
 */
#if defined(OSAL_ENV_OPTEE_TA)
#include "cJSON_optee_ta_dep.h"
#elif defined(OSAL_ENV_UBL)
#include "cJSON_ubl_dep.h"
#elif defined(OSAL_ENV_LINUX_USER)
#include "cJSON_linux_user_dep.h"
#else
#error "One of the following should be defined: OSAL_ENV_OPTEE_TA, OSAL_ENV_UBL, OSAL_ENV_LINUX_USER"
#endif