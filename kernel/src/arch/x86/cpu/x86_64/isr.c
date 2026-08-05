#include "cpu/isr.h"
#include "error.h"
#include "config.h"
#include "irq.h"
#include "cpu.h"
#include "bits.h"

uint64_t irqcounter = 0;

extern void apic_lapic_eoi(void);
extern adr_t isr_stub_table[];

adr_t isr_get_isr_stub_addr(uint8_t isrno) {
    adr_t stub_addr = isr_stub_table[isrno];
    return stub_addr;
}

void isr_pagefault_handler(trapframe_t *tframe) {
    log_msg("PAGEFAULT trapframe=%#lx vectorno=%d, errno=%#lx, ip=%#lx\n"
            ,tframe,tframe->isrno,tframe->errno,tframe->instruction_pointer);

    // A page fault has occurred.
    // The faulting address is stored in the CR2 register.
    adr_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r" (faulting_address));

    log_msg("Page fault (faulting addr 0x%lx )\n", faulting_address);
        /*
        // The error code gives us details of what happened.
        int notpresent   = !(err & 0x1); // Page not present
        int rw = err & 0x2;           // Write operation?
        int us = err & 0x4;           // Processor was in user-mode?
        int reserved = err & 0x8;     // Overwritten CPU-reserved bits of page entry?
        int id = err & 0x10;          // Caused by an instruction fetch?

        kprintf("Trap Err: %x\n", err);
        // Output an error message.
        kprintf("Page fault! (");
        if (notpresent) {kprintf(" [page not present] ");}
        if (rw) {kprintf("[writeop] ");}
        if (us) {kprintf("[user-mode] ");}
        if (reserved) {kprintf("[reserved] ");}
        if (id) {kprintf("[instruction] ");}
        kprintf(" at 0x%x )\n", faulting_address);
        PANIC("Page fault");
        */
    log_msg("Before PANIC");

    PANIC("PAGEFAULT, UNHANDLED!");
}

void isr_gpf_handler(trapframe_t *tframe) {
    log_msg("PAGEFAULT trapframe=%#lx vectorno=%d, errno=%#lx, ip=%#lx\n"
            ,tframe,tframe->isrno,tframe->errno,tframe->instruction_pointer);

    log_msg("GPF");
    log_msg("Before PANIC");

    PANIC("PAGEFAULT, UNHANDLED!");
}


void isr_division_handler(trapframe_t *tframe) {
    log_msg("PAGEFAULT trapframe=%#lx vectorno=%d, errno=%#lx, ip=%#lx\n"
            ,tframe,tframe->isrno,tframe->errno,tframe->instruction_pointer);

    log_msg("DIVISION");
    log_msg("Before PANIC");

    PANIC("DIVISION BY ZERO, UNHANDLED!");
}

extern void check_stackalign(void);
void isr_generic(trapframe_t *tframe) {
    //check_stackalign();
    irqcounter++;

    if ( ((uintptr_t)tframe & 0xF) != 0) {
        PANIC("ISR CALLED WITH invalid alignement on stack. Should be aligned to 16 bytes\n");
    }

    cpu_t* cpu = CURRENTCPU;
    int irqOn = cpu_read_irq();


    if (tframe->isrno==0) {
        isr_division_handler(tframe);
    }else if (tframe->isrno==6) {
        log_msg("ISR: intenable=%d irqon=%d\n",cpu->currentproc->intenable,irqOn);
        log_msg("TRAP trapframe=%#lx vectorno=%d, errno=%#lx, ip=%#lx\n"
                ,tframe,tframe->isrno,tframe->errno,tframe->instruction_pointer);
        PANIC("Invalid OPCODE");
    }else if (tframe->isrno==14) {
        //temp to get pagefault until snyggifiering
        isr_pagefault_handler(tframe);
    }else if (tframe->isrno==13) {
            //temp to get pagefault until snyggifiering
            isr_gpf_handler(tframe);
    }else if (tframe->isrno==0xFF) {
        //Spurious handler
        //DO NOTHING
    }else if (tframe->isrno>=32 && tframe->isrno<=95) {
        //IRQ 32-63 = Normal IRQ
        //IRQ 64-95 = MSI IRQ
        int irq= tframe->isrno-IRQ_STARTVECTOR;
        log_msg("-- CPU[%d] IRQ=%d  (isrno=%d)\n",cpu->cpuid,irq,tframe->isrno);
        if (irq==0) {
            //do EOI in reverse order. Due to scheduling
            //if not eoi before context_switch. Timer will never trigger again. (since we switch a irq-thread to a kernel/user thread.)
            //Snygga upp beroende på att timer ligger på IRQ0.
            //ev kanske inte hårdkodat till att vara just ett apic triggat IRQ?
            apic_lapic_eoi();
            irq_process_irq(tframe->isrno,irq);
        }else {
            irq_process_irq(tframe->isrno,irq);
            apic_lapic_eoi();
        }
    }else {
        log_msg("ISR: intenable=%d irqon=%d\n",cpu->currentproc->intenable,irqOn);
        log_msg("TRAP trapframe=%#lx vectorno=%d, errno=%#lx, ip=%#lx\n"
                ,tframe,tframe->isrno,tframe->errno,tframe->instruction_pointer);
        PANIC("IN ISR, UNHANDLED!");
    }
    //kprintf("CPU[%d] IRQ count=%d \n",CURRENTCPU->cpuid,irqcounter);

}