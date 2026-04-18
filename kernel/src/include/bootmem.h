#ifndef _BOOTMEM_H_
#define _BOOTMEM_H_


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


#endif // _BOOTMEM_H_