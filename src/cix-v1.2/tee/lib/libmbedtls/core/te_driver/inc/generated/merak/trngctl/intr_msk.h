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
 *
 * Auto generated file (Fri Oct 21 20:27:00 2022).
 * DO NOT EDIT!!
 *
 */

#ifndef __TRNGCTL_INTR_MSK_REG_H__
#define __TRNGCTL_INTR_MSK_REG_H__

#define TRNGCTL_INTR_MSK_ADAP_TST_ERR_MSK_SHIFT 3
#define TRNGCTL_INTR_MSK_ADAP_TST_ERR_MSK_WIDTH 1
#define TRNGCTL_INTR_MSK_REP_TST_ERR_MSK_SHIFT 2
#define TRNGCTL_INTR_MSK_REP_TST_ERR_MSK_WIDTH 1
#define TRNGCTL_INTR_MSK_CRNG_ERR_MSK_SHIFT 1
#define TRNGCTL_INTR_MSK_CRNG_ERR_MSK_WIDTH 1
#define TRNGCTL_INTR_MSK_VN_ERR_MSK_SHIFT 0
#define TRNGCTL_INTR_MSK_VN_ERR_MSK_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * trngctl_intr_msk register definition.
 */
typedef union trngctl_intr_mskReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        vn_err_msk: 1,
        crng_err_msk: 1,
        rep_tst_err_msk: 1,
        adap_tst_err_msk: 1,
        hole0: 28; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} trngctl_intr_mskReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_trngctl_intr_msk(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("trngctl_intr_msk: 0x%08x is\n", x);


    t = (x >> TRNGCTL_INTR_MSK_ADAP_TST_ERR_MSK_SHIFT) & ((1U << TRNGCTL_INTR_MSK_ADAP_TST_ERR_MSK_WIDTH) - 1);
    BITFIELD_LOG(" adap_tst_err_msk=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_INTR_MSK_ADAP_TST_ERR_MSK_WIDTH) - 1) << TRNGCTL_INTR_MSK_ADAP_TST_ERR_MSK_SHIFT);


    t = (x >> TRNGCTL_INTR_MSK_REP_TST_ERR_MSK_SHIFT) & ((1U << TRNGCTL_INTR_MSK_REP_TST_ERR_MSK_WIDTH) - 1);
    BITFIELD_LOG(" rep_tst_err_msk=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_INTR_MSK_REP_TST_ERR_MSK_WIDTH) - 1) << TRNGCTL_INTR_MSK_REP_TST_ERR_MSK_SHIFT);


    t = (x >> TRNGCTL_INTR_MSK_CRNG_ERR_MSK_SHIFT) & ((1U << TRNGCTL_INTR_MSK_CRNG_ERR_MSK_WIDTH) - 1);
    BITFIELD_LOG(" crng_err_msk=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_INTR_MSK_CRNG_ERR_MSK_WIDTH) - 1) << TRNGCTL_INTR_MSK_CRNG_ERR_MSK_SHIFT);


    t = (x >> TRNGCTL_INTR_MSK_VN_ERR_MSK_SHIFT) & ((1U << TRNGCTL_INTR_MSK_VN_ERR_MSK_WIDTH) - 1);
    BITFIELD_LOG(" vn_err_msk=0x%x\n", t);
    x &= ~(((1U << TRNGCTL_INTR_MSK_VN_ERR_MSK_WIDTH) - 1) << TRNGCTL_INTR_MSK_VN_ERR_MSK_SHIFT);

    if (x) {
        int i = 0;
        BITFIELD_LOG(" (Unknown :");
        while (x) {
            if (x & 1) {
                BITFIELD_LOG(" %d", i);
            }
            x >>= 1;
            i++;
        }
        BITFIELD_LOG(")\n");
    }

    BITFIELD_LOG("\n");
}

#else  /* WITH_BITFIELD_LOG */

#define log_trngctl_intr_msk(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNGCTL_INTR_MSK_REG_H__ */
