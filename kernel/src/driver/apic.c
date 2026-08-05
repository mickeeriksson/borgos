#include "types.h"
#include "config.h"
#include "cpu.h"
#include "cpu/mmio.h"
#include "delay.h"
#include "error.h"
#include "bits.h"
#include "driver/apic.h"
#include "mm.h"

#define  LAPIC_REG_APICID	   0x0020
#define  LAPIC_REG_APICVER	   0x0030
#define  LAPIC_REG_TASKPRIOR   0x0080  // Task Priority
#define  LAPIC_REG_LDR         0x00D0  // Logical Destination
#define  LAPIC_REG_DFR         0x00E0  // Destination Format
#define  LAPIC_REG_SPURIOUS	   0xF0

#define  LAPIC_ICR_LOW         0x300
#define  LAPIC_ICR_HIGH        0x310

#define  APIC_REG_LVT_TMR	   0x0320
#define  APIC_REG_LVT_PERF	   0x0340
#define  APIC_REG_LVT_LINT0    0x0350
#define  APIC_REG_LVT_LINT1    0x0360

#define  APIC_REG_TMRINITCNT   0x0380
#define  APIC_REG_TMRCURRCNT   0x0390
#define  APIC_REG_TMRDIV	   0x03E0
#define  APIC_REG_EOI          0x00B0

#define  APIC_LVT_DISABLE      0x10000 //Write to LVT to disable this vector
#define  APIC_LVT_TMR_MODE_PERIODIC   1<<17


adr_t cpulapicadr=NULL;



#define IOAPIC_MAX 32
uint8_t ioapic_count=0;
ioapic_t* ioapics[IOAPIC_MAX] ;




ioapic_irq_override_t* ioapic_irq_source_override[MAX_INT_VECTORS];


void apic_set_lapic_addr(adr_t local_apic_addr) {
    cpulapicadr = local_apic_addr;
}


#ifdef CONFIG_APIC

cpu_t* apic_id_cpumap[MAXAPIC_LOGICAL_CPUID];

uint32_t lapic_rd(adr_t lapicadr,uint32_t reg) {
    adr_t adr = lapicadr+reg;
    uint32_t val = mmio_read32(adr);
    //log_msg("lapic_rd 0x%lx = 0x%x\n", adr, val);
    return val;
}


void lapic_wr(adr_t lapicadr,uint32_t reg,uint32_t val) {
    adr_t adr = lapicadr+reg;
    //log_msg("lapic_wr 0x%lx = 0x%x\n", adr, val);
    mmio_write32(adr,val);
}

static void lapic_wait_idle(adr_t lapicadr) {
    while ( lapic_rd(lapicadr,LAPIC_ICR_LOW) & (1 << 12)) // delivery status bit
        __asm__ volatile("pause");
}

void apic_init(void) {
    log_msg("APIC init(), do nothing for now!\n");
    ioapic_count=0;
    for (int i=0;i<IOAPIC_MAX;i++) {
        ioapics[i]=0;
    }
}

uint32_t apic_ioapic_read(adr_t ioapicadr,uint8_t index) {
    mmio_write8(ioapicadr+0x00,index); //index id
    uint32_t data = mmio_read32(ioapicadr+0x10); //data
    return data;
}
void apic_ioapic_write(adr_t ioapicadr,uint8_t index,uint32_t data) {
    mmio_write8(ioapicadr+0x00,index); //index id
    mmio_write32(ioapicadr+0x10,data); //data
    return;
}

void apic_ioapic_irqoverride_add(uint8_t bus_source,uint8_t irq_source, uint16_t flags,uint32_t global_sys_intr) {
    ioapic_irq_override_t* e = ioapic_irq_source_override[irq_source];
    if (e!=NULL) {
        PANIC("DUAL OVERRIDE ENTRIES FOR IRQ");
    }
    e = kmalloc(sizeof(ioapic_irq_override_t),NULL);
    e->bus_source=bus_source;
    e->irq_source=irq_source;
    e->flags=flags;
    e->global_sys_intr=global_sys_intr;
    ioapic_irq_source_override[irq_source] = e;
}

ioapic_irq_override_t* apic_ioapic_irqoverride_get(uint8_t irq_source) {
    return ioapic_irq_source_override[irq_source];
}


void apic_ioapic_add(uint8_t ioapicid,adr_t ioapicadr,uint32_t global_sys_intr_base) {
    ioapic_t* ioapic;
    //test if this I/O APIC already exists?
    for (int i=0;i<IOAPIC_MAX;i++) {
        if (ioapics[i]>0) {
            ioapic = ioapics[i];
            if (ioapic->ioapicid==ioapicid) {
                log_msg("This I/O APIC is already registered skip!\n");
            }
        }
    }

    ioapic = kmalloc(sizeof(ioapic_t),NULL);


    ioapic->ioapicid = ioapicid;
    ioapic->ioapicadr = ioapicadr;
    ioapic->global_sys_intr_base = global_sys_intr_base;

    //Scan I/O APIC
    //mmio_write8(ioapicadr+0x00,0x00); //index id
    //uint32_t index = mmio_read32(ioapicadr+0x10); //data
    uint32_t index = apic_ioapic_read(ioapicadr,0x00);

    //mmio_write8(ioapicadr+0x00,0x01); //index id
    //uint32_t versionreg = mmio_read32(ioapicadr+0x10); //data
    uint32_t versionreg = apic_ioapic_read(ioapicadr,0x01);

    uint8_t version = bits_32_get(versionreg,0,7);
    uint8_t prq = bits_32_get(versionreg,15,15);
    uint8_t pincount = bits_32_get(versionreg,16,23)+1;

    log_msg("I/O APIC index = %d\n",index);
    log_msg("I/O APIC version = %#x (0x20=2.0)\n",version);
    log_msg("I/O APIC prq = %d\n",prq);
    log_msg("I/O APIC pincount = %d\n",pincount);

    ioapic->index = index;
    ioapic->version = version;
    ioapic->prq = prq;
    ioapic->pincount=pincount;

    ioapics[ioapic_count] = ioapic;
    ioapic_count++;
    return;
}

ioapic_t* apic_ioapic_get(uint8_t irq) {
    ioapic_t* ioapic;
    for (int i=0;i<IOAPIC_MAX;i++) {
        if (ioapics[i]>0) {
            ioapic = ioapics[i];
            if (irq>=ioapic->global_sys_intr_base
                && irq<(ioapic->global_sys_intr_base+ioapic->pincount)) {
                //found matching ioapic
                return ioapic;
            }
        }
    }
    return 0;
}

void apic_ioapic_route_irq(uint8_t irqno, uint8_t xapicid,uint8_t vector) {
    uint8_t triggermode = 1; //(level)
    uint8_t polarity = 1; //(active low)
    uint8_t irqsource = irqno; //(active low)

    //get override
    ioapic_irq_override_t* ovr = apic_ioapic_irqoverride_get(irqno);
    if (ovr!=NULL) {
        irqsource = ovr->irq_source;

        if ((ovr->flags & 0x03) != 0x03) {
            log_msg("Override polarity\n");
            if ((ovr->flags & 0x03) == 0x01) {
                log_msg("Override polarity as active high\n");
                polarity = 0; //active low
            }else {
                PANIC("Unsupported polarity override\n");
            }
        }
        if ( ((ovr->flags>>2) & 0x03) != 0x03) {
            log_msg("Override trigger mode\n");
            PANIC("Unsupported trigger mode override\n");
        }

        //build RT entry
        uint64_t e=0;
        e = bits_64_set(e, vector,0,7);     //isr vector , normally irq + IRQ_STARTVECTOR
        e = bits_64_set(e, 0,8,10);    //delivery mode 000 = fixed
        e = bits_64_set(e, 0,11,11);    //destination mode = physical
        e = bits_64_set(e, polarity,13,13);    //polarity, 1=low, 0=high
        e = bits_64_set(e, triggermode,15,15);    //trigger mode, 1=level
        e = bits_64_set(e, 1,16,16);    //1=masked ,
        e = bits_64_set(e, xapicid,56,63);   //destination

        ioapic_t* ioapic = apic_ioapic_get(irqsource);
        if (ioapic==NULL) {
            PANIC("NO I/O APIC found for IRQ. Is APIC supported on this system?\n");
        }
        apic_ioapic_write(ioapic->ioapicadr,0x10+2*irqsource+1,(uint32_t) (e>>32));
        apic_ioapic_write(ioapic->ioapicadr,0x10+2*irqsource,(uint32_t) (e & 0xFFFFFFFF));

        //unmask
        e = bits_64_set(e, 0,16,16);    //1=masked ,
        apic_ioapic_write(ioapic->ioapicadr,0x10+2*irqsource,(uint32_t) (e & 0xFFFFFFFF));
    }
    return;
}

void apic_lapic_init(void) {
    log_msg("APIC LAPIC init()\n");

    adr_t apic_lapic_ptr = cpulapicadr;
    if (apic_lapic_ptr==NULL) {
        PANIC("LAPIC adr is not configured, does CPU lack APIC support?");
    }

    lapic_wr(apic_lapic_ptr,LAPIC_REG_DFR, 0xffffffff);   // Flat mode
    lapic_wr(apic_lapic_ptr,LAPIC_REG_LDR, 0x01000000);   // All cpus use logical id 1


    lapic_wr(apic_lapic_ptr,APIC_REG_LVT_TMR,APIC_LVT_DISABLE);
    lapic_wr(apic_lapic_ptr,APIC_REG_LVT_PERF,APIC_LVT_DISABLE);
    lapic_wr(apic_lapic_ptr,APIC_REG_LVT_LINT0,APIC_LVT_DISABLE);
    lapic_wr(apic_lapic_ptr,APIC_REG_LVT_LINT1,APIC_LVT_DISABLE);

    // Clear task priority to enable all interrupts
    lapic_wr(apic_lapic_ptr,LAPIC_REG_TASKPRIOR, 0);

    //apic_enable();

    // Configure Spurious Interrupt Vector Register
    lapic_wr(apic_lapic_ptr,LAPIC_REG_SPURIOUS, 0x100 | 0xff);   // or by 0x100 to enable.

}

void apic_lapic_timer_init(uint8_t irqno) {
    log_msg("Init LAPIC Timer for this CPU \n");

    adr_t apic_lapic_ptr = cpulapicadr;
    //map APIC timer to an interrupt, and by that enable it in one-shot mode
    lapic_wr(apic_lapic_ptr,APIC_REG_LVT_TMR, (IRQ_STARTVECTOR+irqno) );  //map to irq0
    // Tell APIC timer to use divider 16
    lapic_wr(apic_lapic_ptr,APIC_REG_TMRDIV,0x03);

    lapic_wr(apic_lapic_ptr,APIC_REG_TMRINITCNT,0xFFFFFFFF);
    log_msg("Init LAPIC Timer read right after init 0x%x\n",lapic_rd(apic_lapic_ptr,APIC_REG_TMRCURRCNT));
    size_t samplemillis=10;
    mdelay(samplemillis);     //sleep 10ms
    //stop timer
    lapic_wr(apic_lapic_ptr,APIC_REG_LVT_TMR,APIC_LVT_DISABLE);
    size_t ticks_in_1ms = (0xFFFFFFFF - lapic_rd(apic_lapic_ptr,APIC_REG_TMRCURRCNT)) /samplemillis;
    log_msg("Init LAPIC Timer ticksIn1ms=%d\n",ticks_in_1ms);

    cpu_t* cpu = CURRENTCPU;
    cpu->ticks_in_1ms=ticks_in_1ms;

}

void apic_lapic_timer_start(uint8_t irqno, uint32_t intervallmillis){
    adr_t apic_lapic_ptr = cpulapicadr;
    cpu_t* cpu = CURRENTCPU;
    size_t ticks_in_1ms = cpu->ticks_in_1ms;
    uint32_t ticks = ticks_in_1ms*intervallmillis;
    log_msg("Start LAPIC Timer intervallmillis=%d totaltick=%d\n",intervallmillis,ticks);

    // Start timer as periodic on IRQ 0, divider 16, with the number of ticks we counted
    lapic_wr(apic_lapic_ptr,APIC_REG_TMRINITCNT,ticks);
    lapic_wr(apic_lapic_ptr,APIC_REG_TMRDIV,0x03);
    lapic_wr(apic_lapic_ptr,APIC_REG_LVT_TMR, (IRQ_STARTVECTOR+irqno)  | APIC_LVT_TMR_MODE_PERIODIC);
}

void apic_lapic_eoi(void) {
    adr_t apic_lapic_ptr = cpulapicadr;
    if (apic_lapic_ptr!=NULL) {
        //log_msg("APIC_lapic EOI: lapic_ptr=%#lx\n",apic_lapic_ptr);
        lapic_wr(apic_lapic_ptr,APIC_REG_EOI,0x0);
    }
}

void apic_start_ap(uint8_t apic_id, uint8_t vector) {
    adr_t apic_lapic_ptr = cpulapicadr;

    lapic_wr(apic_lapic_ptr,0x280, 0x0);   // clear APIC errors

    // 1. INIT IPI
    lapic_wr(apic_lapic_ptr,LAPIC_ICR_HIGH, (uint32_t)apic_id << 24);
    lapic_wr(apic_lapic_ptr,LAPIC_ICR_LOW, 0x00004500); // INIT, edge, assert
    lapic_wait_idle(apic_lapic_ptr);

    mdelay(10); // busy-wait 10 ms, PIT eller liknande

    // 2. Första SIPI
    lapic_wr(apic_lapic_ptr,LAPIC_ICR_HIGH, (uint32_t)apic_id << 24);
    lapic_wr(apic_lapic_ptr,LAPIC_ICR_LOW, 0x00004600 | vector);
    lapic_wait_idle(apic_lapic_ptr);
    mdelay(2);

    // 3. Andra SIPI (krävs enligt Intel MP-spec, äldre CPU:er kan behöva den)
    lapic_wr(apic_lapic_ptr,0x280, 0x0);   // clear APIC errors

    lapic_wr(apic_lapic_ptr,LAPIC_ICR_HIGH, (uint32_t)apic_id << 24);
    lapic_wr(apic_lapic_ptr,LAPIC_ICR_LOW, 0x00004600 | vector);
    lapic_wait_idle(apic_lapic_ptr);
    mdelay(2);
}

#endif // CONFIG_APIC