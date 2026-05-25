#ifndef _BOOTMEM_H_
#define _BOOTMEM_H_

#include "types.h"

/*
 * Bootmem is used to handle memory before everything about MMU, paging, slabs etc are initialized and setup.
 * Bootmem will use a linear allocpool, right after kernelimage to draw its allocations from.
 */

struct bootmem_info {
    adr_t addr;
    adr_t heaptop;
    adr_t maxsize;
};

extern struct bootmem_info bootmeminfo;

enum BootMemType {
    BOOTMEMTYPE_FREE = 1,
    BOOTMEMTYPE_RESERVED = 2,
    BOOTMEMTYPE_ACPI_RECLAIMABLE = 3,
    //BOOTMEMTYPE_NVS = 4,
    //BOOTMEMTYPE_BADRAM = 5,
    BOOTMEMTYPE_UNKNOWN = 99,

    BOOTMEMTYPE_END = 0,    // last entry
};

struct bootmem_memmapentry {
    adr_t addr;
    size_t size;
    enum BootMemType type;
};

extern struct bootmem_memmapentry *bootmem_memmapentry;


extern void bootmem_debugprint(void);
extern void* bootmem_alloc(size_t size);
extern void* bootmem_alloc_phys(size_t size,size_t align);
extern void* bootmem_alloc_virt(size_t size,size_t align);
extern void bootmem_memmnap_debugprint(void);

#endif // _BOOTMEM_H_