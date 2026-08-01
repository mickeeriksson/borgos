#include "types.h"
#include "config.h"
#include "cpu.h"
#include "cpu/mmio.h"
#include "delay.h"
#include "error.h"

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