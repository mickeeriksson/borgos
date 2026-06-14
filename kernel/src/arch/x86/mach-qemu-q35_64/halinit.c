#include "types.h"
#include "error.h"
#include "vfb.h"
#include "driver/video/ega.h"
#include "driver/video/vga.h"
#include "bootmem.h"
#include "mm.h"
#include "cpu.h"
#include "cpu/cpu_cpu.h"
#include "cpu/cpu_getcpu.h"
#include "driver/acpi.h"
#include "driver/hpet.h"
#include "driver/vsystimer.h"
#include "bits.h"
#include "delay.h"
#include "irq.h"
#include <cpuid.h>

extern void bootdebug_init(void);
extern void bootdebug_putc(uint8_t c);
extern void bootdebug_printf (const char *format, ...);

extern void log_init(int (*putchar_ptr)(int));
extern void log_msg(const char *format, ...);

extern uint64_t _bootinfosignature;
extern uint64_t _bootinfoaddr;
extern adr_t _bootimage_start;
extern adr_t _bootimage_end;

extern adr_t bootimage_start; //This should be a phys address
extern adr_t bootimage_end; //This should be a phys address

//extern adr_t bootmem_start_address;

extern void multiboot2_init(adr_t bootinfoaddr);
extern RESULT multiboot2_set_boot_vfb(struct fb_info* vfb);
extern void multiboot2_verify_freemem(void);
extern RESULT multiboot2_set_bootmem(struct bootmem_info* bootmem,adr_t bootimage_end);
extern RESULT multiboot2_set_bootmem_map(struct bootmem_info* bootmem) ;

extern void arch_cpu_debugprint( cpu_t* cpup) ;
extern void arch_cpu_init(int cpuid) ;
extern int arch_cpu_hasfeature_APIC(void);

extern void phys_init(void);
extern void phys_freepages_high(void);

extern page_t* page_alloc_pages(uint32_t flags, unsigned int order);

extern void gdt_init(void);
extern void idt_init(void);
extern void mmu_init_kernel_pagetable(void);
extern void mmu_map_device_region(adr_t adrstart,adr_t adrend);

extern void kmain_bp_enter(void);

extern void kmalloc_init(void);
extern void kmalloc_debug_walk(void);

extern void apic_init(void) ;
extern void apic_set_lapic_addr(adr_t local_apic_addr) ;
extern void apic_lapic_init(void);
extern void apic_lapic_timer_init(uint8_t irqno);
extern void apic_lapic_timer_start(uint8_t irqno, uint32_t intervallmillis);

extern void pic8259_init_disabled(void);


struct fb_info boot_vfb;
struct fb_ops boot_vfb_ops;

struct vsystimer_info vsystimerinfo;
struct vsystimer_ops vsystimerops;
struct hpetinfo hpetinfo;

int ksimpleputchar(int c) {
    bootdebug_putc(c);
    return 0;
}

int klogchar(int c) {
    bootdebug_putc(c);
    return 0;
}

void hal_init_with_bootinfo2(adr_t bootinfoaddr ) {
    //parse multiboot2
    multiboot2_init(bootinfoaddr);
    if (multiboot2_set_boot_vfb(&boot_vfb) == OK) {
        uint64_t vfb_pfn = boot_vfb.fbaddr / PAGESIZE;
        uint64_t maxvga_pfn = 256;
        switch(boot_vfb.type) {
            case VFB_TEXT:
                boot_vfb.fbops = &boot_vfb_ops;
                fb_text_init_default_ega(&boot_vfb);
                kprintf("INIT kkonsole as VFB_TEXT\n");
                break;
            case VFB_RGB:
                log_msg("vfb_pfn=%d  maxvga_pfn=%d\n", vfb_pfn,maxvga_pfn);
                if (vfb_pfn>maxvga_pfn) {
                    log_msg("Cant map kkonsole > 1meg, wait for highmem pagetable\n");
                }else {
                    boot_vfb.fbops = &boot_vfb_ops;
                    fb_rgb_init_default_vga(&boot_vfb);
                    kprintf("INIT kkonsole as VFB_RGB\n");
                }
                break;
            default:
                log_msg("Unable to set ops for VFB of type %d\n", boot_vfb.type);
        }
    }else {
        log_msg("Failed to VFB continue without kconsole\n");
    }

    //adr_t bstart = (adr_t) &_bootimage_start;
    //adr_t bend = (adr_t) &_bootimage_end;
    //log_msg("bstart: 0x%x\n", bstart);
    //log_msg("bend: 0x%x\n", bend);

    bootimage_start = (adr_t) KERNEL_PHYS_BASE; //This should be a phys address
    bootimage_end = (adr_t) V2P(&_bootimage_end); //This should be a phys address
    log_msg("bootimage_start: 0x%x\n", bootimage_start);
    log_msg("bootimage_end: 0x%x\n", bootimage_end);
    multiboot2_verify_freemem();
    if (multiboot2_set_bootmem(&bootmeminfo,bootimage_end) == OK) {
        kprintf("INIT bootmem at 0x%x\n", bootmeminfo.addr);
    }else {
        PANIC("Failed to set bootmem from multiboot2\n");
    }

    multiboot2_set_bootmem_map(&bootmeminfo);

}

void hal_init_bootinfo(void) {
    //adr_t bootimage_end = (adr_t) &_bootimage_end;
    //log_msg("_bootimage_end: 0x%x\n", bootimage_end);


    uint64_t signature = _bootinfosignature;
    uint64_t addr = _bootinfoaddr;

    log_msg("signature: 0x%x\n", signature);
    log_msg("addr: 0x%x\n", addr);

    switch(signature) {
        case 0x2BADB002:
            log_msg("bootinfo1 magic signature: 0x%x (NOT SUPPORTED)\n", signature);
            break;
        case 0x36d76289:
            log_msg("bootinfo2 magic signature: 0x%x\n", signature);
            hal_init_with_bootinfo2(addr);
            break;
        case 0x71FF0EF1:
            log_msg("UEFI magic signature: 0x%x (NOT SUPPORTED)\n", signature);
            break;
        default:
            log_msg("Unknown bootinfo magic signature: 0x%x\n", signature);
            break;
    }
}

/*
void hal_init_cpu(int cpuid) {
    log_msg("\n\n\nGET SOME INFO ABOUT BP CPU\n\n");

    cpu_init_cpustate(cpuid);

    cpu_t *cpup = &cpu[cpuid];
    cpup->cpuid=cpuid;
    struct arch_cpu_info* acpu = &cpup->archcpu;

    //get vendorstring
    unsigned int eax,ebx,ecx,edx;

    //get vendor & maxleafs
    __get_cpuid(0, &eax, (uint32_t *) &acpu->cpuid_vendorstring[0], (uint32_t *) &acpu->cpuid_vendorstring[8], (uint32_t *) &acpu->cpuid_vendorstring[4]);
    acpu->cpuid_vendorstring[12]=0; // terminate
    acpu->cpuid_maxbasicleafs=eax;

    //get features
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    acpu->cpuid_feature_ebx = ebx;
    acpu->cpuid_feature_ecx = ecx;
    acpu->cpuid_feature_edx = edx;

    if (edx & CPUID_FEAT_EDX_APIC) {
        int xAPIXid = ebx>>24; //get_cpu_coreid();
        acpu->xapic_logicalid = xAPIXid;
        apic_id_cpumap[xAPIXid] = cpup;
    }
}*/

void hal_init_bootvfb_high() {
    if (boot_vfb.type>0) {
        if (boot_vfb.type == VFB_RGB) {
            adr_t fbaddr = boot_vfb.fbaddr;
            size_t fbsize = boot_vfb.fbpitch*boot_vfb.fbheight;
            log_msg("VFB Map address: 0x%lx - 0x%lx\n", fbaddr, fbaddr+fbsize);

            int pfnstart = PHYS2PFN(fbaddr);
            int pfnend = PHYS2PFN(fbaddr+fbsize);
            for (int p=pfnstart; p<=pfnend; p++) {
                //log_msg("MAP VFB device page %d at phys 0x%lx\n",p,PFN2PHYS(p));
                mmu_map_page( kernelpagetable,PFN2PHYS(p),PFN2VIRT(p),MMU_DEVICE);
            }
            boot_vfb.fbops = &boot_vfb_ops;
            boot_vfb.fbaddr = P2V(fbaddr);
            fb_rgb_init_default_vga(&boot_vfb);
            kprintf("INIT kkonsole as VFB_RGB at fbaddr = 0x%lx\n", boot_vfb.fbaddr);
        }
    }

/*


        switch(boot_vfb.type) {
            case VFB_TEXT:
                boot_vfb.fbops = &boot_vfb_ops;
                fb_text_init_default_ega(&boot_vfb);
                kprintf("INIT kkonsole as VFB_TEXT\n");
                break;
            case VFB_RGB:

                boot_vfb.fbops = &boot_vfb_ops;
                fb_rgb_init_default_vga(&boot_vfb);
                kprintf("INIT kkonsole as VFB_RGB\n");
                break;
            default:
                log_msg("Unable to set ops for VFB of type %d\n", boot_vfb.type);
        }
    }else {
        log_msg("boot VFB not set, continue without kconsole\n");
    }*/
}


extern void testcpu(void);


void hal_init_mem(void) {
    gdt_init();
    idt_init();
    /*
    int num = 8;
    int divisor = 0;

    int div = num / divisor;
    log_msg("AFTER div = %d",div);
    */
    mmu_init_kernel_pagetable();
    log_msg("Switch to new kernelpagetable CR3 DONE!\n");
    kprintf("Switch to new kernelpagetable CR3 DONE!\n");

    //Map BIOS
    log_msg("MAP BIOS 0x80000-0xFFFFF as DEVICE MEM\n");
    mmu_map_device_region(0x00080000, 0x000FFFFF);
    log_msg("MAP BIOS 0x3FDF000-0x3FFFFFF as DEVICE MEM\n");
    mmu_map_device_region(0x3FDF000, 0x3FFFFFF);
    log_msg("MAP BIOS 0xFEC00000-0xFFFFFFFF as DEVICE MEM\n");
    mmu_map_device_region(0xFEC00000, 0xFFFFFFFF);


    //Init Malloc
    kmalloc_init();
    kmalloc_debug_walk();
}



void hal_bp_restack(void) {
    log_msg("HAL BP Start!\n");
    kprintf("HAL BP Start!\n");

    //Allocate new stack for BP
    page_t* stackpage = page_alloc_pages(0, 5); //128Kb
    adr_t stackaddr = PAGE2VIRT(stackpage);
    size_t stacksize = PAGESIZE << 5;
    log_msg("BP stack addr: 0x%lx  size:0x%lx\n", stackaddr,stacksize);

    cpu_t* bpcpu = &cpu[0];
    bpcpu->stackadr = stackaddr;
    bpcpu->stacksize = stacksize;
    adr_t newrsp = stackaddr + stacksize - 64;
    log_msg("Set BP stackpointer to : 0x%lx\n", newrsp);

    __asm__ __volatile__(
       "mov %0, %%rsp\n"
       "call kmain_bp_enter\n"
        : /* Inga utmatningar */
        : "r" (newrsp) /* Inmatning: Den nya adressen */
        : "rsp", "memory"     /* Berätta för Clang att rsp och minnet har ändrats */
    );

    PANIC("hal_bp_restack, should never get here!");
}


void hal_init_hpet(void) {
    // INIT HPET
    HPET_t* hpet = (HPET_t*) acpi_find("HPET");
    log_msg("got hpet at 0x%lx\n",hpet);

    adr_t hpetaddr = P2V(hpet->address.address);
    adr_t hpetaddrspace_id = hpet->address.address_space_id;

    log_msg("Init HPET at 0x%lx spaceid=%d\n", hpetaddr, hpetaddrspace_id);
    hpetinfo.base = hpetaddr;
    hpet_init(&hpetinfo);

    vsystimerinfo.private_timerdata = &hpetinfo;
    vsystimerinfo.ops = &vsystimerops;
    vsystimerops.getmillis = hpet_vsystimer_get_millis;
    vsystimer_init(&vsystimerinfo);

    uint64_t millis = vsystimer_millis();
    log_msg("millis=0x%lx (%d)\n", millis,millis);
    return;
}


void hal_parse_APIC_from_madt(void) {
    log_msg("\nPARSE APIC\n");
    MADT_t* madt = (MADT_t*) acpi_find("APIC");
    log_msg("GOT MADT at 0x%lx\n", madt);

    adr_t lapicadr = madt->local_apic_addr;
    uint64_t madtflags = madt->flags;
    log_msg("lapicadr=0x%lx flags=0x%x\n", lapicadr,madtflags);
    apic_set_lapic_addr(P2V(lapicadr)) ;

    int numcores = 1;
    cpu_t* cpu = CURRENTCPU;
    log_msg("numcores=%d cpu->archcpu.xapic_logicalid=%d\n", numcores,cpu->archcpu.xapic_logicalid);


    adr_t entriesstart = (adr_t) madt + sizeof(MADT_t);
    uint64_t entriessize = madt->header.length - sizeof(MADT_t);
    void* entryptr = (void*) entriesstart;
    while (entryptr < (void*) (entriesstart + entriessize)) {
        MADT_entry_t* entry = (MADT_entry_t*) entryptr;
        log_msg("PARSE MADT Entry type = %d\n", entry->type);
        if (entry->type == MADT_type_CPULAPIC) {
            log_msg("  PARSE Entry Type 0: Processor Local APIC\n");
            struct MADT_entry_CPULAPIC* e = (struct MADT_entry_CPULAPIC*) entryptr;
            //log_msg("    acpiid=%d\n", e->acpiid);
            //log_msg("    apicid=%d\n", e->apicid);

            if (e->apicid == cpu->archcpu.xapic_logicalid) {
                //BP
                log_msg("    This IS BP, skip mapping, already done\n");
            }else {
                //AP
                log_msg("    MAP AP cpuid=%d to apicid=%d\n",numcores,e->apicid);
                cpu_t* ap = &cpu[numcores];
                cpu_init_cpustate(numcores);
                ap->archcpu.xapic_logicalid=e->apicid;
                apic_id_cpumap[e->apicid] = ap;
                numcores++;
                cpu_numcores++;
            }
        }else if (entry->type == MADT_type_IOAPIC) {
            log_msg("  PARSE Entry Type 1: I/O APIC\n");
        }else if (entry->type == MADT_type_IOAPICINTOVERRIDE) {
            log_msg("  PARSE Entry Type 2: I/O APIC Interrupt Source Override\n");
        }else if (entry->type == MADT_type_IOAPICNMISOURCE) {
            log_msg("  PARSE Entry type 3: I/O APIC Non-maskable interrupt source\n");
        }else if (entry->type == MADT_type_LAPICNMI) {
            log_msg("  PARSE Entry Type 4: Local APIC Non-maskable interrupts\n");
        }else if (entry->type == MADT_type_LAPICADR) {
            log_msg("  PARSE Entry Type 5: Local APIC Address Override\n");
        }else if (entry->type == MADT_type_CPUX2APIC) {
            log_msg("  PARSE Entry Type 9: Processor Local x2APIC\n");
        }else {
            log_msg("  UNKOWN Entry Type %d\n", entry->type);
        }
        entryptr = entryptr + entry->size;
    }

    log_msg("MADT PARSING DONE!");

}

void hal_init_interrupts() {
    //called once,only from BP

    log_msg("Init Interrupts\n");
    log_msg("Disable PIC\n");
    pic8259_init_disabled();

    //Spurious handler is harcoded to 0xFF in isr.c
    //No need to declare it here
    //TODO wire spurios to reduced assemler stub in isr_inc.S



    //PANIC("TODO hal_init_interrupts");
}

void hal_init_cpuinterrupts() {
    //called once from each BP/AP
    apic_lapic_init();
    //PANIC("TODO hal_init_cpuinterrupts!");
}

void hal_start_cpuinterrupts() {
    //called once from each BP/AP

    cpu_t* cpu = CURRENTCPU;
    //volatile int irqBefLog = cpu_read_irq();
    cpu->intenable = 1;
    cpu_enable_irq();
    //log_msg("**********************************************************************\n");
    //log_msg("intenable=%d irqon=%d irqBefLog=%d\n",cpu->intenable,cpu_read_irq(),irqBefLog);
    apic_lapic_timer_init(0); //init timer on irq=0
    irq_add_irq_handler(0,irq_cpu_localtimer_tick_cb);
    //log_msg("**********************************************************************\n");
    //log_msg("intenable=%d irqon=%d\n",cpu->intenable,cpu_read_irq());
    //apic_lapic_timer_start(0,1000);
}


//extern void acpi_test_sdt(void);
void hal_bp_init(void) {
    //called from kmain_bp_enter

    //Only done for BP.
    log_msg("\nPARSE ACPI\n");
    acpi_debugprint_entries();

    hal_init_hpet();

    //test if CPU support APIC features.
    if (arch_cpu_hasfeature_APIC()==0) {
        PANIC("NO APIC Support!");
    }
    apic_init(); //does nothing for now
    hal_parse_APIC_from_madt();
    delay_initcpu();

    /*
    log_msg("********************** Start delay test\n");
    mdelay(1000);
    log_msg("-------\n");
    mdelay(1000);
    log_msg("-------\n");
    mdelay(1000);
    log_msg("-------\n");
    */
    irq_inithandlers();
    hal_init_interrupts();

    //The following is done both for BP and AP.
    hal_init_cpuinterrupts();
    hal_start_cpuinterrupts();

    /*
    while (1) {
        mdelay(100);
        cpu_t* cpu = CURRENTCPU;
        int irqOn = cpu_read_irq();
        log_msg("HAL: intenable=%d irqon=%d\n",cpu->intenable,irqOn);
    }*/

    //PANIC("TODO hal_bp_init!");

}

void hal_ap_init(void) {
    log_msg("hal_ap_init\n");
    int cpuid = arch_getcpu_cpuid();
    arch_cpu_init(cpuid);
    arch_cpu_debugprint(&cpu[cpuid]);



    PANIC("TODO LOAD IDT for AP"); //interrupt::init_exceptions_ap();  //loads IDT (same as for BSP)
    //load IDT HERE!
    delay_initcpu();
    hal_init_cpuinterrupts();
    hal_start_cpuinterrupts();
    /*
    let irqbforelog = interrupt::cpu_read_irq();
    let cpu = cpu::getCurrentCPU();
    cpu.intenable=true;
    interrupt::cpu_enable_irq();  //start interrupts on this processor (AP)
    debug!("**********************************************************************\n");
    debug!("intenable={} irqon={} irqbforelog={}",cpu.intenable,interrupt::cpu_read_irq(),irqbforelog);
    apic::apic_lapic_timer_init(0);
    //idt_set_irq_kernelvector((adr_t)irq0,IRQ_STARTVECTOR+0);
    //interrupt::register_irq_handler(0,123); //excahnge 123 with functionpointer
    debug!("**********************************************************************\n");
    debug!("intenable={} irqon={}\n",cpu.intenable,interrupt::cpu_read_irq());
    //apic::apic_lapic_timer_start(0,1500);
    */
    PANIC("TODO hal_ap_init!");
}

void hal_start(void) {
    //enter here from boot.s
    bootdebug_init();
    bootdebug_printf("\n-------------- HAL (hal_init_early): ----------------\n");

    log_init(klogchar);
    log_msg("* Init logging!\n");

    //Test if this is BP
    int logicalcpuid = arch_getcpu_xAPIC_logical_id();
    if (logicalcpuid > 0){
        //halt forever
        log_msg("logicalcpuid %d, halt until BP is started\n", logicalcpuid);
        while (1);
    }


    //init cpus
    for (int n=0; n<MAXCPU; n++) {
        cpu_init_cpustate(n);
    }

    arch_cpu_init(0);
    cpu_numcores=1; //only boot processor so far

    hal_init_bootinfo();

    //TEST SOME CPU STUFF
    //log_msg("\n\n\nTEST SOME CPU STUFF\n\n");
    //testcpu();

    arch_cpu_debugprint( &cpu[0] );

    bootmem_debugprint();
    bootmem_memmnap_debugprint();

    log_msg("I***********************************************************************\n");
    log_msg("I*                         PAGEMAP INIT!                               *\n");
    log_msg("I***********************************************************************\n");
    phys_init();
    log_msg("hal_start Init Phys Done!\n");


    log_msg("***********************************************************************\n");
    log_msg("*                     GDT + IDT + PAGETABLE INIT!                     *\n");
    log_msg("***********************************************************************\n");
    hal_init_mem();    // setup allocators, pagetables and switch to kernelpagetable.
    log_msg("hal_start Init Paging, Done!\n");
    //mem::phys::init_pagemap_high();
    phys_freepages_high();
    log_msg("hal_start Init Phys (High), Done!\n");

    //page_debugprint_free();

    hal_init_bootvfb_high() ;

    hal_bp_restack(); //reset stack and go to kmain_bp_enter
    //Never to return here.
}
