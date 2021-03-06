/*
 * SiFive PLIC (Platform Level Interrupt Controller)
 *
 * Copyright (c) 2017 SiFive, Inc.
 *
 * This provides a parameterizable interrupt controller based on SiFive's PLIC.
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
#include "hw/sysbus.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/nexell_plic.h"

#define RISCV_DEBUG_PLIC 0

static PLICMode char_to_mode(char c)
{
    switch (c) {
    case 'U': return PLICMode_U;
    case 'S': return PLICMode_S;
    case 'H': return PLICMode_H;
    case 'M': return PLICMode_M;
    default:
        error_report("plic: invalid mode '%c'", c);
        exit(1);
    }
}

static char mode_to_char(PLICMode m)
{
    switch (m) {
    case PLICMode_U: return 'U';
    case PLICMode_S: return 'S';
    case PLICMode_H: return 'H';
    case PLICMode_M: return 'M';
    default: return '?';
    }
}

static void nexell_plic_print_state(NexellPLICState *plic)
{
    int i;
    int addrid;

    /* pending */
    qemu_log("pending       : ");
    for (i = plic->bitfield_words - 1; i >= 0; i--) {
        qemu_log("%08x", plic->pending[i]);
    }
    qemu_log("\n");

    /* pending */
    qemu_log("claimed       : ");
    for (i = plic->bitfield_words - 1; i >= 0; i--) {
        qemu_log("%08x", plic->claimed[i]);
    }
    qemu_log("\n");

    for (addrid = 0; addrid < plic->num_addrs; addrid++) {
        qemu_log("hart%d-%c enable: ",
            plic->addr_config[addrid].hartid,
            mode_to_char(plic->addr_config[addrid].mode));
        for (i = plic->bitfield_words - 1; i >= 0; i--) {
            qemu_log("%08x", plic->enable[addrid * plic->bitfield_words + i]);
        }
        qemu_log("\n");
    }
}

static
void nexell_plic_set_pending(NexellPLICState *plic, int irq, bool pending)
{
    qemu_mutex_lock(&plic->lock);
    uint32_t word = irq >> 5;
    if (pending) {
        plic->pending[word] |= (1 << (irq & 31));
    } else {
        plic->pending[word] &= ~(1 << (irq & 31));
    }
    qemu_mutex_unlock(&plic->lock);
}

static
void nexell_plic_set_claimed(NexellPLICState *plic, int irq, bool claimed)
{
    qemu_mutex_lock(&plic->lock);
    uint32_t word = irq >> 5;
    if (claimed) {
        plic->claimed[word] |= (1 << (irq & 31));
    } else {
        plic->claimed[word] &= ~(1 << (irq & 31));
    }
    qemu_mutex_unlock(&plic->lock);
}

static
int nexell_plic_num_irqs_pending(NexellPLICState *plic, uint32_t addrid)
{
    int i, j, count = 0;
    for (i = 0; i < plic->bitfield_words; i++) {
        uint32_t pending_enabled_not_claimed =
            (plic->pending[i] & ~plic->claimed[i]) &
            plic->enable[addrid * plic->bitfield_words + i];
        if (!pending_enabled_not_claimed) {
            continue;
        }
        for (j = 0; j < 32; j++) {
            int irq = (i << 5) + j;
            uint32_t prio = plic->source_priority[irq];
            int enabled = pending_enabled_not_claimed & (1 << j);
            if (enabled && prio > plic->target_priority[addrid]) {
                count++;
            }
        }
    }
    return count;
}

static void nexell_plic_update(NexellPLICState *plic)
{
    int addrid;

    /* raise irq on harts where this irq is enabled */
    for (addrid = 0; addrid < plic->num_addrs; addrid++) {
        uint32_t hartid = plic->addr_config[addrid].hartid;
        PLICMode mode = plic->addr_config[addrid].mode;
        CPUState *cpu = qemu_get_cpu(hartid);
        CPURISCVState *env = cpu ? cpu->env_ptr : NULL;
        if (!env) {
            continue;
        }
        int level = nexell_plic_num_irqs_pending(plic, addrid) > 0;
        switch (mode) {
        case PLICMode_M:
            riscv_set_local_interrupt(RISCV_CPU(cpu), MIP_MEIP, level);
            break;
        case PLICMode_S:
            riscv_set_local_interrupt(RISCV_CPU(cpu), MIP_SEIP, level);
            break;
        default:
            break;
        }
    }

    if (RISCV_DEBUG_PLIC) {
        nexell_plic_print_state(plic);
    }
}

void nexell_plic_raise_irq(NexellPLICState *plic, uint32_t irq)
{
    nexell_plic_set_pending(plic, irq, true);
    nexell_plic_update(plic);
}

void nexell_plic_lower_irq(NexellPLICState *plic, uint32_t irq)
{
    nexell_plic_set_pending(plic, irq, false);
    nexell_plic_update(plic);
}

static uint32_t nexell_plic_claim(NexellPLICState *plic, uint32_t addrid)
{
    int i, j;
    for (i = 0; i < plic->bitfield_words; i++) {
        uint32_t pending_enabled_not_claimed =
            (plic->pending[i] & ~plic->claimed[i]) &
            plic->enable[addrid * plic->bitfield_words + i];
        if (!pending_enabled_not_claimed) {
            continue;
        }
        for (j = 0; j < 32; j++) {
            int irq = (i << 5) + j;
            uint32_t prio = plic->source_priority[irq];
            int enabled = pending_enabled_not_claimed & (1 << j);
            if (enabled && prio > plic->target_priority[addrid]) {
                nexell_plic_set_pending(plic, irq, false);
                nexell_plic_set_claimed(plic, irq, true);
                return irq;
            }
        }
    }
    return 0;
}

static uint64_t nexell_plic_read(void *opaque, hwaddr addr, unsigned size)
{
    NexellPLICState *plic = opaque;

    /* writes must be 4 byte words */
    if ((addr & 0x3) != 0) {
        goto err;
    }

    if (addr >= plic->priority_base && /* 4 bytes per source */
        addr < plic->priority_base + (plic->num_sources << 2))
    {
        uint32_t irq = (addr - plic->priority_base) >> 2;
        if (RISCV_DEBUG_PLIC) {
            qemu_log("plic: read priority: irq=%d priority=%d\n",
                irq, plic->source_priority[irq]);
        }
        return plic->source_priority[irq];
    } else if (addr >= plic->pending_base && /* 1 bit per source */
               addr < plic->pending_base + (plic->num_sources >> 3))
    {
        uint32_t word = (addr - plic->priority_base) >> 2;
        if (RISCV_DEBUG_PLIC) {
            qemu_log("plic: read pending: word=%d value=%d\n",
                word, plic->pending[word]);
        }
        return plic->pending[word];
    } else if (addr >= plic->enable_base && /* 1 bit per source */
             addr < plic->enable_base + plic->num_addrs * plic->enable_stride)
    {
        uint32_t addrid = (addr - plic->enable_base) / plic->enable_stride;
        uint32_t wordid = (addr & (plic->enable_stride - 1)) >> 2;
        if (wordid < plic->bitfield_words) {
            if (RISCV_DEBUG_PLIC) {
                qemu_log("plic: read enable: hart%d-%c word=%d value=%x\n",
                    plic->addr_config[addrid].hartid,
                    mode_to_char(plic->addr_config[addrid].mode), wordid,
                    plic->enable[addrid * plic->bitfield_words + wordid]);
            }
            return plic->enable[addrid * plic->bitfield_words + wordid];
        }
    } else if (addr >= plic->context_base && /* 1 bit per source */
             addr < plic->context_base + plic->num_addrs * plic->context_stride)
    {
        uint32_t addrid = (addr - plic->context_base) / plic->context_stride;
        uint32_t contextid = (addr & (plic->context_stride - 1));
        if (contextid == 0) {
            if (RISCV_DEBUG_PLIC) {
                qemu_log("plic: read priority: hart%d-%c priority=%x\n",
                    plic->addr_config[addrid].hartid,
                    mode_to_char(plic->addr_config[addrid].mode),
                    plic->target_priority[addrid]);
            }
            return plic->target_priority[addrid];
        } else if (contextid == 4) {
            uint32_t value = nexell_plic_claim(plic, addrid);
            if (RISCV_DEBUG_PLIC) {
                qemu_log("plic: read claim: hart%d-%c irq=%x\n",
                    plic->addr_config[addrid].hartid,
                    mode_to_char(plic->addr_config[addrid].mode),
                    value);
                nexell_plic_print_state(plic);
            }
            return value;
        }
    }

err:
    error_report("plic: invalid register read: %08x", (uint32_t)addr);
    return 0;
}

static void nexell_plic_write(void *opaque, hwaddr addr, uint64_t value,
        unsigned size)
{
    NexellPLICState *plic = opaque;

    /* writes must be 4 byte words */
    if ((addr & 0x3) != 0) {
        goto err;
    }

    if (addr >= plic->priority_base && /* 4 bytes per source */
        addr < plic->priority_base + (plic->num_sources << 2))
    {
        uint32_t irq = (addr - plic->priority_base) >> 2;
        plic->source_priority[irq] = value & 7;
        if (RISCV_DEBUG_PLIC) {
            qemu_log("plic: write priority: irq=%d priority=%d\n",
                irq, plic->source_priority[irq]);
        }
        return;
    } else if (addr >= plic->pending_base && /* 1 bit per source */
               addr < plic->pending_base + (plic->num_sources >> 3))
    {
        error_report("plic: invalid pending write: %08x", (uint32_t)addr);
        return;
    } else if (addr >= plic->enable_base && /* 1 bit per source */
        addr < plic->enable_base + plic->num_addrs * plic->enable_stride)
    {
        uint32_t addrid = (addr - plic->enable_base) / plic->enable_stride;
        uint32_t wordid = (addr & (plic->enable_stride - 1)) >> 2;
        if (wordid < plic->bitfield_words) {
            plic->enable[addrid * plic->bitfield_words + wordid] = value;
            if (RISCV_DEBUG_PLIC) {
                qemu_log("plic: write enable: hart%d-%c word=%d value=%x\n",
                    plic->addr_config[addrid].hartid,
                    mode_to_char(plic->addr_config[addrid].mode), wordid,
                    plic->enable[addrid * plic->bitfield_words + wordid]);
            }
            return;
        }
    } else if (addr >= plic->context_base && /* 4 bytes per reg */
        addr < plic->context_base + plic->num_addrs * plic->context_stride)
    {
        uint32_t addrid = (addr - plic->context_base) / plic->context_stride;
        uint32_t contextid = (addr & (plic->context_stride - 1));
        if (contextid == 0) {
            if (RISCV_DEBUG_PLIC) {
                qemu_log("plic: write priority: hart%d-%c priority=%x\n",
                    plic->addr_config[addrid].hartid,
                    mode_to_char(plic->addr_config[addrid].mode),
                    plic->target_priority[addrid]);
            }
            if (value <= plic->num_priorities) {
                plic->target_priority[addrid] = value;
                nexell_plic_update(plic);
            }
            return;
        } else if (contextid == 4) {
            if (RISCV_DEBUG_PLIC) {
                qemu_log("plic: write claim: hart%d-%c irq=%x\n",
                    plic->addr_config[addrid].hartid,
                    mode_to_char(plic->addr_config[addrid].mode),
                    (uint32_t)value);
            }
            if (value < plic->num_sources) {
                nexell_plic_set_claimed(plic, value, false);
                nexell_plic_update(plic);
            }
            return;
        }
    }

err:
    error_report("plic: invalid register write: %08x", (uint32_t)addr);
}

static const MemoryRegionOps nexell_plic_ops = {
    .read = nexell_plic_read,
    .write = nexell_plic_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};

static Property nexell_plic_properties[] = {
    DEFINE_PROP_STRING("hart-config", NexellPLICState, hart_config),
    DEFINE_PROP_UINT32("num-sources", NexellPLICState, num_sources, 0),
    DEFINE_PROP_UINT32("num-priorities", NexellPLICState, num_priorities, 0),
    DEFINE_PROP_UINT32("priority-base", NexellPLICState, priority_base, 0),
    DEFINE_PROP_UINT32("pending-base", NexellPLICState, pending_base, 0),
    DEFINE_PROP_UINT32("enable-base", NexellPLICState, enable_base, 0),
    DEFINE_PROP_UINT32("enable-stride", NexellPLICState, enable_stride, 0),
    DEFINE_PROP_UINT32("context-base", NexellPLICState, context_base, 0),
    DEFINE_PROP_UINT32("context-stride", NexellPLICState, context_stride, 0),
    DEFINE_PROP_UINT32("aperture-size", NexellPLICState, aperture_size, 0),
    DEFINE_PROP_END_OF_LIST(),
};

/*
 * parse PLIC hart/mode address offset config
 *
 * "M"              1 hart with M mode
 * "MS,MS"          2 harts, 0-1 with M and S mode
 * "M,MS,MS,MS,MS"  5 harts, 0 with M mode, 1-5 with M and S mode
 */
static void parse_hart_config(NexellPLICState *plic)
{
    int addrid, hartid, modes;
    const char *p;
    char c;

    /* count and validate hart/mode combinations */
    addrid = 0, hartid = 0, modes = 0;
    p = plic->hart_config;
    while ((c = *p++)) {
        if (c == ',') {
            addrid += __builtin_popcount(modes);
            modes = 0;
            hartid++;
        } else {
            int m = 1 << char_to_mode(c);
            if (modes == (modes | m)) {
                error_report("plic: duplicate mode '%c' in config: %s",
                             c, plic->hart_config);
                exit(1);
            }
            modes |= m;
        }
    }
    if (modes) {
        addrid += __builtin_popcount(modes);
    }
    hartid++;

    /* store hart/mode combinations */
    plic->num_addrs = addrid;
    plic->addr_config = g_new(PLICAddr, plic->num_addrs);
    addrid = 0, hartid = 0;
    p = plic->hart_config;
    while ((c = *p++)) {
        if (c == ',') {
            hartid++;
        } else {
            plic->addr_config[addrid].addrid = addrid;
            plic->addr_config[addrid].hartid = hartid;
            plic->addr_config[addrid].mode = char_to_mode(c);
            addrid++;
        }
    }
}

static void nexell_plic_irq_request(void *opaque, int irq, int level)
{
    NexellPLICState *plic = opaque;
    if (RISCV_DEBUG_PLIC) {
        qemu_log("nexell_plic_irq_request: irq=%d level=%d\n", irq, level);
    }
    nexell_plic_set_pending(plic, irq, level > 0);
    nexell_plic_update(plic);
}

static void nexell_plic_realize(DeviceState *dev, Error **errp)
{
    NexellPLICState *plic = NEXELL_PLIC(dev);
    int i;

    memory_region_init_io(&plic->mmio, OBJECT(dev), &nexell_plic_ops, plic,
                          TYPE_NEXELL_PLIC, plic->aperture_size);
    parse_hart_config(plic);
    qemu_mutex_init(&plic->lock);
    plic->bitfield_words = (plic->num_sources + 31) >> 5;
    plic->source_priority = g_new0(uint32_t, plic->num_sources);
    plic->target_priority = g_new(uint32_t, plic->num_addrs);
    plic->pending = g_new0(uint32_t, plic->bitfield_words);
    plic->claimed = g_new0(uint32_t, plic->bitfield_words);
    plic->enable = g_new0(uint32_t, plic->bitfield_words * plic->num_addrs);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &plic->mmio);
    plic->irqs = g_new0(qemu_irq, plic->num_sources + 1);
    for (i = 0; i <= plic->num_sources; i++) {
        plic->irqs[i] = qemu_allocate_irq(nexell_plic_irq_request, plic, i);
    }
}

static void nexell_plic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->props = nexell_plic_properties;
    dc->realize = nexell_plic_realize;
}

static const TypeInfo nexell_plic_info = {
    .name          = TYPE_NEXELL_PLIC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(NexellPLICState),
    .class_init    = nexell_plic_class_init,
};

static void nexell_plic_register_types(void)
{
    type_register_static(&nexell_plic_info);
}

type_init(nexell_plic_register_types)

/*
 * Create PLIC device.
 */
DeviceState *nexell_plic_create(hwaddr addr, char *hart_config,
    uint32_t num_sources, uint32_t num_priorities,
    uint32_t priority_base, uint32_t pending_base,
    uint32_t enable_base, uint32_t enable_stride,
    uint32_t context_base, uint32_t context_stride,
    uint32_t aperture_size)
{
    DeviceState *dev = qdev_create(NULL, TYPE_NEXELL_PLIC);
    assert(enable_stride == (enable_stride & -enable_stride));
    assert(context_stride == (context_stride & -context_stride));
    qdev_prop_set_string(dev, "hart-config", hart_config);
    qdev_prop_set_uint32(dev, "num-sources", num_sources);
    qdev_prop_set_uint32(dev, "num-priorities", num_priorities);
    qdev_prop_set_uint32(dev, "priority-base", priority_base);
    qdev_prop_set_uint32(dev, "pending-base", pending_base);
    qdev_prop_set_uint32(dev, "enable-base", enable_base);
    qdev_prop_set_uint32(dev, "enable-stride", enable_stride);
    qdev_prop_set_uint32(dev, "context-base", context_base);
    qdev_prop_set_uint32(dev, "context-stride", context_stride);
    qdev_prop_set_uint32(dev, "aperture-size", aperture_size);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);
    return dev;
}
