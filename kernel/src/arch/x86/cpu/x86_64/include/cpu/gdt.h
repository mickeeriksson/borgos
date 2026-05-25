#ifndef __CPU_GDT_H_
#define __CPU_GDT_H_

#include "types.h"


#define GDTSIZE		20

typedef struct gdt_entry_64 {
    uint32_t lim_15_0 : 16;  // Low bits of segment limit
    uint32_t base_15_0 : 16; // Low bits of segment base address
    uint32_t base_23_16 : 8; // Middle bits of segment base address
    uint32_t type : 4;       // Segment type (see STS_ constants)
    uint32_t s : 1;          // 0 = system, 1 = application
    uint32_t dpl : 2;        // Descriptor Privilege Level
    uint32_t p : 1;          // Present
    uint32_t lim_19_16 : 4;  // High bits of segment limit
    uint32_t avl : 1;        // Unused (available for software use)
    uint32_t l : 1;          // Long Mode
    uint32_t db : 1;         // 0 = 16-bit segment, 1 = 32-bit segment
    uint32_t g : 1;          // Granularity: limit scaled by 4K when set
    uint32_t base_31_24 : 8; // High bits of segment base address
    uint32_t base_32_63 : 32; // High bits of segment base address
    uint32_t reserved_31_0 : 32; // High bits of segment base address
} __attribute__((packed)) gdt_entry_64_t;


typedef struct gdt_ptr {
    uint16_t size;     //256 * 16 (size of gdt_entry)
    uint64_t base;      //virtual adress to gdt-table
    uint16_t unused;
} __attribute__((packed)) gdtr_ptr_t;



#endif