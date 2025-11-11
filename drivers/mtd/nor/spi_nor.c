/*
 * Copyright (c) 2019-2022, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <stddef.h>

#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <drivers/spi_nor.h>
#include <lib/utils.h>

#define SR_WIP			BIT(0)	/* Write in progress */
#define CR_QUAD_EN_SPAN		BIT(1)	/* Spansion Quad I/O */
#define SR_QUAD_EN_MX		BIT(6)	/* Macronix Quad I/O */
#define FSR_READY		BIT(7)	/* Device status, 0 = Busy, 1 = Ready */

/* Defined IDs for supported memories */
#define SPANSION_ID		0x01U
#define MACRONIX_ID		0xC2U
#define MICRON_ID		0x2CU

#define BANK_SIZE		0x1000000U

#define SPI_READY_TIMEOUT_US	40000U
#define QSPI_FLASH_CONFIG       0x188800

static struct nor_device nor_dev;

#pragma weak plat_get_nor_data
int plat_get_nor_data(struct nor_device *device)
{
	return 0;
}

static int spi_nor_reg(uint8_t reg, uint8_t *buf, size_t len,
		       enum spi_mem_data_dir dir)
{
	struct spi_mem_op op;

	zeromem(&op, sizeof(struct spi_mem_op));
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

static inline int spi_nor_read_jedec(uint8_t *id)
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
	return spi_nor_reg(SPI_NOR_OP_WREN, NULL, 0U, SPI_MEM_DATA_OUT);
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
	if (ret != 0) {
		return ret;
	}

	if ((nor_dev.flags & SPI_NOR_USE_FSR) != 0U) {
		uint8_t fsr;

		ret = spi_nor_read_fsr(&fsr);
		if (ret != 0) {
			return ret;
		}

		return (((fsr & FSR_READY) != 0U) && ((sr & SR_WIP) == 0U)) ?
			0 : 1;
	}

	return (((sr & SR_WIP) == 0U) ? 0 : 1);
}

static int spi_nor_wait_ready(void)
{
	int ret;
	uint64_t timeout = timeout_init_us(SPI_READY_TIMEOUT_US);

	while (!timeout_elapsed(timeout)) {
		ret = spi_nor_ready();
		if (ret <= 0) {
			return ret;
		}
	}

	return -ETIMEDOUT;
}

static int spi_nor_macronix_quad_enable(void)
{
	uint8_t sr;
	int ret;

	ret = spi_nor_read_sr(&sr);
	if (ret != 0) {
		return ret;
	}

	if ((sr & SR_QUAD_EN_MX) != 0U) {
		return 0;
	}

	ret = spi_nor_write_en();
	if (ret != 0) {
		return ret;
	}

	sr |= SR_QUAD_EN_MX;
	ret = spi_nor_reg(SPI_NOR_OP_WRSR, &sr, 1U, SPI_MEM_DATA_OUT);
	if (ret != 0) {
		return ret;
	}

	ret = spi_nor_wait_ready();
	if (ret != 0) {
		return ret;
	}

	ret = spi_nor_read_sr(&sr);
	if ((ret != 0) || ((sr & SR_QUAD_EN_MX) == 0U)) {
		return -EINVAL;
	}

	return 0;
}

static int spi_nor_write_sr_cr(uint8_t *sr_cr)
{
	int ret;

	ret = spi_nor_write_en();
	if (ret != 0) {
		return ret;
	}

	ret = spi_nor_reg(SPI_NOR_OP_WRSR, sr_cr, 2U, SPI_MEM_DATA_OUT);
	if (ret != 0) {
		return -EINVAL;
	}

	ret = spi_nor_wait_ready();
	if (ret != 0) {
		return ret;
	}

	return 0;
}

static int spi_nor_quad_enable(void)
{
	uint8_t sr_cr[2];
	int ret;

	ret = spi_nor_read_cr(&sr_cr[1]);
	if (ret != 0) {
		return ret;
	}

	if ((sr_cr[1] & CR_QUAD_EN_SPAN) != 0U) {
		return 0;
	}

	sr_cr[1] |= CR_QUAD_EN_SPAN;
	ret = spi_nor_read_sr(&sr_cr[0]);
	if (ret != 0) {
		return ret;
	}

	ret = spi_nor_write_sr_cr(sr_cr);
	if (ret != 0) {
		return ret;
	}

	ret = spi_nor_read_cr(&sr_cr[1]);
	if ((ret != 0) || ((sr_cr[1] & CR_QUAD_EN_SPAN) == 0U)) {
		return -EINVAL;
	}

	return 0;
}

static int spi_nor_clean_bar(void)
{
	int ret;

	if (nor_dev.selected_bank == 0U) {
		return 0;
	}

	nor_dev.selected_bank = 0U;

	ret = spi_nor_write_en();
	if (ret != 0) {
		return ret;
	}

	return spi_nor_reg(nor_dev.bank_write_cmd, &nor_dev.selected_bank,
			   1U, SPI_MEM_DATA_OUT);
}

static int spi_nor_write_bar(uint32_t offset)
{
	uint8_t selected_bank = offset / BANK_SIZE;
	int ret;

	if (selected_bank == nor_dev.selected_bank) {
		return 0;
	}

	ret = spi_nor_write_en();
	if (ret != 0) {
		return ret;
	}

	ret = spi_nor_reg(nor_dev.bank_write_cmd, &selected_bank,
			  1U, SPI_MEM_DATA_OUT);
	if (ret != 0) {
		return ret;
	}

	nor_dev.selected_bank = selected_bank;

	return 0;
}

static int spi_nor_read_bar(void)
{
	uint8_t selected_bank = 0U;
	int ret;

	ret = spi_nor_reg(nor_dev.bank_read_cmd, &selected_bank,
			  1U, SPI_MEM_DATA_IN);
	if (ret != 0) {
		return ret;
	}

	nor_dev.selected_bank = selected_bank;

	return 0;
}

int spi_nor_read(unsigned int offset, uintptr_t buffer, size_t length,
		 size_t *length_read)
{
	size_t remain_len;
	int ret;

	*length_read = 0U;
	nor_dev.read_op.addr.val = offset;
	nor_dev.read_op.data.buf = (void *)buffer;

	VERBOSE("%s offset %u length %zu\n", __func__, offset, length);

	if ( nor_dev.read_op.data.buswidth == SPI_MEM_BUSWIDTH_4_LINE) {
		ret = spi_nor_quad_read(nor_dev.read_op.dummy.nbytes, offset, buffer, length);
		if(ret == 0) {
			*length_read = length;
		} else {
			*length_read = 0;
		}
		return ret;
	}

	while (length != 0U) {
		if ((nor_dev.flags & SPI_NOR_USE_BANK) != 0U) {
			ret = spi_nor_write_bar(nor_dev.read_op.addr.val);
			if (ret != 0) {
				return ret;
			}

			remain_len = (BANK_SIZE * (nor_dev.selected_bank + 1)) -
				nor_dev.read_op.addr.val;
			nor_dev.read_op.data.nbytes = MIN(length, remain_len);
		} else {
			nor_dev.read_op.data.nbytes = length;
		}

		ret = spi_mem_exec_op(&nor_dev.read_op);
		if (ret != 0) {
			spi_nor_clean_bar();
			return ret;
		}

		length -= nor_dev.read_op.data.nbytes;
		nor_dev.read_op.addr.val += nor_dev.read_op.data.nbytes;
		nor_dev.read_op.data.buf += nor_dev.read_op.data.nbytes;
		*length_read += nor_dev.read_op.data.nbytes;
	}

	if ((nor_dev.flags & SPI_NOR_USE_BANK) != 0U) {
		ret = spi_nor_clean_bar();
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

int spi_nor_init(unsigned long long *size, unsigned int *erase_size)
{
	int ret;
	uint8_t id;

	/* Default read command used */
	nor_dev.read_op.cmd.opcode = SPI_NOR_OP_READ;
	nor_dev.read_op.cmd.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	nor_dev.read_op.addr.nbytes = 3U;
	nor_dev.read_op.addr.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	nor_dev.read_op.data.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	nor_dev.read_op.data.dir = SPI_MEM_DATA_IN;

	if (plat_get_nor_data(&nor_dev) != 0) {
		return -EINVAL;
	}

	assert(nor_dev.size != 0U);

	if (nor_dev.size > BANK_SIZE) {
		nor_dev.flags |= SPI_NOR_USE_BANK;
	}

	*size = nor_dev.size;

	ret = spi_nor_read_id(&id);
	if (ret != 0) {
		return ret;
	}

	if ((nor_dev.flags & SPI_NOR_USE_BANK) != 0U) {
		switch (id) {
		case SPANSION_ID:
			nor_dev.bank_read_cmd = SPINOR_OP_BRRD;
			nor_dev.bank_write_cmd = SPINOR_OP_BRWR;
			break;
		default:
			nor_dev.bank_read_cmd = SPINOR_OP_RDEAR;
			nor_dev.bank_write_cmd = SPINOR_OP_WREAR;
			break;
		}
	}

	if (nor_dev.read_op.data.buswidth == 4U) {
		switch (id) {
		case MACRONIX_ID:
			INFO("Enable Macronix quad support\n");
			ret = spi_nor_macronix_quad_enable();
			break;
		case MICRON_ID:
			break;
		default:
			ret = spi_nor_quad_enable();
			break;
		}
	}

	if ((ret == 0) && ((nor_dev.flags & SPI_NOR_USE_BANK) != 0U)) {
		ret = spi_nor_read_bar();
	}

	return ret;
}


int spi_nor_get_config(int *dummy_nbytes,int *op_mode, uint8_t *clk_index)
{
	int ret = 0;
	int num = 0;
	uint8_t exp_jedec_id[6];
	uint8_t actual_jedec_id[6];
	uint8_t config_num;
	uint8_t buffer[1024];
	uint32_t offset = 0;
	unsigned int len = 0;

	/*read jedec id to match json config*/
        ret = spi_nor_read_jedec(actual_jedec_id);

	/*use one line read to get config from flash*/
	zeromem(buffer, sizeof(buffer));
	zeromem(&nor_dev.read_op, sizeof(struct spi_mem_op));
	offset = QSPI_FLASH_CONFIG;
	len = 1;
	nor_dev.read_op.cmd.opcode = SPI_NOR_OP_READ;
	nor_dev.read_op.cmd.buswidth = SPI_MEM_BUSWIDTH_1_LINE;

	nor_dev.read_op.addr.nbytes = 3U;
	nor_dev.read_op.addr.val = offset;
	nor_dev.read_op.addr.buswidth = SPI_MEM_BUSWIDTH_1_LINE;

	nor_dev.read_op.data.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	nor_dev.read_op.data.dir = SPI_MEM_DATA_IN;
	nor_dev.read_op.data.buf = buffer;
	nor_dev.read_op.data.nbytes = len;
	ret += spi_mem_exec_op(&nor_dev.read_op);
	if(!ret)
		config_num = buffer[0];
	else{
		INFO("%s error\n", __func__);
	        goto one_line_read;
	}
	/*config para start at offset 4,and each flash para size is 16 */
	offset += 4;
        len = 16 * config_num;
	nor_dev.read_op.data.nbytes = len;
	nor_dev.read_op.addr.val = offset;
	ret += spi_mem_exec_op(&nor_dev.read_op);
	if(ret) {
		INFO("read config fail\n");
	        goto one_line_read;
	}
	while(num < config_num)
	{
		exp_jedec_id[0] = buffer[2 + 16 * num];
		exp_jedec_id[1] = buffer[1 + 16 * num];
		exp_jedec_id[2] = buffer[0 + 16 * num];
		if(!memcmp(actual_jedec_id, exp_jedec_id, 3)){
			*dummy_nbytes = buffer[6 + 16 * num] / 8;
			*op_mode = buffer[5 + 16 * num];
			*clk_index = buffer[4 + 16 * num];
			return 0;
		}
		num++;
	}

one_line_read:
	*op_mode = READ_INDEX_1_1_1;
	return 0;
}


int spi_nor_quad_read(uint8_t dummy_nbytes,unsigned int offset, uintptr_t buffer,size_t length)
{
	int ret;
	struct spi_mem_op read_op;

	/* cmd */
	read_op.cmd.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	read_op.cmd.opcode = SPI_NOR_OP_READ_1_1_4;

	/* addr */
	read_op.addr.val = offset;
	read_op.addr.nbytes = 3;
	read_op.addr.buswidth = SPI_MEM_BUSWIDTH_1_LINE;

	/* dummy */
	read_op.dummy.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	read_op.dummy.nbytes = dummy_nbytes;

	/* data */
	read_op.data.buswidth = SPI_MEM_BUSWIDTH_4_LINE;
	read_op.data.buf = (void *)buffer;
	read_op.data.nbytes = length;
	read_op.data.dir = SPI_MEM_DATA_IN;

	VERBOSE("%s offset %u length %zu\n", __func__, offset, length);
	ret = spi_mem_exec_op(&read_op);
	if (ret != 0) {
		spi_nor_clean_bar();
		return ret;
	}

	return 0;
}
