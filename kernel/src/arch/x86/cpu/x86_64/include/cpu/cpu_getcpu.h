#ifndef _CPU_GETCPU_H_
#define _CPU_GETCPU_H_

#include "types.h"
#include "cpu.h"
//#include "driver/apic.h"
#include "log.h"

extern cpu_t* apic_id_cpumap[MAXAPIC_LOGICAL_CPUID];

/*
static inline int cpu_get_xAPIC_id(void){
    uint32_t ebx;

    //get local APIC ID
    //https://c9x.me/x86/html/file_module_x86_id_45.html
    __asm__ __volatile__ (
            "movl  $0x000000001,%%eax\n"
            "cpuid\n"
            "movl  %%ebx,%0"
            : "=g" (ebx)
            :
            : "eax", "ebx", "ecx", "edx"
            );

    int xAPIXid = ebx>>24; //get_cpu_coreid();
    return xAPIXid;
}*/

static inline int arch_getcpu_xAPIC_logical_id(void){
    //PANIC("arch_getcpu_cpuid - NOT IMPLEMENTED!");
    unsigned int eax,ebx,ecx,edx;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    int xAPIXid = ebx>>24; //logicalid
    log_msg("arch_getcpu_xAPIC_logical_id return xAPIXid=%d\n",xAPIXid);
    //    int logicalid = cpu_get_xAPIC_id();
    return xAPIXid;
    //return 0;
}

static inline int arch_getcpu_cpuid(void){
    //PANIC("arch_getcpu_cpuid - NOT IMPLEMENTED!");
    unsigned int eax,ebx,ecx,edx;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    int xAPIXid = ebx>>24; //logicalid
    log_msg("arch_getcpu_cpuid return xAPIXid=%d\n",xAPIXid);
    //    int logicalid = cpu_get_xAPIC_id();
    return apic_id_cpumap[xAPIXid]->cpuid;
    //return 0;
}

static inline cpu_t* arch_getcpu_cpu(void){
    //PANIC("arch_getcpu_cpu - NOT IMPLEMENTED!");
    unsigned int eax,ebx,ecx,edx;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    int xAPIXid = ebx>>24; //logicalid
//    int logicalid = cpu_get_xAPIC_id();
    log_msg("arch_getcpu_cpu return xAPIXid=%d\n",xAPIXid);
    return apic_id_cpumap[xAPIXid];
    //return 0;
}

#define CURRENTCPU  (arch_getcpu_cpu())


#endif