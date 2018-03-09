/*
 * SiFive UART interface
 *
 * Copyright (c) 2016 Stefan O'Rear
 * Copyright (c) 2017 SiFive, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_NEXELL_UART_H
#define HW_NEXELL_UART_H

enum {
    NEXELL_UART_TXFIFO        = 0,
    NEXELL_UART_RXFIFO        = 4,
    NEXELL_UART_TXCTRL        = 8,
    NEXELL_UART_TXMARK        = 10,
    NEXELL_UART_RXCTRL        = 12,
    NEXELL_UART_RXMARK        = 14,
    NEXELL_UART_IE            = 16,
    NEXELL_UART_IP            = 20,
    NEXELL_UART_DIV           = 24,
    NEXELL_UART_MAX           = 32
};

enum {
    NEXELL_UART_IE_TXWM       = 1, /* Transmit watermark interrupt enable */
    NEXELL_UART_IE_RXWM       = 2  /* Receive watermark interrupt enable */
};

enum {
    NEXELL_UART_IP_TXWM       = 1, /* Transmit watermark interrupt pending */
    NEXELL_UART_IP_RXWM       = 2  /* Receive watermark interrupt pending */
};

#define TYPE_NEXELL_UART "riscv.nexell.uart"

#define NEXELL_UART(obj) \
    OBJECT_CHECK(NexellUARTState, (obj), TYPE_NEXELL_UART)

typedef struct NexellUARTState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    qemu_irq irq;
    MemoryRegion mmio;
    CharBackend chr;
    uint8_t rx_fifo[8];
    unsigned int rx_fifo_len;
    uint32_t ie;
    uint32_t ip;
    uint32_t txctrl;
    uint32_t rxctrl;
    uint32_t div;
} NexellUARTState;

NexellUARTState *nexell_uart_create(MemoryRegion *address_space, hwaddr base,
    Chardev *chr, qemu_irq irq);

#endif
