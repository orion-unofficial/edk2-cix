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

#define SR_WIP			BIT(0)	/* Write in progress */
#define CR_QUAD_EN_SPAN		BIT(1)	/* Spansion Quad I/O */
#define SR_QUAD_EN_MX		BIT(6)	/* Macronix Quad I/O */
#define FSR_READY		BIT(7)	/* Device status, 0 = Busy, 1 = Ready */

/* Defined IDs for supported memories */
#define SPANSION_ID		0x01U
#define MACRONIX_ID		0xC2U
#define MICRON_ID		0x2CU
#define W25R64JW                0x17
#define W25R128JW               0x18
#define NOR_JEDEC_OFFSET        0x100000
#define BANK_SIZE		0x1000000U
#define WRITE_SECTION_SIZE      256
#define ERASE_SECTION_SIZE       0x1000
#define SPI_READY_TIMEOUT_US	80000U
#define QSPI_FLASH_CONFIG       0x188800

uint32_t nor_fs_offset = CFG_NOR_FS_OFFSET;
uint32_t nor_fs_size = 0x200000;

static int spi_nor_reg(uint8_t reg, uint8_t *buf, size_t len,
					   enum spi_mem_data_dir dir)
{
	struct spi_mem_op op;

	memzero_explicit(&op, sizeof(struct spi_mem_op));
	op.cmd.opcode = reg;
	op.cmd.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	op.data.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	op.data.dir = dir;
	op.data.nbytes = len;
	op.data.buf = buf;

	return spi_mem_exec_op(&op);
}

static inline int spi_nor_read_id(uint8_t *id)
{
	return spi_nor_reg(SPI_NOR_OP_READ_ID, id, 1U, SPI_MEM_DATA_IN);
}

int spi_nor_read_jedec(uint8_t *id)
{
	return spi_nor_reg(SPI_NOR_OP_READ_ID, id, 3U, SPI_MEM_DATA_IN);
}

static inline int spi_nor_read_cr(uint8_t *cr)
{
	return spi_nor_reg(SPI_NOR_OP_READ_CR, cr, 1U, SPI_MEM_DATA_IN);
}

static inline int spi_nor_read_sr(uint8_t *sr)
{
	return spi_nor_reg(SPI_NOR_OP_READ_SR, sr, 1U, SPI_MEM_DATA_IN);
}

static inline int spi_nor_read_fsr(uint8_t *fsr)
{
	return spi_nor_reg(SPI_NOR_OP_READ_FSR, fsr, 1U, SPI_MEM_DATA_IN);
}

static inline int spi_nor_write_en(void)
{
	return spi_nor_reg(SPI_NOR_OP_WREN, NULL, 0U, SPI_MEM_NO_DATA);
}


/*
 * set secure storage offset and size, while init spi nor.
 */
static int spi_nor_set_offset_by_jedec()
{
        uint8_t jedec_id[6];
        spi_nor_read_jedec(jedec_id);

        if(W25R64JW == jedec_id[2])
	{
                nor_fs_offset += NOR_JEDEC_OFFSET;
		nor_fs_size = 0x100000;
	}
        else if(W25R128JW == jedec_id[2])
	{
                nor_fs_offset += 2 * NOR_JEDEC_OFFSET;
		nor_fs_size = 0x800000;
	}
}


/*
 * Check if device is ready.
 *
 * Return 0 if ready, 1 if busy or a negative error code otherwise
 */
static int spi_nor_ready(void)
{
	uint8_t sr;
	int ret;

	ret = spi_nor_read_sr(&sr);
	if (ret != 0)
		return ret;

	return (((sr & SR_WIP) == 0U) ? 0 : 1);
}

static int spi_nor_wait_ready(void)
{
	int ret;
	uint64_t timeout = timeout_init_us(SPI_READY_TIMEOUT_US);

	while (!timeout_elapsed(timeout))
	{
		ret = spi_nor_ready();
		if (ret <= 0)
			return ret;
	}

	return -1;
}

static int spi_nor_clean_bar(void)
{
	int ret;
	ret = spi_nor_write_en();
	if (ret != 0)
		return ret;

	// return spi_nor_reg(nor_dev.bank_write_cmd, &nor_dev.selected_bank,
	// 		   1U, SPI_MEM_DATA_OUT);
	return 0;
}

int spi_nor_init(struct spi_nor *nor __unused)
{
	int ret = 0;
	uint8_t id;

	ret = spi_nor_read_id(&id);
	if (ret != 0)
		return ret;

	return ret;
}

int spi_nor_read(struct spi_nor *nor __unused, uint8_t *buffer, uint32_t offset, uint32_t length)
{
	int ret = 0;
	struct spi_mem_op read_op;

	offset += nor_fs_offset;

	/* cmd */
	read_op.cmd.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	read_op.cmd.opcode = SPI_NOR_OP_READ_1_1_4;

	/* addr */
	read_op.addr.val = offset;
	read_op.addr.nbytes = 3;
	read_op.addr.buswidth = SPI_MEM_BUSWIDTH_1_LINE;

	/* dummy */
	read_op.dummy.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	read_op.dummy.nbytes = nor->read_dummy;

	/* data */
	read_op.data.buswidth = SPI_MEM_BUSWIDTH_4_LINE;
	read_op.data.buf = (void *)buffer;
	read_op.data.nbytes = length;
	read_op.data.dir = SPI_MEM_DATA_IN;

	ret = spi_mem_exec_op(&read_op);
	if (ret != 0)
	{
		spi_nor_clean_bar();
		return ret;
	}

	return 0;
}

int spi_nor_write_data(struct spi_nor *nor __unused, uint8_t *buffer, uint32_t offset, uint32_t length)
{
	int ret;
	struct spi_mem_op write_op;
	DMSG("%s offset %u length %u\n", __func__, offset, length);

	/* cmd */
	write_op.cmd.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	write_op.cmd.opcode = SPI_NOR_OP_WRITE;

	/* addr */
	write_op.addr.val = offset;
	write_op.addr.nbytes = 3;
	write_op.addr.buswidth = SPI_MEM_BUSWIDTH_1_LINE;

	/* dummy */
	write_op.dummy.buswidth = 0;
	write_op.dummy.nbytes = 0;

	/* data */
	write_op.data.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	write_op.data.buf = (void *)buffer;
	write_op.data.nbytes = length;
	write_op.data.dir = SPI_MEM_DATA_OUT;

	spi_nor_write_en();
	ret = spi_mem_exec_op(&write_op);
	if (ret != 0)
	{
		spi_nor_clean_bar();
		return ret;
	}

	ret = spi_nor_wait_ready();
	if (ret != 0)
		return ret;

	return length;
}

int spi_nor_write(struct spi_nor *nor __unused, uint8_t *buffer, uint32_t offset, uint32_t length)
{
	int ret = 0;
	uint32_t to_write = 0;

	offset += nor_fs_offset;
	while (length)
	{
		to_write = MIN(length, nor->sector_size);
		ret = spi_nor_write_data(nor, buffer, offset, to_write);
		if (ret <= 0)
			return ret;
		offset += ret;
		buffer += ret;
		length -= ret;
	}

	return 0;
}

int spi_nor_erase_sector(struct spi_nor *nor __unused, uint32_t addr)
{
	int ret;
	struct spi_mem_op erase_op;
	DMSG("%s addr 0x%x\n", __func__, addr);

	/* cmd */
	erase_op.cmd.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	erase_op.cmd.opcode = SPI_NOR_OP_ERASE;

	/* addr */
	erase_op.addr.val = addr;
	erase_op.addr.nbytes = 3;
	erase_op.addr.buswidth = SPI_MEM_BUSWIDTH_1_LINE;

	/* dummy */
	erase_op.dummy.buswidth = 0;
	erase_op.dummy.nbytes = 0;

	/* data */
	erase_op.data.buswidth = 0;
	erase_op.data.buf = 0;
	erase_op.data.nbytes = 0;
	erase_op.data.dir = 0;
	spi_nor_write_en();

	ret = spi_mem_exec_op(&erase_op);
	if (ret != 0)
	{
		spi_nor_clean_bar();
		return ret;
	}

	ret = spi_nor_wait_ready();
	if (ret != 0)
	{
		return ret;
	}

	return nor->erase_size;
}

int spi_nor_erase(struct spi_nor *nor __unused, uint32_t addr, uint32_t len)
{
	int ret = 0;
	uint32_t rem = len % (nor->erase_size);

	if (rem)
		return -1;

	addr += nor_fs_offset;
	while (len)
	{
		ret = spi_nor_erase_sector(nor, addr);
		if (ret < 0)
			return -1;
		addr += ret;
		len -= ret;
	}

	return 0;
}

#define CIX_SIP_NOR_STORAGE 0xC200000B

int spi_mem_exec_op(const struct spi_mem_op *op)
{
	int dummy_cycle = op->dummy.nbytes * 8 / op->dummy.buswidth;
	uint32_t payload = (uint32_t)(virt_to_phys(op->data.buf));

	struct thread_smc_args args = {
		.a0 = CIX_SIP_NOR_STORAGE,
		.a1 = reg_pair_to_64(op->addr.val, op->cmd.opcode),
		.a2 = reg_pair_to_64(op->data.dir, op->addr.nbytes),
		.a3 = reg_pair_to_64(dummy_cycle, op->data.buswidth),
		.a4 = reg_pair_to_64(op->data.nbytes, payload),
	};
	DMSG("%s: cmd:%x mode:%d.%d.%d.%d addqr:%" PRIx64 " len:%x\n",
		 __func__, op->cmd.opcode, op->cmd.buswidth, op->addr.buswidth,
		 op->dummy.buswidth, op->data.buswidth,
		 op->addr.val, op->data.nbytes);

	thread_smccc(&args);

	return 0;
}

void spi_nor_register_normal(struct spi_nor *nor)
{
	spi_nor_set_offset_by_jedec();
	nor->read_dummy = 1;
	nor->is_secure = true;
	nor->size = nor_fs_size;
	nor->sector_size = WRITE_SECTION_SIZE;
	nor->erase_size = ERASE_SECTION_SIZE;

	nor->init = spi_nor_init;
	nor->erase = spi_nor_erase;
	nor->write = spi_nor_write;
	nor->read = spi_nor_read;
	return;
}
