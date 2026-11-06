/**
 * Copyright (C), 2018-2022, Arm Technology (China) Co., Ltd.
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

#include <kernel/tee_time.h>
#include <kernel/delay.h>
#include <mm/core_memprot.h>
#include "osal_utils.h"
#include "osal_assert.h"
#include "arm64.h"

uint32_t osal_read_timestamp_ms(void)
{
    TEE_Time tm;
    tee_time_get_sys_time(&tm);
    return tm.seconds * 1000 + tm.millis;
}

void osal_delay_us(uint32_t us)
{
    udelay(us);
}

void osal_delay_ms(uint32_t ms)
{
    mdelay(ms);
    return;
}

void osal_sleep_ms(uint32_t ms)
{
    tee_time_wait(ms);
    return;
}

uintptr_t osal_virt_to_phys(void *va)
{
    paddr_t pa;

    if (va == 0) {
        OSAL_LOG_TRACE("%s lr:0x%x\n", __func__, read_lr());
        return 0;
    }

    pa = virt_to_phys(va);

    /* DDR addr in AP start from 0x80000000 */
    /* DDR addr in CSU_SE start from 0x60000000 */
    /* Here is AP's DDR addr send into JIAYU(CSU_SE) DMA */
    /* That is: virt is AP's DDR addr, phys is CSU_SE's DDR addr */
    OSAL_ASSERT_MSG((pa >= 0x80000000), "Not DDR phy address!\n");

    return (pa - 0x20000000);
}

void *osal_phys_to_virt(uintptr_t pa)
{
    void *va = NULL;

    /* firstly TEE RAM */
    va = phys_to_virt(pa, MEM_AREA_TEE_RAM, DEFAULT_PHY_TO_VIRT_SIZE);
    if (va != NULL)
        return va;

    /* then TA RAM */
    va = phys_to_virt(pa, MEM_AREA_TA_RAM, DEFAULT_PHY_TO_VIRT_SIZE);
    if (va != NULL)
        return va;

    /* and SHM */
    va = phys_to_virt(pa, MEM_AREA_NSEC_SHM, DEFAULT_PHY_TO_VIRT_SIZE);
    if (va != NULL)
        return va;

    /* last IO */
    return phys_to_virt_io(pa, DEFAULT_PHY_TO_VIRT_SIZE);
}

osal_err_t osal_get_mem_attr(uint64_t paddr, uint64_t sz, osal_mem_attr_t *attr)
{
    if (NULL == attr) {
        return OSAL_ERROR_BAD_PARAMETERS;
    }

    if (paddr < 0x60000000) {
        OSAL_LOG_ERR("%s, wrong phy address:0x%x\n", __func__, paddr);
        return OSAL_ERROR_BAD_PARAMETERS;
    }

    paddr += 0x20000000;

    if (tee_pbuf_is_non_sec(paddr, sz)) {
        *attr = OSAL_MEM_ATTR_NONSECURE;
    } else if (tee_pbuf_is_sec(paddr, sz)) {
        *attr = OSAL_MEM_ATTR_SECURE;
    } else {
        *attr = OSAL_MEM_ATTR_UNKNOWN;
    }

    return OSAL_SUCCESS;
}
