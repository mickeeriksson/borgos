
#include "cpu/isr.h"
#include "error.h"

uint64_t irqcounter = 0;

void isr_generic(trapframe_t *tframe) {
    irqcounter++;
    //log_msg("CPU[%d] IRQ count=%d \n",CURRENTCPU->cpuid,irqcounter);
    log_msg("TRAP trapframe=%#lx vectorno=%d, irqno=%#lx, ip=%#lx\n"
            ,tframe,tframe->isrno,tframe->errno,tframe->instruction_pointer);

    //kprintf("CPU[%d] IRQ count=%d \n",CURRENTCPU->cpuid,irqcounter);

    PANIC("IN ISR, UNHANDLED!");
}