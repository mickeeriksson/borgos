#ifndef _CPU_ARCHCPU_H_
#define _CPU_ARCHCPU_H_

#include "types.h"
#include "log.h"
#include <cpuid.h>

//struct cpustate;
//extern struct cpustate *apic_id_cpumap[MAXAPIC_LOGICAL_CPUID];

struct arch_cpu_info {
    uint8_t     acpi_id;                    // ID from ACPI MADT tables
    uint8_t     xapic_logicalid;            // 8 bit  xAPIC ID
    uint32_t    x2apic_logicalid;           // 32 bit x2APIC ID

    uint32_t    cpuid_maxbasicleafs;
    char cpuid_vendorstring[32];
    uint32_t cpuid_feature_ebx;
    uint32_t cpuid_feature_ecx;
    uint32_t cpuid_feature_edx;

};

#define CPU_FLAG_IF 0x200

//https://wiki.osdev.org/CPUID
#define CPUID_FEAT_EDX_APIC         (1 << 9)   // 0x200


/* issue a single request to CPUID.*/
/*
static inline void cpu_cpuid(int code, uint32_t *eax, uint32_t *ebx,uint32_t *ecx,uint32_t *edx) {
//    __asm__ volatile("cpuid":"=a"(*eax),"=d"(*edx):"a"(code):"ecx","ebx");
    __asm__ volatile("cpuid":"=a"(*eax),"=b"(*ebx),"=c"(*ecx),"=d"(*edx):"a"(code):);
}
*/
/*
static inline uint32_t cpu_hasfaeture_APIC(void) {
    unsigned int eax, unused, edx;
    cpu_cpuid(1, &eax, &unused, &unused, &edx);
    return edx & CPUID_FEAT_EDX_APIC;
}
static inline int cpu_get_maxcpuidfunc(void)
{
    unsigned int eax, unused;
    cpu_cpuid(0, &eax, &unused, &unused, &unused);
    return eax;
}
static inline void cpu_get_cpustring(char* str)
{
    unsigned int eax;
    cpu_cpuid(0, &eax, (uint32_t *) &str[0], (uint32_t *) &str[8], (uint32_t *) &str[4]);
    //return ;
}
*/


/** issue a complete request, storing general registers output as a string
 */
//static inline int cpuid_string(int code, uint32_t where[4]) {
//    asm volatile("cpuid":"=a"(*where),"=b"(*(where+1)),
//    "=c"(*(where+2)),"=d"(*(where+3)):"a"(code));
//    return (int)where[0];
//}

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
}



static inline int cpu_has_RDTSC_support(void){
    uint32_t edx;

    //Get info from APIC for this CPU core
    __asm__ __volatile__ (
            "movl  $0x000000001,%%eax\n"
            "cpuid\n"
            "movl  %%edx,%0"
            : "=g" (edx)
            :
            : "eax", "ebx", "ecx", "edx"
            );
    //RDTSC instruction support is ruturned in bit 4 in edx register
    return (edx & 0x10);
}

// read the Time Stamp Count
//static inline uint64_t rdtsc() {
static inline uint64_t cpu_getticks64(void) {
    uint64_t val;
    __asm__ __volatile__ ("rdtsc" : "=A" (val));
    return val;
}

// n = amount of rdtsc timer ticks to delay
//static inline void _delay(const uint64_t n) {
static inline void cpu_delayticks(const uint64_t n) {
    volatile const uint64_t timeout = (cpu_getticks64() + n);
    while (cpu_getticks64() < timeout)
        ;
}

*/

static inline int cpu_read_irq(void){
    reg_t flags;


 // * "=rm" is safe here, because "pop" adjusts the stack before
 // * it evaluates its effective address -- this is part of the
 // * documented behavior of the "pop" instruction.

    __asm__ volatile(
            "pushf \n"
            "pop %0"
            : "=rm" (flags)
            : /* no input */
            : "memory");
    //log_msg("raw FLAGS = %#x\n",flags);
    return ((flags & CPU_FLAG_IF)>>9);
}


static inline int cpu_disable_irq(void)
{
    int old = cpu_read_irq();
    //log_msg("--------------irq before cli is %d\n",old);
    __asm__ volatile("cli");
    return old;
}

static inline void cpu_enable_irq(void)
{
    __asm__ volatile("sti");
}

//extern uint64_t get_cpu_sp(void);

//extern int spinlock(void* lock);
//extern int spinunlock(void* lock);
//extern int spinlock_test(void* lock);


//extern uint64_t cpu_get_current_el(void);

//extern void cpu_enable_irq(void);
//extern uint32_t cpu_disable_irq(void);
//extern uint32_t cpu_read_irq(void);
//extern uint32_t cpu_read_DAIF(void);

#endif