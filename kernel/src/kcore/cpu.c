#include "types.h"
#include "error.h"
#include "vfb.h"
#include "driver/video/ega.h"
#include "driver/video/vga.h"
#include "bootmem.h"
#include "mm.h"
#include "cpu.h"
#include <cpuid.h>

cpu_t cpu[MAXCPU];
uint32_t cpu_numcores=0;   //count of detected cores


void cpu_init_cpustate(int cpun){
    cpu[cpun].cpuid=cpun;
    cpu[cpun].intenable=0;
    cpu[cpun].ncli=0;
    //cpu[cpun].currentproc=0;
    //cpu[cpun].ticks=0;
    cpu[cpun].status=CPU_STATUS_HALTED;
    cpu[cpun].ticks_in_1ms=0;
}


/*
int get_model(void)
{
    int ebx, unused;
    __cpuid(0, unused, ebx, unused, unused);
    return ebx;
}


int get_vendor(void)
{
    unsigned int eax,ebx,ecx,edx;
    __get_cpuid(0, &eax, &ebx, &ecx, &edx);

    char vendor[13];
    vendor[12] = '\0';

    vendor[0] = ebx & 0xff;
    vendor[1] = ebx>>8 & 0xff;
    vendor[2] = ebx>>16 & 0xff;
    vendor[3] = ebx>>24 & 0xff;

    vendor[4] = edx & 0xff;
    vendor[5] = edx>>8 & 0xff;
    vendor[6] = edx>>16 & 0xff;
    vendor[7] = edx>>24 & 0xff;

    vendor[8] = ecx & 0xff;
    vendor[9] = ecx>>8 & 0xff;
    vendor[10] = ecx>>16 & 0xff;
    vendor[11] = ecx>>24 & 0xff;

    log_msg("vendor=%s\n", vendor);

    return eax;
}

int check_apic(void)
{
    unsigned int eax, unused, edx;
    __get_cpuid(1, &eax, &unused, &unused, &edx);
    return edx & (bit_APIC);
}

void testcpu(void) {
    log_msg("TEST CPU");

    int model = get_model();
    log_msg("model=%d 0x%x\n", model,model);

    int checkapic = check_apic();
    log_msg("checkapic=%d 0x%x\n", checkapic,checkapic);

    int maxleaf = get_vendor();
    log_msg("maxleaf=%d 0x%x\n", maxleaf,maxleaf);

}*/