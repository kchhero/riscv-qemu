#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "hw/sysbus.h"
#include "hw/riscv/nexell_mmio.h"
#include <dlfcn.h>

#define NEXELL_MMIO_MAX 0x10000000

static uint64_t (*nx_mmio_read) (void *opaque, hwaddr addr, unsigned size);
static void (*nx_mmio_write) (void *opaque, hwaddr addr, uint64_t value,
        unsigned size);
static void (*nx_mmio_init) (hwaddr base);
static int (*nx_mmio_max) (void);
int mmio_max;

#if 0
static uint64_t nexell_mmio_read (void *opaque, hwaddr addr, unsigned size) {
  return (*nx_mmio_read)(opaque, addr, size);
}

static void nexell_mmio_write (void *opaque, hwaddr addr, uint64_t value,
        unsigned size) {
  (*nx_mmio_write)(opaque, addr, value, size);
}

static MemoryRegionOps nexell_mmio_ops = {
    .read = nexell_mmio_read,
    .write = nexell_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,   /* 1 byte  */
        .max_access_size = 4,   /* 4 bytes */
    }
};
#endif

NexellMMIOState *nexell_mmio_create(MemoryRegion *address_space, hwaddr base) {
    NexellMMIOState *s = g_malloc0(sizeof(NexellMMIOState));
    MemoryRegionOps *nx_mmio_ops = malloc(sizeof(MemoryRegionOps));
    void *dl_handle;
    char *error;

    s->base = base;

    dl_handle = dlopen ("libnxriscv.so", RTLD_LAZY);
    if (!dl_handle) {
      printf ("dynamic loading error: %s\n", dlerror());
      exit (1);
    }
    nx_mmio_read = dlsym (dl_handle, "nx_mmio_read");
    error = dlerror();
    if (error) {
      printf("dynamic linking error: %s\n", error);
      exit (1);
    }

    nx_mmio_write = dlsym (dl_handle, "nx_mmio_write");
    error = dlerror();
    if (error) {
      printf("dynamic linking error: %s\n", error);
      exit (1);
    }

    nx_mmio_init = dlsym (dl_handle, "nx_mmio_init");
    error = dlerror();
    if (error) {
      printf("dynamic linking error: %s\n", error);
      exit (1);
    }

    nx_mmio_max = dlsym (dl_handle, "nx_mmio_max");
    error = dlerror();
    if (error) {
      printf("dynamic linking error: %s\n", error);
      exit (1);
    }

    mmio_max = (*nx_mmio_max)();

    /* connect to simulator */
    (*nx_mmio_init) (base);

    {
      nx_mmio_ops->read = nx_mmio_read;
      nx_mmio_ops->write = nx_mmio_write;
      nx_mmio_ops->endianness = DEVICE_LITTLE_ENDIAN,
      nx_mmio_ops->valid.min_access_size = 1,   /* 1 byte  */
      nx_mmio_ops->valid.max_access_size = 4,   /* 4 bytes */

      memory_region_init_io(&s->mmio, NULL, nx_mmio_ops, s,
                            TYPE_NEXELL_MMIO, mmio_max);

    }
    
    /* memory_region_init_io(&s->mmio, NULL, &nexell_mmio_ops, s, */
    /*                       TYPE_NEXELL_MMIO, mmio_max); */
    memory_region_add_subregion(address_space, base, &s->mmio);

    return s;
}
