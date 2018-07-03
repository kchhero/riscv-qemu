/*
 * QEMU RISC-V Nexell Swallow machine interface
 *
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

#ifndef HW_NEXELL_SWALLOW_H
#define HW_NEXELL_SWALLOW_H

#define TYPE_NEXELL_SWALLOW_BOARD "riscv.nexell_swallow"
#define NEXELL_SWALLOW(obj) \
    OBJECT_CHECK(NexellSwallowState, (obj), TYPE_NEXELL_SWALLOW_BOARD)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    RISCVHartArrayState soc;
    DeviceState *plic;
    void *fdt;
    int fdt_size;
} NexellSwallowState;

enum {
    NEXELL_SWALLOW_DEBUG,
    NEXELL_SWALLOW_MROM,
    NEXELL_SWALLOW_TEST,
    NEXELL_SWALLOW_CLINT,
    NEXELL_SWALLOW_PLIC,
    NEXELL_SWALLOW_MMIO,
    NEXELL_SWALLOW_UART9,
    NEXELL_SWALLOW_UART0,
    NEXELL_SWALLOW_SDMMC0,
    NEXELL_SWALLOW_SDMMC1,   
    NEXELL_SWALLOW_SRAM,
    NEXELL_SWALLOW_DRAM
};

enum {
    SDMMC0_IRQ = 27,
    SDMMC1_IRQ = 28,    
    UART0_IRQ = 58,
    IRQ_NDEV = 89,
};

enum {
    NEXELL_SWALLOW_CLOCK_FREQ = 1000000000
};

#define NEXELL_SWALLOW_PLIC_HART_CONFIG "MS"
#define NEXELL_SWALLOW_PLIC_NUM_SOURCES 127
#define NEXELL_SWALLOW_PLIC_NUM_PRIORITIES 7
#define NEXELL_SWALLOW_PLIC_PRIORITY_BASE 0x0
#define NEXELL_SWALLOW_PLIC_PENDING_BASE 0x1000
#define NEXELL_SWALLOW_PLIC_ENABLE_BASE 0x2000
#define NEXELL_SWALLOW_PLIC_ENABLE_STRIDE 0x80
#define NEXELL_SWALLOW_PLIC_CONTEXT_BASE 0x200000
#define NEXELL_SWALLOW_PLIC_CONTEXT_STRIDE 0x1000

#if defined(TARGET_RISCV32)
#define NEXELL_SWALLOW_CPU TYPE_RISCV_CPU_NEXELL_SWALLOW32
#elif defined(TARGET_RISCV64)
#define NEXELL_SWALLOW_CPU TYPE_RISCV_CPU_NEXELL_SWALLOW64
#endif

#endif
