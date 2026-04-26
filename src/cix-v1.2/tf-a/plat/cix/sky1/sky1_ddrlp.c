/*
 * Copyright (c) 2022, CIX Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>
#include <plat_cix.h>

#define DDR_CH0_CTL_REG	0xc0102ec
#define DDR_CH1_CTL_REG	0xc0302ec
#define DDR_CH2_CTL_REG	0xc0502ec
#define DDR_CH3_CTL_REG	0xc0702ec
#define DDR_CTL_OFFSET	0xc

#define DDR_CTL_MASK	0xf0000

static int save_reg_val[8] = {0};

static void ddrlp_clear_bits(int addr, int mask)
{
	int val;

	val = mmio_read_32(addr);
	val &= ~mask;
	mmio_write_32(addr, val);
}

static void save_register(void)
{
	MEM_INIT_OUTPUT_BUFFER *buf = GetMemOutputBuffer();
	if (buf->ChannelMask == 0xc) {
		save_reg_val[2] = mmio_read_32(DDR_CH2_CTL_REG);
		save_reg_val[3] = mmio_read_32(DDR_CH3_CTL_REG);
		save_reg_val[6] = mmio_read_32(DDR_CH2_CTL_REG + DDR_CTL_OFFSET);
		save_reg_val[7] = mmio_read_32(DDR_CH3_CTL_REG + DDR_CTL_OFFSET);
	} else {
		save_reg_val[0] = mmio_read_32(DDR_CH0_CTL_REG);
		save_reg_val[1] = mmio_read_32(DDR_CH1_CTL_REG);
		save_reg_val[2] = mmio_read_32(DDR_CH2_CTL_REG);
		save_reg_val[3] = mmio_read_32(DDR_CH3_CTL_REG);
		save_reg_val[4] = mmio_read_32(DDR_CH0_CTL_REG + DDR_CTL_OFFSET);
		save_reg_val[5] = mmio_read_32(DDR_CH1_CTL_REG + DDR_CTL_OFFSET);
		save_reg_val[6] = mmio_read_32(DDR_CH2_CTL_REG + DDR_CTL_OFFSET);
		save_reg_val[7] = mmio_read_32(DDR_CH3_CTL_REG + DDR_CTL_OFFSET);
	}
}

int sky1_set_ddrlp(int on)
{
	MEM_INIT_OUTPUT_BUFFER *buf = GetMemOutputBuffer();
	if (on) {
		if (buf->ChannelMask == 0xc) {
			mmio_write_32(DDR_CH2_CTL_REG, save_reg_val[2]);
			mmio_write_32(DDR_CH3_CTL_REG, save_reg_val[3]);
			mmio_write_32(DDR_CH2_CTL_REG + DDR_CTL_OFFSET, save_reg_val[6]);
			mmio_write_32(DDR_CH3_CTL_REG + DDR_CTL_OFFSET, save_reg_val[7]);
		} else {
			mmio_write_32(DDR_CH0_CTL_REG, save_reg_val[0]);
			mmio_write_32(DDR_CH1_CTL_REG, save_reg_val[1]);
			mmio_write_32(DDR_CH2_CTL_REG, save_reg_val[2]);
			mmio_write_32(DDR_CH3_CTL_REG, save_reg_val[3]);
			mmio_write_32(DDR_CH0_CTL_REG + DDR_CTL_OFFSET, save_reg_val[4]);
			mmio_write_32(DDR_CH1_CTL_REG + DDR_CTL_OFFSET, save_reg_val[5]);
			mmio_write_32(DDR_CH2_CTL_REG + DDR_CTL_OFFSET, save_reg_val[6]);
			mmio_write_32(DDR_CH3_CTL_REG + DDR_CTL_OFFSET, save_reg_val[7]);
		}
	} else {
		save_register();
		if (buf->ChannelMask == 0xc) {
			mmio_write_32(DDR_CH2_CTL_REG, 0);
			mmio_write_32(DDR_CH3_CTL_REG, 0);
			ddrlp_clear_bits(DDR_CH2_CTL_REG + DDR_CTL_OFFSET, DDR_CTL_MASK);
			ddrlp_clear_bits(DDR_CH3_CTL_REG + DDR_CTL_OFFSET, DDR_CTL_MASK);
		} else {
			mmio_write_32(DDR_CH0_CTL_REG, 0);
			mmio_write_32(DDR_CH1_CTL_REG, 0);
			mmio_write_32(DDR_CH2_CTL_REG, 0);
			mmio_write_32(DDR_CH3_CTL_REG, 0);
			ddrlp_clear_bits(DDR_CH0_CTL_REG + DDR_CTL_OFFSET, DDR_CTL_MASK);
			ddrlp_clear_bits(DDR_CH1_CTL_REG + DDR_CTL_OFFSET, DDR_CTL_MASK);
			ddrlp_clear_bits(DDR_CH2_CTL_REG + DDR_CTL_OFFSET, DDR_CTL_MASK);
			ddrlp_clear_bits(DDR_CH3_CTL_REG + DDR_CTL_OFFSET, DDR_CTL_MASK);
		}
	}
	return 0;
}
