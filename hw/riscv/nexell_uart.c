/*
 * QEMU model of the UART on the SiFive E300 and U500 series SOCs.
 *
 * Copyright (c) 2016 Stefan O'Rear
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

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "chardev/char.h"
#include "chardev/char-fe.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/nexell_uart.h"

/*
 * Not yet implemented:
 *
 * Transmit FIFO using "qemu/fifo8.h"
 * NEXELL_UART_IE_TXWM interrupts
 * NEXELL_UART_IE_RXWM interrupts must honor fifo watermark
 * Rx FIFO watermark interrupt trigger threshold
 * Tx FIFO watermark interrupt trigger threshold.
 */

static void update_irq(NexellUARTState *s)
{
    int cond = 0;
    if ((s->ie & NEXELL_UART_IE_RXWM) && s->rx_fifo_len) {
        cond = 1;
    }
    if (cond) {
        qemu_irq_raise(s->irq);
    } else {
        qemu_irq_lower(s->irq);
    }
}

static uint64_t
uart_read(void *opaque, hwaddr addr, unsigned int size)
{
    NexellUARTState *s = opaque;
    unsigned char r;
    switch (addr) {
    case NEXELL_UART_RXFIFO:
        if (s->rx_fifo_len) {
            r = s->rx_fifo[0];
            memmove(s->rx_fifo, s->rx_fifo + 1, s->rx_fifo_len - 1);
            s->rx_fifo_len--;
            qemu_chr_fe_accept_input(&s->chr);
            update_irq(s);
            return r;
        }
        return 0x80000000;

    case NEXELL_UART_TXFIFO:
        return 0; /* Should check tx fifo */
    case NEXELL_UART_IE:
        return s->ie;
    case NEXELL_UART_IP:
        return s->rx_fifo_len ? NEXELL_UART_IP_RXWM : 0;
    case NEXELL_UART_TXCTRL:
        return s->txctrl;
    case NEXELL_UART_RXCTRL:
        return s->rxctrl;
    case NEXELL_UART_DIV:
        return s->div;
    }

    hw_error("%s: bad read: addr=0x%x\n",
        __func__, (int)addr);
    return 0;
}

static void
uart_write(void *opaque, hwaddr addr,
           uint64_t val64, unsigned int size)
{
    NexellUARTState *s = opaque;
    uint32_t value = val64;
    unsigned char ch = value;

    switch (addr) {
    case NEXELL_UART_TXFIFO:
        qemu_chr_fe_write(&s->chr, &ch, 1);
        return;
    case NEXELL_UART_IE:
        s->ie = val64;
        update_irq(s);
        return;
    case NEXELL_UART_TXCTRL:
        s->txctrl = val64;
        return;
    case NEXELL_UART_RXCTRL:
        s->rxctrl = val64;
        return;
    case NEXELL_UART_DIV:
        s->div = val64;
        return;
    }
    hw_error("%s: bad write: addr=0x%x v=0x%x\n",
        __func__, (int)addr, (int)value);
}

static const MemoryRegionOps uart_ops = {
    .read = uart_read,
    .write = uart_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};

static void uart_rx(void *opaque, const uint8_t *buf, int size)
{
    NexellUARTState *s = opaque;

    /* Got a byte.  */
    if (s->rx_fifo_len >= sizeof(s->rx_fifo)) {
        printf("WARNING: UART dropped char.\n");
        return;
    }
    s->rx_fifo[s->rx_fifo_len++] = *buf;

    update_irq(s);
}

static int uart_can_rx(void *opaque)
{
    NexellUARTState *s = opaque;

    return s->rx_fifo_len < sizeof(s->rx_fifo);
}

static void uart_event(void *opaque, int event)
{
}

static int uart_be_change(void *opaque)
{
    NexellUARTState *s = opaque;

    qemu_chr_fe_set_handlers(&s->chr, uart_can_rx, uart_rx, uart_event,
        uart_be_change, s, NULL, true);

    return 0;
}

/*
 * Create UART device.
 */
NexellUARTState *nexell_uart_create(MemoryRegion *address_space, hwaddr base,
    Chardev *chr, qemu_irq irq)
{
    NexellUARTState *s = g_malloc0(sizeof(NexellUARTState));
    s->irq = irq;
    qemu_chr_fe_init(&s->chr, chr, &error_abort);
    qemu_chr_fe_set_handlers(&s->chr, uart_can_rx, uart_rx, uart_event,
        uart_be_change, s, NULL, true);
    memory_region_init_io(&s->mmio, NULL, &uart_ops, s,
                          TYPE_NEXELL_UART, NEXELL_UART_MAX);
    memory_region_add_subregion(address_space, base, &s->mmio);
    return s;
}
