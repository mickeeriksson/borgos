#include "cpu/isr.h"
#include "error.h"
#include "config.h"
#include "irq.h"
#include "cpu.h"

uint64_t irqcounter = 0;

extern void apic_lapic_eoi(void);

void isr_pagefault_handler(trapframe_t *tframe) {
    log_msg("PAGEFAULT trapframe=%#lx vectorno=%d, irqno=%#lx, ip=%#lx\n"
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

void isr_generic(trapframe_t *tframe) {
    irqcounter++;

    cpu_t* cpu = CURRENTCPU;
    int irqOn = cpu_read_irq();
    log_msg("ISR: intenable=%d irqon=%d\n",cpu->intenable,irqOn);

    //log_msg("CPU[%d] IRQ count=%d \n",CURRENTCPU->cpuid,irqcounter);
    log_msg("TRAP trapframe=%#lx vectorno=%d, irqno=%#lx, ip=%#lx\n"
            ,tframe,tframe->isrno,tframe->errno,tframe->instruction_pointer);

    if (tframe->isrno==14) {
        //temp to get pagefault until snyggifiering
        isr_pagefault_handler(tframe);
    }else if (tframe->isrno==0xFF) {
        //Spurious handler
        //DO NOTHING
    }else if (tframe->isrno>=32 && tframe->isrno<=47) {
        //IRQ
        irq_process_irq(tframe->isrno-IRQ_STARTVECTOR);
        apic_lapic_eoi();
    }else {
        PANIC("IN ISR, UNHANDLED!");
    }
    //kprintf("CPU[%d] IRQ count=%d \n",CURRENTCPU->cpuid,irqcounter);

}