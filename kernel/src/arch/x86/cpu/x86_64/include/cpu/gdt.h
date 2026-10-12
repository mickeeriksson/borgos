#ifndef __CPU_GDT_H_
#define __CPU_GDT_H_

#include "types.h"


#define GDTSIZE		16

//#define GDT64_KERNEL_CS	 0x10
//#define GDT64_KERNEL_DS  0x20
//#define GDT64_USER_CS	 0x30
//#define GDT64_USER_DS    0x40

#define GDT64_NULL       0x00
#define GDT64_KERNEL_CS	 0x08
#define GDT64_KERNEL_DS  0x10
#define GDT64_USER_DS    0x18
#define GDT64_USER_CS	 0x20
#define GDT64_TSS	     0x28
#define GDT64_TSS_PART2  0x30


typedef struct gdt_entry_64 {
    uint32_t lim_15_0   : 16; // Low bits of segment limit
    uint32_t base_15_0  : 16; // Low bits of segment base address
    uint32_t base_23_16 : 8;  // Middle bits of segment base address
    uint32_t type       : 4;  // Segment type (see STS_ constants)
    uint32_t s          : 1;  // 0 = system, 1 = application
    uint32_t dpl        : 2;  // Descriptor Privilege Level
    uint32_t p          : 1;  // Present
    uint32_t lim_19_16  : 4;  // High bits of segment limit
    uint32_t avl        : 1;  // Unused (available for software use)
    uint32_t l          : 1;  // Long Mode
    uint32_t db         : 1;  // 0 = 16-bit segment, 1 = 32-bit segment
    uint32_t g          : 1;  // Granularity: limit scaled by 4K when set
    uint32_t base_31_24 : 8;  // High bits of segment base address
} __attribute__((packed)) gdt_entry_64_t;

// TSS-descriptor i GDT: 16 byte, upptar två 8-byte-slots
typedef struct tss_descriptor {
    uint32_t lim_15_0   : 16;
    uint32_t base_15_0  : 16;
    uint32_t base_23_16 : 8;
    uint32_t type       : 4;   // 0x9 = ledig 64-bit TSS, 0xB = upptagen (sätts av ltr)
    uint32_t s          : 1;   // 0 = systemsegment
    uint32_t dpl        : 2;
    uint32_t p          : 1;
    uint32_t lim_19_16  : 4;
    uint32_t avl        : 1;
    uint32_t l          : 1;   // 0
    uint32_t db         : 1;   // 0
    uint32_t g          : 1;   // 0 (limit i byte)
    uint32_t base_31_24 : 8;
    uint32_t base_63_32 : 32;
    uint32_t reserved   : 32;  // måste vara 0
} __attribute__((packed)) tss_descriptor_t;

_Static_assert(sizeof(tss_descriptor_t) == 16, "TSS descriptor must be 16 bytes");


typedef struct gdt_ptr {
    uint16_t size;     //256 * 16 (size of gdt_entry)
    uint64_t base;      //virtual adress to gdt-table
    uint16_t unused;
} __attribute__((packed)) gdtr_ptr_t;


// Själva TSS:en (64-bit)
typedef struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];           // ist[0] = IST1 osv.
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss64_t;

_Static_assert(sizeof(tss64_t) == 104, "TSS must be 104 bytes");


extern void gdt_init_per_cpu();
extern void gdt_flush_gdt64_current_cpu();
extern void gdt_install_tss_current_cpu();

#endif