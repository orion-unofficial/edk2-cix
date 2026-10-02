/*
 * Copyright (c) 2019, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arm.h>
#include <stdbool.h>
#include <stdint.h>
#include <tee_api_types.h>

#ifndef DRIVERS_SPI_NOR_H
#define DRIVERS_SPI_NOR_H

/* OPCODE */
#define SPI_NOR_OP_WREN 0x06U	  /* Write enable */
#define SPI_NOR_OP_WRSR 0x01U	  /* Write status register 1 byte */
#define SPI_NOR_OP_READ_ID 0x9FU  /* Read JEDEC ID */
#define SPI_NOR_OP_READ_CR 0x35U  /* Read configuration register */
#define SPI_NOR_OP_READ_SR 0x05U  /* Read status register */
#define SPI_NOR_OP_READ_FSR 0x70U /* Read flag status register */
#define SPINOR_OP_RDEAR 0xC8U	  /* Read Extended Address Register */
#define SPINOR_OP_WREAR 0xC5U	  /* Write Extended Address Register */

/* Used for Spansion flashes only. */
#define SPINOR_OP_BRWR 0x17U /* Bank register write */
#define SPINOR_OP_BRRD 0x16U /* Bank register read */

#define SPI_NOR_OP_WRITE 0x02U /* Write data bytes (low frequency) */
#define SPI_NOR_OP_ERASE 0x20U /* Erase data bytes (low frequency) */

#define SPI_NOR_OP_READ 0x03U		/* Read data bytes (low frequency) */
#define SPI_NOR_OP_READ_FAST 0x0BU	/* Read data bytes (high frequency) */
#define SPI_NOR_OP_READ_1_1_2 0x3BU /* Read data bytes (Dual Output SPI) */
#define SPI_NOR_OP_READ_1_2_2 0xBBU /* Read data bytes (Dual I/O SPI) */
#define SPI_NOR_OP_READ_1_1_4 0x6BU /* Read data bytes (Quad Output SPI) */
#define SPI_NOR_OP_READ_1_4_4 0xEBU /* Read data bytes (Quad I/O SPI) */

/* Flags for NOR specific configuration */
#define SPI_NOR_USE_FSR BIT(0)
#define SPI_NOR_USE_BANK BIT(1)

#define READ_INDEX_1_1_1 0
#define READ_INDEX_1_1_2 1
#define READ_INDEX_1_1_4 2
#define JEDEC_ID_LEN 6

#define SPI_MEM_BUSWIDTH_1_LINE 1U
#define SPI_MEM_BUSWIDTH_2_LINE 2U
#define SPI_MEM_BUSWIDTH_4_LINE 4U

/*
 * enum spi_mem_data_dir - Describes the direction of a SPI memory data
 *			   transfer from the controller perspective.
 * @SPI_MEM_DATA_IN: data coming from the SPI memory.
 * @SPI_MEM_DATA_OUT: data sent to the SPI memory.
 */
enum spi_mem_data_dir
{
	SPI_MEM_NO_DATA,
	SPI_MEM_DATA_IN,
	SPI_MEM_DATA_OUT,
};

/*
 * struct spi_mem_op - Describes a SPI memory operation.
 *
 * @cmd.buswidth: Number of IO lines used to transmit the command.
 * @cmd.opcode: Operation opcode.
 * @addr.nbytes: Number of address bytes to send. Can be zero if the operation
 *		 does not need to send an address.
 * @addr.buswidth: Number of IO lines used to transmit the address.
 * @addr.val: Address value. This value is always sent MSB first on the bus.
 *	      Note that only @addr.nbytes are taken into account in this
 *	      address value, so users should make sure the value fits in the
 *	      assigned number of bytes.
 * @dummy.nbytes: Number of dummy bytes to send after an opcode or address. Can
 *		  be zero if the operation does not require dummy bytes.
 * @dummy.buswidth: Number of IO lines used to transmit the dummy bytes.
 * @data.buswidth: Number of IO lines used to send/receive the data.
 * @data.dir: Direction of the transfer.
 * @data.nbytes: Number of data bytes to transfer.
 * @data.buf: Input or output data buffer depending on data::dir.
 */
struct spi_mem_op
{
	struct
	{
		uint8_t buswidth;
		uint8_t opcode;
	} cmd;

	struct
	{
		uint8_t nbytes;
		uint8_t buswidth;
		uint64_t val;
	} addr;

	struct
	{
		uint8_t nbytes;
		uint8_t buswidth;
	} dummy;

	struct
	{
		uint8_t buswidth;
		enum spi_mem_data_dir dir;
		uint32_t nbytes;
		void *buf;
	} data;
};

/* SPI mode flags */
#define SPI_CPHA BIT(0)		 /* clock phase */
#define SPI_CPOL BIT(1)		 /* clock polarity */
#define SPI_CS_HIGH BIT(2)	 /* CS active high */
#define SPI_LSB_FIRST BIT(3) /* per-word bits-on-wire */
#define SPI_3WIRE BIT(4)	 /* SI/SO signals shared */
#define SPI_PREAMBLE BIT(5)	 /* Skip preamble bytes */
#define SPI_TX_DUAL BIT(6)	 /* transmit with 2 wires */
#define SPI_TX_QUAD BIT(7)	 /* transmit with 4 wires */
#define SPI_RX_DUAL BIT(8)	 /* receive with 2 wires */
#define SPI_RX_QUAD BIT(9)	 /* receive with 4 wires */

struct spi_nor
{
	uint8_t read_dummy;
	bool is_secure;
	uint32_t size;
	uint32_t sector_size;
	uint32_t erase_size;

	int (*init)(struct spi_nor *nor __unused);
	int (*erase)(struct spi_nor *nor __unused, uint32_t addr, uint32_t len);
	int (*write)(struct spi_nor *nor __unused, uint8_t *buffer, uint32_t offset, uint32_t length);
	int (*read)(struct spi_nor *nor __unused, uint8_t *buffer, uint32_t offset, uint32_t length);
};

void spi_nor_register_normal(struct spi_nor *nor);
void spi_nor_register_w77(struct spi_nor *nor);

int spi_mem_exec_op(const struct spi_mem_op *op);
int spi_nor_read_jedec(uint8_t *id);
#endif /* DRIVERS_SPI_NOR_H */
