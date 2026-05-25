#include "cpu/idt.h"
#include "log.h"
#include <string.h>

idt_entry_64_t idt[IDTSIZE];
idtr_ptr_t idtr;


extern adr_t isr_stub_table[IDTSIZE];

uint16_t _asm_get_cs_reg()  {
    uint16_t segment;
    __asm__ ("mov %%cs, %0" : "=r" (segment));
    return segment;
}

void idt_lidt (adr_t idtptr_virtaddr) {
    __asm__ volatile("lidt (%0)" : : "r" (idtptr_virtaddr));
}


void _set_idt_desc_64(uint16_t select, uint64_t offset, uint8_t type, idt_entry_64_t *desc)
{
    desc->selector = select;
    desc->ist = 0;
    desc->type_attributes = type;
    desc->offset_15_0 = offset & 0xFFFF;
    desc->offset_31_16 = (offset & 0xFFFF0000) >> 16;
    desc->offset_63_32 = (offset & 0xFFFFFFFF00000000) >> 32;
}


void idt_init(void) {
    log_msg("Init IDT\n");

    uint16_t cs_seg = _asm_get_cs_reg();
    log_msg("cs_seg: 0x%x\n", cs_seg);

    //clear table
    void* toclear = (char*) &idt[0];
    int tablesize = IDTSIZE*sizeof(idt_entry_64_t);
    memset(toclear, 0, tablesize);

    for (int i = 0; i < IDTSIZE; i++) {
        adr_t adr = isr_stub_table[i];
        if (adr>0) {
            uint8_t type = (uint8_t) (0xE | (1<<7)); // 64bit irq gate + present, (DPL=0 ??)
            //set IDT entry to adr
            _set_idt_desc_64(cs_seg ,adr, type, &idt[i]);

            idt_entry_64_t *entry = &idt[i];
            log_msg("Set IDT entry %i to 0x%lx entry at 0x%lx\n", i, adr,entry);
        }
    }

    idtr.size = IDTSIZE*sizeof(idt_entry_64_t)-1;
    idtr.base = (uint64_t) &idt[0];

    idtr_ptr_t *dbgidtr = &idtr;
    log_msg("idt_ptr: 0x%lx\n", dbgidtr);
    idt_lidt ((adr_t) &idtr);


}

