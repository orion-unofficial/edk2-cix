/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2014, Linaro Limited
 */
#ifndef PL011_H
#define PL011_H

#include <types_ext.h>
#include <drivers/serial.h>

#define PL011_REG_SIZE	0x1000

struct cix_pl011_data {
	struct io_pa_va base;
	struct serial_chip chip;
};

void cix_pl011_init(struct cix_pl011_data *pd, paddr_t pbase, uint32_t uart_clk,
		uint32_t baud_rate);
void cix_pl011_register(struct cix_pl011_data *pd, paddr_t pbase);


#endif /* PL011_H */
