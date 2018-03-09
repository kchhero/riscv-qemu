/*
 * QEMU model of the SDMMC on the SiFive E300 and U500 series SOCs.
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
#include "hw/riscv/nexell_sdmmc.h"

/*
 * Not yet implemented:
 *
 * Transmit FIFO using "qemu/fifo8.h"
 * NEXELL_UART_IE_TXWM interrupts
 * NEXELL_UART_IE_RXWM interrupts must honor fifo watermark
 * Rx FIFO watermark interrupt trigger threshold
 * Tx FIFO watermark interrupt trigger threshold.
 */

/* static void update_irq(NexellSDMMCState *s) */
/* { */
/*     /\* int cond = 0; *\/ */
/*     /\* if ((s->ie & NEXELL_SDMMC_IE_RXWM) && s->rx_fifo_len) { *\/ */
/*     /\*     cond = 1; *\/ */
/*     /\* } *\/ */
/*     /\* if (cond) { *\/ */
/*     /\*     qemu_irq_raise(s->irq); *\/ */
/*     /\* } else { *\/ */
/*     /\*     qemu_irq_lower(s->irq); *\/ */
/*     /\* } *\/ */
/*     printf("[SUKER-QEMU-SDMMC] %s \n", __func__);     */
/* } */

static uint64_t
sdmmc_read(void *opaque, hwaddr addr, unsigned int size)
{
    NexellSDMMCState *s = opaque;

    switch (addr) {
        case NEXELL_SDMMC__PWREN:
            printf("[SUKER-QEMU-SDMMC] %s: PWREN: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN);
            return s->PWREN;
        case NEXELL_SDMMC__CLKENA:
            printf("[SUKER-QEMU-SDMMC] %s: CLKENA: addr=0x%lx,val=%d \n", __func__, addr, s->CLKENA);
            return s->CLKENA;
        /* case NEXELL_SDMMC__PWREN: */
        /*     printf("[SUKER-QEMU-SDMMC] %s: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN); */
        /* case NEXELL_SDMMC__PWREN: */
        /*     printf("[SUKER-QEMU-SDMMC] %s: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN); */
        /* case NEXELL_SDMMC__PWREN: */
        /*     printf("[SUKER-QEMU-SDMMC] %s: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN); */
        /* case NEXELL_SDMMC__PWREN: */
        /*     printf("[SUKER-QEMU-SDMMC] %s: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN); */
        /* case NEXELL_SDMMC__PWREN: */
        /*     printf("[SUKER-QEMU-SDMMC] %s: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN); */
        /* case NEXELL_SDMMC__PWREN: */
        /*     printf("[SUKER-QEMU-SDMMC] %s: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN); */
        /* case NEXELL_SDMMC__PWREN: */
        /*     printf("[SUKER-QEMU-SDMMC] %s: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN); */
        /* case NEXELL_SDMMC__PWREN: */
        /*     printf("[SUKER-QEMU-SDMMC] %s: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN); */
        /* case NEXELL_SDMMC__PWREN: */
        /*     printf("[SUKER-QEMU-SDMMC] %s: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN); */
        /* case NEXELL_SDMMC__PWREN: */
        /*     printf("[SUKER-QEMU-SDMMC] %s: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN); */
        /* case NEXELL_SDMMC__PWREN: */
        /*     printf("[SUKER-QEMU-SDMMC] %s: addr=0x%lx,val=%d \n", __func__, addr, s->PWREN);             */
    }
    
//    printf("[SUKER-QEMU-SDMMC] %s \n", __func__);
    
    
    return 0;
}

static void
sdmmc_write(void *opaque, hwaddr addr,
           uint64_t val64, unsigned int size)
{
    NexellSDMMCState *s = opaque;

    switch (addr) {
        case NEXELL_SDMMC__PWREN:
            s->PWREN = (unsigned int)val64;
            printf("[SUKER-QEMU-SDMMC] %s: PWREN: write to addr=0x%lx, val=0x%lx \n", __func__,addr,val64);
            break;
        case NEXELL_SDMMC__CLKENA:
            s->CLKENA = (unsigned int)val64;
            printf("[SUKER-QEMU-SDMMC] %s: CLKENA: write to addr=0x%lx, val=0x%lx \n", __func__,addr,val64);
            break;
    }    
    //printf("[SUKER-QEMU-SDMMC] %s \n", __func__);
}

static const MemoryRegionOps sdmmc_ops = {
    .read = sdmmc_read,
    .write = sdmmc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};


/* static int sdmmc_be_change(void *opaque) */
/* { */
/*     NexellSDMMCState *s = opaque; */

/*     qemu_chr_fe_set_handlers(&s->chr, sdmmc_can_rx, sdmmc_rx, sdmmc_event, */
/*         sdmmc_be_change, s, NULL, true); */

/*     return 0; */
/* } */

/*
 * Create SDMMC device.
 */
NexellSDMMCState *nexell_sdmmc_create(MemoryRegion *address_space, hwaddr base,
    qemu_irq irq)
{
    NexellSDMMCState *s = g_malloc0(sizeof(NexellSDMMCState));
    s->irq = irq;
    /* qemu_chr_fe_init(&s->chr, chr, &error_abort); */
    /* qemu_chr_fe_set_handlers(&s->chr, sdmmc_can_rx, sdmmc_rx, sdmmc_event, */
    /*     sdmmc_be_change, s, NULL, true); */
    memory_region_init_io(&s->mmio, NULL, &sdmmc_ops, s,
                          TYPE_NEXELL_SDMMC, sizeof(NexellSDMMCState));
    //NEXELL_SDMMC_MAX);
    memory_region_add_subregion(address_space, base, &s->mmio);
    return s;
}
