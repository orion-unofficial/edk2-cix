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

#ifndef __TRUSTENGINE_HWA_HASH_H__
#define __TRUSTENGINE_HWA_HASH_H__

#include "te_hwa_sca.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_hash_regs;

typedef te_hwa_sca_t te_hwa_hash_t;

static inline int te_hwa_hash_alloc( struct te_hash_regs *regs,
                                     struct te_hwa_host *host,
                                     te_hwa_hash_t **hwa )
{
    return te_hwa_sca_alloc((struct te_sca_regs*)regs, host, true, hwa);
}

static inline int te_hwa_hash_free( te_hwa_hash_t *hwa )
{
    return te_hwa_sca_free(hwa);
}

static inline int te_hwa_hash_init( struct te_hash_regs *regs,
                                    struct te_hwa_host *host,
                                    te_hwa_hash_t *hwa )
{
    return te_hwa_sca_init((struct te_sca_regs*)regs, host, true, hwa);
}

static inline int te_hwa_hash_exit( te_hwa_hash_t *hwa )
{
    return te_hwa_sca_exit(hwa);
}

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_HASH_H__ */
