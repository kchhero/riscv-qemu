/*
 * QEMU RISC-V Nexell_Swallow Board
 *
 * Copyright (c) 2018 Nexell, Inc.
 *
 * RISC-V machine with 16550a UART and Nexell MMIO
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
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/hw.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/sysbus.h"
#include "hw/char/serial.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_htif.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/nexell_plic.h"
#include "hw/riscv/nexell_clint.h"
#include "hw/riscv/nexell_uart.h"
#include "hw/riscv/nexell_swallow.h"
#include "hw/riscv/nexell_mmio.h"
#include "hw/riscv/nexell_sdmmc.h"
#include "hw/riscv/nexell_test.h"
#include "chardev/char.h"
#include "sysemu/arch_init.h"
#include "sysemu/device_tree.h"
#include "exec/address-spaces.h"
#include "elf.h"

#define LIBNX_USE    1

#ifdef LIBNX_USE
#include <dlfcn.h>

void (*nx_init)(void);
#endif

static const struct MemmapEntry {
    hwaddr base;
    hwaddr size;
} nexell_swallow_memmap[] = {
    [NEXELL_SWALLOW_DEBUG] =    {        0x0,     0x1000 },
    [NEXELL_SWALLOW_MROM] =     {    0x1000,     0x10000 },
    [NEXELL_SWALLOW_CLINT] =    {  0x2000000,    0x10000 },
    [NEXELL_SWALLOW_PLIC] =     {  0xc000000, 0x10000000 },
#ifdef LIBNX_USE    
    [NEXELL_SWALLOW_MMIO] =     { 0x40000000, 0x10000000 },
#endif
//    [NEXELL_SWALLOW_UART9] =    { 0x10013000,     0x1000 },
    [NEXELL_SWALLOW_UART0] =    { 0x208A0000,     0x1000 },
    [NEXELL_SWALLOW_SDMMC0] =   { 0x20930000,    0x10000 },
    [NEXELL_SWALLOW_SDMMC1] =   { 0x20940000,    0x10000 },
    
//    [NEXELL_SWALLOW_SRAM] =     { 0x40000000,    0x10000 },
    [NEXELL_SWALLOW_DRAM] =     { 0x80000000,        0x0 },
};

static uint64_t load_kernel(const char *kernel_filename)
{
    uint64_t kernel_entry, kernel_high;

    if (load_elf(kernel_filename, NULL, NULL,
                 &kernel_entry, NULL, &kernel_high,
                 0, EM_RISCV, 1, 0) < 0) {
        error_report("qemu: could not load kernel '%s'", kernel_filename);
        exit(1);
    }
    return kernel_entry;
}

static hwaddr load_initrd(const char *filename, uint64_t mem_size,
                          uint64_t kernel_entry, hwaddr *start)
{
    int size;

    /* We want to put the initrd far enough into RAM that when the
     * kernel is uncompressed it will not clobber the initrd. However
     * on boards without much RAM we must ensure that we still leave
     * enough room for a decent sized initrd, and on boards with large
     * amounts of RAM we must avoid the initrd being so far up in RAM
     * that it is outside lowmem and inaccessible to the kernel.
     * So for boards with less  than 256MB of RAM we put the initrd
     * halfway into RAM, and for boards with 256MB of RAM or more we put
     * the initrd at 128MB.
     */
    *start = kernel_entry + MIN(mem_size / 2, 128 * 1024 * 1024);

    size = load_ramdisk(filename, *start, mem_size - *start);
    if (size == -1) {
        size = load_image_targphys(filename, *start, mem_size - *start);
        if (size == -1) {
            error_report("qemu: could not load ramdisk '%s'", filename);
            exit(1);
        }
    }
    return *start + size;
}

static void *create_fdt(NexellSwallowState *s, const struct MemmapEntry *memmap,
    uint64_t mem_size, const char *cmdline)
{
    void *fdt;
    int cpu;
    uint32_t *cells;
    char *nodename;
    uint32_t plic_phandle, phandle = 1;

    fdt = s->fdt = create_device_tree(&s->fdt_size);
    if (!fdt) {
        error_report("create_device_tree() failed");
        exit(1);
    }

    qemu_fdt_setprop_string(fdt, "/", "model", "nexell-swallow,qemu");
    qemu_fdt_setprop_string(fdt, "/", "compatible", "nexell-swallow");
    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);

    qemu_fdt_add_subnode(fdt, "/soc");
    qemu_fdt_setprop(fdt, "/soc", "ranges", NULL, 0);
    qemu_fdt_setprop_string(fdt, "/soc", "compatible", "nexell-swallow-soc");
    qemu_fdt_setprop_cell(fdt, "/soc", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/soc", "#address-cells", 0x2);

    nodename = g_strdup_printf("/memory@%lx",
        (long)memmap[NEXELL_SWALLOW_DRAM].base);
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_cells(fdt, nodename, "reg",
        memmap[NEXELL_SWALLOW_DRAM].base >> 32, memmap[NEXELL_SWALLOW_DRAM].base,
        mem_size >> 32, mem_size);
    qemu_fdt_setprop_string(fdt, nodename, "device_type", "memory");
    g_free(nodename);

    qemu_fdt_add_subnode(fdt, "/cpus");
    qemu_fdt_setprop_cell(fdt, "/cpus", "timebase-frequency",
                          NEXELL_CLINT_TIMEBASE_FREQ);
    qemu_fdt_setprop_cell(fdt, "/cpus", "#size-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, "/cpus", "#address-cells", 0x1);

    for (cpu = s->soc.num_harts - 1; cpu >= 0; cpu--) {
        int cpu_phandle = phandle++;
        nodename = g_strdup_printf("/cpus/cpu@%d", cpu);
        char *intc = g_strdup_printf("/cpus/cpu@%d/interrupt-controller", cpu);
        char *isa = riscv_isa_string(&s->soc.harts[cpu]);
        qemu_fdt_add_subnode(fdt, nodename);
        qemu_fdt_setprop_cell(fdt, nodename, "clock-frequency",
                              NEXELL_SWALLOW_CLOCK_FREQ);
        qemu_fdt_setprop_string(fdt, nodename, "mmu-type", "riscv,sv48");
        qemu_fdt_setprop_string(fdt, nodename, "riscv,isa", isa);
        qemu_fdt_setprop_string(fdt, nodename, "compatible", "riscv");
        qemu_fdt_setprop_string(fdt, nodename, "status", "okay");
        qemu_fdt_setprop_cell(fdt, nodename, "reg", cpu);
        qemu_fdt_setprop_string(fdt, nodename, "device_type", "cpu");
        qemu_fdt_add_subnode(fdt, intc);
        qemu_fdt_setprop_cell(fdt, intc, "phandle", cpu_phandle);
        qemu_fdt_setprop_cell(fdt, intc, "linux,phandle", cpu_phandle);
        qemu_fdt_setprop_string(fdt, intc, "compatible", "riscv,cpu-intc");
        qemu_fdt_setprop(fdt, intc, "interrupt-controller", NULL, 0);
        qemu_fdt_setprop_cell(fdt, intc, "#interrupt-cells", 1);
        g_free(isa);
        g_free(intc);
        g_free(nodename);
    }

    cells =  g_new0(uint32_t, s->soc.num_harts * 4);
    for (cpu = 0; cpu < s->soc.num_harts; cpu++) {
        nodename =
            g_strdup_printf("/cpus/cpu@%d/interrupt-controller", cpu);
        uint32_t intc_phandle = qemu_fdt_get_phandle(fdt, nodename);
        cells[cpu * 4 + 0] = cpu_to_be32(intc_phandle);
        cells[cpu * 4 + 1] = cpu_to_be32(IRQ_M_SOFT);
        cells[cpu * 4 + 2] = cpu_to_be32(intc_phandle);
        cells[cpu * 4 + 3] = cpu_to_be32(IRQ_M_TIMER);
        g_free(nodename);
    }
    nodename = g_strdup_printf("/soc/clint@%lx",
        (long)memmap[NEXELL_SWALLOW_CLINT].base);
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_string(fdt, nodename, "compatible", "riscv,clint0");
    qemu_fdt_setprop_cells(fdt, nodename, "reg",
        0x0, memmap[NEXELL_SWALLOW_CLINT].base,
        0x0, memmap[NEXELL_SWALLOW_CLINT].size);
    qemu_fdt_setprop(fdt, nodename, "interrupts-extended",
        cells, s->soc.num_harts * sizeof(uint32_t) * 4);
    g_free(cells);
    g_free(nodename);

    plic_phandle = phandle++;
    cells =  g_new0(uint32_t, s->soc.num_harts * 4);
    for (cpu = 0; cpu < s->soc.num_harts; cpu++) {
        nodename =
            g_strdup_printf("/cpus/cpu@%d/interrupt-controller", cpu);
        uint32_t intc_phandle = qemu_fdt_get_phandle(fdt, nodename);
        cells[cpu * 4 + 0] = cpu_to_be32(intc_phandle);
        cells[cpu * 4 + 1] = cpu_to_be32(IRQ_M_EXT);
        cells[cpu * 4 + 2] = cpu_to_be32(intc_phandle);
        cells[cpu * 4 + 3] = cpu_to_be32(IRQ_S_EXT);
        g_free(nodename);
    }
    nodename = g_strdup_printf("/soc/interrupt-controller@%lx",
        (long)memmap[NEXELL_SWALLOW_PLIC].base);
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_cell(fdt, nodename, "#interrupt-cells", 1);
    qemu_fdt_setprop_string(fdt, nodename, "compatible", "riscv,plic0");
    qemu_fdt_setprop(fdt, nodename, "interrupt-controller", NULL, 0);
    qemu_fdt_setprop(fdt, nodename, "interrupts-extended",
        cells, s->soc.num_harts * sizeof(uint32_t) * 4);
    qemu_fdt_setprop_cells(fdt, nodename, "reg",
        0x0, memmap[NEXELL_SWALLOW_PLIC].base,
        0x0, memmap[NEXELL_SWALLOW_PLIC].size);
    qemu_fdt_setprop_string(fdt, nodename, "reg-names", "control");
    qemu_fdt_setprop_cell(fdt, nodename, "riscv,max-priority", 7);
    qemu_fdt_setprop_cell(fdt, nodename, "riscv,ndev", IRQ_NDEV);
    qemu_fdt_setprop_cells(fdt, nodename, "phandle", plic_phandle);
    qemu_fdt_setprop_cells(fdt, nodename, "linux,phandle", plic_phandle);
    plic_phandle = qemu_fdt_get_phandle(fdt, nodename);
    g_free(cells);
    g_free(nodename);

    nodename = g_strdup_printf("/uart@%lx",
        (long)memmap[NEXELL_SWALLOW_UART0].base);
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_string(fdt, nodename, "compatible", "ns16550a");
    qemu_fdt_setprop_cells(fdt, nodename, "reg",
        0x0, memmap[NEXELL_SWALLOW_UART0].base,
        0x0, memmap[NEXELL_SWALLOW_UART0].size);
    qemu_fdt_setprop_cell(fdt, nodename, "clock-frequency", 3686400);
        qemu_fdt_setprop_cells(fdt, nodename, "interrupt-parent", plic_phandle);
        qemu_fdt_setprop_cells(fdt, nodename, "interrupts", UART0_IRQ);

    qemu_fdt_add_subnode(fdt, "/chosen");
    qemu_fdt_setprop_string(fdt, "/chosen", "stdout-path", nodename);
    qemu_fdt_setprop_string(fdt, "/chosen", "bootargs", cmdline);
    g_free(nodename);

    return fdt;
}

static void nexell_swallow_board_init(MachineState *machine)
{
    const struct MemmapEntry *memmap = nexell_swallow_memmap;

    NexellSwallowState *s = g_new0(NexellSwallowState, 1);
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *main_mem = g_new(MemoryRegion, 1);
    MemoryRegion *mask_rom = g_new(MemoryRegion, 1);
    char *plic_hart_config;
    size_t plic_hart_config_len;
    int i;

    const char *dtb_filename = machine->dtb;

#ifdef LIBNX_USE    
    void *dl_handle;
    char *error;
#endif

    /* Initialize SOC */
    object_initialize(&s->soc, sizeof(s->soc), TYPE_RISCV_HART_ARRAY);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(&s->soc),
                              &error_abort);
    object_property_set_str(OBJECT(&s->soc), NEXELL_SWALLOW_CPU, "cpu-type",
                            &error_abort);
    object_property_set_int(OBJECT(&s->soc), smp_cpus, "num-harts",
                            &error_abort);
    object_property_set_bool(OBJECT(&s->soc), true, "realized",
                            &error_abort);

    /* register system main memory (actual RAM) */
    memory_region_init_ram(main_mem, NULL, "nexell_swallow_board.ram",
                           machine->ram_size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[NEXELL_SWALLOW_DRAM].base,
        main_mem);

    /* create device tree */
    if (machine->dtb)
	s->fdt = load_device_tree(dtb_filename, &s->fdt_size);
    else
	create_fdt(s, memmap, machine->ram_size, machine->kernel_cmdline);

    /* boot rom */
    memory_region_init_rom(mask_rom, NULL, "nexell_swallow_board.mrom",
                           memmap[NEXELL_SWALLOW_MROM].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[NEXELL_SWALLOW_MROM].base,
                                mask_rom);

    if (machine->kernel_filename) {
        uint64_t kernel_entry = load_kernel(machine->kernel_filename);

        if (machine->initrd_filename) {
            hwaddr start;
            hwaddr end = load_initrd(machine->initrd_filename,
                                     machine->ram_size, kernel_entry,
                                     &start);
            qemu_fdt_setprop_cell(s->fdt, "/chosen",
                                  "linux,initrd-start", start);
            qemu_fdt_setprop_cell(s->fdt, "/chosen", "linux,initrd-end",
                                  end);
        }
    }

    /* reset vector */
    uint32_t reset_vec[8] = {
        0x00000297,                  /* 1:  auipc  t0, %pcrel_hi(dtb) */
        0x02028593,                  /*     addi   a1, t0, %pcrel_lo(1b) */
        0xf1402573,                  /*     csrr   a0, mhartid  */
#if defined(TARGET_RISCV32)
        0x0182a283,                  /*     lw     t0, 24(t0) */
#elif defined(TARGET_RISCV64)
        0x0182b283,                  /*     ld     t0, 24(t0) */
#endif
        0x00028067,                  /*     jr     t0 */
        0x00000000,
        memmap[NEXELL_SWALLOW_DRAM].base,      /* start: .dword memmap[NEXELL_SWALLOW_DRAM].base */
        0x00000000,
                                     /* dtb: */
    };

    /* copy in the reset vector in little_endian byte order */
    for (i = 0; i < sizeof(reset_vec) >> 2; i++) {
        reset_vec[i] = cpu_to_le32(reset_vec[i]);
    }
    rom_add_blob_fixed_as("mrom.reset", reset_vec, sizeof(reset_vec),
                          memmap[NEXELL_SWALLOW_MROM].base, &address_space_memory);

    /* copy in the device tree */
    if (s->fdt_size >= memmap[NEXELL_SWALLOW_MROM].size - sizeof(reset_vec)) {
        error_report("qemu: not enough space to store device-tree");
        exit(1);
    }
    qemu_fdt_dumpdtb(s->fdt, s->fdt_size);
    rom_add_blob_fixed_as("mrom.fdt", s->fdt, s->fdt_size,
                          memmap[NEXELL_SWALLOW_MROM].base + sizeof(reset_vec),
                          &address_space_memory);

    /* create PLIC hart topology configuration string */
    plic_hart_config_len = (strlen(NEXELL_SWALLOW_PLIC_HART_CONFIG) + 1) * smp_cpus;
    plic_hart_config = g_malloc0(plic_hart_config_len);
    for (i = 0; i < smp_cpus; i++) {
        if (i != 0) {
            strncat(plic_hart_config, ",", plic_hart_config_len);
        }
        strncat(plic_hart_config, NEXELL_SWALLOW_PLIC_HART_CONFIG, plic_hart_config_len);
        plic_hart_config_len -= (strlen(NEXELL_SWALLOW_PLIC_HART_CONFIG) + 1);
    }

    /* MMIO */
    s->plic = nexell_plic_create(memmap[NEXELL_SWALLOW_PLIC].base,
        plic_hart_config,
        NEXELL_SWALLOW_PLIC_NUM_SOURCES,
        NEXELL_SWALLOW_PLIC_NUM_PRIORITIES,
        NEXELL_SWALLOW_PLIC_PRIORITY_BASE,
        NEXELL_SWALLOW_PLIC_PENDING_BASE,
        NEXELL_SWALLOW_PLIC_ENABLE_BASE,
        NEXELL_SWALLOW_PLIC_ENABLE_STRIDE,
        NEXELL_SWALLOW_PLIC_CONTEXT_BASE,
        NEXELL_SWALLOW_PLIC_CONTEXT_STRIDE,
        memmap[NEXELL_SWALLOW_PLIC].size);
    nexell_clint_create(memmap[NEXELL_SWALLOW_CLINT].base,
        memmap[NEXELL_SWALLOW_CLINT].size, smp_cpus,
        NEXELL_SIP_BASE, NEXELL_TIMECMP_BASE, NEXELL_TIME_BASE);

    //-----------------------------------------------------------------
    // SD / MMC set
    //-----------------------------------------------------------------
    nexell_sdmmc_create(system_memory, memmap[NEXELL_SWALLOW_SDMMC0].base,
        NEXELL_PLIC(s->plic)->irqs[SDMMC0_IRQ]);
    
    /* for snsp,dw-apb-uart */
    /* nexell_uart_create(system_memory, memmap[NEXELL_SWALLOW_UART9].base, */
    /*                    serial_hds[0], 0); //NEXELL_PLIC(s->plic)->irqs[UART0_IRQ]); */
    
    /* for n16550a */
//#ifndef LIBNX_USE    
    serial_mm_init(system_memory, memmap[NEXELL_SWALLOW_UART0].base,
        0, NEXELL_PLIC(s->plic)->irqs[UART0_IRQ], 399193,
        serial_hds[0], DEVICE_LITTLE_ENDIAN);
//#endif

    //-----------------------------------------------------------------
    // mmio device set
    //-----------------------------------------------------------------
    nexell_test_create(memmap[NEXELL_SWALLOW_MMIO].base);

#ifdef LIBNX_USE
    nexell_mmio_create(system_memory, memmap[NEXELL_SWALLOW_MMIO].base);

    /* connect to simulator */
    dl_handle = dlopen ("libnxriscv.so", RTLD_LAZY);
    if (!dl_handle) {
      printf ("dynamic loading error: %s\n", dlerror());
      exit (1);
    }

    nx_init = dlsym (dl_handle, "nx_init");
    error = dlerror();
    if (error) {
      printf("dynamic linking error: %s\n", error);
      exit (1);
    }

    /*
    if (dlclose(dl_handle) ) {
      printf("dynamic close error\n");
      exit (1);
    }
    */

    (*nx_init)();
    printf ("nx_init done\n");
//    nexell_sim_clock_init();
//    printf ("nx_clock_init done\n");
#endif
}

static void nexell_swallow_board_machine_init(MachineClass *mc)
{
    mc->desc = "RISC-V Nexell Swallow Board (Privileged ISA v1.10)";
    mc->init = nexell_swallow_board_init;
    mc->max_cpus = 2; /* hardcoded limit in BBL */
}

DEFINE_MACHINE("nexell_swallow", nexell_swallow_board_machine_init)
