/*
 * simple mmio redirector
 */

#ifndef HW_NEXELL_MMIO_H
#define HW_NEXELL_MMIO_H

#define TYPE_NEXELL_MMIO "nexell.mmio"

#define NEXELL_MMIO(obj) \
        OBJECT_CHECK(NexellMMIOState, (obj), TYPE_NEXELL_MMIO)
typedef struct NexellMMIOState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion mmio;
    hwaddr base;
} NexellMMIOState;

NexellMMIOState *nexell_mmio_create(MemoryRegion *address_space, hwaddr base);
#endif
