#include "driver/apic.h"
#include "cpu.h"
#include "cpu/cpu_cpu.h"
#include "error.h"


void arch_irq_route_irq(uint8_t irqno, uint8_t cpuid) {
    log_msg("Route IRQ, assume I/O APIC\n");
    ioapic_t* ioapic = apic_ioapic_get(irqno);
    if (ioapic==NULL) {
        PANIC("NO I/O APIC found for IRQ. Is APIC supported on this system?\n");
    }

    //adr_t iobase = ioapic->ioapicadr;
    cpu_t* c = &cpu[cpuid];
    uint8_t xapicid = c->archcpu.xapic_logicalid;

    log_msg("route irq %d to cpu[%d] xapic id = %d\n", irqno, cpuid, xapicid);

    apic_ioapic_route_irq(irqno, xapicid,irqno+IRQ_STARTVECTOR);


    return;

}