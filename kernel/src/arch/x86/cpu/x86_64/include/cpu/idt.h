#ifndef __CPU_IDT_H_
#define __CPU_IDT_H_

#include "types.h"


#define IDTSIZE		256

typedef struct idt_entry_64 {
    uint16_t offset_15_0;        // offset bits 0..15
    uint16_t selector;        // a code segment selector in GDT or LDT
    uint8_t  ist;             // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
    uint8_t  type_attributes; // gate type, dpl, and p fields
    uint16_t offset_31_16;        // offset bits 16..31
    uint32_t offset_63_32;        // offset bits 32..63
    uint32_t zero;            // reserved
} __attribute__((packed)) idt_entry_64_t;


typedef struct idt_ptr {
    uint16_t size;     //256 * 16 (size of idt_entry)
    uint64_t base;      //virtual adress to idt-table
    uint16_t unused;
} __attribute__((packed)) idtr_ptr_t;



#endif