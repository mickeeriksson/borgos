#include "cpu/idt.h"
#include "cpu.h"
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


void _set_idt_desc_64(uint16_t select, uint8_t ist,uint64_t offset, uint8_t type, idt_entry_64_t *desc)
{
    desc->selector = select;
    desc->ist = ist;
    desc->type_attributes = type;
    desc->offset_15_0 = offset & 0xFFFF;
    desc->offset_31_16 = (offset & 0xFFFF0000) >> 16;
    desc->offset_63_32 = (offset & 0xFFFFFFFF00000000) >> 32;
}

void idt_load_idt64_current_cpu() {
    cpu_t* cpup = CURRENTCPU;
    idtr_ptr_t *dbgidtr = &idtr;
    log_msg("Load IDT fort CPU[%d] idt_ptr: 0x%lx\n", cpup->cpuid,dbgidtr);
    idt_lidt ((adr_t) &idtr);
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

            uint8_t ist = 0;
            if (i==18 || i==8) {
                //DF(8), MC(18)
                ist = 1;  // use special CPU stack in IST 1
            }
            if (i==2) {
                //NMI(2)
                ist = 2;  // use special CPU stack in IST 2
            }
            if (i==1 || i==3) {
                //DB / BP
                ist = 3;  // use special CPU stack in IST 3
            }
            if (i==0x80 || i==0x81) {
                //syscall, 0x80 for linux campat, 0x81 for Borg-syscalls
                //ist = 3;  // use special CPU stack in IST 3
                type = (uint8_t) (0xE | (1<<7) | (3<<5)); // 64bit irq gate + present, DPL=3 , should be 0xEE
                log_msg("Set type = %#x for SYSCALL IDT\n", type);
            }


            //set IDT entry to adr
            _set_idt_desc_64(cs_seg ,ist,adr, type, &idt[i]);

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

