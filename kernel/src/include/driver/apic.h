#ifndef _APIC_H_
#define _APIC_H_

#include "types.h"


typedef struct ioapic {
    uint8_t ioapicid;
    adr_t ioapicadr;
    uint32_t global_sys_intr_base;

    uint8_t index;
    uint8_t version;
    uint8_t prq;
    uint8_t pincount;

}ioapic_t;

typedef struct ioapic_irq_override {
    uint8_t bus_source;
    uint8_t irq_source;
    uint16_t flags;
    uint32_t global_sys_intr;
}ioapic_irq_override_t;

extern ioapic_t* apic_ioapic_get(uint8_t irq);
extern void apic_ioapic_irqoverride_add(uint8_t bus_source,uint8_t irq_source, uint16_t flags,uint32_t global_sys_intr);
extern ioapic_irq_override_t* apic_ioapic_irqoverride_get(uint8_t irq_source) ;
extern void apic_ioapic_route_irq(uint8_t irqno, uint8_t xapicid,uint8_t vector);
#endif