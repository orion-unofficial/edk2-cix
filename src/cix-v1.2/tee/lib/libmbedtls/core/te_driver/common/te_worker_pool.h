/*
 * Copyright (c) 2020, Arm Technology (China) Co., Ltd.
 * All rights reserved.
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#include "te_worker.h"
#ifndef __TRUSTENGINE_WORKER_POOL_H__
#define __TRUSTENGINE_WORKER_POOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * \brief           Create global worker pool.
 *
 * \return          \c None.
 */
int te_worker_pool_create(void);

/**
 * \brief               This function enqueue a task to worker pool.
 *
 * \param[int] task     The task instance.
 * \return              \c None
 */
void te_worker_pool_enqueue(te_worker_task_t *task);

/**
 * \brief               Destroy global worker pool.
 *
 * \return              \c None
 */
void te_worker_pool_destroy(void);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_COMMON_H__ */
