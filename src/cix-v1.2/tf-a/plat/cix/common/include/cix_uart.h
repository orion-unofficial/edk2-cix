/*
 * Copyright (c) 2013-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CIX_UART_H
#define CIX_UART_H

#include <drivers/console.h>

/* CIX_UART Registers */
#define UARTDR                    0x000
#define UARTRSR                   0x004
#define UARTECR                   0x004
#define UARTFR                    0x018
#define UARTIMSC                  0x038
#define UARTRIS                   0x03C
#define UARTICR                   0x044

/* CIX_UART registers (out of the SBSA specification) */
#if !CIX_UART_GENERIC_UART
#define UARTILPR                  0x020
#define UARTIBRD                  0x024
#define UARTFBRD                  0x028
#define UARTLCR_H                 0x02C
#define UARTCR                    0x030
#define UARTIFLS                  0x034
#define UARTMIS                   0x040
#define UARTDMACR                 0x048
#endif /* !Cix UART_GENERIC_UART */

/* Data status bits */
#define UART_DATA_ERROR_MASK      0x0F00

/* Status reg bits */
#define UART_STATUS_ERROR_MASK    0x0F

/* Flag reg bits */
#define CIX_UARTFR_RI           (1 << 8)	/* Ring indicator */
#define CIX_UARTFR_TXFE         (1 << 7)	/* Transmit FIFO empty */
#define CIX_UARTFR_RXFF         (1 << 6)	/* Receive  FIFO full */
#define CIX_UARTFR_TXFF         (1 << 5)	/* Transmit FIFO full */
#define CIX_UARTFR_RXFE         (1 << 4)	/* Receive  FIFO empty */
#define CIX_UARTFR_BUSY         (1 << 3)	/* UART busy */
#define CIX_UARTFR_DCD          (1 << 2)	/* Data carrier detect */
#define CIX_UARTFR_DSR          (1 << 1)	/* Data set ready */
#define CIX_UARTFR_CTS          (1 << 0)	/* Clear to send */

#define CIX_UARTFR_TXFF_BIT	5	/* Transmit FIFO full bit in UARTFR register */
#define CIX_UARTFR_RXFE_BIT	4	/* Receive FIFO empty bit in UARTFR register */
#define CIX_UARTFR_BUSY_BIT	3	/* UART busy bit in UARTFR register */

/* Control reg bits */
#if !CIX_UART_GENERIC_UART
#define CIX_UARTCR_CTSEN        (1 << 15)	/* CTS hardware flow control enable */
#define CIX_UARTCR_RTSEN        (1 << 14)	/* RTS hardware flow control enable */
#define CIX_UARTCR_RTS          (1 << 11)	/* Request to send */
#define CIX_UARTCR_DTR          (1 << 10)	/* Data transmit ready. */
#define CIX_UARTCR_RXE          (1 << 9)	/* Receive enable */
#define CIX_UARTCR_TXE          (1 << 8)	/* Transmit enable */
#define CIX_UARTCR_LBE          (1 << 7)	/* Loopback enable */
#define CIX_UARTCR_UARTEN       (1 << 0)	/* UART Enable */

#if !defined(CIX_UART_LINE_CONTROL)
/* FIFO Enabled / No Parity / 8 Data bit / One Stop Bit */
#define CIX_UART_LINE_CONTROL  (CIX_UARTLCR_H_FEN | CIX_UARTLCR_H_WLEN_8)
#endif

/* Line Control Register Bits */
#define CIX_UARTLCR_H_SPS       (1 << 7)	/* Stick parity select */
#define CIX_UARTLCR_H_WLEN_8    (3 << 5)
#define CIX_UARTLCR_H_WLEN_7    (2 << 5)
#define CIX_UARTLCR_H_WLEN_6    (1 << 5)
#define CIX_UARTLCR_H_WLEN_5    (0 << 5)
#define CIX_UARTLCR_H_FEN       (1 << 4)	/* FIFOs Enable */
#define CIX_UARTLCR_H_STP2      (1 << 3)	/* Two stop bits select */
#define CIX_UARTLCR_H_EPS       (1 << 2)	/* Even parity select */
#define CIX_UARTLCR_H_PEN       (1 << 1)	/* Parity Enable */
#define CIX_UARTLCR_H_BRK       (1 << 0)	/* Send break */

#endif /* !CIX_UART_GENERIC_UART */

#ifndef __ASSEMBLER__

#include <stdint.h>

/*
 * Initialize a new CIX_UART console instance and register it with the console
 * framework. The |console| pointer must point to storage that will be valid
 * for the lifetime of the console, such as a global or static local variable.
 * Its contents will be reinitialized from scratch.
 */
int console_cix_uart_register(uintptr_t baseaddr, uint32_t clock, uint32_t baud,
			   console_t *console);

#endif /*__ASSEMBLER__*/

#endif /* CIX_UART_H */
