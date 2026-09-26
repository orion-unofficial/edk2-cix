/*
 * Copyright (c) 2019-2022, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <util.h>
#include <stdio.h>
#include <string_ext.h>
#include <drivers/spi_nor.h>
#include <kernel/delay.h>
#include <kernel/thread.h>
#include <trace.h>
#include <mm/core_memprot.h>
#include <kernel/tee_time.h>
#include <qlib.h>
#include "qlib_sample_qconf.h"
#include <qconf.h>

QLIB_CONTEXT_T g_qlibContext;

#ifndef CFG_NOR_FS_SECTION_ID
    #define CFG_NOR_FS_SECTION_ID 1
#endif
uint16_t nor_fs_section = CFG_NOR_FS_SECTION_ID;

#define QLIB_DTR_TO_DTR_FLAGS(qlibContext, cmdFormat, cmdDtr) ((cmdDtr) == FALSE ? QLIB_DTR__NO_DTR : QLIB_DTR__ADDR_DATA)

int w77_init(struct spi_nor *nor __unused)
{
    int res = 0;
#if 0
	EMSG("recovery qlib...\n");
	QLIB_SAMPLE_QconfRecoveryRun(NULL);
#endif
    QLIB_BUS_FORMAT_T busFormat;
    QCONF_T qlibTable;

    QLIB_CONTEXT_T *qlibContext = &g_qlibContext;

    busFormat = QLIB_BUS_FORMAT(QLIB_BUS_MODE_1_4_4, FALSE);

    memset(qlibContext, 0x00, sizeof(QLIB_CONTEXT_T));
    DMSG("%s w77_init start, busFormat:%d, nor_fs_section:%d\n",  __func__, busFormat, nor_fs_section);
    /*-------------------------------------------------------------------------------------------------------
    Init QLIB  - needed when running QLIB either on a device or on a remote server.
	-------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_InitLib(qlibContext));
    EMSG("w77_init QLIB_InitLib\n");
	/*-------------------------------------------------------------------------------------------------------
	Take the ownership of flash communication channel (it belongs to local MCU or to remote server, exclusively)
	-------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QLIB_Connect(qlibContext));
    EMSG("w77_init QLIB_Connect\n");
	/*-------------------------------------------------------------------------------------------------------
	Init Flash Device - not needed when using QLIB on a remote server
	-------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_InitDevice(qlibContext, busFormat), res, disconnect);
    EMSG("w77_init QLIB_InitDevice\n");
    /*-------------------------------------------------------------------------------------------------------
    Load the key for the secure write operations
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK(QCONF_Fetch_Config(&qlibTable));

    QLIB_STATUS_RET_CHECK(QLIB_LoadKey(qlibContext, nor_fs_section, qlibTable.otc.fullAccessKeys[nor_fs_section], TRUE));
    EMSG("w77_init QLIB_LoadKey\n");
    /*-------------------------------------------------------------------------------------------------------
    Open secure session for the secure commands
    -------------------------------------------------------------------------------------------------------*/
    QLIB_STATUS_RET_CHECK_GOTO(QLIB_OpenSession(qlibContext, nor_fs_section, QLIB_SESSION_ACCESS_FULL), res, remove_key);
    EMSG("w77_init QLIB_OpenSession successfully \n");
    return res;

remove_key:
    (void)QLIB_RemoveKey(qlibContext, nor_fs_section, TRUE);

disconnect:
    (void)QLIB_Disconnect(qlibContext);
    return res;
}

int w77_read(struct spi_nor *nor __unused, uint8_t *buffer, uint32_t offset, uint32_t length)
{
    int res = 0;
    DMSG("offset %u length %u\n", offset, length);
    res = QLIB_Read(&g_qlibContext, buffer, nor_fs_section, offset, length, TRUE, FALSE);
    return res;
}

int w77_write(struct spi_nor *nor __unused, uint8_t *buffer, uint32_t offset, uint32_t length)
{
    int res = 0;
    DMSG("offset %u length %u\n", offset, length);
    res = QLIB_Write(&g_qlibContext, buffer, nor_fs_section, offset, length, TRUE);
    return res;
}

int w77_erase(struct spi_nor *nor __unused, uint32_t addr, uint32_t len)
{
    int res = 0;

    DMSG("addr %u len %u\n", addr, len);
    res = QLIB_Erase(&g_qlibContext, nor_fs_section, addr, len, TRUE);

    return res;
}

void spi_nor_register_w77(struct spi_nor *nor)
{
    nor->is_secure = true;
    nor->size = 0x800000;

    nor->init = w77_init;
    nor->erase = w77_erase;
    nor->write = w77_write;
    nor->read = w77_read;
    return;
}

