/*
 *  Copyright 2024 Cix Technology Group Co., Ltd.
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Cix Technology Group
 * Co., Ltd., and contain its proprietary and confidential information.
 */

#include <inttypes.h>

#include <common/debug.h>
#include <common/fdt_wrappers.h>
#include <drivers/clk.h>
#include <drivers/delay_timer.h>
#include <drivers/spi_mem.h>
#include <lib/mmio.h>
#include <lib/utils_def.h>
#include <libfdt.h>

#include <platform_def.h>

struct cdns_xspi_dev {
  void *iobase;
  void *sdmabase;
  int cur_cs;
  unsigned int sdmasize;
  void *in_buffer;
  void *out_buffer;
} g_cdns_xspi;


#define CDNS_XSPI_AXI_WIDTH_BYTES 4
#define CDNS_XSPI_MAGIC_NUM_VALUE 0x6523
#define CDNS_XSPI_BASE 0x4180000
#define CDNS_XSPI_SDMA 0x10000
#define CDNS_XSPI_FCH_BASE 0x4160000
#define QSPI_BUSY_TIMEOUT_US 1000
#define __bf_shf(x) (__builtin_ffsll(x) - 1)
#define FIELD_PREP(_mask, _val)                                                \
  ({ ((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask); })

#define FIELD_GET(_mask, _reg)                                                 \
  ({ (typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask)); })

static inline unsigned int ilog2(unsigned int v) {
  unsigned int r;
  unsigned int shift;
  r = (v > 0xffff) << 4;
  v >>= r;
  shift = (v > 0xff) << 3;
  v >>= shift;
  r |= shift;
  shift = (v > 0xf) << 2;
  v >>= shift;
  r |= shift;
  shift = (v > 0x3) << 1;
  v >>= shift;
  r |= shift;
  r |= (v >> 1);
  return r;
}

/* Command registers */
#define CDNS_XSPI_CMD_REG_0 0x0000
#define CDNS_XSPI_CMD_REG_1 0x0004
#define CDNS_XSPI_CMD_REG_2 0x0008
#define CDNS_XSPI_CMD_REG_3 0x000C
#define CDNS_XSPI_CMD_REG_4 0x0010
#define CDNS_XSPI_CMD_REG_5 0x0014

/* Command status registers */
#define CDNS_XSPI_CMD_STATUS_REG 0x0044

/* Controller status register */
#define CDNS_XSPI_CTRL_STATUS_REG 0x0100
#define CDNS_XSPI_INIT_COMPLETED BIT(16)
#define CDNS_XSPI_INIT_LEGACY BIT(9)
#define CDNS_XSPI_INIT_FAIL BIT(8)
#define CDNS_XSPI_CTRL_BUSY BIT(7)

/* clk freq and mode register */
#define CDNS_XSPI_CLK_MODE_SETTINGS 0x1008
#define CDNS_XSPI_CLK_DIVIDER_MASK GENMASK(27, 24)

/* Controller interrupt status register */
#define CDNS_XSPI_INTR_STATUS_REG 0x0110
#define CDNS_XSPI_STIG_DONE BIT(23)
#define CDNS_XSPI_SDMA_ERROR BIT(22)
#define CDNS_XSPI_SDMA_TRIGGER BIT(21)
#define CDNS_XSPI_CMD_IGNRD_EN BIT(20)
#define CDNS_XSPI_DDMA_TERR_EN BIT(18)
#define CDNS_XSPI_CDMA_TREE_EN BIT(17)
#define CDNS_XSPI_CTRL_IDLE_EN BIT(16)

#define CDNS_XSPI_TRD_COMP_INTR_STATUS 0x0120
#define CDNS_XSPI_TRD_ERR_INTR_STATUS 0x0130
#define CDNS_XSPI_TRD_ERR_INTR_EN 0x0134

/* Controller interrupt enable register */
#define CDNS_XSPI_INTR_ENABLE_REG 0x0114
#define CDNS_XSPI_INTR_EN BIT(31)
#define CDNS_XSPI_STIG_DONE_EN BIT(23)
#define CDNS_XSPI_SDMA_ERROR_EN BIT(22)
#define CDNS_XSPI_SDMA_TRIGGER_EN BIT(21)

#define CDNS_XSPI_INTR_MASK                                                    \
  (CDNS_XSPI_INTR_EN | CDNS_XSPI_STIG_DONE_EN | CDNS_XSPI_SDMA_ERROR_EN |      \
   CDNS_XSPI_SDMA_TRIGGER_EN)

/* Controller config register */
#define CDNS_XSPI_CTRL_CONFIG_REG 0x0230
#define CDNS_XSPI_CTRL_WORK_MODE GENMASK(6, 5)

#define CDNS_XSPI_WORK_MODE_DIRECT 0
#define CDNS_XSPI_WORK_MODE_STIG 1
#define CDNS_XSPI_WORK_MODE_ACMD 3

/* SDMA trigger transaction registers */
#define CDNS_XSPI_SDMA_SIZE_REG 0x0240
#define CDNS_XSPI_SDMA_TRD_INFO_REG 0x0244
#define CDNS_XSPI_SDMA_DIR BIT(8)

/* Controller features register */
#define CDNS_XSPI_CTRL_FEATURES_REG 0x0F04
#define CDNS_XSPI_NUM_BANKS GENMASK(25, 24)
#define CDNS_XSPI_DMA_DATA_WIDTH BIT(21)
#define CDNS_XSPI_NUM_THREADS GENMASK(3, 0)

/* Controller version register */
#define CDNS_XSPI_CTRL_VERSION_REG 0x0F00
#define CDNS_XSPI_MAGIC_NUM GENMASK(31, 16)
#define CDNS_XSPI_CTRL_REV GENMASK(7, 0)

/* STIG Profile 1.0 instruction fields (split into registers) */
#define CDNS_XSPI_CMD_INSTR_TYPE GENMASK(6, 0)
#define CDNS_XSPI_CMD_P1_R1_ADDR0 GENMASK(31, 24)
#define CDNS_XSPI_CMD_P1_R2_ADDR1 GENMASK(7, 0)
#define CDNS_XSPI_CMD_P1_R2_ADDR2 GENMASK(15, 8)
#define CDNS_XSPI_CMD_P1_R2_ADDR3 GENMASK(23, 16)
#define CDNS_XSPI_CMD_P1_R2_ADDR4 GENMASK(31, 24)
#define CDNS_XSPI_CMD_P1_R3_ADDR5 GENMASK(7, 0)
#define CDNS_XSPI_CMD_P1_R3_CMD GENMASK(23, 16)
#define CDNS_XSPI_CMD_P1_R3_NUM_ADDR_BYTES GENMASK(30, 28)
#define CDNS_XSPI_CMD_P1_R4_ADDR_IOS GENMASK(1, 0)
#define CDNS_XSPI_CMD_P1_R4_CMD_IOS GENMASK(9, 8)
#define CDNS_XSPI_CMD_P1_R4_BANK GENMASK(14, 12)

/* STIG data sequence instruction fields (split into registers) */
#define CDNS_XSPI_CMD_DSEQ_R2_DCNT_L GENMASK(31, 16)
#define CDNS_XSPI_CMD_DSEQ_R3_DCNT_H GENMASK(15, 0)
#define CDNS_XSPI_CMD_DSEQ_R3_NUM_OF_DUMMY GENMASK(25, 20)
#define CDNS_XSPI_CMD_DSEQ_R4_BANK GENMASK(14, 12)
#define CDNS_XSPI_CMD_DSEQ_R4_DATA_IOS GENMASK(9, 8)
#define CDNS_XSPI_CMD_DSEQ_R4_DIR BIT(4)

/* STIG command status fields */
#define CDNS_XSPI_CMD_STATUS_COMPLETED BIT(15)
#define CDNS_XSPI_CMD_STATUS_FAILED BIT(14)
#define CDNS_XSPI_CMD_STATUS_DQS_ERROR BIT(3)
#define CDNS_XSPI_CMD_STATUS_CRC_ERROR BIT(2)
#define CDNS_XSPI_CMD_STATUS_BUS_ERROR BIT(1)
#define CDNS_XSPI_CMD_STATUS_INV_SEQ_ERROR BIT(0)

#define CDNS_XSPI_STIG_DONE_FLAG BIT(0)
#define CDNS_XSPI_TRD_STATUS 0x0104

/* Helper macros for filling command registers */
#define CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_1(op, data_phase)                       \
  (FIELD_PREP(CDNS_XSPI_CMD_INSTR_TYPE, (data_phase)                           \
                                            ? CDNS_XSPI_STIG_INSTR_TYPE_1      \
                                            : CDNS_XSPI_STIG_INSTR_TYPE_0) |   \
   FIELD_PREP(CDNS_XSPI_CMD_P1_R1_ADDR0, (op)->addr.val & 0xff))

#define CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_2(op)                                   \
  (FIELD_PREP(CDNS_XSPI_CMD_P1_R2_ADDR1, ((op)->addr.val >> 8) & 0xFF) |       \
   FIELD_PREP(CDNS_XSPI_CMD_P1_R2_ADDR2, ((op)->addr.val >> 16) & 0xFF) |      \
   FIELD_PREP(CDNS_XSPI_CMD_P1_R2_ADDR3, ((op)->addr.val >> 24) & 0xFF) |      \
   FIELD_PREP(CDNS_XSPI_CMD_P1_R2_ADDR4, ((op)->addr.val >> 32) & 0xFF))

#define CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_3(op)                                   \
  (FIELD_PREP(CDNS_XSPI_CMD_P1_R3_ADDR5, ((op)->addr.val >> 40) & 0xFF) |      \
   FIELD_PREP(CDNS_XSPI_CMD_P1_R3_CMD, (op)->cmd.opcode) |                     \
   FIELD_PREP(CDNS_XSPI_CMD_P1_R3_NUM_ADDR_BYTES, (op)->addr.nbytes))

#define CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_4(op, chipsel)                          \
  (FIELD_PREP(CDNS_XSPI_CMD_P1_R4_ADDR_IOS, ilog2((op)->addr.buswidth)) |      \
   FIELD_PREP(CDNS_XSPI_CMD_P1_R4_CMD_IOS, ilog2((op)->cmd.buswidth)) |        \
   FIELD_PREP(CDNS_XSPI_CMD_P1_R4_BANK, chipsel))

#define CDNS_XSPI_CMD_FLD_DSEQ_CMD_1(op)                                       \
  FIELD_PREP(CDNS_XSPI_CMD_INSTR_TYPE, CDNS_XSPI_STIG_INSTR_TYPE_DATA_SEQ)

#define CDNS_XSPI_CMD_FLD_DSEQ_CMD_2(op)                                       \
  FIELD_PREP(CDNS_XSPI_CMD_DSEQ_R2_DCNT_L, (op)->data.nbytes & 0xFFFF)

#define CDNS_XSPI_CMD_FLD_DSEQ_CMD_3(op)                                       \
  (FIELD_PREP(CDNS_XSPI_CMD_DSEQ_R3_DCNT_H,                                    \
              ((op)->data.nbytes >> 16) & 0xffff) |                            \
 FIELD_PREP(CDNS_XSPI_CMD_DSEQ_R3_NUM_OF_DUMMY, \
                  (op)->dummy.buswidth != 0 ? \
                  (((op)->dummy.nbytes * 8) / (op)->dummy.buswidth) : \
                  0))

#define CDNS_XSPI_CMD_FLD_DSEQ_CMD_4(op, chipsel)                              \
  (FIELD_PREP(CDNS_XSPI_CMD_DSEQ_R4_BANK, chipsel) |                           \
   FIELD_PREP(CDNS_XSPI_CMD_DSEQ_R4_DATA_IOS, ilog2((op)->data.buswidth)) |    \
   FIELD_PREP(CDNS_XSPI_CMD_DSEQ_R4_DIR, ((op)->data.dir == SPI_MEM_DATA_IN)   \
                                             ? CDNS_XSPI_STIG_CMD_DIR_READ     \
                                             : CDNS_XSPI_STIG_CMD_DIR_WRITE))

enum cdns_xspi_stig_instr_type {
  CDNS_XSPI_STIG_INSTR_TYPE_0,
  CDNS_XSPI_STIG_INSTR_TYPE_1,
  CDNS_XSPI_STIG_INSTR_TYPE_DATA_SEQ = 127,
};

enum cdns_xspi_sdma_dir {
  CDNS_XSPI_SDMA_DIR_READ,
  CDNS_XSPI_SDMA_DIR_WRITE,
};

enum cdns_xspi_stig_cmd_dir {
  CDNS_XSPI_STIG_CMD_DIR_READ,
  CDNS_XSPI_STIG_CMD_DIR_WRITE,
};

static int cdns_xspi_wait_for_controller_idle(void) {
  uint64_t timeout = timeout_init_us(QSPI_BUSY_TIMEOUT_US);
  uint32_t value = 0;
  value = mmio_read_32(CDNS_XSPI_BASE + CDNS_XSPI_CTRL_STATUS_REG) &
          CDNS_XSPI_CTRL_BUSY;
  while (value != 0U) {
     if (timeout_elapsed(timeout)) {
        ERROR("%s: busy timeout\n", __func__);
        return -ETIMEDOUT;
    }
    value = mmio_read_32(CDNS_XSPI_BASE + CDNS_XSPI_CTRL_STATUS_REG) &
            CDNS_XSPI_CTRL_BUSY;
  }

  return 0;
}

static void cdns_xspi_trigger_command(uint32_t cmd_regs[6]) {
  mmio_write_32(CDNS_XSPI_BASE + CDNS_XSPI_CMD_REG_5, cmd_regs[5]);
  mmio_write_32(CDNS_XSPI_BASE + CDNS_XSPI_CMD_REG_4, cmd_regs[4]);
  mmio_write_32(CDNS_XSPI_BASE + CDNS_XSPI_CMD_REG_3, cmd_regs[3]);
  mmio_write_32(CDNS_XSPI_BASE + CDNS_XSPI_CMD_REG_2, cmd_regs[2]);
  mmio_write_32(CDNS_XSPI_BASE + CDNS_XSPI_CMD_REG_1, cmd_regs[1]);
  mmio_write_32(CDNS_XSPI_BASE + CDNS_XSPI_CMD_REG_0, cmd_regs[0]);
}

static int cdns_xspi_check_command_status() {
  int ret = 0;
  uint32_t cmd_status = mmio_read_32(CDNS_XSPI_BASE + CDNS_XSPI_CMD_STATUS_REG);

  if (cmd_status & CDNS_XSPI_CMD_STATUS_COMPLETED) {
    if ((cmd_status & CDNS_XSPI_CMD_STATUS_FAILED) != 0) {
      if (cmd_status & CDNS_XSPI_CMD_STATUS_DQS_ERROR) {
        ERROR("Incorrect DQS pulses detected\n");
        ret = -EIO;
      }
      if (cmd_status & CDNS_XSPI_CMD_STATUS_CRC_ERROR) {
        ERROR("CRC error received\n");
        ret = -EIO;
      }
      if (cmd_status & CDNS_XSPI_CMD_STATUS_BUS_ERROR) {
        ERROR("Error resp on system DMA interface\n");
        ret = -EIO;
      }
      if (cmd_status & CDNS_XSPI_CMD_STATUS_INV_SEQ_ERROR) {
        ERROR("Invalid command sequence detected\n");
        ret = -EIO;
      }
    }
  } else {
    ERROR("Fatal err - command not completed\n");
    ret = -EIO;
  }

  return ret;
}

static void cdns_xspi_set_interrupts(bool enabled) {
  uint32_t intr_enable;

  intr_enable = mmio_read_32(CDNS_XSPI_BASE + CDNS_XSPI_INTR_ENABLE_REG);
  if (enabled)
    intr_enable |= CDNS_XSPI_INTR_MASK;
  else
    intr_enable &= ~CDNS_XSPI_INTR_MASK;
  mmio_write_32(CDNS_XSPI_BASE + CDNS_XSPI_INTR_ENABLE_REG, intr_enable);
}

static void ioread8_rep(uint32_t addr, void *buffer,
			  unsigned int count)
{
  if (count) {
    uint8_t *buf = buffer;
    do {
      uint8_t x = mmio_read_8(addr);
      *buf++ = x;
    } while (--count);
  }
}

static void ioread32_rep(uint32_t addr, void *buffer,
                          unsigned int count)
{
  if (count) {
    uint32_t *buf = buffer;
    do {
      uint32_t x = mmio_read_32(addr);
      *buf++ = x;
    } while (--count);
  }
}

static void iowrite8_rep(uint32_t addr, const void *buffer,
			   unsigned int count)
{
  if (count) {
    const uint8_t *buf = buffer;

    do {
      mmio_write_8(addr, *buf++);
      addr++;
    } while (--count);
  }
}

static void cdns_xspi_sdma_handle(struct cdns_xspi_dev *cdns_xspi) {
  uint32_t sdma_size, sdma_trd_info;
  uint8_t sdma_dir;
  uint32_t length_4;

  sdma_size = mmio_read_32(CDNS_XSPI_BASE + CDNS_XSPI_SDMA_SIZE_REG);
  sdma_trd_info = mmio_read_32(CDNS_XSPI_BASE + CDNS_XSPI_SDMA_TRD_INFO_REG);
  sdma_dir = FIELD_GET(CDNS_XSPI_SDMA_DIR, sdma_trd_info);
  length_4 = sdma_size / CDNS_XSPI_AXI_WIDTH_BYTES;

  switch (sdma_dir) {
  case CDNS_XSPI_SDMA_DIR_READ:
	if(sdma_size < CDNS_XSPI_AXI_WIDTH_BYTES)
		ioread8_rep(CDNS_XSPI_SDMA, cdns_xspi->in_buffer, sdma_size);
	else {
		ioread32_rep(CDNS_XSPI_SDMA, cdns_xspi->in_buffer, length_4);
		cdns_xspi->in_buffer += length_4 * CDNS_XSPI_AXI_WIDTH_BYTES;
		if(sdma_size % CDNS_XSPI_AXI_WIDTH_BYTES)
			ioread8_rep(CDNS_XSPI_SDMA, cdns_xspi->in_buffer, (sdma_size % CDNS_XSPI_AXI_WIDTH_BYTES));
	}
    break;

  case CDNS_XSPI_SDMA_DIR_WRITE:
    iowrite8_rep(CDNS_XSPI_SDMA,
            cdns_xspi->out_buffer, sdma_size);
    break;
  }
}

static int __cdns_xspi_mem_op_execute(struct spi_mem_op *op) {
  int ret;
  enum spi_mem_data_dir dir = op->data.dir;
  bool data_phase = (dir != SPI_MEM_NO_DATA);
  uint32_t cmd_regs[6];
  uint32_t cmd_status;

VERBOSE("cmd:%x,cmd.width:%x,addr.nbytes:%x,addr.width:%x,addr.val:%lx,\
	dummy.width:%x,dummy.nbytes:%x,data.width:%x,data.nbytes:%x,dir:\
	%d\n",op->cmd.opcode, op->cmd.buswidth, op->addr.nbytes,op->addr.buswidth,\
	op->addr.val,op->dummy.buswidth, op->dummy.nbytes,op->data.buswidth, op->data.nbytes,dir);

  if (op->data.nbytes >= g_cdns_xspi.sdmasize) {
    ERROR("data nbytes too big 0x%x\n", op->data.nbytes);
    return -EIO;
  }
  ret = cdns_xspi_wait_for_controller_idle();
  if (ret < 0)
    return -EIO;

  mmio_write_32(CDNS_XSPI_BASE + CDNS_XSPI_CTRL_CONFIG_REG,
                FIELD_PREP(CDNS_XSPI_CTRL_WORK_MODE, CDNS_XSPI_WORK_MODE_STIG));
  memset(cmd_regs, 0, sizeof(cmd_regs));
  cmd_regs[1] = CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_1(op, data_phase);
  cmd_regs[2] = CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_2(op);
  cmd_regs[3] = CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_3(op);
  cmd_regs[4] = CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_4(op, g_cdns_xspi.cur_cs);

  cdns_xspi_trigger_command(cmd_regs);

  if (data_phase) {
    cmd_regs[0] = CDNS_XSPI_STIG_DONE_FLAG;
    cmd_regs[1] = CDNS_XSPI_CMD_FLD_DSEQ_CMD_1(op);
    cmd_regs[2] = CDNS_XSPI_CMD_FLD_DSEQ_CMD_2(op);
    cmd_regs[3] = CDNS_XSPI_CMD_FLD_DSEQ_CMD_3(op);
    cmd_regs[4] = CDNS_XSPI_CMD_FLD_DSEQ_CMD_4(op, g_cdns_xspi.cur_cs);
    g_cdns_xspi.in_buffer = op->data.buf;
    g_cdns_xspi.out_buffer = op->data.buf;

    cdns_xspi_trigger_command(cmd_regs);
    cdns_xspi_sdma_handle(&g_cdns_xspi);
  }

  ret = cdns_xspi_wait_for_controller_idle();
  if (ret < 0)
    return -EIO;

  cmd_status = cdns_xspi_check_command_status();
  if (cmd_status)
    return -EPROTO;

  return ret;
}

static int cdns_xspi_read_flash_size(uint32_t *size)
{
	struct spi_mem_op op;
	uint8_t id[3];
	int ret = 0;

	memset(&op, 0,sizeof(struct spi_mem_op));
	op.cmd.opcode = 0x9F;
	op.cmd.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	op.data.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	op.data.dir = SPI_MEM_DATA_IN;
	op.data.nbytes = 3;
	op.data.buf = id;
	ret = __cdns_xspi_mem_op_execute(&op);
	*size  = 2 << id[2];
	VERBOSE("%s jedec=0x%x,0x%x,0x%x\n",__func__,id[0],id[1],id[2]);
	return ret;
}

static bool is_cross_regional(uint64_t addr_val,unsigned int len,uint8_t *cur_section_num)
{
	uint8_t section_num = 0;
	uint64_t end_addr = addr_val + len;
	uint32_t size = 0;
	static uint8_t max_section_num = 0;
	static bool already_read_jedec_id = false;

	if(already_read_jedec_id == false){
		cdns_xspi_read_flash_size(&size);
		max_section_num = size / SZ_2M;
		already_read_jedec_id = true;
	}
	for (section_num = 1; section_num < max_section_num; section_num++) {
		if ((addr_val < (section_num * SZ_2M)) && (end_addr > (section_num * SZ_2M))) {
			*cur_section_num = section_num;
			return true;
		}
	}
	return false;
}

static int cdns_xspi_mem_op_execute(struct spi_mem_op *op)
{
	uint8_t cur_section_num;
	uint8_t section_num = 0;
	uint64_t raw_addr = op->addr.val;
	uint32_t raw_len = op->data.nbytes;
	uint32_t remaining_bytes = raw_len;
	int ret = 0;

	if(is_cross_regional(op->addr.val,op->data.nbytes,&cur_section_num)) {
		while(remaining_bytes > 0){
			if(section_num == 0){
				op->data.nbytes = cur_section_num * SZ_2M - raw_addr;
				ret +=__cdns_xspi_mem_op_execute(op);
				remaining_bytes = raw_len - (cur_section_num * SZ_2M - raw_addr);
			}
			if(remaining_bytes > SZ_2M){
				op->data.nbytes = SZ_2M;
				remaining_bytes -= SZ_2M;

			}else{
				op->data.nbytes = remaining_bytes;
				remaining_bytes = 0;
			}
			op->data.buf = (uint8_t *)op->data.buf + \
			(cur_section_num + section_num) * SZ_2M - raw_addr;
			op->addr.val = (cur_section_num + section_num) * SZ_2M;
			ret += __cdns_xspi_mem_op_execute(op);
			section_num++;
		}
	}else
		ret = __cdns_xspi_mem_op_execute(op);
	return ret;
}

static int cdns_xspi_claim_bus(unsigned int cs) { return 0; }

static void cdns_xspi_release_bus(void) {}

static int cdns_xspi_set_mode(unsigned int mode) { return 0; }

static int cdns_xspi_set_speed(unsigned int hz) { return 0; }

static const struct spi_bus_ops cdns_xspi_bus_ops = {
    .claim_bus = cdns_xspi_claim_bus,
    .release_bus = cdns_xspi_release_bus,
    .exec_op = cdns_xspi_mem_op_execute,
    .set_speed = cdns_xspi_set_speed,
    .set_mode = cdns_xspi_set_mode,
};

#define XSPI_CLK_NUM 7 /* The last entry is for FPGA */
static uint8_t xspi_bus_clk_table[XSPI_CLK_NUM] = { 25, 20, 50, 33, 16, 12, 5 };

static uint8_t xspi_get_bus_clk(uint8_t clk_index)
{
        uint8_t bus_clk = xspi_bus_clk_table[clk_index];

        if (bus_clk <= 0 || bus_clk > 50) {
                ERROR( "xSPI clk setting is incorrect, using default 25Mhz\n");
                bus_clk = 25;
        }

        return bus_clk;
}

static uint8_t xspi_get_apb_clk()
{
#ifdef CIX_BOARD_FPGA
	return 5;
#elif defined(CIX_BOARD_EMU)
	return 200;
#else
	return 200;
#endif
}

void set_xspi_bus_clock(uint8_t xspi_clk_index)
{
        uint32_t clk_divider = 0;
        uint32_t divider = 0;

	if(xspi_clk_index > XSPI_CLK_NUM)
		xspi_clk_index = 0;

	clk_divider = (xspi_get_apb_clk() / xspi_get_bus_clk(xspi_clk_index)) / 2;

        if (clk_divider == 0)
                clk_divider = 1;
        else if (clk_divider > 8)
                clk_divider = 8;
	divider = (mmio_read_32(CDNS_XSPI_BASE + CDNS_XSPI_CLK_MODE_SETTINGS) & CDNS_XSPI_CLK_DIVIDER_MASK) >> 24;
	if(clk_divider != divider)
		mmio_write_32(CDNS_XSPI_BASE + CDNS_XSPI_CLK_MODE_SETTINGS, FIELD_PREP(CDNS_XSPI_CLK_DIVIDER_MASK, clk_divider));
}

int cdns_xspi_init(void)
{
        uint32_t  val;
        /* Enable xspi function clock */
        val = mmio_read_32(CDNS_XSPI_FCH_BASE + 0x00);
        val |= (0x3 << 10);
        mmio_write_32(CDNS_XSPI_FCH_BASE + 0x00, val);

        /* Enable xspi apb clock */
        val = mmio_read_32(CDNS_XSPI_FCH_BASE + 0x04);
        val |= (0x1 << 10);
        mmio_write_32(CDNS_XSPI_FCH_BASE + 0x04, val);
        /* Release xspi reset signal*/
        val = mmio_read_32(CDNS_XSPI_FCH_BASE + 0x10);
        val |= 0x3;
        mmio_write_32(CDNS_XSPI_FCH_BASE + 0x10, val);

	cdns_xspi_set_interrupts(false);
	g_cdns_xspi.sdmasize = SZ_16M;
	return spi_mem_init(&cdns_xspi_bus_ops);
}
