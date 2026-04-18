#include "types.h"
#include "error.h"
#include "vfb.h"
#include "driver/video/ega.h"
#include "driver/video/vga.h"
#include "bootmem.h"
#include "mm.h"
#include "cpu.h"
#include "cpu/cpu_cpu.h"
#include <cpuid.h>

extern void bootdebug_init(void);
extern void bootdebug_putc(uint8_t c);
extern void bootdebug_printf (const char *format, ...);

extern void log_init(int (*putchar_ptr)(int));
extern void log_msg(const char *format, ...);

extern uint64_t _bootinfosignature;
extern uint64_t _bootinfoaddr;
extern adr_t _bootimage_end;
//extern adr_t bootmem_start_address;

extern void multiboot2_init(adr_t bootinfoaddr);
extern RESULT multiboot2_set_boot_vfb(struct fb_info* vfb);
extern void multiboot2_verify_freemem(void);
extern RESULT multiboot2_set_bootmem(struct bootmem_info* bootmem,adr_t bootimage_end);

extern void arch_cpu_debugprint( cpu_t* cpup) ;
extern struct cpustate *apic_id_cpumap[MAXAPIC_LOGICAL_CPUID];

struct fb_info boot_vfb;
struct fb_ops boot_vfb_ops;

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
        log_msg("Failed to VFB continue without kconsole\n");
    }


    adr_t bootimage_end = (adr_t) V2P(&_bootimage_end); //This should be a phys address
    log_msg("_bootimage_end: 0x%x\n", bootimage_end);
    multiboot2_verify_freemem();
    if (multiboot2_set_bootmem(&bootmeminfo,bootimage_end) == OK) {
        kprintf("INIT bootmem at 0x%x\n", bootmeminfo.addr);
    }else {
        PANIC("Failed to set bootmem from multiboot2\n");
    }


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

}


extern void testcpu(void);

void hal_start(void) {
    bootdebug_init();
    bootdebug_printf("\n-------------- HAL (hal_init_early): ----------------\n");

    log_init(klogchar);
    log_msg("* Init logging!\n");

    hal_init_cpu(0);
    cpu_numcores=1; //only boot processor so far

    hal_init_bootinfo();

    //TEST SOME CPU STUFF
    //log_msg("\n\n\nTEST SOME CPU STUFF\n\n");
    //testcpu();

    arch_cpu_debugprint( &cpu[0] );

    while(1);
}
